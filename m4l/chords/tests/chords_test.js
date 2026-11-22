// LT CHORDS tests. Run with macOS JavaScriptCore:
//   /System/Library/Frameworks/JavaScriptCore.framework/Versions/A/Helpers/jsc m4l/chords/tests/chords_test.js
var failures = 0;
function check(ok, what) { print((ok ? "  ok    " : "  FAIL  ") + what); if (!ok) failures++; }

var sent = [[], [], []];
function outlet(i) { sent[i].push(Array.prototype.slice.call(arguments, 1)); }
function post() {}
load("m4l/chords/lt_chords.js");

function reset(s) {
    for (var k in settings) settings[k] = 0;
    for (var j in s) settings[j] = s[j];
}
function chord(pitch, s) { reset(s || {}); return buildChord(pitch); }
function same(a, b) { return a.join(",") === b.join(","); }
function abs(pitch, c) { return c.notes.map(function (n) { return pitch + n; }); }

print("chords");
var c = chord(60);                                     check(same(abs(60, c), [60, 64, 67]) && c.name === "C", "C -> C major " + c.name);
c = chord(57, { quality: 1 });                         check(same(abs(57, c), [57, 60, 64]) && c.name === "Am", "A + minor -> " + c.name);
c = chord(60, { seventh: 1 });                         check(same(abs(60, c), [60, 64, 67, 71]) && c.name === "Cmaj7", "7th on a major chord -> " + c.name);
c = chord(60, { seventh: 1, dom: 1 });                 check(abs(60, c)[3] === 70 && c.name === "C7", "DOM gives the b7 -> " + c.name);
c = chord(57, { quality: 1, seventh: 1, ninth: 1 });   check(same(abs(57, c), [57, 60, 64, 67, 71]) && c.name === "Am9", "minor 7 + 9 -> " + c.name);
c = chord(60, { ninth: 1 });                           check(same(abs(60, c), [60, 64, 67, 74]) && c.name === "Cadd9", "9 without 7 -> " + c.name);
c = chord(60, { octave: 1 });                          check(same(abs(60, c), [60, 64, 67, 72]), "octave adds the root an octave up");
c = chord(60, { seventh: 1, octave: 1 });              check(same(abs(60, c), [60, 64, 67, 71, 72]), "7 + octave");
c = chord(60, { seventh: 1, ninth: 1, octave: 1 });    check(same(abs(60, c), [60, 64, 67, 71, 74, 72]) && c.name === "Cmaj9", "7 + 8 + 9 -> " + c.name);

print("in key");
c = chord(62, { quality: 2, key: 0 });                 check(c.name === "Dm" && same(abs(62, c), [62, 65, 69]), "D in C major -> " + c.name);
c = chord(67, { quality: 2, key: 0, seventh: 1 });      check(c.name === "G7", "G in C major with 7 -> " + c.name);
c = chord(71, { quality: 2, key: 0 });                 check(c.name === "Bdim", "B in C major -> " + c.name);
c = chord(71, { quality: 2, key: 0, seventh: 1 });     check(c.name === "Bm7b5", "B7 in C major -> " + c.name);
c = chord(64, { quality: 2, key: 9, scale: 1 });       check(c.name === "E" && same(abs(64, c), [64, 68, 71]), "E in A minor is major (harmonic) -> " + c.name);
c = chord(64, { quality: 2, key: 9, scale: 1, seventh: 1 }); check(c.name === "E7", "E7 in A minor -> " + c.name);
c = chord(65, { quality: 2, key: 9, scale: 1, seventh: 1 }); check(c.name === "Fmaj7", "F in A minor with 7 -> " + c.name);
c = chord(70, { quality: 2, key: 5 });                 check(c.name === "Bb", "flats in F major -> " + c.name);

print("notes on / off");
reset({});
sent = [[], [], []];
list(60, 100); list(64, 90);                           // C (60 64 67) and E (64 68 71) share E
var ons = sent[0].filter(function (m) { return m[1] > 0; }).map(function (m) { return m[0]; });
check(same(ons, [60, 64, 67, 68, 71]), "two chords, a shared note is played once: " + ons);
sent[0] = [];
list(60, 0);
var offs = sent[0].map(function (m) { return m[0]; });
check(same(offs, [60, 67]), "releasing C keeps the shared E sounding: offs " + offs);
sent[0] = [];
list(64, 0);
offs = sent[0].map(function (m) { return m[0]; }).sort();
check(same(offs, [64, 68, 71]), "releasing E ends the rest: " + offs);
sent[0] = [];
list(60, 100); panic();
check(sent[0].filter(function (m) { return m[1] === 0; }).length === 3, "panic ends every chord");
check(sent[1].length > 0 && sent[1][sent[1].length - 1][1] === "C", "the label shows the chord name");

print("suggestions");
function tip(root, q, g) { var t = suggest(root, q, g); return t.length ? (["C","C#","D","D#","E","F","F#","G","G#","A","A#","B"][t[0].root] + (t[0].quality === "m" ? "m" : "")) : "-"; }
check(tip(9, "m", 0) === "F", "reggaeton after Am -> " + tip(9, "m", 0) + " (i-VI)");
check(tip(5, "M", 0) === "C", "reggaeton after F -> " + tip(5, "M", 0) + " (VI-III)");
check(tip(0, "M", 3) === "G", "pop after C -> " + tip(0, "M", 3) + " (I-V)");
check(tip(2, "m", 2) === "G", "r&b after Dm -> " + tip(2, "m", 2) + " (ii-V)");
check(tip(0, "m", 5) === "Fm" || tip(0, "m", 5) === "A#", "bachata after Cm -> " + tip(0, "m", 5));
var all = true;
for (var g = 0; g < GENRES.length; g++)
    for (var r = 0; r < 12; r++) { if (suggest(r, "M", g).length + suggest(r, "m", g).length === 0) all = false; }
check(all, "every genre has a tip for every major and minor chord");
reset({});
list(57, 100);
var text = sent[2][sent[2].length - 1][1];
check(/SIGUIENTE/.test(text) && /\(mayor\)|\(menor\)/.test(text), "the panel says what comes next and if it is major or minor:\n        " + text.replace(/\n/g, "\n        "));

print(failures === 0 ? "ALL PASSED" : failures + " FAILED");
if (failures) throw new Error(failures + " failed");
