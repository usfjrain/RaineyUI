#!/bin/sh
# Cross-compile RaineyUI for TrimUI Brick/Smart Pro (aarch64, glibc 2.33) with zig.
set -e
cd "$(dirname "$0")"
ZIG=${ZIG:-$HOME/.local/share/rainey-toolchain/zig-macos-x86_64-0.13.0/zig}
OUT=package/RaineyUI
mkdir -p build
if [ ! -x "$ZIG" ]; then
  echo "zig 0.13.0 not found at $ZIG (download from https://ziglang.org/download/ and set ZIG=)"; exit 1
fi
# SDL2 link libraries come from the device itself (TrimUI firmware), pulled once over adb
if [ ! -f devlibs/libSDL2.so ]; then
  echo "fetching SDL2 libraries from the device (adb)…"
  mkdir -p devlibs
  for l in libSDL2-2.0.so.0 libSDL2_image-2.0.so.0 libSDL2_ttf-2.0.so.0; do adb pull /usr/trimui/lib/$l devlibs/ >/dev/null; done
  ln -sf libSDL2-2.0.so.0 devlibs/libSDL2.so
  ln -sf libSDL2_image-2.0.so.0 devlibs/libSDL2_image.so
  ln -sf libSDL2_ttf-2.0.so.0 devlibs/libSDL2_ttf.so
fi
$ZIG cc -target aarch64-linux-gnu.2.33 -O2 -std=gnu11 -Wall -Wno-unused-function \
  -Ithird_party/include -Ithird_party/include/SDL2 -Ithird_party -Ithird_party/miniz \
  -D_REENTRANT -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -Wno-#pragma-messages \
  src/main.c src/common.c src/gfx.c src/ui.c src/player.c src/leds.c src/leds_ui.c src/bg.c src/sort.c src/wifi.c third_party/miniz/miniz.c \
  -Ldevlibs -lSDL2 -lSDL2_image -lSDL2_ttf \
  -Wl,--no-as-needed -lpthread -ldl -lm -Wl,--allow-shlib-undefined -rdynamic \
  -o $OUT/rainey
echo "built $OUT/rainey"
