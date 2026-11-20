#!/usr/bin/env python3
"""Builds the LT VACUUM Max for Live device.

Writes, next to this script:
  LT VACUUM.maxpat  - the patch as plain Max JSON (open it in Max to inspect or edit)
  LT VACUUM.amxd    - the same patch wrapped as a Max Audio Effect for Ableton Live

The device needs lt_vacuum.js in the same folder (or frozen into the device from the Max editor).
"""
import json
import struct
from pathlib import Path

HERE = Path(__file__).resolve().parent
W, H = 300, 169          # Live device height is fixed at 169 px

BG = [0.05, 0.04, 0.09, 1.0]
PANEL = [0.09, 0.08, 0.14, 1.0]
TEXT = [0.92, 0.90, 0.96, 1.0]
DIM = [0.62, 0.58, 0.72, 1.0]
ACCENT = [0.48, 0.17, 1.0, 1.0]
DANGER = [0.97, 0.44, 0.44, 1.0]

boxes, lines = [], []


def box(id_, maxclass, rect, present=None, **kw):
    b = {"id": id_, "maxclass": maxclass, "patching_rect": rect}
    if present:
        b["presentation"] = 1
        b["presentation_rect"] = present
    b.update(kw)
    boxes.append({"box": b})


def newobj(id_, text, rect, ins, outs, types=None):
    box(id_, "newobj", rect, text=text, numinlets=ins, numoutlets=outs, outlettype=types or [""] * outs)


def message(id_, text, rect):
    box(id_, "message", rect, text=text, numinlets=2, numoutlets=1, outlettype=[""])


def wire(src, so, dst, di):
    lines.append({"patchline": {"source": [src, so], "destination": [dst, di]}})


def button(id_, label, rect, present, colour):
    box(id_, "live.text", rect, present, text=label, texton=label, mode=0, numinlets=1, numoutlets=2, outlettype=["", ""],
        parameter_enable=0, fontname="Arial Bold", fontsize=11.0,
        bgcolor=PANEL, bgoncolor=colour, activebgcolor=PANEL, activebgoncolor=colour,
        textcolor=TEXT, textoncolor=[1, 1, 1, 1], activetextcolor=TEXT, activetextoncolor=[1, 1, 1, 1],
        bordercolor=colour, focusbordercolor=colour, rounded=6.0)


# ------------------------------------------------------------------ UI
box("title", "comment", [20, 20, 120, 20], [10, 8, 140, 20], text="LT VACUUM", fontname="Arial Bold", fontsize=13.0, textcolor=TEXT)
box("sub", "comment", [20, 40, 200, 18], [10, 26, 200, 16], text="removes tracks that never reach Main", fontname="Arial", fontsize=9.0, textcolor=DIM)
button("scan", "SCAN", [20, 70, 80, 24], [10, 50, 86, 26], ACCENT)
button("del", "DELETE", [120, 70, 80, 24], [102, 50, 86, 26], DANGER)
box("emptyToggle", "live.toggle", [220, 70, 20, 20], [196, 54, 18, 18], numinlets=1, numoutlets=1, outlettype=[""], parameter_enable=0,
    bgcolor=PANEL, activebgoncolor=ACCENT, bordercolor=DIM)
box("emptyLabel", "comment", [244, 70, 60, 18], [216, 54, 80, 18], text="+ EMPTY", fontname="Arial Bold", fontsize=9.5, textcolor=DIM)
box("list", "textedit", [20, 110, 280, 80], [10, 82, 280, 80], numinlets=1, numoutlets=4, outlettype=["", "int", "", ""],
    readonly=1, fontname="Arial", fontsize=9.5, textcolor=TEXT, bgcolor=PANEL, bordercolor=PANEL, rounded=6.0, wordwrap=1,
    text="SCAN finds tracks that never reach Main.")
box("bg", "panel", [0, 0, W, H], [0, 0, W, H], numinlets=1, numoutlets=0, bgcolor=BG, background=1, border=0, rounded=0)

# ------------------------------------------------------------------ logic
newobj("thisdev", "live.thisdevice", [320, 20, 90, 22], 1, 3, ["bang", "int", "int"])
message("msgInit", "init", [320, 50, 40, 22])
newobj("tScan", "t b", [20, 210, 30, 22], 1, 1, ["bang"])
newobj("tDel", "t b", [120, 210, 30, 22], 1, 1, ["bang"])
message("msgScan", "scan", [20, 240, 40, 22])
message("msgDel", "clean", [120, 240, 44, 22])
newobj("prepEmpty", "prepend empty", [220, 210, 90, 22], 1, 1)
newobj("js", "js lt_vacuum.js", [20, 290, 200, 22], 1, 3, ["", "", ""])
newobj("prepActive", "prepend active", [120, 330, 100, 22], 1, 1)

# audio passes straight through, so the device can sit on any track (even the master) without touching the sound
newobj("plugin", "plugin~", [440, 20, 60, 22], 1, 2, ["signal", "signal"])
newobj("plugout", "plugout~", [440, 70, 60, 22], 2, 0, [])
wire("plugin", 0, "plugout", 0)
wire("plugin", 1, "plugout", 1)

wire("thisdev", 0, "msgInit", 0)
wire("msgInit", 0, "js", 0)
wire("scan", 0, "tScan", 0)
wire("tScan", 0, "msgScan", 0)
wire("msgScan", 0, "js", 0)
wire("del", 0, "tDel", 0)
wire("tDel", 0, "msgDel", 0)
wire("msgDel", 0, "js", 0)
wire("emptyToggle", 0, "prepEmpty", 0)
wire("prepEmpty", 0, "js", 0)
wire("js", 0, "list", 0)
wire("js", 1, "prepActive", 0)
wire("prepActive", 0, "del", 0)

patcher = {
    "patcher": {
        "fileversion": 1,
        "appversion": {"major": 8, "minor": 5, "revision": 5, "architecture": "x64", "modernui": 1},
        "classnamespace": "box",
        "rect": [100.0, 100.0, 640.0, 480.0],
        "openinpresentation": 1,
        "default_fontsize": 10.0,
        "default_fontface": 0,
        "default_fontname": "Arial",
        "gridonopen": 1,
        "gridsize": [8.0, 8.0],
        "devicewidth": float(W),
        "description": "LT VACUUM: finds and deletes tracks whose signal never reaches Main",
        "boxes": boxes,
        "lines": lines,
        "dependency_cache": [{"name": "lt_vacuum.js", "type": "TEXT", "implicit": 1}],
        "autosave": 0,
    }
}

text = json.dumps(patcher, indent=1).encode("utf-8") + b"\n\x00"
(HERE / "LT VACUUM.maxpat").write_bytes(json.dumps(patcher, indent=1).encode("utf-8") + b"\n")

# .amxd container: 'ampf' header with the device type ('aaaa' = audio effect), an empty 'meta' chunk,
# then the patch JSON in a 'ptch' chunk.
amxd = b"ampf" + struct.pack("<I", 4) + b"aaaa" + b"meta" + struct.pack("<I", 4) + b"\x00\x00\x00\x00" + b"ptch" + struct.pack("<I", len(text)) + text
(HERE / "LT VACUUM.amxd").write_bytes(amxd)
print("wrote", HERE / "LT VACUUM.maxpat", "and", HERE / "LT VACUUM.amxd", f"({len(amxd)} bytes)")
