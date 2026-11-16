#!/bin/bash
# Builds every LT plugin + LT License Studio on this Mac and creates the .pkg installers.
# Run:  curl -fsSL https://raw.githubusercontent.com/taliubeats/LT-AUDIO/main/scripts/build-mac.sh | bash
set -euo pipefail

SRC="$HOME/LT-AUDIO-build/source"
OUT="$HOME/Desktop/LT-AUDIO-Installers"
TOOLS="$HOME/LT-AUDIO-build/tools"

say () { printf "\n\033[1;35m==> %s\033[0m\n" "$1"; }

# 1. Apple's command line tools (compiler)
if ! xcode-select -p >/dev/null 2>&1 || ! command -v clang++ >/dev/null 2>&1; then
  say "Installing Apple's developer tools. A window will open: click Install."
  xcode-select --install || true
  echo "When that installation finishes, run this same command again."
  exit 0
fi

# 2. CMake + Ninja (private copy, no Homebrew needed)
if ! command -v cmake >/dev/null 2>&1 || ! command -v ninja >/dev/null 2>&1; then
  if [ ! -x "$TOOLS/bin/cmake" ] || [ ! -x "$TOOLS/bin/ninja" ]; then
    say "Installing CMake and Ninja (first time only)"
    python3 -m venv "$TOOLS"
    "$TOOLS/bin/pip" install --quiet --upgrade pip cmake ninja
  fi
  export PATH="$TOOLS/bin:$PATH"
fi

# 3. Latest source code
say "Downloading the latest LT AUDIO code"
if [ -d "$SRC/.git" ]; then
  git -C "$SRC" fetch --depth 1 origin main && git -C "$SRC" reset --hard origin/main
else
  mkdir -p "$(dirname "$SRC")"
  git clone --depth 1 https://github.com/taliubeats/LT-AUDIO.git "$SRC"
fi
cd "$SRC"

# 4. Build (Universal: Apple Silicon + Intel)
say "Building (the first time takes 10-25 minutes)"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build -j "$(sysctl -n hw.ncpu)"

# 5. Installers
say "Creating the installers"
rm -rf pkg dist && mkdir -p dist pkg/VST3 pkg/Components
APP="build/apps/license-studio/LTLicenseStudio_artefacts/Release/LT License Studio.app"
[ -d "$APP" ] || APP="$(find build/apps -name 'LT License Studio.app' -type d | head -1)"
codesign --force --deep --sign - "$APP"
for d in build/plugins/*/*_artefacts/Release; do
  [ -d "$d/VST3" ] && cp -R "$d"/VST3/*.vst3 pkg/VST3/
  [ -d "$d/AU" ] && cp -R "$d"/AU/*.component pkg/Components/
done
for b in pkg/VST3/* pkg/Components/*; do codesign --force --deep --sign - "$b"; done
./installers/mac/build_pkgs.sh pkg "$APP" dist "0.1.local"

rm -rf "$OUT" && mkdir -p "$OUT"
cp dist/*.pkg "$OUT/"
say "Done! Your installers are in: $OUT"
ls -1 "$OUT"
open "$OUT"
