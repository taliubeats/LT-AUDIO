#!/bin/bash
# Builds every LT Max for Live device and zips them together: <out dir>/LT-MaxForLive-Devices.zip
# Each device folder holds the .amxd, its .js (must stay next to it, or Freeze the device in Max),
# the plain .maxpat and a README.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$(cd "${1:-$ROOT/dist}" 2>/dev/null && pwd || (mkdir -p "$1" && cd "$1" && pwd))"
STAGE="$(mktemp -d)/LT Max for Live"
mkdir -p "$STAGE"
for dev in vacuum chords clipgain bounce; do
  python3 "$ROOT/m4l/$dev/make_device.py" >/dev/null
  NAME="$(cd "$ROOT/m4l/$dev" && ls *.amxd | head -1 | sed 's/\.amxd$//')"
  mkdir -p "$STAGE/$NAME"
  cp "$ROOT/m4l/$dev/$NAME.amxd" "$ROOT/m4l/$dev/$NAME.maxpat" "$ROOT"/m4l/$dev/lt_*.js "$STAGE/$NAME/"
done
cp "$ROOT/docs/LT-MAX-FOR-LIVE.md" "$STAGE/README.md"
rm -f "$OUT/LT-MaxForLive-Devices.zip"
(cd "$(dirname "$STAGE")" && zip -qry "$OUT/LT-MaxForLive-Devices.zip" "LT Max for Live")
echo "built $OUT/LT-MaxForLive-Devices.zip"
