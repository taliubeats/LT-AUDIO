# LT VACUUM

A Max for Live audio effect for Ableton Live 11 and 12. It finds the tracks whose signal never reaches Main (called Master in Live 11) and deletes them, in two steps: **SCAN**, then **DELETE**.

The device lets audio pass straight through, so it can sit on any track, including Main.

## Use it
1. Drop `LT VACUUM.amxd` on any track. Keep `lt_vacuum.js` in the same folder (see Install).
2. Press **SCAN**. Nothing is deleted yet. The box lists each track that will go, with the reason:
   - output: none
   - sends only, all sends down
   - MIDI going nowhere
   - routed to "X", which never reaches Main
3. Press **DELETE**. Before deleting, the device scans again. A track you fixed after the scan is spared. Live's Undo (Cmd/Ctrl+Z) brings tracks back.

**+ EMPTY** also lists tracks that do reach Main but hold no clips and no devices. Groups are never listed as empty.

## What counts as reaching Main
A track is kept when any path takes its signal to Main:
- its output is Main / Master
- its output is a group, a return or another track that reaches Main
- it is in "Sends Only" mode with a send turned up to a return that reaches Main
- another track listens to it on its input (resampling, bounces)
- it is a MIDI track playing an instrument track that reaches Main
- its output is external (Ext. Out, or any hardware output). These are kept for safety.

Also:
- A group is deleted only when every track inside it goes too.
- The track that holds LT VACUUM is never deleted.
- Tracks are deleted from the bottom of the set upwards.
- If two tracks share a name, the box warns you: Live shows routing by name, so a route to that name could mean either track.

## Install
- Put `LT VACUUM.amxd` and `lt_vacuum.js` in the same folder, for example `User Library/Presets/Audio Effects/Max Audio Effect/LT VACUUM/`.
- To get one self-contained file: open the device's editor in Live (the Max editor button), click **Freeze**, then save. The frozen `.amxd` carries the script inside it.
- If Live refuses to load the `.amxd`:
  1. Drop an empty "Max Audio Effect" on a track.
  2. Open its editor.
  3. Open `LT VACUUM.maxpat` in Max, copy everything, and paste it into the new device.
  4. Save it as `LT VACUUM.amxd`.
  The `.maxpat` is the same patch as plain JSON.

## Files
| File | What it is |
|---|---|
| `m4l/vacuum/lt_vacuum.js` | All the logic. `planCleanup()` decides what to delete from a plain model of the set; the Live API part only reads the set and calls `delete_track`. |
| `m4l/vacuum/make_device.py` | Writes `LT VACUUM.maxpat` and `LT VACUUM.amxd`. |
| `m4l/vacuum/tests/vacuum_test.js` | 13 routing cases, plus SCAN → DELETE end to end against a fake Live API (it includes the re-check before deleting). Runs in Node or with macOS's built-in `jsc`. |

```
/System/Library/Frameworks/JavaScriptCore.framework/Versions/A/Helpers/jsc m4l/vacuum/tests/vacuum_test.js
```

## Not verified
Ableton Live and Max are not installed on the build machine, so the device has not been opened in Live. Four things are untested there:
- that the hand-built `.amxd` container loads
- the exact text format Live returns for routing names (the script accepts both JSON and plain text)
- the look of the panel
- that a real `delete_track` call behaves as the fake one does

Test it on a copy of a set the first time.
