#!/usr/bin/env python3
"""
Generates the LT QUASAR factory banks (plugins/quasar/Resources/banks/*.json).

Usage:
  python3 make_banks.py [--profiles ref_profiles.json] [--levels levels.json]

Every preset is built from an archetype (pluck, keys, bell, pad, lead, bass, 808, reese, arp, texture)
plus musical tweaks. Values are real units (Hz, ms, %, dB). Routes and effects are written by name;
the plugin resolves them when it loads the preset. --levels applies the per-preset gain measured by
the quasar_render tool so every preset plays at a similar loudness.
"""
import json, os, random, sys, argparse, copy

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "..", "plugins", "quasar", "Resources", "banks")

TABLES = ["BASIC SHAPES", "ANALOG SAW", "PWM", "WARM SWEEP", "SAW STACK", "HOLLOW", "ORGAN", "E-PIANO", "GLASS", "FM 1:1", "FM 2:1",
          "FM 3:1", "VOWELS", "CHOIR", "HARMONIC SWEEP", "SYNC SAW", "SYNC SQUARE", "DIGITAL", "FOLD", "808 DRIVE", "TAPE SINE", "REESE",
          "PLUCK", "STRINGS", "BRASS", "FLUTE", "MALLET", "GUITAR", "CHIP", "NASAL", "SINE STACK", "DIRTY SAW", "COMB SWEEP", "METALLIC",
          "DUST", "GROWL"]
FILTERS = ["LADDER 24", "LADDER 12", "LP 24", "LP 12", "HP 24", "HP 12", "BP 24", "BP 12", "NOTCH", "PEAK", "COMB +", "COMB -", "FORMANT", "DIODE"]
WARPS = ["OFF", "SYNC", "BEND +", "BEND -", "PWM", "MIRROR", "ASYM", "FM", "RING", "FOLD", "QUANTIZE"]
DIVS = ["8/1", "4/1", "2/1", "1/1", "1/2", "1/2T", "1/2D", "1/4", "1/4T", "1/4D", "1/8", "1/8T", "1/8D", "1/16", "1/16T", "1/16D", "1/32", "1/32T", "1/64"]
NOISES = ["WHITE", "PINK", "BROWN", "VINYL", "TAPE HISS", "AIR", "CRACKLE", "DIGITAL"]
PATTERNS = ["STRAIGHT", "TRAP ROLL", "TRIPLET FLOW", "BOUNCE", "DEMBOW", "SKIP", "STAIRS", "RATCHET", "OCT JUMP", "DRILL", "PLUGG", "LO-FI SWING"]
ARPMODES = ["UP", "DOWN", "UP/DOWN", "DOWN/UP", "RANDOM", "ORDER", "CHORD"]
OSC = "ABC"
# measured on the references: modern trap / reggaeton sounds are darker than a raw synth -> scale the filters
WARM = {"PLUCK": 0.55, "LEAD": 0.55, "KEYS": 0.6, "BELL": 0.75, "PAD": 0.85, "FX": 0.5}


class P:
    def __init__(self, name, category, style, description=""):
        self.name, self.category, self.style, self.description = name, category, style, description
        self.v = {}
        self.mods = []
        self.fx = {"MAIN": [], "BUS 1": [], "BUS 2": []}
        self.macros = ["BRIGHTNESS", "TIMBRE", "TIME", "MOVEMENT", "DRIVE", "WIDTH", "DELAY", "SPACE"]
        self.lfo = [None] * 4
        self.v["oscA_on"] = 1

    # ---- sources
    def osc(self, i, table="ANALOG SAW", type=0, pos=0, unison=1, detune=25, blend=75, spread=100, level=75, pan=0,
            oct=0, semi=0, fine=0, warp=None, warpamt=0, rand=100, phase=0, dest=0):
        k = "osc" + OSC[i] + "_"
        self.v[k + "on"] = 1
        self.v[k + "type"] = type
        self.v[k + "table"] = TABLES.index(table)
        for name, val in (("wtpos", pos), ("unison", unison), ("detune", detune), ("blend", blend), ("spread", spread), ("level", level),
                          ("pan", pan), ("oct", oct), ("semi", semi), ("fine", fine), ("rand", rand), ("phase", phase), ("dest", dest)):
            self.v[k + name] = val
        if warp:
            self.v[k + "warpmode"] = WARPS.index(warp)
            self.v[k + "warp"] = warpamt
        return self

    def off(self, i):
        self.v["osc" + OSC[i] + "_on"] = 0
        return self

    def sub(self, level=60, shape=0, oct=-1, dest=0):
        self.v.update({"sub_on": 1, "sub_level": level, "sub_shape": shape, "sub_oct": oct, "sub_dest": dest})
        return self

    def noise(self, kind="VINYL", level=30, color=0, dest=0):
        self.v.update({"noise_on": 1, "noise_type": NOISES.index(kind), "noise_level": level, "noise_color": color, "noise_dest": dest})
        return self

    def filt(self, i, type="LADDER 24", cutoff=2000, reso=15, drive=0, mix=100, key=0, level=75, dest=None):
        k = "flt%d_" % (i + 1)
        self.v.update({k + "on": 1, k + "type": FILTERS.index(type), k + "cutoff": cutoff, k + "reso": reso, k + "drive": drive,
                       k + "mix": mix, k + "key": key, k + "level": level})
        if dest is not None:
            self.v[k + "dest"] = dest
        return self

    def env(self, i, att=1, dec=600, sus=0, rel=300, hold=0, ac=-30, dc=60, rc=60):
        k = "envP_" if i == 3 else "env%d_" % (i + 1)
        self.v.update({k + "att": att, k + "dec": dec, k + "sus": sus, k + "rel": rel, k + "hold": hold, k + "acurve": ac, k + "dcurve": dc, k + "rcurve": rc})
        return self

    def amp(self, **kw):
        return self.env(0, **kw)

    def lfo_(self, i, rate=None, div=None, mode="RETRIG", shape=None, rise=0, smooth=0, delay=0):
        k = "lfo%d_" % (i + 1)
        if div is not None:
            self.v[k + "sync"] = 1
            self.v[k + "div"] = DIVS.index(div)
        else:
            self.v[k + "sync"] = 0
            self.v[k + "rate"] = rate if rate is not None else 2.0
        self.v[k + "mode"] = ["FREE", "RETRIG", "ENV"].index(mode)
        self.v[k + "rise"] = rise; self.v[k + "smooth"] = smooth; self.v[k + "delay"] = delay
        if shape is not None:
            self.lfo[i] = shape
        return self

    def mod(self, src, dst, amt, bip=False, aux=None, curve="LINEAR"):
        m = {"src": src, "dst": dst, "amt": amt}
        if bip: m["bip"] = True
        if aux: m["aux"] = aux
        if curve != "LINEAR": m["curve"] = curve
        self.mods.append(m)
        return self

    def add_fx(self, fxtype, mix=100, bus="MAIN", out=0, **params):
        # parameter names as shown on the module: pre_dly -> PRE-DLY, lo_cut -> LO CUT
        fix = {"PRE DLY": "PRE-DLY"}
        names = {fix.get(k.replace("_", " ").upper(), k.replace("_", " ").upper()): v for k, v in params.items()}
        self.fx[bus].append({"type": fxtype, "mix": mix, "out": out, "params": names})
        return self

    def route(self, src, dest):
        key = {"SUB": "sub_dest", "NOISE": "noise_dest"}.get(src, "osc%s_dest" % src)
        self.v[key] = dest
        return self

    def set(self, **kw):
        for k, v in kw.items():
            self.v[k] = v
        return self

    def voice(self, mode="POLY", glide=0, legato=False, poly=16, stack=1):
        self.v.update({"voice_mode": ["POLY", "MONO", "LEGATO"].index(mode), "glide": glide, "glide_legato": 1 if legato else 0, "poly": poly, "stack": stack})
        return self

    def arp(self, mode="UP", rate="1/16", pattern="STRAIGHT", octaves=1, gate=60, swing=0):
        self.v.update({"arp_on": 1, "arp_mode": ARPMODES.index(mode), "arp_rate": DIVS.index(rate), "arp_pattern": PATTERNS.index(pattern),
                       "arp_oct": octaves, "arp_gate": gate, "arp_swing": swing})
        return self

    def json(self):
        lfo = []
        for s in self.lfo:
            lfo.append(s if s is not None else SHAPES["sine"])
        return {"name": self.name, "category": self.category, "style": self.style, "author": "LT SOUNDS", "description": self.description,
                "values": self.v, "mods": self.mods, "fx": {k: v for k, v in self.fx.items() if v}, "lfo": lfo, "macros": self.macros, "samples": ["", "", ""]}


def pts(*p, kind=0):
    return {"kind": kind, "points": [list(x) for x in p]}


SHAPES = {
    "sine": pts((0, 0.5, -0.45), (0.25, 1, 0.45), (0.5, 0.5, -0.45), (0.75, 0, 0.45)),
    "tri": pts((0, 0, 0), (0.5, 1, 0)),
    "saw": pts((0, 1, 0), (0.999, 0, 0)),
    "ramp": pts((0, 0, 0), (0.999, 1, 0)),
    "square": pts((0, 1, 0), (0.499, 1, 0), (0.5, 0, 0), (0.999, 0, 0)),
    "pump": pts((0, 0, 0.6), (0.4, 1, 0), (0.999, 1, 0)),
    "sh": pts((0, 0.5, 0), kind=1),
    "smooth": pts((0, 0.5, 0), kind=2),
    "gate": pts((0, 1, 0), (0.2, 1, 0), (0.25, 0, 0), (0.5, 0, 0), (0.5, 1, 0), (0.7, 1, 0), (0.75, 0, 0), (0.999, 0, 0)),
    "wobble": pts((0, 0, -0.6), (0.5, 1, 0.6)),
}


# ================================================================== standard macros

def macros(p, bright=("flt1_cutoff", 45), timbre=("oscA_wtpos", 40), time=None, movement=None, drive=None, width=None, delay=None, space=None):
    """MACRO 1..8 -> routes. Effects macros look the effect up by its position in MAIN."""
    def fxslot(t):
        for i, e in enumerate(p.fx["MAIN"]):
            if e["type"] == t:
                return i
        return None
    if bright: p.mod("MACRO 1", bright[0], bright[1])
    if timbre: p.mod("MACRO 2", timbre[0], timbre[1])
    p.mod("MACRO 3", time[0] if time else "env1_dec", time[1] if time else 35)
    if time is None and "env2_dec" in p.v: p.mod("MACRO 3", "env2_dec", 35)
    if movement:
        p.mod(movement[0], movement[1], movement[2], aux="MACRO 4", bip=True)
    else:
        p.lfo_(3, div="1/2", mode="FREE", shape=SHAPES["sine"])
        p.mod("LFO 4", "oscA_wtpos", 30, aux="MACRO 4", bip=True)
    s = fxslot("SATURATION") if fxslot("SATURATION") is not None else fxslot("DISTORTION")
    if drive: p.mod("MACRO 5", drive[0], drive[1])
    elif s is not None: p.mod("MACRO 5", "fxM%d_p2" % (s + 1), 45)
    else: p.mod("MACRO 5", "flt1_drive", 60)
    c = fxslot("CHORUS")
    if width: p.mod("MACRO 6", width[0], width[1])
    elif c is not None: p.mod("MACRO 6", "fxM%d_mix" % (c + 1), 60)
    else: p.mod("MACRO 6", "oscA_spread", 60)
    d = fxslot("DELAY")
    if delay: p.mod("MACRO 7", delay[0], delay[1])
    elif d is not None: p.mod("MACRO 7", "fxM%d_mix" % (d + 1), 50)
    r = fxslot("REVERB")
    if space: p.mod("MACRO 8", space[0], space[1])
    elif r is not None: p.mod("MACRO 8", "fxM%d_mix" % (r + 1), 50)
    return p


# ================================================================== effect chains

def space_fx(p, style, delay_mix=22, verb_mix=24, verb="PLATE", decay=2.2, div="1/8D", tape=False, chorus=True, sat=25):
    if sat: p.add_fx("SATURATION", 100, mode="TAPE" if style == "LO-FI" else "TUBE", drive=sat, warmth=35, tone=-10)
    if tape or style == "LO-FI": p.add_fx("TAPE", 100, wow=30, flutter=22, saturation=30, hiss=12, age=40)
    if chorus: p.add_fx("CHORUS", 45, rate=0.5, depth=40, delay=9, width=100)
    if delay_mix: p.add_fx("DELAY", delay_mix, mode="PING-PONG" if style in ("REGGAETON", "AFRO") else "TAPE", time=div, feedback=32,
                           lo_cut=220, hi_cut=5500, width=110)
    if verb_mix: p.add_fx("REVERB", verb_mix, type=verb, size=62, pre_dly=14, decay=decay, damp=7000, width=100, lo_cut=180, mod=40)
    return p


# ================================================================== archetypes

def pluck(name, style, table="ANALOG SAW", pos=0, cutoff=700, env_amt=38, decay=380, reso=18, unison=3, detune=18, filt="LADDER 24",
          sub=0, warp=None, warpamt=0, osc2=None, fx=True, verb_mix=22, delay_mix=24, div="1/8D", sat=20, noise=None, desc="", category="PLUCK"):
    p = P(name, category, style, desc)
    p.osc(0, table, pos=pos, unison=unison, detune=detune, blend=70, spread=90, warp=warp, warpamt=warpamt)
    if osc2: p.osc(1, osc2[0], pos=osc2[1], oct=osc2[2], level=osc2[3], unison=1)
    if sub: p.sub(sub)
    if noise: p.noise(noise[0], noise[1])
    p.filt(0, filt, cutoff=cutoff * WARM["PLUCK"], reso=reso, key=35, drive=10)
    p.amp(att=2.5, dec=decay * 1.6, sus=0, rel=decay * 0.9, ac=0, dc=70, rc=65)
    p.env(1, att=1.5, dec=decay * 0.8, sus=0, rel=decay * 0.6, ac=0, dc=75)
    p.mod("ENV 2", "flt1_cutoff", env_amt)
    p.mod("VELOCITY", "flt1_cutoff", 12)
    if fx: space_fx(p, style, delay_mix=delay_mix, verb_mix=verb_mix, div=div, sat=sat)
    macros(p)
    return p


def keys(name, style, kind="E-PIANO", cutoff=3500, decay=1400, sus=25, trem=0, unison=1, desc="", fx=True, sat=25, verb_mix=20, tape=False):
    p = P(name, "KEYS", style, desc)
    evolving = kind in ("E-PIANO", "MALLET", "PLUCK")
    # E-PIANO / MALLET frames go bright -> mellow: start bright (ENV 2), settle into the body
    p.osc(0, kind, pos=85 if evolving else 20, unison=unison, detune=10, blend=60)
    p.osc(1, "TAPE SINE", pos=30, level=55, unison=1)
    if evolving:
        p.env(1, att=0, dec=decay * 0.45, sus=0, rel=400, dc=65)
        p.mod("ENV 2", "oscA_wtpos", -80)
    p.filt(0, "LP 12", cutoff=cutoff * WARM["KEYS"], reso=8, key=40)
    p.amp(att=2, dec=decay, sus=sus, rel=420, ac=0, dc=55, rc=55)
    p.mod("VELOCITY", "flt1_cutoff", 25)
    if trem:
        p.lfo_(0, rate=trem, mode="FREE", shape=SHAPES["sine"])
        p.mod("LFO 1", "oscA_level", -12, bip=True)
        p.mod("LFO 1", "oscA_pan", 22, bip=True)
    if fx: space_fx(p, style, delay_mix=12, verb_mix=verb_mix, decay=2.6, sat=sat, tape=tape)
    macros(p, timbre=("oscB_level", 40))
    return p


def bell(name, style, table="GLASS", pos=40, decay=1200, fm=None, cutoff=6000, unison=2, verb_mix=30, delay_mix=22, desc="", sat=10):
    p = P(name, "BELL", style, desc)
    p.osc(0, table, pos=pos, unison=unison, detune=8, blend=55, spread=100)
    if fm: p.osc(1, fm[0], pos=fm[1], oct=fm[2], level=fm[3])
    p.filt(0, "LP 12", cutoff=cutoff * WARM["BELL"], reso=10, key=50)
    p.amp(att=1.5, dec=decay, sus=0, rel=decay * 0.8, ac=0, dc=72, rc=70)
    p.env(1, att=0, dec=decay * 0.4, sus=0, rel=300)
    p.mod("ENV 2", "oscA_wtpos", 35)
    space_fx(p, style, delay_mix=delay_mix, verb_mix=verb_mix, decay=3.0, sat=sat, chorus=True)
    macros(p)
    return p


def pad(name, style, table="SAW STACK", pos=30, attack=900, release=1800, cutoff=1800, unison=6, detune=30, lfo_rate=0.15,
        osc2=None, noise=None, desc="", verb_mix=35, chorus=True, verb="HALL", sat=15):
    p = P(name, "PAD", style, desc)
    p.osc(0, table, pos=pos, unison=unison, detune=detune, blend=75, spread=100)
    if osc2: p.osc(1, osc2[0], pos=osc2[1], oct=osc2[2], level=osc2[3], unison=osc2[4] if len(osc2) > 4 else 4, detune=22)
    if noise: p.noise(noise[0], noise[1], noise[2] if len(noise) > 2 else 0)
    p.filt(0, "LADDER 12", cutoff=cutoff * WARM["PAD"], reso=20, key=20)
    p.amp(att=attack, dec=2000, sus=85, rel=release, ac=-20, rc=50)
    p.env(1, att=attack * 1.5, dec=3000, sus=60, rel=release)
    p.mod("ENV 2", "flt1_cutoff", 18)
    p.lfo_(0, rate=lfo_rate, mode="FREE", shape=SHAPES["smooth"] if lfo_rate < 0.3 else SHAPES["sine"])
    p.mod("LFO 1", "oscA_wtpos", 25, bip=True)
    p.lfo_(1, rate=lfo_rate * 2.3, mode="FREE")
    p.mod("LFO 2", "flt1_cutoff", 10, bip=True)
    space_fx(p, style, delay_mix=15, verb_mix=verb_mix, verb=verb, decay=4.5, div="1/4D", chorus=chorus, sat=sat)
    macros(p, movement=("LFO 1", "oscA_wtpos", 45))
    return p


def lead(name, style, table="SAW STACK", pos=20, cutoff=4200, unison=4, detune=20, glide=60, vibrato=True, mono=True, drive=25,
         osc2=None, desc="", delay_mix=24, verb_mix=18, warp=None, warpamt=0, filt="LADDER 24"):
    p = P(name, "LEAD", style, desc)
    p.osc(0, table, pos=pos, unison=unison, detune=detune, blend=70, warp=warp, warpamt=warpamt)
    if osc2: p.osc(1, osc2[0], pos=osc2[1], oct=osc2[2], level=osc2[3], unison=osc2[4] if len(osc2) > 4 else 1, semi=osc2[5] if len(osc2) > 5 else 0)
    p.filt(0, filt, cutoff=cutoff * WARM["LEAD"], reso=22, drive=drive, key=40)
    p.amp(att=4, dec=800, sus=80, rel=260)
    p.env(1, att=0, dec=500, sus=30, rel=300)
    p.mod("ENV 2", "flt1_cutoff", 20)
    if vibrato:
        p.lfo_(0, rate=5.5, mode="RETRIG", delay=280, rise=400)
        p.mod("LFO 1", "oscA_fine", 9, bip=True)
        if osc2: p.mod("LFO 1", "oscB_fine", 9, bip=True)
    p.mod("MOD WHEEL", "oscA_fine", 12, bip=True)
    if mono: p.voice("LEGATO", glide=glide, legato=True)
    space_fx(p, style, delay_mix=delay_mix, verb_mix=verb_mix, sat=drive)
    macros(p, movement=("LFO 1", "oscA_fine", 20))
    return p


def bass(name, style, table="ANALOG SAW", pos=0, cutoff=500, env_amt=30, decay=350, sus=60, drive=30, sub=70, mono=True, glide=40,
         osc2=None, desc="", warp=None, warpamt=0, filt="LADDER 24", reso=20, category="BASS"):
    p = P(name, category, style, desc)
    p.osc(0, table, pos=pos, unison=1, warp=warp, warpamt=warpamt)
    if osc2: p.osc(1, osc2[0], pos=osc2[1], oct=osc2[2], level=osc2[3], unison=osc2[4] if len(osc2) > 4 else 1, detune=osc2[5] if len(osc2) > 5 else 20)
    if sub: p.sub(sub, oct=-1)
    p.filt(0, filt, cutoff=cutoff, reso=reso, drive=drive, key=30)
    p.amp(att=2, ac=0, dec=decay * 2, sus=sus, rel=120, dc=60)
    p.env(1, att=0, dec=decay, sus=10, rel=120)
    p.mod("ENV 2", "flt1_cutoff", env_amt)
    p.mod("VELOCITY", "flt1_cutoff", 10)
    if mono: p.voice("LEGATO", glide=glide, legato=True)
    p.add_fx("SATURATION", 100, mode="TUBE", drive=drive, warmth=40, character=35)
    p.add_fx("EQUALIZER", 100, low_freq=60, low_gain=2, mid_freq=900, mid_gain=-2, mid_q=0.8, high_freq=9000, high_gain=-3)
    p.add_fx("STEREO WIDTH", 100, width=110, bass_mono=180)
    macros(p, drive=("fxM1_p2", 50))
    return p


def eight08(name, style, drive=35, decay=1800, punch=12, glide=90, table="808 DRIVE", pos=10, dist=None, desc="", sub_click=None):
    if sub_click is None: sub_click = punch > 6   # clean subs stay clean
    p = P(name, "808", style, desc)
    p.osc(0, table, pos=pos, unison=1, rand=0, phase=0)
    p.amp(att=1.5, ac=0, dec=decay, sus=0 if decay < 3000 else 40, rel=220, dc=40)
    p.env(3, att=0, dec=70, sus=0, rel=40, dc=70)
    p.set(envP_amt=punch)
    p.env(1, att=0, dec=decay * 0.5, sus=0, rel=200)
    p.mod("ENV 2", "oscA_wtpos", 25)
    if sub_click: p.noise("WHITE", 0, 30); p.set(noise_dest=3); p.mod("ENV 3", "noise_level", 55); p.env(2, att=0, dec=14, sus=0, rel=10)
    p.voice("LEGATO", glide=glide, legato=True)
    p.add_fx("DISTORTION" if dist else "SATURATION", 100, **({"mode": dist, "drive": drive, "tone": 6000} if dist else {"mode": "TAPE" if drive < 30 else "TRANSISTOR", "drive": drive * 0.6, "warmth": 30}))
    p.add_fx("EQUALIZER", 100, low_freq=55, low_gain=3, mid_freq=180, mid_gain=-2, mid_q=1.0, high_freq=6000, high_gain=-4)
    p.add_fx("COMPRESSOR", 100, threshold=-14, ratio=3, attack=12, release=150, knee=6, makeup=2, mode="PUNCH")
    p.add_fx("UTILITY", 100, mono=1)
    macros(p, bright=("fxM1_p3" if dist else "fxM2_p6", 35), timbre=("oscA_wtpos", 45), time=("env1_dec", 40), drive=("fxM1_p2", 50),
           width=None, movement=("LFO 1", "oscA_fine", 15))
    p.macros[1] = "GRIT"; p.macros[2] = "LENGTH"; p.macros[3] = "WOBBLE"
    return p


def reese(name, style, cutoff=700, detune=35, drive=35, desc="", table="REESE", lfo="1/2"):
    p = P(name, "REESE", style, desc)
    p.osc(0, table, pos=30, unison=4, detune=detune, blend=80, spread=60)
    p.osc(1, "ANALOG SAW", pos=10, oct=-1, unison=2, detune=12, level=60)
    p.sub(55)
    p.filt(0, "LADDER 24", cutoff=cutoff, reso=25, drive=drive, key=20)
    p.amp(att=3, dec=1500, sus=90, rel=200)
    p.lfo_(0, div=lfo, mode="FREE", shape=SHAPES["tri"])
    p.mod("LFO 1", "oscA_wtpos", 55, bip=True)
    p.mod("LFO 1", "flt1_cutoff", 15, bip=True)
    p.voice("LEGATO", glide=60, legato=True)
    p.add_fx("DISTORTION", 100, mode="TUBE", drive=drive * 0.8, tone=7000)
    p.add_fx("CHORUS", 30, rate=0.3, depth=50)
    p.add_fx("STEREO WIDTH", 100, width=120, bass_mono=160)
    macros(p, movement=("LFO 1", "flt1_cutoff", 40))
    return p


def arp_patch(name, style, base, pattern="STRAIGHT", rate="1/16", mode="UP", octaves=2, gate=55, swing=0, desc=""):
    p = copy.deepcopy(base)
    p.name, p.category, p.description = name, "ARP", desc
    p.arp(mode, rate, pattern, octaves, gate, swing)
    return p


def texture(name, style, table="DUST", noise=("VINYL", 35), cutoff=2500, desc=""):
    p = P(name, "FX", style, desc)
    p.osc(0, table, pos=40, unison=5, detune=40, blend=80)
    p.noise(noise[0], noise[1] * 0.6, -30)
    p.filt(0, "BP 12", cutoff=cutoff * WARM["FX"], reso=45, key=30)
    p.amp(att=1500, dec=3000, sus=70, rel=3500)
    p.lfo_(0, rate=0.08, mode="FREE", shape=SHAPES["smooth"])
    p.mod("LFO 1", "flt1_cutoff", 40, bip=True)
    p.mod("LFO 1", "oscA_wtpos", 50, bip=True)
    space_fx(p, style, delay_mix=30, verb_mix=50, verb="SPACE", decay=8, div="1/4D")
    macros(p, movement=("LFO 1", "flt1_cutoff", 50))
    return p


# ================================================================== banks

def bank_factory():
    b = []
    b.append(pluck("Dark Pluck", "TRAP", table="BASIC SHAPES", pos=62, cutoff=650, env_amt=40, decay=420, desc="Soft square pluck, dotted delay, glassy tail."))
    b.append(pluck("Perreo Lead", "REGGAETON", table="PWM", pos=40, cutoff=1800, env_amt=30, decay=300, unison=4, detune=22, div="1/8", category="LEAD",
                   desc="Bright PWM pluck-lead for dembow melodies."))
    b.append(keys("Midnight Keys", "R&B", desc="Warm electric piano with gentle tremolo.", trem=4.5))
    b.append(pad("Quasar Dream", "TRAP", desc="Wide analog pad that slowly breathes."))
    b.append(eight08("Galaxy 808", "TRAP", desc="Round 808 with punch, glide and grit on MACRO 2."))
    b.append(reese("Event Horizon", "TRAP", desc="Moving reese bass, mono below 160 Hz."))
    b.append(bell("Glass Moon", "TRAP", desc="Glassy bell with long shimmer."))
    b.append(lead("Neon Saw", "REGGAETON", desc="Supersaw-style lead with glide and delayed vibrato."))
    b.append(bass("Warm Sub Pluck", "R&B", cutoff=380, decay=260, sus=40, drive=20, desc="Plucky low bass, warm and tight."))
    b.append(texture("Cosmic Dust", "LO-FI", desc="Evolving texture with vinyl air."))
    b.append(pluck("Lucid Pluck", "R&B", table="E-PIANO", pos=10, cutoff=1500, env_amt=25, decay=520, unison=2, desc="Rounded keys-pluck."))
    b.append(arp_patch("Orbit Arp", "TRAP", pluck("x", "TRAP", table="BASIC SHAPES", pos=55, cutoff=900, decay=240), pattern="TRAP ROLL", octaves=2,
                       desc="Trap roll arpeggio: hold a chord."))
    b.append(keys("Tape Rhodes", "LO-FI", trem=3.2, tape=True, cutoff=2200, desc="Dusty electric piano on tape."))
    b.append(pad("Choir of Stars", "R&B", table="CHOIR", pos=40, cutoff=2400, attack=600, desc="Airy vocal pad."))
    b.append(lead("Mono Synth", "TRAP", table="ANALOG SAW", unison=1, cutoff=2600, osc2=("BASIC SHAPES", 75, -1, 55), desc="Classic mono lead."))
    b.append(bell("Kalimba Night", "AFRO", table="MALLET", pos=10, decay=900, desc="Wooden kalimba / marimba bell."))
    b.append(eight08("Slide 808", "DRILL", drive=50, punch=18, glide=140, dist="TUBE", desc="Distorted 808 for drill slides."))
    b.append(pad("Analog Brass Pad", "REGGAETON", table="BRASS", pos=60, attack=180, cutoff=2600, unison=4, desc="Warm brassy pad, like video 1."))
    b.append(pluck("Chip Bounce", "REGGAETON", table="CHIP", pos=20, cutoff=3000, env_amt=20, decay=220, unison=1, div="1/16", desc="8-bit pluck for bouncy hooks."))
    b.append(arp_patch("Dembow Arp", "REGGAETON", pluck("x", "REGGAETON", table="PWM", pos=30, cutoff=1400, decay=200), pattern="DEMBOW", rate="1/16", octaves=1,
                       desc="3-3-2 dembow rhythm arpeggio."))
    return b


def bank_trap():
    b = []
    tables = [("BASIC SHAPES", 60), ("HOLLOW", 70), ("PWM", 20), ("GLASS", 30), ("PLUCK", 10), ("SINE STACK", 50)]
    names = ["Sin Luz", "Hielo Negro", "Codeina Pluck", "Vapor", "Ghost Note", "Purple Rain Pluck"]
    for (t, pos), n in zip(tables, names):
        b.append(pluck(n, "TRAP", table=t, pos=pos, cutoff=random.choice([550, 700, 900]), env_amt=random.choice([30, 38, 45]),
                       decay=random.choice([300, 380, 480, 600]), unison=random.choice([1, 3, 4]), div=random.choice(["1/8D", "1/4D", "1/8"])))
    for n, t in [("Bando Bell", "GLASS"), ("Ice Bell", "FM 3:1"), ("Dark Music Box", "MALLET"), ("Moonlight Bell", "METALLIC")]:
        b.append(bell(n, "TRAP", table=t, pos=random.choice([20, 40, 60]), decay=random.choice([900, 1300, 1800])))
    for n, d in [("Trap 808 Clean", 4), ("Trap 808 Grit", 45), ("Long 808", 18), ("Knock 808", 60)]:
        b.append(eight08(n, "TRAP", drive=d, decay=2600 if "Long" in n else 1600, punch=20 if "Knock" in n else 12))
    for n, t in [("Lonely Pad", "WARM SWEEP"), ("Fog Pad", "STRINGS"), ("Dark Choir", "CHOIR")]:
        b.append(pad(n, "TRAP", table=t, pos=40, cutoff=1300))
    for n, t in [("Phantom Lead", "SAW STACK"), ("Flute Trap", "FLUTE"), ("Sad Square", "BASIC SHAPES")]:
        b.append(lead(n, "TRAP", table=t, pos=75 if t == "BASIC SHAPES" else 30, unison=2, cutoff=3000))
    b.append(reese("Night Reese", "TRAP", cutoff=600))
    b.append(texture("Smoke", "TRAP", table="VOWELS", noise=("AIR", 25)))
    b.append(arp_patch("Roll Up", "TRAP", pluck("x", "TRAP", table="HOLLOW", pos=70, cutoff=800, decay=220), pattern="TRAP ROLL", rate="1/16", octaves=2))
    b.append(arp_patch("Triplet Flow", "TRAP", bell("x", "TRAP", table="GLASS", decay=700), pattern="TRIPLET FLOW", rate="1/16T", octaves=2))
    return b


def bank_perreo():
    b = []
    names = ["Bellakeo", "Perreo Intenso", "Calle Luna", "Tusi Pluck", "Mami Pluck", "Fuego Lento", "Playa Noche", "Bzz Pluck"]
    for i, n in enumerate(names):
        t = ["PWM", "CHIP", "SAW STACK", "DIGITAL", "BASIC SHAPES", "NASAL", "E-PIANO", "SYNC SQUARE"][i]
        b.append(pluck(n, "REGGAETON", table=t, pos=[35, 25, 15, 30, 70, 40, 10, 30][i], cutoff=[1800, 2600, 1500, 2200, 1200, 2000, 1600, 2400][i],
                       env_amt=25, decay=[260, 200, 320, 220, 280, 300, 450, 240][i], unison=[4, 1, 5, 1, 3, 2, 2, 3][i], div=["1/8", "1/16", "1/8D", "1/4D"][i % 4]))
    for n, t in [("Reggaeton Lead", "SAW STACK"), ("Square Hook", "BASIC SHAPES"), ("Sirena", "SYNC SAW"), ("Trompeta Synth", "BRASS")]:
        b.append(lead(n, "REGGAETON", table=t, pos=75 if t == "BASIC SHAPES" else 35, unison=3, cutoff=4500, glide=40))
    for n, t in [("Dembow Bass", "ANALOG SAW"), ("Moombah Bass", "SAW STACK"), ("Square Bass", "BASIC SHAPES")]:
        b.append(bass(n, "REGGAETON", table=t, pos=70 if t == "BASIC SHAPES" else 0, cutoff=600, decay=220, sus=50))
    b.append(eight08("Perreo 808", "REGGAETON", drive=30, decay=1200))
    for n, t in [("Guitarra Synth", "GUITAR"), ("Marimba Tropical", "MALLET")]:
        b.append(bell(n, "REGGAETON", table=t, pos=20, decay=700, verb_mix=18))
    b.append(pad("Brass Pad Calle", "REGGAETON", table="BRASS", pos=55, attack=150, cutoff=2400))
    b.append(pad("Noche Pad", "REGGAETON", table="WARM SWEEP", pos=60, attack=500, cutoff=1800))
    b.append(arp_patch("Dembow Machine", "REGGAETON", pluck("x", "REGGAETON", table="CHIP", pos=20, cutoff=2400, decay=180), pattern="DEMBOW"))
    b.append(arp_patch("Bounce Arp", "REGGAETON", pluck("x", "REGGAETON", table="PWM", pos=40, cutoff=1600, decay=200), pattern="BOUNCE", octaves=2))
    b.append(keys("Organo Perreo", "REGGAETON", kind="ORGAN", decay=3000, sus=80, cutoff=5000))
    return b


def bank_rnb():
    b = []
    b.append(keys("Velvet EP", "R&B", trem=4.0))
    b.append(keys("Satin Rhodes", "R&B", trem=5.2, cutoff=2600))
    b.append(keys("Glass Keys", "R&B", kind="GLASS", cutoff=5000, decay=1800))
    b.append(keys("Church Organ R&B", "R&B", kind="ORGAN", decay=4000, sus=85, cutoff=4200))
    b.append(keys("Soft Wurli", "R&B", kind="HOLLOW", cutoff=2200, trem=6))
    for n, t in [("Silk Pad", "STRINGS"), ("Late Night Pad", "WARM SWEEP"), ("Angel Choir", "CHOIR"), ("Haze Pad", "SAW STACK")]:
        b.append(pad(n, "R&B", table=t, pos=35, cutoff=2000, attack=700))
    for n, t in [("Smooth Lead", "TAPE SINE"), ("Whistle", "FLUTE"), ("Sine Lead R&B", "BASIC SHAPES")]:
        b.append(lead(n, "R&B", table=t, pos=5 if t == "BASIC SHAPES" else 40, unison=1, cutoff=5000, drive=10, glide=90))
    for n, t in [("Rubber Bass", "BASIC SHAPES"), ("Finger Bass Synth", "ANALOG SAW")]:
        b.append(bass(n, "R&B", table=t, pos=30 if t == "BASIC SHAPES" else 20, cutoff=420, decay=300, sus=55, drive=18))
    b.append(pluck("R&B Pluck", "R&B", table="E-PIANO", pos=0, cutoff=1300, decay=500, unison=2))
    b.append(pluck("Harp Dream", "R&B", table="GUITAR", pos=40, cutoff=2600, decay=700, unison=2))
    b.append(bell("Music Box", "R&B", table="MALLET", pos=0, decay=1400))
    b.append(eight08("Silk 808", "R&B", drive=10, decay=2200, punch=8))
    return b


def bank_lofi():
    b = []
    for n, t, trem in [("Dusty Keys", "E-PIANO", 3.5), ("Cassette Piano", "TAPE SINE", 0), ("Rainy Rhodes", "E-PIANO", 4.8)]:
        b.append(keys(n, "LO-FI", kind=t, tape=True, cutoff=1800, trem=trem))
    for n, t in [("Crumbled Pluck", "BASIC SHAPES"), ("Warm Lo-Fi Pluck", "TAPE SINE"), ("Old Guitar", "GUITAR"), ("Bit Pluck", "DIGITAL")]:
        p = pluck(n, "LO-FI", table=t, pos=40, cutoff=900, decay=450, unison=2, noise=("VINYL", 22), sat=30)
        p.add_fx("BITCRUSH", 25, rate=14000, bits=10)
        b.append(p)
    for n, t in [("VHS Pad", "WARM SWEEP"), ("Bedroom Pad", "STRINGS"), ("Tape Choir", "CHOIR")]:
        b.append(pad(n, "LO-FI", table=t, pos=40, cutoff=1400, noise=("TAPE HISS", 18)))
    b.append(lead("Lo-Fi Flute", "LO-FI", table="FLUTE", pos=40, unison=1, cutoff=3000, drive=15))
    b.append(bass("Lo-Fi Bass", "LO-FI", table="TAPE SINE", pos=60, cutoff=500, decay=350, sus=60, drive=25))
    b.append(bell("Lo-Fi Bell", "LO-FI", table="MALLET", pos=30, decay=900))
    b.append(texture("Vinyl Room", "LO-FI", table="DUST", noise=("VINYL", 40)))
    b.append(arp_patch("Lazy Arp", "LO-FI", keys("x", "LO-FI", tape=True, cutoff=1600), pattern="LO-FI SWING", rate="1/8", octaves=1, swing=35))
    return b


def bank_808():
    b = []
    for n, d, dec, pu, dist in [("Bombastic 808", 40, 1800, 14, None), ("Sine 808", 5, 2000, 10, None), ("Distorted 808", 70, 1600, 16, "FUZZ"),
                                ("Long Slide 808", 25, 3200, 12, None), ("Punchy 808", 35, 1100, 24, None), ("Drill 808", 55, 1400, 20, "TUBE"),
                                ("Clean Sub", 0, 4000, 0, None), ("Folded 808", 50, 1500, 14, "SINE FOLD")]:
        b.append(eight08(n, "TRAP" if "Drill" not in n else "DRILL", drive=d, decay=dec, punch=pu, dist=dist))
    for n, c in [("Reese Abyss", 500), ("Reese Champagne", 900), ("Reese Red", 700)]:
        b.append(reese(n, "TRAP", cutoff=c))
    for n, t in [("Growl Bass", "GROWL"), ("Saturated Bass", "DIRTY SAW"), ("Sub Pluck", "BASIC SHAPES"), ("Paranormal Bass", "VOWELS")]:
        b.append(bass(n, "TRAP", table=t, pos=30, cutoff=700, decay=300, sus=50, drive=40))
    p = bass("Sub Easy", "TRAP", table="BASIC SHAPES", pos=0, cutoff=2000, decay=500, sus=90, drive=10, sub=0, category="SUB")
    b.append(p)
    return b


def bank_arps():
    b = []
    base_pl = lambda t, pos, c: pluck("x", "TRAP", table=t, pos=pos, cutoff=c, decay=200, unison=2, delay_mix=18)
    specs = [("Phrase Machine", "STRAIGHT", "UP", "1/16", 2, base_pl("PWM", 30, 1400)),
             ("Roll Arp", "TRAP ROLL", "UP", "1/16", 2, base_pl("BASIC SHAPES", 60, 900)),
             ("Skip Arp", "SKIP", "UP/DOWN", "1/16", 2, base_pl("HOLLOW", 70, 1100)),
             ("Stairs", "STAIRS", "UP", "1/16", 2, base_pl("GLASS", 30, 2600)),
             ("Ratchet Bell", "RATCHET", "RANDOM", "1/8", 2, bell("x", "TRAP", decay=600)),
             ("Oct Jumper", "OCT JUMP", "ORDER", "1/16", 1, base_pl("CHIP", 20, 2400)),
             ("Drill Pattern", "DRILL", "UP", "1/16", 2, base_pl("METALLIC", 40, 1800)),
             ("Plugg Arp", "PLUGG", "UP/DOWN", "1/16", 2, base_pl("SINE STACK", 40, 1600)),
             ("Bounce House", "BOUNCE", "UP", "1/16", 2, base_pl("NASAL", 40, 1700)),
             ("Chord Stabs", "DEMBOW", "CHORD", "1/16", 1, keys("x", "REGGAETON", kind="E-PIANO")),
             ("Triplet Bells", "TRIPLET FLOW", "UP", "1/16T", 2, bell("x", "TRAP", table="MALLET", decay=500)),
             ("Lo-Fi Arp", "LO-FI SWING", "UP", "1/8", 1, keys("x", "LO-FI", tape=True)),
             ("Tusi Arp", "DEMBOW", "UP", "1/16", 2, base_pl("DIGITAL", 30, 2000)),
             ("Arp Down", "STRAIGHT", "DOWN", "1/8T", 2, base_pl("FM 2:1", 30, 2200))]
    for n, pat, mode, rate, octs, base in specs:
        style = "LO-FI" if "Lo-Fi" in n else "REGGAETON" if pat in ("DEMBOW", "BOUNCE") else "DRILL" if pat == "DRILL" else "TRAP"
        b.append(arp_patch(n, style, base, pattern=pat, rate=rate, mode=mode, octaves=octs, desc="Hold a chord: the arp plays the " + pat.lower() + " pattern."))
    return b


def bank_space():
    b = []
    for n, t, pos in [("Nebula Wash", "SAW STACK", 40), ("Saturn Rings", "HARMONIC SWEEP", 30), ("Solar Wind", "COMB SWEEP", 50),
                      ("Deep Field", "WARM SWEEP", 70), ("Aurora", "VOWELS", 50), ("Infinite", "STRINGS", 40), ("Polaris", "GLASS", 40)]:
        b.append(pad(n, "TRAP", table=t, pos=pos, attack=1400, release=3500, verb_mix=45, verb="SPACE", osc2=("SINE STACK", 40, -1, 50, 2)))
    for n, t in [("Black Hole", "DUST"), ("Radio Signal", "DIGITAL"), ("Comet Tail", "METALLIC"), ("Stardust Rise", "HARMONIC SWEEP")]:
        b.append(texture(n, "TRAP", table=t, noise=("AIR", 30)))
    return b


# ================================================================== presets from the analysed references

def from_profile(prof, idx, rnd):
    """A preset that follows a measured reference: envelope, brightness, harmonic character, width, tail."""
    cat = prof["category"]
    att = max(0.5, min(1500.0, prof["attack_s"] * 1000 * (0.6 if prof["attack_s"] < 0.05 else 1.0)))
    dec20 = max(0.08, prof["decay20_s"])
    sus_db = prof["sustain_db"]
    sus = 0 if sus_db < -40 else max(0, min(100, 10 ** (sus_db / 20) * 100))
    f0 = prof["f0"] or 262
    bright = prof["brightness_hz"][0] if prof["brightness_hz"] else 1000
    late = prof["brightness_hz"][2] if len(prof["brightness_hz"]) > 2 else bright
    h = prof["harmonics"] + [0] * 16
    oddish = prof["odd_even"] > 6
    # oscillator: sine-like, triangle, square or saw from the harmonic profile
    if h[1] < 0.12 and h[2] < 0.12:
        table, pos = ("TAPE SINE", 20) if cat in ("KEYS", "BELL") else ("BASIC SHAPES", 8)
    elif oddish and h[2] < 0.25:
        table, pos = "BASIC SHAPES", 27      # triangle
    elif oddish:
        table, pos = "BASIC SHAPES", 74      # square
    else:
        table, pos = ("ANALOG SAW", 15) if h[3] > 0.2 else ("WARM SWEEP", 60)
    cutoff = max(250, min(12000, bright * 2.6))
    env_amt = max(0, min(50, (bright - late) / max(1, bright) * 120)) if bright > late else 8
    width = prof["width"]
    unison = 1 if width < 0.25 else 3 if width < 0.6 else 5
    detune = 10 + min(30, prof["beating"] * 25)
    tail = max(0.0, prof["tail60_s"] - dec20)
    verb_mix = int(min(45, 10 + tail * 10))
    delay_mix = int(min(30, 6 + prof["beating"] * 18)) if cat in ("PLUCK", "BELL", "KEYS", "LEAD") else 0
    style = ["TRAP", "REGGAETON", "R&B", "LO-FI"][idx % 4]
    name = NAME_POOL[cat].pop(rnd.randrange(len(NAME_POOL[cat]))) if NAME_POOL.get(cat) else "%s %d" % (cat.title(), idx)
    if cat in ("PLUCK", "BELL", "KEYS", "ARP"):
        p = pluck(name, style, table=table, pos=pos, cutoff=cutoff, env_amt=env_amt, decay=dec20 * 1000 * 0.6, unison=unison, detune=detune,
                  verb_mix=verb_mix, delay_mix=delay_mix, category=cat if cat != "ARP" else "PLUCK", noise=("VINYL", 15) if prof["noise"] > 0.35 else None)
        p.amp(att=max(2.0, att), ac=0, dec=dec20 * 1000 * 1.6, sus=sus, rel=max(120, min(1500, tail * 500)), dc=70, rc=60)
    elif cat == "PAD":
        p = pad(name, style, table=table if table != "BASIC SHAPES" else "SAW STACK", pos=pos, attack=max(80, att), cutoff=cutoff, unison=max(4, unison + 2),
                detune=detune + 8, verb_mix=verb_mix)
        p.amp(att=max(80, att), dec=2000, sus=max(50, sus), rel=max(800, min(4000, tail * 900)))
    elif cat == "LEAD":
        p = lead(name, style, table=table if table != "TAPE SINE" else "FLUTE", pos=pos, cutoff=cutoff, unison=unison, detune=detune, delay_mix=delay_mix,
                 verb_mix=verb_mix, mono=width < 0.5)
        p.amp(att=att, dec=dec20 * 1600, sus=max(30, sus), rel=max(150, min(1200, tail * 400)))
    elif cat in ("BASS", "SUB", "REESE"):
        p = bass(name, style, table=table, pos=pos, cutoff=max(200, min(2000, cutoff)), decay=dec20 * 1000 * 0.7, sus=max(20, sus), category=cat)
    elif cat == "808":
        p = eight08(name, style, decay=dec20 * 1200)
    else:
        p = texture(name, style)
    p.description = "Inspired by a reference sound: same envelope, brightness and width."
    return p


NAME_POOL = {
    "PLUCK": ["Cristal", "Amanecer", "Gota", "Neblina", "Lucero", "Marea", "Brisa", "Selene", "Orion", "Vega", "Sirio", "Andromeda", "Lyra",
              "Nova Pluck", "Cometa", "Eclipse", "Kepler", "Pulso", "Zafiro", "Ambar", "Rocio", "Ceniza", "Plata", "Jade", "Opalo", "Coral",
              "Niebla Pluck", "Aura", "Halo Pluck", "Destello", "Chispa", "Luz Tenue", "Mirage", "Velvet Pluck", "Sueño", "Ritmo Lunar"],
    "KEYS": ["Terciopelo", "Medianoche", "Ensueño", "Nostalgia", "Luna Llena", "Mar de Noche", "Seda", "Bruma", "Calma", "Aurora Keys",
             "Ocaso", "Café", "Lluvia", "Miel", "Sombra", "Brillo", "Cielo", "Eterno", "Ritual", "Penumbra", "Velas", "Horizonte"],
    "BELL": ["Campana Lunar", "Cristalina", "Polvo de Hada", "Estrella Fugaz", "Hielo", "Carrusel", "Glaciar", "Diamante", "Faro", "Prisma",
             "Copo", "Espejo", "Reloj", "Luciernaga", "Galaxia", "Satelite", "Meteoro", "Quimera", "Nebulosa", "Vidrio"],
    "LEAD": ["Sirena Lead", "Neon", "Laser", "Fantasma", "Ultravioleta", "Corona", "Relampago", "Mercurio", "Cobalto", "Titan", "Flama",
             "Rayo", "Plasma", "Voltaje", "Fenix", "Delirio", "Hechizo", "Tormenta", "Vibra", "Ciclon", "Obsidiana", "Cosmos Lead",
             "Iris", "Zenit", "Nadir", "Equinoccio", "Solsticio", "Perigeo", "Apogeo", "Pulsar Lead", "Orbita", "Alba", "Cenit",
             "Magma", "Lava", "Volcan", "Brasa", "Fuego Azul", "Escarcha", "Granate", "Rubi", "Topacio", "Onix", "Perla", "Cuarzo",
             "Nácar", "Esmeralda", "Malaquita", "Lapislazuli", "Turquesa", "Amatista", "Citrino", "Agata", "Berilo"],
    "PAD": ["Nube", "Oceano", "Desierto", "Estratosfera", "Ionosfera", "Silencio", "Infinito", "Abismo", "Cielo Rojo", "Madrugada",
            "Atardecer", "Amnesia", "Ingravido", "Etereo", "Flotar", "Vacio", "Origen", "Eden", "Bosque", "Montaña", "Valle", "Rio",
            "Glaciar Pad", "Tundra", "Sabana", "Selva", "Pantano", "Laguna", "Cascada", "Arrecife", "Volcan Pad", "Isla", "Peninsula",
            "Archipielago", "Fiordo", "Meseta", "Canon", "Duna", "Oasis", "Espejismo", "Aurora Boreal"],
    "BASS": ["Profundo", "Terremoto", "Gravedad", "Magnetico", "Ancla", "Raiz", "Sotano", "Tunel", "Caverna", "Mina", "Ballena", "Trueno",
             "Rugido", "Pantera", "Lobo", "Jaguar", "Toro", "Bufalo", "Mamut", "Gorila", "Oso", "Tiburon", "Orca"],
    "SUB": ["Sub Cero", "Sub Marino", "Sub Suelo"],
    "REESE": ["Reese Oscuro", "Reese Toxico", "Reese Nocturno", "Reese Metal", "Reese Fango"],
    "808": ["808 Bombo", "808 Asfalto", "808 Volcan"],
}


def bank_inspired(profiles):
    rnd = random.Random(7)
    picked = []
    by = {}
    for p in profiles:
        by.setdefault(p["category"], []).append(p)
    quota = {"PLUCK": 14, "KEYS": 8, "BELL": 8, "LEAD": 10, "PAD": 8, "BASS": 6, "REESE": 2, "SUB": 1, "808": 2}
    for cat, n in quota.items():
        items = by.get(cat, [])
        # spread over the range of brightness so the bank is varied
        items = sorted(items, key=lambda r: (r["brightness_hz"][0] if r["brightness_hz"] else 0))
        if not items:
            continue
        step = max(1, len(items) // n)
        picked += items[::step][:n]
    return [from_profile(p, i, rnd) for i, p in enumerate(picked)]



# ================================================================== presets fitted to the reference sounds (quasar_match)

def lgv(t, lo, hi):
    return lo * (hi / lo) ** max(0.0, min(1.0, t))


def from_match(m, name, style, extra_fx=None):
    """matched.json entry -> preset. Mirrors Rig::apply in tests/QuasarMatch.cpp."""
    c, cat = m["params"], m["category"]
    bass = cat in ("BASS", "808", "SUB", "REESE")
    category = {"ARP": "PLUCK", "OTHER": "FX"}.get(cat, cat)
    p = P(name, category, style, "Fitted by QUASAR to a reference %s sound (envelope, spectrum over time, width)." % cat.lower())
    p.v.update({"oscA_on": 1, "oscA_type": int(c["typeA"]), "oscA_table": int(c["tableA"]), "oscA_wtpos": round(c["posA"] * 100, 1),
                "oscA_unison": int(c["uniA"]), "oscA_detune": round(c["detA"] * 60, 1), "oscA_spread": round(c["spread"] * 100, 1),
                "oscA_blend": round(c["blend"] * 100, 1), "oscA_warpmode": int(c["warpA"]), "oscA_warp": round(c["warpAmt"] * 80, 1)})
    if c["onB"] > 0.5:
        p.v.update({"oscB_on": 1, "oscB_table": int(c["tableB"]), "oscB_wtpos": round(c["posB"] * 100, 1), "oscB_oct": int(c["octB"]),
                    "oscB_semi": int(c["semiB"]), "oscB_level": round(30 + c["levelB"] * 50, 1)})
    if c["sub"] > 0.5: p.v.update({"sub_on": 1, "sub_level": round(30 + c["subLevel"] * 50, 1), "sub_shape": int(c["subShape"])})
    if c["noise"] > 0.5: p.v.update({"noise_on": 1, "noise_type": int(c["noiseType"]), "noise_level": round(10 + c["noiseLevel"] * 45, 1)})
    if c["filt"] > 0.5:
        p.v.update({"flt1_on": 1, "flt1_type": int(c["ftype"]), "flt1_cutoff": round(lgv(c["cutoff"], 80, 16000)), "flt1_reso": round(c["reso"] * 70, 1),
                    "flt1_drive": round(c["drive"] * 60, 1), "flt1_key": round(c["key"] * 60, 1)})
    p.v.update({"env1_att": round(lgv(c["att"], 0.5, 1500), 2), "env1_dec": round(lgv(c["dec"], 30, 6000)), "env1_sus": round(c["sus"] * 100, 1),
                "env1_rel": round(lgv(c["rel"], 20, 4000)), "env1_acurve": 0,
                "env2_att": 0.5, "env2_dec": round(lgv(c["dec2"], 20, 3000)), "env2_sus": 0,
                "lfo1_sync": 0, "lfo1_mode": 0, "lfo1_rate": round(lgv(c["lfoRate"], 0.1, 8), 3)})
    if bass:
        p.v.update({"envP_amt": round(c["pitch"] * 24, 2), "envP_att": 0, "envP_dec": 80, "envP_sus": 0, "voice_mode": 2, "glide": 70, "glide_legato": 1})
    if c["envCut"] > 0.02: p.mod("ENV 2", "flt1_cutoff", round(c["envCut"] * 60, 1))
    if abs(c["envPos"] - 0.5) > 0.02: p.mod("ENV 2", "oscA_wtpos", round((c["envPos"] - 0.5) * 120, 1))
    if c["lfoPos"] > 0.02: p.mod("LFO 1", "oscA_wtpos", round(c["lfoPos"] * 40, 1), bip=True)
    p.mod("VELOCITY", "flt1_cutoff", 10)
    if c["sat"] > 0.05: p.add_fx("SATURATION", 100, drive=round(c["sat"] * 60, 1))
    if extra_fx: extra_fx(p)
    if c["chorus"] > 0.03: p.add_fx("CHORUS", round(c["chorus"] * 70, 1))
    if c["delay"] > 0.03: p.add_fx("DELAY", round(c["delay"] * 40, 1))
    if c["verb"] > 0.03: p.add_fx("REVERB", round(c["verb"] * 50, 1), decay=round(lgv(c["verbDecay"], 0.5, 6.0), 2))
    if bass: p.add_fx("STEREO WIDTH", 100, width=100, bass_mono=150)
    # macros that make sense for this sound
    bright = ("flt1_cutoff", 40) if c["filt"] > 0.5 else ("oscA_wtpos", 35)
    if not p.lfo[3] and c["lfoPos"] <= 0.02: pass
    macros(p, bright=bright, movement=("LFO 1", "oscA_wtpos", 35) if c["lfoPos"] > 0.02 else None)
    return p


def lofi_fx(p):
    p.add_fx("TAPE", 100, wow=35, flutter=25, saturation=35, hiss=15, age=50)
    p.add_fx("BITCRUSH", 22, rate=16000, bits=11)


def build_matched_banks(matched, levels):
    rnd = random.Random(11)
    pools = {k: list(v) for k, v in NAME_POOL.items()}
    extra = ["Kairos", "Lumen", "Ether", "Onda", "Fractal", "Prana", "Nexo", "Umbral", "Vortice", "Cenizas", "Atlas", "Arcano", "Brujula",
             "Cumbre", "Delta", "Fulgor", "Gema", "Hiedra", "Indigo", "Jazmin", "Karma", "Lirio", "Mística", "Nectar", "Ostra", "Pétalo",
             "Quasar Dust", "Raices", "Sauce", "Tinta", "Ultra", "Vidrio Azul", "Xenon", "Yuca", "Zenda", "Alquimia", "Bahia", "Caramelo",
             "Dorado", "Encanto", "Farol", "Girasol", "Horizonte Rojo", "Isla Negra", "Jungla", "Kiwi", "Lima", "Mango", "Naranja", "Azahar", "Bruma Rosa", "Canela", "Duna Azul", "Estela", "Fuga", "Gaviota", "Humo", "Iman", "Jade Negro", "Koi", "Lava Fria", "Mareo", "Nimbo", "Obsequio", "Papiro", "Quietud", "Remolino", "Salitre", "Tregua", "Ulises", "Vertigo", "Wabi", "Xilofono", "Yugo", "Zarza", "Alba Roja", "Brillo Lunar", "Cascabel", "Desvelo", "Embrujo", "Fogata", "Granizo", "Hechizo Azul", "Ilusion", "Jengibre", "Lagrima", "Mosaico", "Neblina Roja", "Orquidea", "Pirata", "Ruiseñor", "Serenata", "Tormenta Rosa", "Umbria", "Vals", "Zafra"]
    used = set()
    def name_for(cat):
        pool = pools.get({"ARP": "PLUCK", "OTHER": "PAD", "FX": "PAD", "VOX": "PAD"}.get(cat, cat)) or []
        while pool:
            n = pool.pop(rnd.randrange(len(pool)))
            if n not in used: used.add(n); return n
        while True:
            n = extra.pop(rnd.randrange(len(extra))) if extra else "Quasar %d" % len(used)
            if n not in used: used.add(n); return n

    style_of = {"PERREO": "REGGAETON", "TRAP": "TRAP", "R&B": "R&B"}
    banks = {k: [] for k in ["QUASAR FACTORY", "TRAP ESSENTIALS", "PERREO", "R&B NIGHTS", "LO-FI DUST", "808 GALAXY", "ARP MACHINE", "SPACE PADS"]}
    # best fits of each category open the factory bank
    best = {}
    for m in matched:
        best.setdefault(m["category"], []).append(m)
    factory_ids = set()
    for cat, ms in best.items():
        for m in sorted(ms, key=lambda x: x["distance"])[:3 if cat in ("PLUCK", "LEAD", "KEYS", "PAD", "BELL", "BASS") else 1]:
            factory_ids.add(id(m))
    arps, lofi = [], []
    for m in matched:
        cat, genre = m["category"], m.get("genre", "TRAP")
        style = style_of.get(genre, "TRAP")
        p = from_match(m, name_for(cat), style)
        if id(m) in factory_ids: banks["QUASAR FACTORY"].append(p)
        elif cat in ("BASS", "808", "SUB", "REESE"): banks["808 GALAXY"].append(p)
        elif cat in ("PAD", "FX", "VOX", "OTHER"): banks["SPACE PADS"].append(p)
        elif genre == "PERREO": banks["PERREO"].append(p)
        elif genre == "R&B": banks["R&B NIGHTS"].append(p)
        else: banks["TRAP ESSENTIALS"].append(p)
        if cat in ("PLUCK", "BELL", "ARP") and m["distance"] < 6: arps.append(m)
        if cat in ("KEYS", "PLUCK", "PAD") and genre in ("R&B", "TRAP"): lofi.append(m)
    # arps: different fitted sounds, each with its own pattern
    pats = [("TRAP ROLL", "UP", "1/16", 2), ("DEMBOW", "UP", "1/16", 1), ("TRIPLET FLOW", "UP", "1/16T", 2), ("BOUNCE", "UP/DOWN", "1/16", 2),
            ("DRILL", "UP", "1/16", 2), ("PLUGG", "UP/DOWN", "1/16", 2), ("SKIP", "DOWN", "1/16", 2), ("STAIRS", "UP", "1/16", 2),
            ("RATCHET", "RANDOM", "1/8", 2), ("OCT JUMP", "ORDER", "1/16", 1), ("LO-FI SWING", "UP", "1/8", 1), ("STRAIGHT", "DOWN/UP", "1/8T", 2)]
    rnd.shuffle(arps)
    for i, m in enumerate(arps[:24]):
        pat, mode, rate, octs = pats[i % len(pats)]
        p = from_match(m, name_for("ARP") + " Arp", "REGGAETON" if pat in ("DEMBOW", "BOUNCE") else "TRAP")
        p.category = "ARP"
        p.arp(mode, rate, pat, octs, 55, 30 if pat == "LO-FI SWING" else 0)
        banks["ARP MACHINE"].append(p)
    rnd.shuffle(lofi)
    for m in lofi[:18]:
        p = from_match(m, name_for(m["category"]) + " Lo-Fi", "LO-FI", extra_fx=lofi_fx)
        p.v["noise_on"] = 1; p.v["noise_type"] = 3; p.v["noise_level"] = 18
        banks["LO-FI DUST"].append(p)
    return banks


BANK_INFO = [
    ("00_factory", "QUASAR FACTORY", "ff7b2cff", "The best fits of every category, plus signature QUASAR sounds."),
    ("01_trap", "TRAP ESSENTIALS", "ff5b21b6", "Trap plucks, bells, leads and keys fitted to modern trap one-shots."),
    ("02_perreo", "PERREO", "ffe0458b", "Reggaeton plucks, leads, keys and bells fitted to modern reggaeton one-shots."),
    ("03_rnb", "R&B NIGHTS", "ff3b6bd6", "Warm keys, plucks and leads for R&B."),
    ("04_lofi", "LO-FI DUST", "ffc08a4a", "Fitted keys and plucks pushed through tape, vinyl and crushed bits."),
    ("05_808", "808 GALAXY", "ffff6a3d", "Basses, 808s, reese and subs."),
    ("06_arps", "ARP MACHINE", "ff2ec4b6", "Each arp is a different fitted sound with its own pattern: hold a chord."),
    ("07_space", "SPACE PADS", "ff6a5cff", "Pads, textures and FX."),
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--matched", default=os.path.join(HERE, "matched.json"))
    ap.add_argument("--levels", default=os.path.join(HERE, "levels.json"))
    ap.add_argument("--drop", default=os.path.join(HERE, "drop.json"))
    args = ap.parse_args()
    random.seed(42)
    levels = json.load(open(args.levels)) if os.path.exists(args.levels) else {}
    drop = set(json.load(open(args.drop))) if os.path.exists(args.drop) else set()
    # fits that stay far from their reference (> 9 dB average error) are left out
    matched = [m for m in json.load(open(args.matched)) if m["distance"] <= 9.0]
    banks = build_matched_banks(matched, levels)
    # hand-designed signature sounds that the references do not cover (808 glides, reese motion, arps of the factory)
    sig = bank_factory()
    keep = {"Galaxy 808", "Slide 808", "Event Horizon", "Orbit Arp", "Dembow Arp", "Cosmic Dust", "Quasar Dream", "Midnight Keys", "Analog Brass Pad"}
    banks["QUASAR FACTORY"] += [p for p in sig if p.name in keep]
    banks["808 GALAXY"] += [p for p in bank_808() if p.name in ("Distorted 808", "Long Slide 808", "Folded 808", "Reese Champagne", "Growl Bass")]
    banks["SPACE PADS"] += [p for p in bank_space() if p.name in ("Black Hole", "Radio Signal", "Saturn Rings")]
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(OUT):
        if f.endswith(".json"): os.remove(os.path.join(OUT, f))
    total = 0
    for fname, name, colour, desc in BANK_INFO:
        presets = []
        for p in banks[name]:
            if p.name in drop: continue
            j = p.json()
            j["values"]["master"] = round(levels.get(p.name, 0.0), 2)
            presets.append(j)
        json.dump({"name": name, "author": "LT SOUNDS", "colour": colour, "description": desc, "presets": presets},
                  open(os.path.join(OUT, fname + ".json"), "w"), indent=1, ensure_ascii=False)
        total += len(presets)
        print("%-16s %3d presets" % (name, len(presets)))
    print("total", total)


if __name__ == "__main__":
    main()
