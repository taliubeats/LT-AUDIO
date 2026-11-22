#!/usr/bin/env python3
"""Builds LT CLIP GAIN (Max for Live audio effect, sound passes straight through)."""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "common"))
from maxpat import Patch, ACCENT, DIM, GOOD, TEXT, WARN  # noqa: E402

p = Patch(width=250)
p.panel("bg", [0, 0, 250, 169])
p.comment("title", "LT CLIP GAIN", [20, 20, 120, 18], [10, 6, 140, 16], size=10.5, colour=DIM, bold=True)
p.comment("clipName", "No clip selected", [20, 40, 220, 18], [10, 24, 230, 18], size=10.0, colour=DIM)
p.comment("gain", "-", [20, 60, 220, 36], [10, 42, 230, 38], size=26.0, colour=TEXT, bold=True)

buttons = [("m3", "-3 dB", -3, WARN), ("m1", "-1 dB", -1, WARN), ("p1", "+1 dB", 1, GOOD), ("p3", "+3 dB", 3, GOOD)]
for i, (id_, label, db, col) in enumerate(buttons):
    x = 10 + i * 58
    p.live_button_text(id_, label, [20 + i * 60, 110, 54, 24], [x, 88, 54, 28], "Gain " + label, colour=col)
    p.newobj("t_" + id_, "t b", [20 + i * 60, 150, 30, 22], 1, 1, ["bang"])
    p.message("msg_" + id_, "step " + str(db), [20 + i * 60, 180, 50, 22])
    p.wire(id_, 0, "t_" + id_, 0)
    p.wire("t_" + id_, 0, "msg_" + id_, 0)
    p.wire("msg_" + id_, 0, "js", 0)
p.live_button_text("unity", "0 dB", [20, 220, 100, 22], [10, 122, 112, 26], "Gain 0 dB", colour=ACCENT)
p.newobj("t_unity", "t b", [130, 220, 30, 22], 1, 1, ["bang"])
p.message("msg_unity", "unity", [170, 220, 44, 22])
p.wire("unity", 0, "t_unity", 0)
p.wire("t_unity", 0, "msg_unity", 0)
p.wire("msg_unity", 0, "js", 0)
p.comment("hint", "map the buttons to keys: Cmd/Ctrl+K", [20, 250, 220, 18], [126, 124, 120, 26], size=8.5, colour=DIM)

p.js("js", "lt_clipgain.js", [20, 290, 160, 22], 2)
p.wire("js", 0, "clipName", 0)
p.wire("js", 1, "gain", 0)
p.newobj("thisdev", "live.thisdevice", [300, 20, 90, 22], 1, 3, ["bang", "int", "int"])
p.message("msgInit", "init", [300, 50, 40, 22])
p.wire("thisdev", 0, "msgInit", 0)
p.wire("msgInit", 0, "js", 0)
p.passthrough_audio()

print("wrote", p.write(HERE, "LT CLIP GAIN", kind="audio", description="LT CLIP GAIN: exact dB steps for the selected audio clip, mappable to keys"))
