#!/bin/bash
# Builds LT COSMOS, LT LYRA and LT WARP on this Mac and turns them into installers (.pkg),
# plus the Max for Live ZIP for LT VACUUM. Run it from anywhere:
#   bash installers/mac/make_installers.sh
# Needs: Xcode command line tools (xcode-select --install) and CMake (brew install cmake).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

command -v cmake >/dev/null || { echo "Falta CMake. Instala con:  brew install cmake"; exit 1; }
command -v pkgbuild >/dev/null || { echo "Faltan las herramientas de Xcode. Instala con:  xcode-select --install"; exit 1; }

DIST="$ROOT/dist"
rm -rf "$DIST" "$ROOT/pkg" "$ROOT/m4l-pkg"
mkdir -p "$DIST" "$ROOT/pkg/VST3" "$ROOT/pkg/Components"

echo "==> 1/4 Compilando (la primera vez tarda bastante)"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release -j 4 \
  --target LTCosmos_VST3 LTCosmos_AU LTLyra_VST3 LTLyra_AU LTWarp LTLicenseStudio

echo "==> 2/4 Reuniendo y firmando (firma local, sin certificado de Apple)"
for d in build/plugins/cosmos/*_artefacts/Release build/plugins/lyra/*_artefacts/Release; do
  [ -d "$d/VST3" ] && cp -R "$d"/VST3/*.vst3 pkg/VST3/
  [ -d "$d/AU" ]   && cp -R "$d"/AU/*.component pkg/Components/
done
for b in pkg/VST3/* pkg/Components/*; do codesign --force --deep --sign - "$b"; done
APP="build/apps/license-studio/LTLicenseStudio_artefacts/Release/LT License Studio.app"
WARP="build/apps/warp/LTWarp_artefacts/Release/LT WARP.app"
codesign --force --deep --sign - "$APP"
codesign --force --deep --sign - "$WARP"

echo "==> 3/4 Creando los instaladores .pkg"
./installers/mac/build_pkgs.sh pkg "$APP" "$DIST" "0.1.$(date +%Y%m%d%H%M)" "$WARP"

echo "==> 4/4 Creando el ZIP de Max for Live (VACUUM)"
python3 m4l/vacuum/make_device.py
mkdir -p "m4l-pkg/LT VACUUM"
cp "m4l/vacuum/LT VACUUM.amxd" "m4l/vacuum/LT VACUUM.maxpat" m4l/vacuum/lt_vacuum.js "m4l-pkg/LT VACUUM/"
cp docs/LT-VACUUM.md "m4l-pkg/LT VACUUM/README.md"
(cd m4l-pkg && zip -qry "$DIST/LT-VACUUM-MaxForLive.zip" "LT VACUUM")

echo
echo "LISTO. Tus archivos estan en: $DIST"
ls -la "$DIST"
open "$DIST"
