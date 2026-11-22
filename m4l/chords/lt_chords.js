// LT CHORDS: play one note, get a chord. Major, minor or in key; add 7th, 9th and octave;
// and see which chord usually comes next in the genre you pick.
//
// Max for Live MIDI effect, [js] object (ES5, Live 11 and 12).
// Inlet  : "list pitch velocity" from [midiparse], and the settings:
//          quality 0|1|2 (major, minor, in key), key 0-11, scale 0|1 (major, minor),
//          seventh 0|1, ninth 0|1, octave 0|1, dom 0|1 (major chords get a b7), genre 0-7, init
// Outlet 0: "pitch velocity" notes for [midiformat]
// Outlet 1: chord name for the big label ("set ...")
// Outlet 2: notes + suggestions text ("set ...")

autowatch = 1;
inlets = 1;
outlets = 3;

var settings = { quality: 0, key: 0, scale: 0, seventh: 0, ninth: 0, octave: 0, dom: 0, genre: 0 };
var held = {};      // input pitch -> [output pitches]
var counts = {};    // output pitch -> how many held chords use it

// ======================================================================= music
var SHARPS = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
var FLATS = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"];
var MAJOR_SCALE = [0, 2, 4, 5, 7, 9, 11];
var MINOR_SCALE = [0, 2, 3, 5, 7, 8, 10];

function mod12(v) { return ((v % 12) + 12) % 12; }

function usesFlats() {
    // the selected key decides the spelling (F, Bb, Eb, Ab, Db majors and their relative minors)
    var major = settings.scale === 0 ? settings.key : mod12(settings.key + 3);
    return [5, 10, 3, 8, 1].indexOf(major) >= 0;
}

function noteName(pc) { return (usesFlats() ? FLATS : SHARPS)[mod12(pc)]; }

/** Builds the chord on a root note. Returns { notes: [semitones above root], quality: "M"|"m"|"d", name } */
function buildChord(rootPitch, s) {
    s = s || settings;
    var third = 4, fifth = 7, seventh = 11, ninth = 14, q = "M";
    if (s.quality === 1) { third = 3; seventh = 10; q = "m"; }
    else if (s.quality === 0) { seventh = s.dom ? 10 : 11; }
    else {
        // in key: stack thirds from the scale
        var scale = s.scale === 0 ? MAJOR_SCALE : MINOR_SCALE;
        var pc = mod12(rootPitch - s.key);
        var idx = scale.indexOf(pc);
        if (idx < 0) { seventh = s.dom ? 10 : 11; }   // a note outside the key: plain major
        else {
            var tone = function (k) { var i = idx + k; return scale[i % 7] + 12 * Math.floor(i / 7) - scale[idx]; };
            third = tone(2); fifth = tone(4); seventh = tone(6); ninth = tone(1) + 12;
            // minor keys: the V chord is major (harmonic minor), as in almost every bachata or reggaeton tune
            if (s.scale === 1 && idx === 4) { third = 4; seventh = 10; }
            if (s.dom && third === 4) seventh = 10;
            q = third === 3 ? (fifth === 6 ? "d" : "m") : "M";
        }
    }
    var notes = [0, third, fifth];
    if (s.seventh) notes.push(seventh);
    if (s.ninth) notes.push(ninth);
    if (s.octave) notes.push(12);
    return { notes: notes, quality: q, name: chordName(rootPitch, q, s.seventh ? seventh : 0, s.ninth) };
}

function chordName(root, q, seventh, ninth) {
    var base = noteName(root), suffix = "";
    var isDim = q === "d";
    if (seventh) {
        var ext = ninth ? "9" : "7";
        if (q === "M") suffix = seventh === 11 ? "maj" + ext : ext;
        else if (q === "m") suffix = seventh === 11 ? "mMaj" + ext : "m" + ext;
        else suffix = seventh === 9 ? "dim7" : "m" + ext + "b5";
    } else {
        suffix = q === "m" ? "m" : isDim ? "dim" : "";
        if (ninth) suffix += "add9";
    }
    return base + suffix;
}

// ======================================================================= suggestions
// Common progressions per genre, as [semitones from the tonic, quality]. Weights = how typical.
var GENRES = [
    { name: "REGGAETON", progs: [
        { w: 5, label: "i-VI-III-VII", c: [[0, "m"], [8, "M"], [3, "M"], [10, "M"]] },
        { w: 2, label: "i-iv-VII-III", c: [[0, "m"], [5, "m"], [10, "M"], [3, "M"]] },
        { w: 2, label: "i-VII-VI-VII", c: [[0, "m"], [10, "M"], [8, "M"], [10, "M"]] },
        { w: 1, label: "i-iv-i-V", c: [[0, "m"], [5, "m"], [0, "m"], [7, "M"]] } ] },
    { name: "TRAP", progs: [
        { w: 4, label: "i-VI", c: [[0, "m"], [8, "M"]] },
        { w: 3, label: "i-iv", c: [[0, "m"], [5, "m"]] },
        { w: 2, label: "i-VII-VI", c: [[0, "m"], [10, "M"], [8, "M"]] },
        { w: 2, label: "i-iv-v", c: [[0, "m"], [5, "m"], [7, "m"]] },
        { w: 2, label: "i-bII (dark)", c: [[0, "m"], [1, "M"]] } ] },
    { name: "R&B", progs: [
        { w: 3, label: "ii-V-I", c: [[2, "m"], [7, "M"], [0, "M"]] },
        { w: 3, label: "I-vi-ii-V", c: [[0, "M"], [9, "m"], [2, "m"], [7, "M"]] },
        { w: 2, label: "iii-vi-ii-V", c: [[4, "m"], [9, "m"], [2, "m"], [7, "M"]] },
        { w: 2, label: "IV-iii-ii-I", c: [[5, "M"], [4, "m"], [2, "m"], [0, "M"]] } ] },
    { name: "POP", progs: [
        { w: 5, label: "I-V-vi-IV", c: [[0, "M"], [7, "M"], [9, "m"], [5, "M"]] },
        { w: 4, label: "vi-IV-I-V", c: [[9, "m"], [5, "M"], [0, "M"], [7, "M"]] },
        { w: 3, label: "I-vi-IV-V", c: [[0, "M"], [9, "m"], [5, "M"], [7, "M"]] },
        { w: 2, label: "IV-I-V-vi", c: [[5, "M"], [0, "M"], [7, "M"], [9, "m"]] } ] },
    { name: "LO-FI", progs: [
        { w: 3, label: "ii-V-I-vi", c: [[2, "m"], [7, "M"], [0, "M"], [9, "m"]] },
        { w: 2, label: "I-IV", c: [[0, "M"], [5, "M"]] },
        { w: 2, label: "IV-iii-vi", c: [[5, "M"], [4, "m"], [9, "m"]] },
        { w: 2, label: "i-iv", c: [[0, "m"], [5, "m"]] },
        { w: 2, label: "vi-ii-V-I", c: [[9, "m"], [2, "m"], [7, "M"], [0, "M"]] } ] },
    { name: "BACHATA", progs: [
        { w: 3, label: "i-iv-V", c: [[0, "m"], [5, "m"], [7, "M"]] },
        { w: 2, label: "I-IV-V-IV", c: [[0, "M"], [5, "M"], [7, "M"], [5, "M"]] },
        { w: 3, label: "i-VII-VI-V", c: [[0, "m"], [10, "M"], [8, "M"], [7, "M"]] } ] },
    { name: "AFRO", progs: [
        { w: 3, label: "IV-V-iii-vi", c: [[5, "M"], [7, "M"], [4, "m"], [9, "m"]] },
        { w: 2, label: "I-IV", c: [[0, "M"], [5, "M"]] },
        { w: 2, label: "vi-IV-I-V", c: [[9, "m"], [5, "M"], [0, "M"], [7, "M"]] },
        { w: 1, label: "ii-V-I", c: [[2, "m"], [7, "M"], [0, "M"]] } ] },
    { name: "HOUSE", progs: [
        { w: 2, label: "i-VI-VII", c: [[0, "m"], [8, "M"], [10, "M"]] },
        { w: 2, label: "vi-IV-I-V", c: [[9, "m"], [5, "M"], [0, "M"], [7, "M"]] },
        { w: 2, label: "i-iv", c: [[0, "m"], [5, "m"]] },
        { w: 2, label: "i-VI-III-VII", c: [[0, "m"], [8, "M"], [3, "M"], [10, "M"]] } ] }
];

/** The chords that most often follow (root, quality) in a genre, best first: [{ root, quality, weight, from }] */
function suggest(root, quality, genreIndex) {
    var g = GENRES[Math.max(0, Math.min(GENRES.length - 1, genreIndex))];
    var q = quality === "d" ? "m" : quality;
    var scores = {}, order = [];
    for (var p = 0; p < g.progs.length; p++) {
        var prog = g.progs[p];
        for (var i = 0; i < prog.c.length; i++) {
            if (prog.c[i][1] !== q) continue;
            var tonic = root - prog.c[i][0];
            var next = prog.c[(i + 1) % prog.c.length];
            var key = mod12(tonic + next[0]) + next[1];
            if (mod12(tonic + next[0]) === mod12(root) && next[1] === q) continue;   // "stay on the same chord" is no tip
            if (!scores[key]) { scores[key] = { root: mod12(tonic + next[0]), quality: next[1], weight: 0, from: prog.label }; order.push(key); }
            scores[key].weight += prog.w;
            if (prog.w > (scores[key].best || 0)) { scores[key].best = prog.w; scores[key].from = prog.label; }
        }
    }
    var list = [];
    for (var k = 0; k < order.length; k++) list.push(scores[order[k]]);
    list.sort(function (a, b) { return b.weight - a.weight; });
    return list.slice(0, 3);
}

function describeSuggestions(root, quality) {
    var tips = suggest(root, quality, settings.genre);
    if (tips.length === 0) return "No tip for this chord in " + GENRES[settings.genre].name + ".";
    var parts = [];
    for (var i = 0; i < tips.length; i++)
        parts.push(noteName(tips[i].root) + (tips[i].quality === "m" ? "m (menor)" : " (mayor)"));
    return "SIGUIENTE: " + parts.join("  /  ") + "\n" + GENRES[settings.genre].name + ": " + tips[0].from;
}

// ======================================================================= MIDI
function sendOn(p, v) {
    if (p < 0 || p > 127) return;
    counts[p] = (counts[p] || 0) + 1;
    if (counts[p] === 1) outlet(0, p, v);
}

function sendOff(p) {
    if (p < 0 || p > 127 || !counts[p]) return;
    counts[p]--;
    if (counts[p] === 0) { delete counts[p]; outlet(0, p, 0); }
}

function list(pitch, velocity) {
    if (arguments.length < 2) return;
    if (velocity > 0) {
        if (held[pitch]) release(pitch);   // repeated note-on without an off
        var chord = buildChord(pitch);
        var out = [];
        for (var i = 0; i < chord.notes.length; i++) { out.push(pitch + chord.notes[i]); sendOn(pitch + chord.notes[i], velocity); }
        held[pitch] = out;
        var names = [];
        for (var j = 0; j < out.length; j++) names.push(noteName(out[j]) + (Math.floor(out[j] / 12) - 2));
        outlet(1, "set", chord.name);
        outlet(2, "set", names.join(" ") + "\n" + describeSuggestions(pitch, chord.quality));
    } else release(pitch);
}

function release(pitch) {
    var out = held[pitch];
    if (!out) return;
    for (var i = 0; i < out.length; i++) sendOff(out[i]);
    delete held[pitch];
}

/** Every chord off (transport stop, device off, settings that would leave notes hanging). */
function panic() {
    for (var p in held) release(Number(p));
    held = {};
    counts = {};
}

// ======================================================================= settings
function quality(v) { settings.quality = Math.max(0, Math.min(2, v | 0)); }
function key(v) { settings.key = mod12(v | 0); }
function scale(v) { settings.scale = v ? 1 : 0; }
function seventh(v) { settings.seventh = v ? 1 : 0; }
function ninth(v) { settings.ninth = v ? 1 : 0; }
function octave(v) { settings.octave = v ? 1 : 0; }
function dom(v) { settings.dom = v ? 1 : 0; }
function genre(v) { settings.genre = Math.max(0, Math.min(GENRES.length - 1, v | 0)); }

function init() {
    panic();
    outlet(1, "set", "LT CHORDS");
    outlet(2, "set", "Toca una nota: sale un acorde.\nElige el genero para ver que acorde suele venir despues.");
}

// for the tests
if (typeof module !== "undefined") module.exports = { buildChord: buildChord, suggest: suggest, GENRES: GENRES };
