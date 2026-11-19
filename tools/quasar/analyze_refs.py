#!/usr/bin/env python3
"""
Analyses reference one-shots (WAV) so QUASAR presets can be designed from measured features.
Usage: python3 analyze_refs.py out.json folder [folder ...]

For each file: category (from the file name), f0, envelope (attack, decay, sustain, tail),
harmonic profile, brightness over time, noise amount, stereo width, beating (unison / detune),
pitch drop (808) and vibrato. Needs numpy, scipy, soundfile.
"""
import json, os, re, sys
import numpy as np
import soundfile as sf
from scipy.signal import butter, sosfilt

CATS = [("808", "808"), ("reesebass", "REESE"), ("reese", "REESE"), ("subbass", "SUB"), ("bass", "BASS"), ("pluck", "PLUCK"),
        ("plk", "PLUCK"), ("key", "KEYS"), ("piano", "KEYS"), ("rhode", "KEYS"), ("lead", "LEAD"), ("ld ", "LEAD"),
        ("pad", "PAD"), ("pd ", "PAD"), ("bell", "BELL"), ("bll", "BELL"), ("mallet", "BELL"), ("kalimba", "BELL"),
        ("arp", "ARP"), ("synth", "LEAD"), ("str", "PAD"), ("string", "PAD"), ("violin", "LEAD"), ("guitar", "PLUCK"),
        ("gtr", "PLUCK"), ("brass", "LEAD"), ("trompet", "LEAD"), ("fx", "FX"), ("kz", "FX"), ("vox", "VOX"), ("choir", "VOX")]


def category(name):
    n = name.lower().replace("_", " ").replace("-", " ") + " "
    for key, cat in CATS:
        if key in n:
            return cat
    return "OTHER"


def envelope(x, sr, win=0.005):
    n = max(1, int(win * sr))
    e = np.sqrt(np.convolve(x * x, np.ones(n) / n, mode="same") + 1e-12)
    return e


def f0_yin(x, sr, fmin=30, fmax=1200):
    n = min(len(x), int(0.25 * sr))
    if n < 2048:
        return 0.0
    x = x[:n] - np.mean(x[:n])
    tmin, tmax = int(sr / fmax), int(sr / fmin)
    w = n - tmax
    if w <= 0:
        return 0.0
    d = np.array([np.sum((x[:w] - x[t:t + w]) ** 2) for t in range(tmin, tmax)])
    cum = np.cumsum(d) / (np.arange(len(d)) + 1)
    dp = d / (cum + 1e-12)
    idx = np.where(dp < 0.15)[0]
    i = idx[0] if len(idx) else int(np.argmin(dp))
    while i + 1 < len(dp) and dp[i + 1] < dp[i]:
        i += 1
    if 0 < i < len(d) - 1:   # parabolic interpolation
        a, b, c = d[i - 1], d[i], d[i + 1]
        i = i + 0.5 * (a - c) / (a - 2 * b + c + 1e-12)
    return sr / (i + tmin)


def harmonics(x, sr, f0, count=24):
    n = 1 << int(np.ceil(np.log2(max(4096, len(x)))))
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x)), n))
    freqs = np.fft.rfftfreq(n, 1 / sr)
    amps = []
    for h in range(1, count + 1):
        f = f0 * h
        if f > sr * 0.45:
            amps.append(0.0)
            continue
        band = (freqs > f * 0.97) & (freqs < f * 1.03)
        amps.append(float(spec[band].max()) if band.any() else 0.0)
    amps = np.array(amps)
    total = float(np.sum(spec ** 2))
    harm_energy = 0.0
    for h in range(1, 60):
        f = f0 * h
        if f > sr * 0.45:
            break
        band = (freqs > f * 0.985) & (freqs < f * 1.015)
        harm_energy += float(np.sum(spec[band] ** 2))
    noise = 1.0 - min(1.0, harm_energy / (total + 1e-12))
    return amps / (amps.max() + 1e-12), noise


def centroid(x, sr):
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x))))
    freqs = np.fft.rfftfreq(len(x), 1 / sr)
    return float(np.sum(freqs * spec) / (np.sum(spec) + 1e-12))


def analyse(path):
    data, sr = sf.read(path, always_2d=True)
    data = data.astype(np.float64)
    L = data[:, 0]
    R = data[:, 1] if data.shape[1] > 1 else data[:, 0]
    mono = 0.5 * (L + R)
    peak = float(np.max(np.abs(mono)) + 1e-12)
    env = envelope(mono, sr)
    ep = env.max()
    ipk = int(np.argmax(env))
    above = np.where(env > ep * 0.1)[0]
    start = above[0] if len(above) else 0
    a90 = np.where(env[start:] > ep * 0.9)[0]
    attack = float((a90[0] if len(a90) else 0) / sr)
    after = env[ipk:]
    d20 = np.where(after < ep * 0.1)[0]
    decay20 = float(d20[0] / sr) if len(d20) else float(len(after) / sr)
    d60 = np.where(after < ep * 0.001)[0]
    tail = float(d60[0] / sr) if len(d60) else float(len(after) / sr)
    dur = len(mono) / sr
    mid = env[int(len(env) * 0.4): int(len(env) * 0.7)]
    sustain_db = float(20 * np.log10((np.median(mid) if len(mid) else 1e-9) / ep + 1e-9))
    seg = mono[start + int(0.03 * sr): start + int(0.33 * sr)]
    f0 = f0_yin(seg, sr) if len(seg) > 2048 else 0.0
    f0_late = f0_yin(mono[start + int(0.4 * sr): start + int(0.65 * sr)], sr) if dur > 0.8 else f0
    drop = float(1200 * np.log2(f0 / f0_late)) if f0 > 0 and f0_late > 0 else 0.0
    amps, noise = harmonics(seg, sr, f0) if f0 > 0 else (np.zeros(24), 1.0)
    odd = float(np.sum(amps[0::2] ** 2)); even = float(np.sum(amps[1::2] ** 2))
    bright = []
    for t in (0.02, 0.15, 0.5, 1.2):
        a = start + int(t * sr)
        if a + 4096 < len(mono):
            bright.append(centroid(mono[a:a + 4096], sr))
    side = 0.5 * (L - R)
    width = float(np.sqrt(np.mean(side ** 2)) / (np.sqrt(np.mean(mono ** 2)) + 1e-12))
    # beating: modulation of the envelope between 0.5 and 12 Hz in the sustained part
    body = env[start + int(0.1 * sr): start + int(min(dur, 1.5) * sr)]
    beat = 0.0
    if len(body) > sr * 0.4:
        b = body / (np.mean(body) + 1e-12)
        sos = butter(2, [0.5, 12], btype="band", fs=sr, output="sos")
        beat = float(np.std(sosfilt(sos, b - 1.0)))
    return {
        "file": os.path.basename(path), "category": category(os.path.basename(path)), "sr": sr, "duration": round(dur, 3),
        "peak_db": round(20 * np.log10(peak), 2), "attack_s": round(attack, 4), "decay20_s": round(decay20, 3),
        "tail60_s": round(tail, 3), "sustain_db": round(sustain_db, 1), "f0": round(f0, 2), "pitch_drop_cents": round(drop, 1),
        "harmonics": [round(float(a), 3) for a in amps[:16]], "odd_even": round(odd / (even + 1e-9), 2), "noise": round(noise, 3),
        "brightness_hz": [round(b) for b in bright], "width": round(width, 3), "beating": round(beat, 4),
    }


def main():
    out = sys.argv[1]
    files = []
    for folder in sys.argv[2:]:
        for root, _, names in os.walk(folder):
            for n in names:
                if n.lower().endswith((".wav", ".aif", ".aiff")) and not n.startswith("._"):
                    files.append(os.path.join(root, n))
    results = []
    for i, f in enumerate(sorted(files)):
        try:
            results.append(analyse(f))
        except Exception as e:   # unreadable / silent files are skipped
            print("skip", f, e, file=sys.stderr)
        if i % 25 == 0:
            print(f"{i}/{len(files)}", file=sys.stderr)
    json.dump(results, open(out, "w"), indent=1)
    print(f"analysed {len(results)} files -> {out}")


if __name__ == "__main__":
    main()
