"""Small helper to write Max for Live devices (.maxpat + .amxd) from Python.

    p = Patch(width=360)
    p.live_tab("mode", ["MAJOR", "MINOR"], patch=[20, 20, 120, 20], present=[10, 30, 120, 18])
    p.newobj("js", "js lt_chords.js", [20, 200, 120, 22], 1, 3)
    p.wire("mode", 0, "js", 0)
    p.write(folder, "LT CHORDS", kind="midi")
"""
import json
import struct
from pathlib import Path

BG = [0.05, 0.04, 0.09, 1.0]
PANEL = [0.10, 0.09, 0.15, 1.0]
TEXT = [0.93, 0.91, 0.97, 1.0]
DIM = [0.62, 0.58, 0.72, 1.0]
ACCENT = [0.48, 0.17, 1.0, 1.0]
WARN = [0.97, 0.44, 0.44, 1.0]
GOOD = [0.20, 0.83, 0.60, 1.0]

KINDS = {"audio": b"aaaa", "midi": b"mmmm", "instrument": b"iiii"}


class Patch:
    def __init__(self, width=300, height=169):
        self.width, self.height = width, height
        self.boxes, self.lines = [], []
        self.deps = []

    # ------------------------------------------------------------------ raw
    def box(self, id_, maxclass, rect, present=None, **kw):
        b = {"id": id_, "maxclass": maxclass, "patching_rect": rect}
        if present:
            b["presentation"] = 1
            b["presentation_rect"] = present
        b.update(kw)
        self.boxes.append({"box": b})
        return id_

    def newobj(self, id_, text, rect, ins, outs, types=None):
        return self.box(id_, "newobj", rect, text=text, numinlets=ins, numoutlets=outs, outlettype=types or [""] * outs)

    def message(self, id_, text, rect):
        return self.box(id_, "message", rect, text=text, numinlets=2, numoutlets=1, outlettype=[""])

    def wire(self, src, so, dst, di):
        self.lines.append({"patchline": {"source": [src, so], "destination": [dst, di]}})

    def js(self, id_, filename, rect, outs):
        self.deps.append(filename)
        return self.newobj(id_, "js " + filename, rect, 1, outs)

    # ------------------------------------------------------------------ UI pieces
    def comment(self, id_, text, rect, present, size=10.0, colour=TEXT, bold=False):
        return self.box(id_, "comment", rect, present, text=text, fontname="Arial Bold" if bold else "Arial",
                        fontsize=size, textcolor=colour, numinlets=1, numoutlets=0)

    def panel(self, id_, present, colour=BG, rounded=0):
        return self.box(id_, "panel", [0, 0, present[2], present[3]], present, numinlets=1, numoutlets=0,
                        bgcolor=colour, background=1, border=0, rounded=rounded)

    @staticmethod
    def _param(name, short, enum=None, ptype=2, mmax=None, initial=None):
        v = {"parameter_longname": name, "parameter_shortname": short, "parameter_type": ptype}
        if enum is not None:
            v["parameter_enum"] = enum
            v["parameter_mmax"] = len(enum) - 1
        if mmax is not None:
            v["parameter_mmax"] = mmax
        if initial is not None:
            v["parameter_initial_enable"] = 1
            v["parameter_initial"] = [initial]
        return {"valueof": v}

    def live_tab(self, id_, items, rect, present, name, initial=0):
        return self.box(id_, "live.tab", rect, present, numinlets=1, numoutlets=3, outlettype=["", "", "float"],
                        num_lines_patching=1, num_lines_presentation=1, parameter_enable=1,
                        saved_attribute_attributes=self._param(name, name, items, initial=initial),
                        varname=name, fontname="Arial Bold", fontsize=9.5,
                        activebgoncolor=ACCENT, bgoncolor=ACCENT, activetextoncolor=[1, 1, 1, 1], textoncolor=[1, 1, 1, 1],
                        bgcolor=PANEL, activebgcolor=PANEL, textcolor=TEXT, activetextcolor=TEXT, bordercolor=PANEL)

    def live_menu(self, id_, items, rect, present, name, initial=0):
        return self.box(id_, "live.menu", rect, present, numinlets=1, numoutlets=3, outlettype=["", "", "float"],
                        parameter_enable=1, saved_attribute_attributes=self._param(name, name, items, initial=initial),
                        varname=name, fontsize=9.5, activebgcolor=PANEL, bgcolor=PANEL, textcolor=TEXT, activetextcolor=TEXT)

    def live_toggle_text(self, id_, label, rect, present, name, colour=ACCENT, initial=0):
        """A live.text in toggle mode (key/MIDI mappable, saved with the set)."""
        return self.box(id_, "live.text", rect, present, text=label, texton=label, mode=1, numinlets=1, numoutlets=2,
                        outlettype=["", ""], parameter_enable=1,
                        saved_attribute_attributes=self._param(name, name, ["off", "on"], initial=initial),
                        varname=name, fontname="Arial Bold", fontsize=10.0, rounded=5.0,
                        bgcolor=PANEL, bgoncolor=colour, activebgcolor=PANEL, activebgoncolor=colour,
                        textcolor=TEXT, textoncolor=[1, 1, 1, 1], activetextcolor=TEXT, activetextoncolor=[1, 1, 1, 1],
                        bordercolor=colour, focusbordercolor=colour)

    def live_button_text(self, id_, label, rect, present, name, colour=ACCENT):
        """A live.text in button mode: one press = one action (key/MIDI mappable)."""
        return self.box(id_, "live.text", rect, present, text=label, texton=label, mode=0, numinlets=1, numoutlets=2,
                        outlettype=["", ""], parameter_enable=1,
                        saved_attribute_attributes=self._param(name, name, ["off", "on"]),
                        varname=name, fontname="Arial Bold", fontsize=10.0, rounded=5.0,
                        bgcolor=PANEL, bgoncolor=colour, activebgcolor=PANEL, activebgoncolor=colour,
                        textcolor=TEXT, textoncolor=[1, 1, 1, 1], activetextcolor=TEXT, activetextoncolor=[1, 1, 1, 1],
                        bordercolor=colour, focusbordercolor=colour)

    def textbox(self, id_, rect, present, text="", size=9.5):
        return self.box(id_, "textedit", rect, present, numinlets=1, numoutlets=4, outlettype=["", "int", "", ""],
                        readonly=1, fontname="Arial", fontsize=size, textcolor=TEXT, bgcolor=PANEL,
                        bordercolor=PANEL, rounded=6.0, wordwrap=1, text=text)

    def passthrough_audio(self, x=600, y=20):
        """Audio devices must hand the sound on, or the track goes silent."""
        self.newobj("plugin", "plugin~", [x, y, 60, 22], 1, 2, ["signal", "signal"])
        self.newobj("plugout", "plugout~", [x, y + 50, 60, 22], 2, 0, [])
        self.wire("plugin", 0, "plugout", 0)
        self.wire("plugin", 1, "plugout", 1)

    # ------------------------------------------------------------------ output
    def write(self, folder, name, kind="audio", description=""):
        folder = Path(folder)
        patcher = {
            "patcher": {
                "fileversion": 1,
                "appversion": {"major": 8, "minor": 5, "revision": 5, "architecture": "x64", "modernui": 1},
                "classnamespace": "box",
                "rect": [100.0, 100.0, 900.0, 560.0],
                "openinpresentation": 1,
                "default_fontsize": 10.0,
                "default_fontface": 0,
                "default_fontname": "Arial",
                "gridonopen": 1,
                "gridsize": [8.0, 8.0],
                "devicewidth": float(self.width),
                "description": description,
                "boxes": self.boxes,
                "lines": self.lines,
                "dependency_cache": [{"name": d, "type": "TEXT", "implicit": 1} for d in self.deps],
                "autosave": 0,
            }
        }
        ids = {b["box"]["id"] for b in self.boxes}
        for l in self.lines:
            s, d = l["patchline"]["source"][0], l["patchline"]["destination"][0]
            assert s in ids and d in ids, f"dangling patch cord {s} -> {d}"
        body = json.dumps(patcher, indent=1).encode("utf-8")
        (folder / f"{name}.maxpat").write_bytes(body + b"\n")
        text = body + b"\n\x00"
        amxd = (b"ampf" + struct.pack("<I", 4) + KINDS[kind] + b"meta" + struct.pack("<I", 4) + b"\x00" * 4
                + b"ptch" + struct.pack("<I", len(text)) + text)
        (folder / f"{name}.amxd").write_bytes(amxd)
        return folder / f"{name}.amxd"
