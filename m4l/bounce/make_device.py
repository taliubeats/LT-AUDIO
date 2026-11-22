#!/usr/bin/env python3
"""Builds LT BOUNCE (Max for Live audio effect, sound passes straight through)."""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "common"))
from maxpat import Patch, ACCENT, DIM, WARN  # noqa: E402

p = Patch(width=260)
p.panel("bg", [0, 0, 260, 169])
p.comment("title", "LT BOUNCE", [20, 20, 120, 18], [10, 6, 140, 16], size=10.5, colour=DIM, bold=True)
p.comment("sub", "bounce the selected track in place", [20, 40, 220, 16], [10, 22, 240, 16], size=9.0, colour=DIM)
p.live_button_text("bounce", "BOUNCE", [20, 70, 110, 28], [10, 42, 118, 30], "Bounce", colour=ACCENT)
p.live_button_text("cancel", "CANCEL", [140, 70, 80, 28], [134, 42, 116, 30], "Cancel", colour=WARN)
p.comment("afterLabel", "THEN THE ORIGINAL:", [20, 104, 120, 16], [10, 78, 120, 16], size=8.5, colour=DIM, bold=True)
p.live_tab("after", ["MUTE", "KEEP", "DELETE"], [140, 104, 110, 18], [10, 94, 240, 18], "Original")
p.textbox("status", [20, 130, 230, 44], [10, 118, 240, 44], "Select a track, set the loop, press BOUNCE.")

p.js("js", "lt_bounce.js", [20, 220, 160, 22], 1)
p.wire("js", 0, "status", 0)
for id_, msg in (("bounce", "bounce"), ("cancel", "cancel")):
    p.newobj("t_" + id_, "t b", [300, 70 if id_ == "bounce" else 100, 30, 22], 1, 1, ["bang"])
    p.message("msg_" + id_, msg, [340, 70 if id_ == "bounce" else 100, 54, 22])
    p.wire(id_, 0, "t_" + id_, 0)
    p.wire("t_" + id_, 0, "msg_" + id_, 0)
    p.wire("msg_" + id_, 0, "js", 0)
p.newobj("pre_after", "prepend setAfter", [300, 140, 100, 22], 1, 1)
p.wire("after", 0, "pre_after", 0)
p.wire("pre_after", 0, "js", 0)

p.newobj("thisdev", "live.thisdevice", [420, 20, 90, 22], 1, 3, ["bang", "int", "int"])
p.newobj("fan", "t b b", [420, 50, 50, 22], 1, 2, ["bang", "bang"])
p.message("msgInit", "init", [480, 80, 40, 22])
p.wire("thisdev", 0, "fan", 0)
p.wire("fan", 1, "msgInit", 0)
p.wire("msgInit", 0, "js", 0)
p.wire("fan", 0, "after", 0)
p.passthrough_audio()

print("wrote", p.write(HERE, "LT BOUNCE", kind="audio", description="LT BOUNCE: record the selected track over the loop into a new audio track, in place"))
