# LT COSMOS

A meter and visualizer: VST3 / AU plugin plus a standalone app. It never changes the audio. In the app the output is silent, so the input can't feed back.

## Formats
- `LT COSMOS.vst3`, `LT COSMOS.component` (AU effect), `LT COSMOS.app` (standalone).
- Plugin code `Ltcs`, manufacturer `Ltau`, license product id `COSMOS`. It uses the same trial and activation as the other LT products.

## Loudness (audio thread, `Source/Loudness.*`)
- ITU-R BS.1770-4 / EBU R128. K-weighting is computed for any sample rate.
- Momentary (400 ms), short-term (3 s) and integrated loudness (gates at −70 LUFS and −10 LU), plus LRA (EBU Tech 3342) and true peak (4× oversampling below 96 kHz, 2× below 192 kHz).
- Memory stays fixed: gating uses 0.01 LU histograms.
- Checked in `tests/CosmosTest.cpp` against Tech 3341 cases 1–4, the LRA case and true-peak cases at 44.1, 48 and 96 kHz. All are within ±0.1 LU.
- Targets: −14, −16, −23 or −9 LUFS, or off. RESET (or a double-click on LOUDNESS / LEVELS) restarts integrated, LRA and maxima.

## Analysis (message thread, `Source/Analysis.*`)
The audio reaches the editor through a lock-free FIFO.
- Spectrum: 4096-point FFT, smoothed, with peak hold and a 3 dB/oct display slope.
- Log bands for the spectrogram. Stereo width per 1/3 octave (side / (mid + side)), correlation and balance.
- Drum hits:
  - kick: flux in 35–110 Hz
  - snare: body (260–750 Hz) and crack (1.5–5 kHz) together
  - hats: 7–16 kHz
  - A band must also hold real energy (no more than 30 dB under the whole frame), so leakage doesn't trigger a hit.
- Tempo:
  - plugin: the DAW's exact tempo and bar (playhead).
  - app: autocorrelation of the onset curve plus a beat PLL. The downbeat is the beat with the most kick weight, with hysteresis.
- Harmony:
  - chroma from an 8192-point FFT with interpolated spectral peaks
  - key from Krumhansl–Kessler profiles
  - chords from 10 types, with slash bass and Roman numerals
  - one chord per bar for the strip
- Tuner: YIN on decimated audio.
- Low end: kick spectrum (captured at kicks) vs bass spectrum (captured between kicks), the shared area hatched, envelopes over 2 bars, and a ducking estimate.

## The 21 visuals
| Group | Visuals |
|---|---|
| METER | LOUDNESS, LEVELS (peak / RMS / true peak max), VU (−18 dBFS = 0 VU), VECTORSCOPE + correlation, SCOPE (pitch-triggered), TUNER |
| SPECTRAL | SPECTRUM, SPECTROGRAM, WAVEFORM (coloured low / high) |
| GENERATIVE | STARMAP, ROSETTE, RIDGES, RIPPLES (rings on hits), NUCLEUS, ATTRACTOR (de Jong, driven by the music) |
| MUSICAL | PLATE (annotated L/R spectrum + tempo / key / LUFS / bar card), BAR CLOCK (one turn per bar, hits, chord and bass arcs), STEPS (16 × 9 glyph grid + kick / snare / hat rows), LOW END, WIDTH (mono zone below 120 Hz, warns when the lows are wide), FIFTHS (circle of fifths, key, chord, chord per bar) |

## Skins
MIDNIGHT, PAPER, NEBULA, PHOSPHOR, BLUEPRINT, NEON, ROSE, GLACIER. A skin changes the whole window.

## Window
- Strip of panels:
  - **+ PANEL** adds a panel.
  - Hover a panel to see its header: click the name to swap the visual, ◂ ▸ to move it, × to remove it.
  - Drag a divider to resize two neighbouring panels.
- Above 330 px of height the panels become a grid.
- SIZE gives presets from a 900 × 150 strip up to 1900 × 1040.
- App only:
  - **PIN** keeps the window on top.
  - **DOCK** turns it into a 150 px strip across the top of the screen, on top and without a title bar.
  - **FULL** goes fullscreen with the grid.
  - **INPUT** opens the audio device dialog.
- Layout, skin, target, size and pin are saved in the DAW project.

## System audio (app)
- The app listens to everything the Mac plays by default: **SOURCE: SYSTEM**. It uses a Core Audio process tap on the whole system mix (macOS 14.2 or later) and needs no BlackHole.
- The first time, macOS asks for **System Audio Recording**; allow it. Nothing is recorded or sent anywhere.
- **SOURCE: INPUT** (or **INPUT**) switches back to an audio interface input.
- Verified here: the tap starts and delivers 48 kHz audio. Without the permission (as in a headless test) macOS delivers silence.
- On Windows the app still uses an input device.

## Not done yet
- No icon (`shared/brand/icon-cosmos.png`) and no installer entry yet.

## Developer preview
```
LT_EDITOR_SNAPSHOT=/tmp/x.png LT_SKIN=0..7 LT_SIZE=1280x760 LT_LAYOUT="15:1.3,16:0.8,17:1" "LT COSMOS.app/Contents/MacOS/LT COSMOS"
```
This plays a synthetic 120 BPM groove (Am–F–C–G) and writes a PNG after about 6 s. Screenshots are in `design/cosmos/`.
