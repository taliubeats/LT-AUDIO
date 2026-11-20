# LT WARP

A small, fast media converter for audio, video and images. It is a desktop app for macOS and Windows. Everything runs on your own computer: no account and no cloud.

## Use it
1. Drop files or folders on the speech bubble, or click **OR BROWSE...**. Folders are scanned one level deep.
2. Pick the target format:
   - audio and video files go to the audio formats
   - images go to the image formats
   - you can change the target of any single row by clicking its format chip
3. Press **CONVERT!**.
   - Two files convert at once, each with a progress bar.
   - **SHOW** opens the result in Finder or Explorer.
   - **STOP** cancels, and the half-written file is deleted.

Existing files are never overwritten: a second copy is saved as `name (2).ext`. The output goes next to each source file, or into a folder you choose (**SAVE TO**).

## Formats
| From | To |
|---|---|
| audio: mp3 wav flac aiff m4a aac opus ogg wma caf … | MP3 (128–320 kbps), WAV 16-bit (triangular dither), WAV 24-bit, FLAC, AIFF 24-bit, M4A/AAC (Apple AAC encoder on Mac), OPUS (96–256 kbps, 48 kHz) |
| video: mp4 mov mkv webm avi … | the same audio formats (the first audio track is extracted), or a still frame as an image |
| images: jpg png webp tiff heic bmp gif avif | JPG, PNG, WEBP, TIFF (LZW), HEIC (macOS only), with quality 70–100 |

- **SAMPLE RATE:** keep the original rate, or resample to 44.1, 48 or 96 kHz.
- **TRIM:** a start and an end time, in seconds or as mm:ss. It applies to audio and video. For a video turned into an image, the start time picks the frame.
- Tags are copied from the source file.

## Skins
COLOR (yellow paper with pop accents), INK (black and white), NOIR (night, with one red). The choice is saved with the other settings.

## Engine
`Source/Converter.*` builds and runs `ffmpeg` command lines.
- On macOS, HEIC is read and written with Apple's own `sips` tool.
- Tools are searched in this order:
  1. `LT_WARP_TOOLS`
  2. the app bundle: `Contents/Resources` on macOS, or the exe folder or `tools\` on Windows
  3. Homebrew
  4. PATH
- **Before release:** put LGPL builds of `ffmpeg` and `ffprobe` in `tools/warp/Darwin/`. CMake then copies them into the app. Homebrew's ffmpeg is a GPL build and should not be shipped.

## Not done yet
- Downloading from links was left out on purpose: it goes against the terms of most video sites.
- Licensing works like the other LT products (product `WARP`), so the trial is the family's 10 days, not 7.
- No icon yet.
- No Windows build yet: the code is portable, but it hasn't been compiled for Windows.

## Tests
`tests/WarpTest.cpp` makes real files (a 24-bit WAV, an MP4 with sound, a PNG, a HEIC). It converts them to every target and checks each result with ffprobe:
- codec, sample rate and channel count
- that only the audio comes out of a video
- trimming exact to the millisecond
- image sizes
- refused conversions and the file-naming rule
- cancelling a job
- the queue running two files at once

## Preview
```
"LT WARP.app/Contents/MacOS/LT WARP" --screenshot=/tmp/x.png --skin=0..2
```
