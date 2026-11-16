#!/bin/bash
# Builds macOS installers (.pkg): one per plugin, one with every plugin, one for LT License Studio.
# Usage: build_pkgs.sh <dir with VST3/ and Components/> <License Studio .app> <output dir> <version>
set -euo pipefail
SRC="$1"; APP="$2"; OUT="$3"; VERSION="$4"
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
    /usr/libexec/PlistBuddy -c "Set :$i:BundleIsRelocatable false" "$WORK/$SLUG.plist"
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
for vst in "$SRC"/VST3/*.vst3; do
  NAME="$(basename "$vst" .vst3)"
  COMP="$(make_component "$NAME")"
  ALL+=("$COMP")
  finish "$NAME" "$OUT/$(echo "$NAME" | tr ' ' '-')-Installer-mac.pkg" "$COMP"
done

if [ ${#ALL[@]} -gt 1 ]; then
  finish "LT AUDIO - All plugins" "$OUT/LT-AUDIO-All-Plugins-Installer-mac.pkg" "${ALL[@]}"
fi

# LT License Studio -> /Applications
pkgbuild --component "$APP" --install-location /Applications \
         --identifier com.ltaudio.licensestudio --version "$VERSION" "$WORK/components/LT-License-Studio.pkg" >/dev/null
finish "LT License Studio" "$OUT/LT-License-Studio-Installer-mac.pkg" "$WORK/components/LT-License-Studio.pkg"
