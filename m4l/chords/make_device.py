#!/usr/bin/env python3
"""Builds LT CHORDS (Max for Live MIDI effect): LT CHORDS.amxd + LT CHORDS.maxpat next to this script."""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "common"))
from maxpat import Patch, ACCENT, DIM, GOOD, TEXT  # noqa: E402

p = Patch(width=380)
p.panel("bg", [0, 0, 380, 169])
p.comment("title", "LT CHORDS", [20, 20, 100, 18], [10, 6, 120, 16], size=10.5, colour=DIM, bold=True)
p.comment("chordLabel", "LT CHORDS", [20, 44, 170, 36], [10, 22, 176, 38], size=26.0, colour=TEXT, bold=True)

# settings
p.live_tab("quality", ["MAYOR", "MENOR", "EN TONO"], [220, 20, 180, 18], [190, 8, 182, 18], "Calidad")
p.live_menu("key", ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"], [220, 44, 56, 18], [190, 32, 52, 18], "Tonalidad")
p.live_tab("scale", ["MAY", "MEN"], [280, 44, 70, 18], [246, 32, 72, 18], "Escala")
p.live_toggle_text("dom", "DOM7", [356, 44, 44, 18], [322, 32, 50, 18], "Dom7", colour=GOOD)
p.live_toggle_text("seventh", "7a", [20, 90, 50, 22], [10, 62, 52, 22], "Septima")
p.live_toggle_text("ninth", "9a", [74, 90, 50, 22], [66, 62, 52, 22], "Novena")
p.live_toggle_text("octave", "8a", [128, 90, 50, 22], [122, 62, 52, 22], "Octava")
p.live_menu("genre", ["REGGAETON", "TRAP", "R&B", "POP", "LO-FI", "BACHATA", "AFRO", "HOUSE"], [220, 90, 180, 20], [190, 63, 182, 20], "Genero")
p.textbox("info", [20, 130, 380, 74], [10, 90, 362, 72], "Toca una nota: sale un acorde.")

# MIDI in -> js -> MIDI out (everything that is not a note passes straight through)
p.newobj("midiin", "midiin", [460, 20, 50, 22], 1, 1, ["int"])
p.newobj("parse", "midiparse", [460, 50, 120, 22], 1, 8, ["", "", "", "int", "int", "int", "int", ""])
p.js("js", "lt_chords.js", [460, 200, 180, 22], 3)
p.newobj("format", "midiformat", [460, 250, 120, 22], 7, 2, ["int", ""])
p.newobj("midiout", "midiout", [460, 280, 60, 22], 1, 0, [])
p.wire("midiin", 0, "parse", 0)
p.wire("parse", 0, "js", 0)
for k in range(1, 6):
    p.wire("parse", k, "format", k)
p.wire("js", 0, "format", 0)
p.wire("format", 0, "midiout", 0)
p.wire("js", 1, "chordLabel", 0)
p.wire("js", 2, "info", 0)

# each setting -> "name value" -> js
controls = ["quality", "key", "scale", "dom", "seventh", "ninth", "octave", "genre"]
for i, c in enumerate(controls):
    p.newobj("pre_" + c, "prepend " + c, [700, 20 + i * 28, 100, 22], 1, 1)
    p.wire(c, 0, "pre_" + c, 0)
    p.wire("pre_" + c, 0, "js", 0)

# on load: js init first (rightmost outlet fires first), then every control reports its saved value
p.newobj("thisdev", "live.thisdevice", [860, 20, 90, 22], 1, 3, ["bang", "int", "int"])
p.newobj("fan", "t " + " ".join(["b"] * (len(controls) + 1)), [860, 50, 220, 22], 1, len(controls) + 1, ["bang"] * (len(controls) + 1))
p.message("msgInit", "init", [1090, 80, 40, 22])
p.wire("thisdev", 0, "fan", 0)
p.wire("fan", len(controls), "msgInit", 0)
p.wire("msgInit", 0, "js", 0)
for i, c in enumerate(controls):
    p.wire("fan", i, c, 0)

# all notes off when the device is switched off or the set stops
p.message("msgPanic", "panic", [860, 110, 50, 22])
p.newobj("selOff", "sel 0", [860, 80, 40, 22], 2, 2, ["bang", ""])
p.wire("thisdev", 1, "selOff", 0)
p.wire("selOff", 0, "msgPanic", 0)
p.wire("msgPanic", 0, "js", 0)

out = p.write(HERE, "LT CHORDS", kind="midi", description="LT CHORDS: one note in, a chord out, plus the chord that usually comes next in your genre")
print("wrote", out)
