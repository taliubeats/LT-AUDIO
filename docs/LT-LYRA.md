# LT LYRA

LYRA shows the chords and scales of whatever you play over MIDI. It only reads MIDI and never makes a sound.

Formats: VST3, AU and a standalone app. LYRA loads as an instrument, so put it on a MIDI or instrument track. Its audio output is always silent.

- Plugin code: `Ltly`
- License product id: `LYRA`

## Three ways to load it
| Plugin | Shows up as | Sound |
|---|---|---|
| **LT LYRA** | instrument | its own **piano** (PIANO: -18 to -3 dB, or OFF). Plays on its own, on one track. |
| **LT LYRA FX** | **audio effect** | lets the sound before it through (Serum, any synth); piano OFF by default |
| **LT LYRA MIDI** | MIDI FX (Logic) | none, the instrument after it plays |

## Where to put it
- **Logic**: insert **LT LYRA MIDI** in the **MIDI FX** slot, before the instrument. Notes pass through to the synth.
- **Reaper, Bitwig, FL Studio (Patcher), Cubase**: LT LYRA passes its MIDI straight through, so a synth after it still plays.
- **Ableton, simplest**: load **LT LYRA** on a MIDI track and play: you see the chords and hear its piano.
- **Ableton with Serum**: Live only sends notes to instruments, never to effects on the same track. Two options:
  - Instrument Rack (one track): two chains, Serum in one and LT LYRA (PIANO OFF) in the other. Both get the notes.
  - LT LYRA FX after Serum: put it in Serum's track, then make a second MIDI track with your keyboard and set its *MIDI To* to the Serum track, *LT LYRA FX*. Or simply set the Serum track's notes to come from that MIDI track.
- **App** (`LT LYRA.app`): the **MIDI** button picks your keyboard, so it works outside any DAW.

## What it shows
- **Chord name**
  - Extensions and alterations: `Cmaj9`, `D7♯9`, `C9♯11`, `C13`, `G7♭9`, `Em7♭5`, `Cdim7`, `CmMaj7`, `C6/9`, `Cadd9`, `C7sus4`.
  - Inversions are shown as slash chords: `G/B`, `C/G`.
  - A single note shows its name. Two notes show the interval, for example `C + E · M3`, or `C5` for a fifth.
- **ALSO**: up to two other plain ways to name the same notes, such as `C6` → `Am7/C`.
- **Roman numeral and figures**
  - Examples: `V⁶₅`, `I⁶₄`, `iiø7`, `V7/V`, `♭VI`.
  - Function: tonic, subdominant, dominant, secondary dominant or chromatic. It is worked out against the chosen key (minor keys allow the raised 6th and 7th).
- **Grand staff**
  - Treble and bass clefs, with the key signature.
  - Notes sit at their real octave, with ledger lines.
  - An accidental appears only when the note differs from the key signature (a natural sign when needed).
  - Seconds sit side by side. The root is drawn in the highlight colour.
- **Chromatic wheel**: the voicing drawn as a shape. The root is highlighted, the key's tonic is circled, and the intervals are listed underneath.
- **88-key piano (A0–C8)**
  - Keys you hold light up; the root is shown stronger.
  - Dots mark the notes of the scale.
  - You can click keys to try chords. Right-click clears them.

## Options
- **KEY**
  - AUTO detects the key from what you have been playing. It uses a slowly fading note histogram (Krumhansl–Kessler profiles) and only switches once the new key has held for a moment.
  - You can also pick any of the 24 keys.
- **SPELLING**
  - BY KEY: notes are named from the key's scale degrees (♭II, ♭III, ♯IV, ♭VI, ♭VII). Chord tones follow their interval, so C♯dim7 is spelled C♯ E G B♭.
  - SHARPS or FLATS: fixed spelling.
  - The bass note under a slash is never written as F♭, B♯ or with a double accidental.
- **PEDAL**: when ON, notes held by the sustain pedal (CC64) still count as sounding.
- **THEME**: dark or light.
- **COLOUR**: 8 highlight colours.
- **LANGUAGE**
  - EN, ES, JA, DE.
  - German uses H for B and B for B♭, and writes keys like `a-Moll`.
  - Japanese uses Hiragino Sans on Mac and Yu Gothic UI on Windows.
- **Window**: resizable, with a fixed aspect ratio. Every setting is saved in the DAW project.
- **Last chord**: when you let go of all the keys, the last chord stays on screen, faded.

## Tests
`tests/LyraTest.cpp` checks:
- 27 chord names
- spelling in different keys, including the German note names
- 14 Roman numerals and their functions
- key detection and key signatures

`LTLyra_stress` passes.

## Developer preview
```
LT_EDITOR_SNAPSHOT=/tmp/x.png LT_NOTES="48,64,67,71,74" LT_KEY=0 LT_LANG=0..3 [LT_LIGHT=1] [LT_COLOUR=0..7] "LT LYRA.app/Contents/MacOS/LT LYRA"
```
Screenshots are in `design/lyra/`.
