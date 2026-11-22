// LT CLIP GAIN: change the gain of the selected audio clip in exact dB steps.
// Map the buttons to keys (Live's Key Map) or MIDI and nudge clips without opening the clip view.
//
// Max for Live audio effect, [js] object (ES5, Live 11 and 12).
// Inlet : init, step <dB> (e.g. step 1, step -3), unity
// Outlet 0: clip name for the label ("set ...")
// Outlet 1: gain readout ("set ...")

autowatch = 1;
inlets = 1;
outlets = 2;

var viewApi = null, clipApi = null;
var busy = false;

// ======================================================================= dB helpers (pure)
/** "-3.2 dB", "−3.2 dB", "-inf dB", "0 dB" -> number (-Infinity for -inf). NaN if unreadable. */
function parseDb(text) {
    var t = String(text).replace("−", "-").toLowerCase();
    if (t.indexOf("inf") >= 0) return -Infinity;
    var m = /(-?\d+(\.\d+)?)/.exec(t);
    return m ? parseFloat(m[1]) : NaN;
}

/**
 * Finds the 0..1 gain value whose dB reading is closest to target, by bisection.
 * readDb(value) must set the value and return the dB Live reports for it.
 * The reading only ever grows with the value, so 14 halvings land within ~0.01 dB.
 */
function solveGain(targetDb, readDb, lo, hi) {
    lo = lo === undefined ? 0 : lo;
    hi = hi === undefined ? 1 : hi;
    var best = hi, bestErr = Infinity;
    for (var i = 0; i < 14; i++) {
        var mid = (lo + hi) * 0.5;
        var db = readDb(mid);
        var err = Math.abs(db - targetDb);
        if (err < bestErr) { bestErr = err; best = mid; }
        if (err < 0.01) break;
        if (db < targetDb) lo = mid; else hi = mid;
    }
    return best;
}

// ======================================================================= Live side
function first(api, prop) { var v = api.get(prop); return (v instanceof Array) ? v[0] : v; }

function show() {
    if (!clipApi || Number(clipApi.id) === 0) { outlet(0, "set", "No clip selected"); outlet(1, "set", "-"); return; }
    var name = clipApi.get("name");
    outlet(0, "set", (name instanceof Array) ? name.join(" ") : String(name));
    if (Number(first(clipApi, "is_audio_clip")) !== 1) { outlet(1, "set", "MIDI clip"); return; }
    outlet(1, "set", String(first(clipApi, "gain_display_string")));
}

function onView(args) {
    // "detail_clip id N" whenever the highlighted clip changes
    var flat = [].concat(args);
    var id = Number(flat[flat.length - 1]);
    if (!clipApi) clipApi = new LiveAPI(onClip, "id " + id);
    else clipApi.id = id;
    if (id > 0) clipApi.property = "gain";
    show();
}

function onClip() { if (!busy) show(); }

function init() {
    viewApi = new LiveAPI(onView, "live_set view");
    viewApi.property = "detail_clip";
    show();
}

function currentDb() { return parseDb(first(clipApi, "gain_display_string")); }

function moveTo(targetDb) {
    if (!clipApi || Number(clipApi.id) === 0 || Number(first(clipApi, "is_audio_clip")) !== 1) { show(); return; }
    busy = true;
    var value = solveGain(targetDb, function (v) { clipApi.set("gain", v); return currentDb(); });
    clipApi.set("gain", value);
    busy = false;
    show();
}

/** step 1 = +1 dB, step -3 = -3 dB. From -inf, a step up starts at -24 dB. */
function step(db) {
    if (!clipApi || Number(clipApi.id) === 0) { show(); return; }
    var now = currentDb();
    if (!isFinite(now)) now = db > 0 ? -24 - db : now;
    if (isNaN(now)) { show(); return; }
    moveTo(Math.max(-70, Math.min(24, now + db)));
}

function unity() { moveTo(0); }

if (typeof module !== "undefined") module.exports = { parseDb: parseDb, solveGain: solveGain };
