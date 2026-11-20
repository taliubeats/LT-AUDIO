// LT VACUUM tests: routing decisions on plain models, then SCAN -> DELETE through a fake Live API.
// Run: node m4l/vacuum/tests/vacuum_test.js
// Works in Node or in macOS JavaScriptCore:
//   /System/Library/Frameworks/JavaScriptCore.framework/Versions/A/Helpers/jsc m4l/vacuum/tests/vacuum_test.js
var isNode = typeof require !== "undefined";
var say = isNode ? function (s) { console.log(s); } : function (s) { print(s); };
var failures = 0;
function check(ok, what) { say((ok ? "  ok    " : "  FAIL  ") + what); if (!ok) failures++; }

var scriptPath = "m4l/vacuum/lt_vacuum.js";
var deviceCtx = null;
// Loads the device script once; each test swaps in its own fake Live set and outlet log.
function loadDevice(fakeSet) {
    var out = [[], [], []];
    var hooks = {
        post: function () {},
        outlet: function (i) { out[i].push(Array.prototype.slice.call(arguments, 1)); },
        LiveAPI: makeLiveAPI(fakeSet)
    };
    if (isNode) {
        if (!deviceCtx) {
            var vm = require("vm"), fs = require("fs"), path = require("path");
            deviceCtx = vm.createContext({ module: undefined });
            vm.runInContext(fs.readFileSync(path.join(__dirname, "..", "lt_vacuum.js"), "utf8"), deviceCtx);
        }
        for (var k in hooks) deviceCtx[k] = hooks[k];
        return { ctx: deviceCtx, out: out };
    }
    for (var h in hooks) this[h] = hooks[h];
    if (!deviceCtx) { load(scriptPath); deviceCtx = this; }
    return { ctx: deviceCtx, out: out };
}

// a tiny stand-in for Live's object model, answering the calls the script makes
function makeLiveAPI(set) {
    return function LiveAPI(p) {
        const parts = p.trim().split(/\s+/);
        let node = null, kind = "";
        if (p === "live_set") { node = set; kind = "song"; }
        else if (p === "this_device") { node = { devicePath: true }; kind = "this_device"; }
        else if (parts[0] === "live_set") {
            node = set; kind = "song";
            for (let i = 1; i < parts.length; i++) {
                const key = parts[i];
                if (key === "devices") { node = { parentTrack: node }; i++; kind = "device"; }
                else if (key === "tracks" || key === "return_tracks" || key === "clip_slots" || key === "sends") { node = node[key][Number(parts[++i])]; kind = key; }
                else if (key === "mixer_device") { node = node.mixer || (node.mixer = { sends: (node.sends || []).map(v => ({ value: v })) }); kind = "mixer"; }
                else if (key === "canonical_parent") { node = node.parentTrack; kind = "tracks"; }
            }
        }
        if (kind === "this_device") { this.unquotedpath = "live_set tracks " + set.selfTrack + " devices 0"; this.id = "999"; }
        else { this.unquotedpath = p; this.id = String(node && node.id !== undefined ? node.id : 0); }
        this.get = (prop) => {
            if (prop === "output_routing_type" || prop === "input_routing_type") {
                const v = prop === "output_routing_type" ? node.output : node.input;
                return [JSON.stringify({ [prop]: { display_name: v || "", identifier: 1 } })];
            }
            if (prop === "group_track") return ["id", node.group || 0];
            if (prop === "is_grouped") return [node.group ? 1 : 0];
            if (prop === "is_foldable") return [node.isGroup ? 1 : 0];
            if (prop === "has_audio_output") return [node.midiOnly ? 0 : 1];
            if (prop === "has_clip") return [node.has_clip ? 1 : 0];
            if (prop === "value") return [node.value];
            return [node[prop]];
        };
        this.getcount = (prop) => {
            if (prop === "tracks") return set.tracks.length;
            if (prop === "return_tracks") return set.return_tracks.length;
            if (prop === "sends") return (node.sends || []).length;
            if (prop === "clip_slots") return (node.clip_slots || []).length;
            if (prop === "devices") return node.devices || 0;
            if (prop === "arrangement_clips") return node.arrangement || 0;
            return 0;
        };
        this.call = (fn, arg) => {
            if (fn === "delete_track") {
                const t = set.tracks[arg];
                if (t.isGroup) set.tracks = set.tracks.filter(x => x.group !== t.id);   // Live takes the contents along
                set.tracks.splice(set.tracks.indexOf(t), 1);
                set.deleted.push(t.name);
            }
        };
    };
}

// ------------------------------------------------------------------ pure decisions
const { ctx } = loadDevice({ tracks: [], return_tracks: [], selfTrack: 0, deleted: [] });
const plan = (tracks, returns, empty) => ctx.planCleanup({ tracks, returns: returns || [] }, empty);
const T = (id, name, output, extra) => Object.assign({ id, index: id - 1, name, isGroup: false, groupId: 0, hasAudioOut: true, output, input: "", sends: [], empty: false, isSelf: false }, extra || {});
const names = (p, tracks) => p.remove.map(id => tracks.find(t => t.id === id).name).sort().join(",");

say("routing");
{
    const tr = [T(1, "Kick", "Main"), T(2, "Old idea", "None"), T(3, "Bass", "Master")];
    check(names(plan(tr), tr) === "Old idea", "output None is removed, Main and Master (Live 11) are kept");
}
{
    const tr = [T(1, "Drums", "Main", { isGroup: true }), T(2, "Kick", "Group", { groupId: 1 }), T(3, "Snare", "Group", { groupId: 1 })];
    check(plan(tr).remove.length === 0, "tracks in a group that reaches Main are kept");
}
{
    const tr = [T(1, "Drums", "None", { isGroup: true }), T(2, "Kick", "Group", { groupId: 1 }), T(3, "Snare", "Group", { groupId: 1 })];
    check(names(plan(tr), tr) === "Drums,Kick,Snare", "a dead group takes its children with it");
}
{
    const tr = [T(1, "Drums", "None", { isGroup: true }), T(2, "Kick", "Group", { groupId: 1 }), T(3, "Snare", "Main", { groupId: 1 })];
    const p = plan(tr);
    check(names(p, tr) === "Kick" && p.keep[1] === "group still holds live tracks", "a dead group that holds a live track stays (only the dead child goes)");
}
{
    const rets = [{ id: 10, name: "A-Reverb", output: "Main", sends: [] }];
    const tr = [T(1, "Pad", "Sends Only", { sends: [0.6] }), T(2, "Ghost", "Sends Only", { sends: [0] })];
    check(names(plan(tr, rets), tr) === "Ghost", "sends only: kept while a send is up, removed when all are down");
}
{
    const rets = [{ id: 10, name: "A-Reverb", output: "None", sends: [] }];
    const tr = [T(1, "Pad", "Sends Only", { sends: [0.6] })];
    check(names(plan(tr, rets), tr) === "Pad", "sending only to a return that goes nowhere counts as dead");
}
{
    const tr = [T(1, "Synth", "None"), T(2, "Resample", "Main", { input: "1-Synth" })];
    check(plan(tr).remove.length === 0, "a track recorded by another track (its input) is kept");
}
{
    const tr = [T(1, "Chords MIDI", "3-Keys", { hasAudioOut: false }), T(2, "Lost MIDI", "No Output", { hasAudioOut: false }), T(3, "Keys", "Main")];
    const p = plan(tr);
    check(names(p, tr) === "Lost MIDI" && p.report[0].indexOf("MIDI going nowhere") > 0, "MIDI into an instrument track is kept, MIDI to nowhere is removed");
}
{
    const tr = [T(1, "A", "2-B"), T(2, "B", "1-A")];
    check(names(plan(tr), tr) === "A,B", "a loop that never reaches Main is removed");
}
{
    const tr = [T(1, "Hardware", "Ext. Out"), T(2, "Me", "None", { isSelf: true })];
    const p = plan(tr);
    check(p.remove.length === 0 && p.keep[2] === "holds LT VACUUM", "external outputs are kept; the device's own track is never removed");
}
{
    const tr = [T(1, "Empty", "Main", { empty: true }), T(2, "Full", "Main")];
    check(plan(tr, [], false).remove.length === 0 && names(plan(tr, [], true), tr) === "Empty", "+ EMPTY option adds empty tracks only when on");
}
{
    const tr = [T(1, "Bass", "Main"), T(2, "Bass", "None"), T(3, "Lead", "None")];
    const p = plan(tr);
    check(p.remove[0] === 3 && p.remove[1] === 2, "deletes from the bottom up (indices stay valid)");
    check(p.report.some(l => l.indexOf("two tracks are called") >= 0), "warns about duplicate track names");
}

// ------------------------------------------------------------------ end to end through the fake Live API
say("scan + delete");
{
    const set = {
        selfTrack: 0, deleted: [],
        return_tracks: [{ id: 50, name: "A-Reverb", output: "Main", sends: [] }],
        tracks: [
            { id: 1, name: "Utility", output: "Main", sends: [0] },                       // holds the device
            { id: 2, name: "Drums", output: "Main", isGroup: true, sends: [0] },
            { id: 3, name: "Kick", output: "Group", group: 2, sends: [0] },
            { id: 4, name: "Unused FX", output: "None", sends: [0] },
            { id: 5, name: "Atmos", output: "Sends Only", sends: [0.5] },
            { id: 6, name: "Old Group", output: "None", isGroup: true, sends: [0] },
            { id: 7, name: "Old Lead", output: "Group", group: 6, sends: [0] },
            { id: 8, name: "Old Pad", output: "Group", group: 6, sends: [0] },
            { id: 9, name: "Blank", output: "Main", sends: [0], clip_slots: [{ has_clip: 0 }] },
        ],
    };
    const dev = loadDevice(set);
    dev.ctx.init();
    dev.ctx.scan();
    const shown = dev.out[0][dev.out[0].length - 1][1];
    check(dev.out[2][0][0] === 4 && dev.out[1][dev.out[1].length - 1][0] === 1, "SCAN finds 4 tracks and enables DELETE");
    check(set.deleted.length === 0, "SCAN never deletes anything");
    check(/Unused FX/.test(shown) && /Old Group/.test(shown) && !/Atmos/.test(shown), "the list names what will go: " + shown.split("\n")[0]);
    // the user re-routes "Unused FX" to Main before pressing DELETE: it must survive
    set.tracks.find(t => t.id === 4).output = "Main";
    dev.ctx.clean();
    check(set.deleted.indexOf("Unused FX") < 0, "DELETE re-checks first: a track fixed after SCAN is spared");
    check(!set.tracks.some(t => t.id === 6 || t.id === 7 || t.id === 8), "the dead group and its tracks are gone");
    check(set.tracks.map(t => t.name).join(",") === "Utility,Drums,Kick,Unused FX,Atmos,Blank", "remaining: " + set.tracks.map(t => t.name).join(", "));
    dev.ctx.empty(1);
    dev.ctx.scan();
    dev.ctx.clean();
    check(set.deleted.indexOf("Blank") >= 0 && set.deleted.indexOf("Utility") < 0, "+ EMPTY removes the empty track but never the device's own track");
}

say(failures === 0 ? "ALL PASSED" : failures + " FAILED");
if (isNode) process.exit(failures === 0 ? 0 : 1);
else if (failures) throw new Error(failures + " failed");
