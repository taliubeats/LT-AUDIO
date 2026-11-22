#!/bin/bash
# Builds macOS installers (.pkg): one per plugin, one with every plugin, one for LT License Studio.
# Usage: build_pkgs.sh <dir with VST3/ and Components/> <License Studio .app> <output dir> <version> [more .apps...]
# Every extra .app (LT WARP, LT COSMOS, LT LYRA...) gets its own installer into /Applications.
set -euo pipefail
SRC="$1"; APP="$2"; OUT="$3"; VERSION="$4"; shift 4; EXTRA_APPS=("$@")
HERE="$(cd "$(dirname "$0")" && pwd)"
WORK="$(mktemp -d)"
mkdir -p "$OUT" "$WORK/components"

make_component () {   # <plugin name> -> component pkg path
  local NAME="$1" SLUG ID ROOT
  SLUG="$(echo "$NAME" | tr ' ' '-')"
  ID="com.ltaudio.$(echo "$NAME" | tr -d ' ' | tr '[:upper:]' '[:lower:]')"
  ROOT="$WORK/root-$SLUG"
  mkdir -p "$ROOT/VST3" "$ROOT/Components"
  [ -d "$SRC/VST3/$NAME.vst3" ] && cp -R "$SRC/VST3/$NAME.vst3" "$ROOT/VST3/"
  [ -d "$SRC/Components/$NAME.component" ] && cp -R "$SRC/Components/$NAME.component" "$ROOT/Components/"

  # never let Installer "relocate" the bundles to some other copy on disk
  pkgbuild --analyze --root "$ROOT" "$WORK/$SLUG.plist" >/dev/null
  /usr/libexec/PlistBuddy -c "Print" "$WORK/$SLUG.plist" >/dev/null
  i=0
  while /usr/libexec/PlistBuddy -c "Print :$i" "$WORK/$SLUG.plist" >/dev/null 2>&1; do
    /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$WORK/$SLUG.plist" 2>/dev/null \
      || /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$WORK/$SLUG.plist"
    i=$((i+1))
  done

  pkgbuild --root "$ROOT" --component-plist "$WORK/$SLUG.plist" \
           --install-location "/Library/Audio/Plug-Ins" \
           --scripts "$HERE/scripts" \
           --identifier "$ID" --version "$VERSION" \
           "$WORK/components/$SLUG.pkg" >/dev/null
  echo "$WORK/components/$SLUG.pkg"
}

finish () {   # <title> <output pkg> <component pkgs...>
  local TITLE="$1" OUTPKG="$2"; shift 2
  local DIST="$WORK/dist-$(basename "$OUTPKG").xml" ARGS=()
  for p in "$@"; do ARGS+=(--package "$p"); done
  productbuild --synthesize "${ARGS[@]}" "$DIST" >/dev/null
  # title + nicer window (replace whatever options productbuild synthesised)
  sed -i '' '/<options /d' "$DIST"
  sed -i '' "s#<installer-gui-script minSpecVersion=\"[0-9]*\">#&<title>$TITLE</title><options customize=\"allow\" require-scripts=\"false\" hostArchitectures=\"x86_64,arm64\"/><domains enable_localSystem=\"true\"/>#" "$DIST"
  productbuild --distribution "$DIST" --package-path "$WORK/components" "$OUTPKG" >/dev/null
  echo "built $OUTPKG"
}

ALL=()
# every plugin name found as a VST3 or an AU (some, like LT LYRA MIDI, are AU only)
NAMES="$( { for f in "$SRC"/VST3/*.vst3; do [ -e "$f" ] && basename "$f" .vst3; done; \
            for f in "$SRC"/Components/*.component; do [ -e "$f" ] && basename "$f" .component; done; } | sort -u )"
while IFS= read -r NAME; do
  [ -n "$NAME" ] || continue
  COMP="$(make_component "$NAME")"
  ALL+=("$COMP")
  finish "$NAME" "$OUT/$(echo "$NAME" | tr ' ' '-')-Installer-mac.pkg" "$COMP"
done <<< "$NAMES"

if [ ${#ALL[@]} -ge 1 ]; then
  finish "LT AUDIO - All plugins" "$OUT/LT-AUDIO-All-Plugins-Installer-mac.pkg" "${ALL[@]}"
fi

# LT License Studio -> /Applications
pkgbuild --component "$APP" --install-location /Applications \
         --identifier com.ltaudio.licensestudio --version "$VERSION" "$WORK/components/LT-License-Studio.pkg" >/dev/null
finish "LT License Studio" "$OUT/LT-License-Studio-Installer-mac.pkg" "$WORK/components/LT-License-Studio.pkg"

# Apps (LT WARP, LT COSMOS, LT LYRA...) -> /Applications, one installer each.
# The app is staged in its own root and marked NOT relocatable: otherwise Installer "updates" any other
# copy with the same bundle id it finds on disk (a build folder, a download) instead of /Applications.
for A in "${EXTRA_APPS[@]+"${EXTRA_APPS[@]}"}"; do
  [ -d "$A" ] || continue
  NAME="$(basename "$A" .app)"
  SLUG="$(echo "$NAME" | tr ' ' '-')"
  ID="com.ltaudio.app.$(echo "$NAME" | tr -d ' ' | tr '[:upper:]' '[:lower:]')"
  ROOT="$WORK/approot-$SLUG"
  mkdir -p "$ROOT"
  ditto "$A" "$ROOT/$NAME.app"
  pkgbuild --analyze --root "$ROOT" "$WORK/$SLUG-app.plist" >/dev/null
  i=0
  while /usr/libexec/PlistBuddy -c "Print :$i" "$WORK/$SLUG-app.plist" >/dev/null 2>&1; do
    /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$WORK/$SLUG-app.plist" 2>/dev/null \
      || /usr/libexec/PlistBuddy -c "Add :$i:BundleIsRelocatable bool false" "$WORK/$SLUG-app.plist"
    i=$((i+1))
  done
  pkgbuild --root "$ROOT" --component-plist "$WORK/$SLUG-app.plist" --install-location /Applications \
           --identifier "$ID" --version "$VERSION" "$WORK/components/$SLUG-App.pkg" >/dev/null
  finish "$NAME" "$OUT/$SLUG-App-Installer-mac.pkg" "$WORK/components/$SLUG-App.pkg"
done
