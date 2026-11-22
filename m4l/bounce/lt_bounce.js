// LT BOUNCE: bounce the selected track to audio, in place. Records what the track plays (after its
// effects and mixer) over the loop range into a new audio track right below it, then mutes,
// keeps or deletes the original.
//
// Max for Live audio effect, [js] object (ES5, Live 11 and 12). Live 12.2+ also has its own
// "Bounce Track in Place" in the track's context menu.
// Inlet : init, bounce, cancel, after 0|1|2 (mute, keep, delete the original)
// Outlet 0: status text ("set ...")

autowatch = 1;
inlets = 1;
outlets = 1;

var after = 0;            // 0 mute the original, 1 keep it, 2 delete it
var job = null;           // the bounce in progress
var clock = null;

function status(lines) { outlet(0, "set", [].concat(lines).join("\n")); }
function first(api, prop) { var v = api.get(prop); return (v instanceof Array) ? v[0] : v; }
function nameOf(api) { var v = api.get("name"); return ((v instanceof Array) ? v.join(" ") : String(v)).replace(/^"(.*)"$/, "$1"); }

/** Pulls the list of {display_name, identifier} out of whatever form Live hands back. */
function routingList(api, prop) {
    var raw = api.get(prop);
    var text = (raw instanceof Array) ? raw.join(" ") : String(raw);
    var start = text.indexOf("{");
    if (start < 0) return [];
    try {
        var obj = JSON.parse(text.substring(start));
        return obj[prop] || [];
    } catch (e) { return []; }
}

function currentRouting(api, prop) {
    var raw = api.get(prop);
    var text = (raw instanceof Array) ? raw.join(" ") : String(raw);
    var m = /"display_name"\s*:\s*"([^"]*)"/.exec(text);
    return m ? m[1] : "";
}

/** Sets a routing dictionary, trying both forms Live accepts, and checks it took. */
function setRouting(api, prop, entry) {
    var tries = [function () { api.set(prop, entry); },
                 function () { api.set(prop, JSON.stringify(entry)); },
                 function () { api.set(prop, JSON.stringify({ identifier: entry.identifier })); }];
    for (var i = 0; i < tries.length; i++) {
        try { tries[i](); } catch (e) {}
        if (currentRouting(api, prop) === entry.display_name) return true;
    }
    return false;
}

function matchesTrack(displayName, trackName, index) {
    var plain = displayName.replace(/^\d+[-\s]/, "");
    return plain === trackName || displayName === (index + 1) + "-" + trackName;
}

// ======================================================================= the bounce
function bounce() {
    if (job) { status("Already bouncing... press CANCEL to stop."); return; }
    var song = new LiveAPI("live_set");
    var view = new LiveAPI("live_set view selected_track");
    var path = String(view.unquotedpath || view.path || "");
    var m = /tracks (\d+)$/.exec(path.replace(/"/g, ""));
    if (!m || path.indexOf("return_tracks") >= 0) { status(["Select a normal track first", "(not a return or the main track)."]); return; }
    var srcIndex = Number(m[1]);
    var src = new LiveAPI("live_set tracks " + srcIndex);
    var srcName = nameOf(src);
    var start = Number(first(song, "loop_start")), length = Number(first(song, "loop_length"));
    if (!(length > 0)) { status("Set the loop brace over the part to bounce."); return; }

    song.call("create_audio_track", srcIndex + 1);
    var dst = new LiveAPI("live_set tracks " + (srcIndex + 1));
    dst.set("name", srcName + " (bounce)");

    var types = routingList(dst, "available_input_routing_types");
    var source = null;
    for (var i = 0; i < types.length; i++) if (matchesTrack(types[i].display_name, srcName, srcIndex)) { source = types[i]; break; }
    if (!source || !setRouting(dst, "input_routing_type", source)) {
        song.call("delete_track", srcIndex + 1);
        status(["Could not route \"" + srcName + "\" into a new track.", "Tip: in Live 12.2+ right-click the track > Bounce Track in Place."]);
        return;
    }
    var chans = routingList(dst, "available_input_routing_channels");
    for (var c = 0; c < chans.length; c++)
        if (chans[c].display_name === "Post Mixer") { setRouting(dst, "input_routing_channel", chans[c]); break; }

    dst.set("current_monitoring_state", 2);   // Off: the original keeps playing, no doubled sound
    dst.set("arm", 1);
    job = { song: song, src: src, dst: dst, srcIndex: srcIndex, start: start, end: start + length, name: srcName };
    song.set("current_song_time", start);
    song.set("record_mode", 1);
    song.call("start_playing");
    status(["Bouncing \"" + srcName + "\"...", "bars " + (start / 4 + 1).toFixed(1) + " to " + ((start + length) / 4 + 1).toFixed(1)]);

    // watch the playhead; finishing happens outside the notification (Live forbids changes inside it)
    clock = new LiveAPI(function (args) {
        var a = [].concat(args);
        var t = Number(a[a.length - 1]);
        if (job && t >= job.end && !job.finishing) { job.finishing = true; new Task(finish).schedule(0); }
    }, "live_set");
    clock.property = "current_song_time";
}

function finish() {
    if (!job) return;
    var j = job;
    job = null;
    if (clock) clock.property = "";
    j.song.call("stop_playing");
    j.song.set("record_mode", 0);
    j.dst.set("arm", 0);
    j.dst.set("current_monitoring_state", 1);
    if (after === 0) j.src.set("mute", 1);
    else if (after === 2) j.song.call("delete_track", j.srcIndex);
    status(["Done: \"" + j.name + " (bounce)\"", after === 0 ? "the original is muted" : after === 2 ? "the original was deleted" : "the original is untouched"]);
}

function cancel() {
    if (!job) { status("Nothing to cancel."); return; }
    var j = job;
    job = null;
    if (clock) clock.property = "";
    j.song.call("stop_playing");
    j.song.set("record_mode", 0);
    j.song.call("delete_track", j.srcIndex + 1);
    status("Cancelled: the new track was removed.");
}

function setAfter(v) { after = Math.max(0, Math.min(2, v | 0)); }

function init() {
    job = null;
    status(["Select a track, set the loop over the part,", "then press BOUNCE."]);
}

if (typeof module !== "undefined") module.exports = { matchesTrack: matchesTrack, routingList: routingList };
