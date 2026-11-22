// LT BOUNCE tests (macOS JavaScriptCore):
//   /System/Library/Frameworks/JavaScriptCore.framework/Versions/A/Helpers/jsc m4l/bounce/tests/bounce_test.js
var failures = 0;
function check(ok, what) { print((ok ? "  ok    " : "  FAIL  ") + what); if (!ok) failures++; }
var statusLog = [];
function outlet(i, sel, text) { statusLog.push(text); }
function post() {}
function Task(fn) { this.schedule = function () { fn(); }; }

var set, observers;
function freshSet(acceptsObject) {
    set = { tracks: [{ name: "Synth Lead", mute: 0, input: "", arm: 0, mon: 1 }, { name: "Drums", mute: 0, input: "", arm: 0, mon: 1 }],
            selected: 0, loop_start: 8, loop_length: 16, record_mode: 0, playing: false, time: 0, log: [], acceptsObject: acceptsObject };
    observers = [];
}
function LiveAPI(a, b) {
    var cb = typeof a === "function" ? a : null, path = cb ? b : a;
    var self = this;
    if (cb) observers.push(this);
    this.cb = cb;
    this.unquotedpath = path === "live_set view selected_track" ? "live_set tracks " + set.selected : path;
    function track() { var m = /tracks (\d+)/.exec(self.unquotedpath); return m ? set.tracks[Number(m[1])] : null; }
    this.get = function (prop) {
        var t = track();
        if (prop === "name") return [t.name];
        if (prop === "loop_start") return [set.loop_start];
        if (prop === "loop_length") return [set.loop_length];
        if (prop === "available_input_routing_types")
            return [JSON.stringify({ available_input_routing_types: [{ display_name: "Ext. In", identifier: 1 }].concat(set.hideInputs ? [] : set.tracks.map(function (x, i) { return { display_name: (i + 1) + "-" + x.name, identifier: 10 + i }; })) })];
        if (prop === "available_input_routing_channels") return [JSON.stringify({ available_input_routing_channels: [{ display_name: "Pre FX", identifier: 1 }, { display_name: "Post FX", identifier: 2 }, { display_name: "Post Mixer", identifier: 3 }] })];
        if (prop === "input_routing_type") return [JSON.stringify({ input_routing_type: { display_name: t.input, identifier: 0 } })];
        if (prop === "input_routing_channel") return [JSON.stringify({ input_routing_channel: { display_name: t.channel || "", identifier: 0 } })];
        return [0];
    };
    this.set = function (prop, v) {
        var t = track();
        if (prop === "input_routing_type" || prop === "input_routing_channel") {
            // Live accepts either a dictionary or its JSON text, depending on the version: test both paths
            var entry = typeof v === "string" ? JSON.parse(v) : (set.acceptsObject ? v : null);
            if (!entry || !entry.display_name) return;
            if (prop === "input_routing_type") t.input = entry.display_name; else t.channel = entry.display_name;
            return;
        }
        if (prop === "arm") t.arm = v; else if (prop === "mute") t.mute = v; else if (prop === "name") t.name = v;
        else if (prop === "current_monitoring_state") t.mon = v;
        else if (prop === "record_mode") set.record_mode = v; else if (prop === "current_song_time") set.time = v;
        set.log.push(prop + "=" + v);
    };
    this.call = function (fn, arg) {
        set.log.push(fn + (arg !== undefined ? " " + arg : ""));
        if (fn === "create_audio_track") set.tracks.splice(arg, 0, { name: "Audio", mute: 0, input: "", arm: 0, mon: 1 });
        if (fn === "delete_track") set.tracks.splice(arg, 1);
        if (fn === "start_playing") set.playing = true;
        if (fn === "stop_playing") set.playing = false;
    };
}
load("m4l/bounce/lt_bounce.js");
function tick(t) { observers.forEach(function (o) { if (o.property === "current_song_time" && o.cb) o.cb(["current_song_time", t]); }); }

print("bounce, mute the original");
freshSet(true);
init(); setAfter(0); bounce();
var dst = set.tracks[1];
check(dst.name === "Synth Lead (bounce)" && dst.input === "1-Synth Lead" && dst.channel === "Post Mixer", "new audio track right below, listening to the track post mixer");
check(dst.arm === 1 && dst.mon === 2 && set.record_mode === 1 && set.playing && set.time === 8, "armed, monitoring off, arrangement record from the loop start");
tick(20);
check(set.playing, "keeps recording before the loop end");
tick(24);
check(!set.playing && set.record_mode === 0 && dst.arm === 0 && set.tracks[0].mute === 1, "stops at the loop end, disarms, mutes the original");

print("delete the original, routing given as JSON text");
freshSet(false);
init(); setAfter(2); bounce(); tick(30);
check(set.tracks.length === 2 && set.tracks[0].name === "Synth Lead (bounce)" && set.tracks[0].input === "1-Synth Lead", "the original is gone, the bounce took its place");

print("cancel and errors");
freshSet(true);
init(); bounce(); cancel();
check(set.tracks.length === 2 && !set.playing && set.record_mode === 0, "cancel removes the new track and stops");
freshSet(true);
set.loop_length = 0;
init(); bounce();
check(set.tracks.length === 2 && /loop/.test(statusLog[statusLog.length - 1]), "no loop set: nothing is created");
freshSet(true);
set.hideInputs = true;   // Live does not offer the track as an input
init(); bounce();
check(set.tracks.length === 2 && !set.playing && /Bounce Track in Place/.test(statusLog[statusLog.length - 1]),
      "no route available: the new track is removed and the message points to Live's own bounce");

print(failures === 0 ? "ALL PASSED" : failures + " FAILED");
if (failures) throw new Error(failures + " failed");
