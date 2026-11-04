#!/bin/bash
# Download the arm64 libretro cores the Android build bundles (jniLibs) from the libretro buildbot.
# They are not stored in the repository. Run once before ./gradlew assembleDebug.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=app/src/main/jniLibs/arm64-v8a
BB=https://buildbot.libretro.com/nightly/android/latest/arm64-v8a
mkdir -p "$OUT"
tmp=$(mktemp -d)
# buildbot core name -> file name the app looks for
while read -r core name; do
  echo "  $core"
  curl -fsSL --retry 3 -o "$tmp/$core.zip" "$BB/${core}_libretro_android.so.zip"
  unzip -oq "$tmp/$core.zip" -d "$tmp"
  mv "$tmp/${core}_libretro_android.so" "$OUT/$name"
done <<'LIST'
snes9x snes9x_libretro_android.so
gambatte gambatte_libretro_android.so
genesis_plus_gx genesis_plus_gx_libretro_android.so
mgba libmgba_libretro_android.so
pcsx_rearmed libpcsx_rearmed_libretro_android.so
LIST
rm -rf "$tmp"
echo "Cores in $OUT"
