// LT VACUUM: finds the tracks whose signal never reaches the Main/Master (directly, through a
// group, a return or another track) and deletes them, in two steps: SCAN, then DELETE.
//
// Runs in Max for Live's [js] object (ES5, Live 11 and 12).
// Inlet messages : init, scan, clean (= DELETE), empty <0|1>
// Outlet 0       : text for the result box ("set ...")
// Outlet 1       : 1/0 enables the DELETE button
// Outlet 2       : the number of tracks found
//
// The decision itself lives in planCleanup(), a pure function of a plain model of the set, so it can be
// tested outside Live (tests/vacuum_test.js).

autowatch = 1;
inlets = 1;
outlets = 3;

var includeEmpty = 0;
var lastPlan = null;

// ======================================================================= pure logic
var MAIN_NAMES = ["Master", "Main"];
var NO_OUTPUT = ["None", "No Output", "Sends Only", ""];

function stripNumber(name) {
    // Live may show other tracks as "3-Bass"; compare on the plain name
    var m = /^\d+[-\s](.*)$/.exec(name);
    return m ? m[1] : name;
}

function isExternal(name) {
    return name.indexOf("Ext.") === 0 || name.indexOf("Ext ") === 0;
}

/**
 * model = {
 *   tracks:  [{ id, index, name, isGroup, groupId, hasAudioOut, output, input, sends: [..], empty, isSelf }],
 *   returns: [{ id, name, output, sends: [..] }]
 * }
 * Returns { remove: [track ids, highest index first], keep: { id: reason }, report: [lines] }
 */
function planCleanup(model, withEmpty) {
    var nodes = {};          // id -> { targets: [ids], kind }
    var byName = {};         // plain name -> id (first one wins; duplicates are flagged)
    var dupNames = {};
    var MAIN = "__main__", EXT = "__ext__";
    var all = [];
    var i, j, t, r;

    for (i = 0; i < model.tracks.length; i++) all.push({ n: model.tracks[i], kind: "track" });
    for (i = 0; i < model.returns.length; i++) all.push({ n: model.returns[i], kind: "return" });
    for (i = 0; i < all.length; i++) {
        var nm = stripNumber(all[i].n.name);
        if (byName.hasOwnProperty(nm)) dupNames[nm] = true; else byName[nm] = all[i].n.id;
        nodes[all[i].n.id] = { targets: [], kind: all[i].kind };
    }

    function resolveOutput(n) {
        var out = n.output || "";
        if (MAIN_NAMES.indexOf(out) >= 0) return [MAIN];
        if (isExternal(out)) return [EXT];
        if (out === "Group") return n.groupId ? [n.groupId] : [];
        if (NO_OUTPUT.indexOf(out) >= 0) return [];
        var plain = stripNumber(out);
        if (byName.hasOwnProperty(plain)) return [byName[plain]];
        // unknown destination (a hardware name, a plug-in input...): count it as going somewhere
        return [EXT];
    }

    // edges: output routing, sends to returns, and other tracks listening to this one on their input
    for (i = 0; i < all.length; i++) {
        var n = all[i].n;
        var tg = nodes[n.id].targets;
        var outs = resolveOutput(n);
        for (j = 0; j < outs.length; j++) tg.push(outs[j]);
        if (n.sends) for (j = 0; j < n.sends.length && j < model.returns.length; j++)
            if (n.sends[j] > 0.0001) tg.push(model.returns[j].id);
    }
    for (i = 0; i < model.tracks.length; i++) {
        var listener = model.tracks[i];
        var inp = stripNumber(listener.input || "");
        if (inp && byName.hasOwnProperty(inp) && byName[inp] !== listener.id) nodes[byName[inp]].targets.push(listener.id);
    }

    // reachability to the main output (fixed point)
    var alive = {};
    alive[MAIN] = true;
    alive[EXT] = true;
    var changed = true;
    while (changed) {
        changed = false;
        for (var id in nodes) {
            if (alive[id]) continue;
            var ts = nodes[id].targets;
            for (j = 0; j < ts.length; j++) if (alive[ts[j]]) { alive[id] = true; changed = true; break; }
        }
    }

    var keep = {}, remove = {}, reasons = {};
    for (i = 0; i < model.tracks.length; i++) {
        t = model.tracks[i];
        if (t.isSelf) { keep[t.id] = "holds LT VACUUM"; continue; }
        if (!alive[t.id]) { remove[t.id] = true; reasons[t.id] = describeDead(t); }
        else if (withEmpty && t.empty && !t.isGroup) { remove[t.id] = true; reasons[t.id] = "empty (no clips, no devices)"; }
    }
    // a group goes only if everything inside it goes too
    function childrenOf(gid) {
        var c = [];
        for (var k = 0; k < model.tracks.length; k++) if (model.tracks[k].groupId === gid) c.push(model.tracks[k]);
        return c;
    }
    var again = true;
    while (again) {
        again = false;
        for (i = 0; i < model.tracks.length; i++) {
            t = model.tracks[i];
            if (!t.isGroup || !remove[t.id]) continue;
            var kids = childrenOf(t.id);
            for (j = 0; j < kids.length; j++)
                if (!remove[kids[j].id]) { delete remove[t.id]; keep[t.id] = "group still holds live tracks"; again = true; break; }
        }
    }

    var list = [];
    for (i = 0; i < model.tracks.length; i++) if (remove[model.tracks[i].id]) list.push(model.tracks[i]);
    list.sort(function (a, b) { return b.index - a.index; });   // delete from the bottom up

    var report = [];
    var shown = list.slice().sort(function (a, b) { return a.index - b.index; });
    for (i = 0; i < shown.length; i++) report.push((shown[i].index + 1) + " " + shown[i].name + "  (" + reasons[shown[i].id] + ")");
    for (var d in dupNames) report.push("note: two tracks are called \"" + d + "\", routing by name may be ambiguous");

    var ids = [];
    for (i = 0; i < list.length; i++) ids.push(list[i].id);
    return { remove: ids, keep: keep, report: report };
}

function describeDead(t) {
    var out = t.output || "";
    if (!t.hasAudioOut && (out === "" || out === "None" || out === "No Output")) return "MIDI going nowhere";
    if (out === "Sends Only") return "sends only, all sends down";
    if (out === "None" || out === "No Output" || out === "") return "output: none";
    return "routed to \"" + out + "\", which never reaches Main";
}

// ======================================================================= Live side
function routingName(api, prop) {
    var raw = api.get(prop);
    var text = (raw instanceof Array) ? raw.join(" ") : String(raw);
    var m = /"display_name"\s*:\s*"([^"]*)"/.exec(text);
    if (m) return m[1];
    try { var o = JSON.parse(text); if (o && o[prop]) return o[prop].display_name || ""; } catch (e) {}
    return text.replace(/^\s*\S+\s*/, "");
}

function first(api, prop) {
    var v = api.get(prop);
    return (v instanceof Array) ? v[0] : v;
}

function nameOf(api) {
    // names can come back split into words or wrapped in quotes depending on the Live version
    var v = api.get("name");
    var text = (v instanceof Array) ? v.join(" ") : String(v);
    return text.replace(/^"(.*)"$/, "$1");
}

function readSends(api) {
    var mixer = new LiveAPI(api.unquotedpath + " mixer_device");
    var n = mixer.getcount("sends");
    var s = [];
    for (var i = 0; i < n; i++) s.push(Number(first(new LiveAPI(mixer.unquotedpath + " sends " + i), "value")));
    return s;
}

function isEmptyTrack(api, path) {
    if (api.getcount("devices") > 0) return false;
    var slots = api.getcount("clip_slots");
    for (var i = 0; i < slots; i++)
        if (Number(first(new LiveAPI(path + " clip_slots " + i), "has_clip")) === 1) return false;
    try { if (api.getcount("arrangement_clips") > 0) return false; } catch (e) {}
    return true;
}

function readModel() {
    var song = new LiveAPI("live_set");
    var self = new LiveAPI("this_device");
    var selfTrack = new LiveAPI(self.unquotedpath + " canonical_parent");
    var selfId = Number(selfTrack.id);
    var model = { tracks: [], returns: [] };
    var n = song.getcount("tracks");
    for (var i = 0; i < n; i++) {
        var path = "live_set tracks " + i;
        var t = new LiveAPI(path);
        var gid = 0;
        if (Number(first(t, "is_grouped")) === 1) {
            var g = t.get("group_track");
            gid = Number((g instanceof Array) ? g[g.length - 1] : g);
        }
        model.tracks.push({
            id: Number(t.id), index: i, name: nameOf(t),
            isGroup: Number(first(t, "is_foldable")) === 1, groupId: gid,
            hasAudioOut: Number(first(t, "has_audio_output")) === 1,
            output: routingName(t, "output_routing_type"),
            input: Number(first(t, "is_foldable")) === 1 ? "" : routingName(t, "input_routing_type"),
            sends: readSends(t), empty: includeEmpty ? isEmptyTrack(t, path) : false,
            isSelf: Number(t.id) === selfId
        });
    }
    var rn = song.getcount("return_tracks");
    for (var k = 0; k < rn; k++) {
        var rt = new LiveAPI("live_set return_tracks " + k);
        model.returns.push({ id: Number(rt.id), name: nameOf(rt), output: routingName(rt, "output_routing_type"), sends: readSends(rt) });
    }
    return model;
}

function show(lines) {
    outlet(0, "set", lines.join("\n"));
}

// ======================================================================= messages
function init() {
    lastPlan = null;
    outlet(1, 0);
    show(["SCAN finds tracks that never", "reach Main. Nothing is deleted", "until you press DELETE."]);
}

function empty(v) {
    includeEmpty = v ? 1 : 0;
    lastPlan = null;
    outlet(1, 0);
}

function scan() {
    var model = readModel();
    lastPlan = planCleanup(model, includeEmpty);
    outlet(2, lastPlan.remove.length);
    if (lastPlan.remove.length === 0) {
        show(["All clean: every track reaches", "Main (" + model.tracks.length + " tracks checked)."]);
        outlet(1, 0);
        return;
    }
    show([lastPlan.remove.length + " track" + (lastPlan.remove.length > 1 ? "s" : "") + " found:"].concat(lastPlan.report));
    outlet(1, 1);
}

// the DELETE button sends "clean" ("delete" is a reserved word in JavaScript)
function clean() {
    if (!lastPlan || lastPlan.remove.length === 0) { show(["Press SCAN first."]); return; }
    // scan again right now: only delete what is still dead and still the same track
    var fresh = planCleanup(readModel(), includeEmpty);
    var song = new LiveAPI("live_set");
    var wanted = {};
    for (var i = 0; i < lastPlan.remove.length; i++) wanted[lastPlan.remove[i]] = true;
    var deleted = 0;
    for (var k = 0; k < fresh.remove.length; k++) {
        var id = fresh.remove[k];
        if (!wanted[id]) continue;
        // find its current index (indices move as tracks go)
        var n = song.getcount("tracks");
        for (var j = n - 1; j >= 0; j--) {
            if (Number(new LiveAPI("live_set tracks " + j).id) === id) { song.call("delete_track", j); deleted++; break; }
        }
    }
    lastPlan = null;
    outlet(1, 0);
    outlet(2, 0);
    show(["Deleted " + deleted + " track" + (deleted === 1 ? "" : "s") + ".", "Undo with Cmd/Ctrl+Z if needed."]);
}

// for the Node tests
if (typeof module !== "undefined") module.exports = { planCleanup: planCleanup, stripNumber: stripNumber };
