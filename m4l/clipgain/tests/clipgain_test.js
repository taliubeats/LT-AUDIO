// LT CLIP GAIN tests (macOS JavaScriptCore):
//   /System/Library/Frameworks/JavaScriptCore.framework/Versions/A/Helpers/jsc m4l/clipgain/tests/clipgain_test.js
var failures = 0;
function check(ok, what) { print((ok ? "  ok    " : "  FAIL  ") + what); if (!ok) failures++; }
var shown = [[], []];
function outlet(i, sel, text) { shown[i].push(text); }
function post() {}

// a fake clip whose gain curve is not linear (Live's real curve is not published)
function curveDb(v) { return v <= 0 ? -Infinity : -70 + 94 * Math.pow(v, 0.6); }
function fmt(db) { return isFinite(db) ? (db < 0 ? "−" : "") + Math.abs(db).toFixed(1) + " dB" : "-inf dB"; }
var clip = { id: 7, gain: 0.5, audio: 1, name: "Vox Lead", sets: 0 };
var callbacks = [];
function LiveAPI(cb, path) {
    this.id = path === "live_set view" ? 1 : clip.id;
    this.cb = cb;
    callbacks.push(this);
    var self = this;
    this.get = function (prop) {
        if (prop === "gain_display_string") return [fmt(curveDb(clip.gain))];
        if (prop === "is_audio_clip") return [clip.audio];
        if (prop === "name") return ["Vox", "Lead"];
        if (prop === "gain") return [clip.gain];
        return [0];
    };
    this.set = function (prop, v) { if (prop === "gain") { clip.gain = v; clip.sets++; if (self.property === "gain") self.cb && self.cb(["gain", v]); } };
}
load("m4l/clipgain/lt_clipgain.js");

print("parsing");
check(parseDb("−3.2 dB") === -3.2 && parseDb("0 dB") === 0 && parseDb("12.5 dB") === 12.5 && parseDb("-inf dB") === -Infinity, "reads Live's dB strings (minus sign, -inf)");

print("steps");
init();
callbacks[0].cb(["detail_clip", "id", 7]);
check(shown[0][shown[0].length - 1] === "Vox Lead", "shows the selected clip's name");
var start = curveDb(clip.gain);
step(1);
check(Math.abs(curveDb(clip.gain) - (start + 1)) < 0.05, "+1 dB from " + start.toFixed(2) + " -> " + curveDb(clip.gain).toFixed(2));
step(-3);
check(Math.abs(curveDb(clip.gain) - (start - 2)) < 0.1, "-3 dB (Live shows one decimal, so steps are exact to 0.1 dB) -> " + curveDb(clip.gain).toFixed(2));
unity();
check(Math.abs(curveDb(clip.gain)) < 0.05, "0 dB -> " + curveDb(clip.gain).toFixed(3));
clip.gain = 0;
step(1);
check(isFinite(curveDb(clip.gain)) && Math.abs(curveDb(clip.gain) + 24) < 0.1, "from -inf, a step up starts at -24 dB -> " + curveDb(clip.gain).toFixed(2));
clip.gain = 0.99;
step(3);
check(curveDb(clip.gain) <= 24.01, "never goes past the top of the range");
clip.audio = 0;
var before = clip.gain;
step(1);
check(clip.gain === before && shown[1][shown[1].length - 1] === "MIDI clip", "MIDI clips are left alone");

print(failures === 0 ? "ALL PASSED" : failures + " FAILED");
if (failures) throw new Error(failures + " failed");
