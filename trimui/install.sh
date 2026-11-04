#!/bin/bash
# Install RaineyUI on a TrimUI Brick / Smart Pro (stock firmware) from a Mac or Linux PC.
# Needs: adb (Android platform-tools), python3, curl, and the device connected over USB.
#   ./install.sh               app + emulators
#   ./install.sh --app-only    just the RaineyUI app (no emulator download)
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
APP="$HERE/RaineyUI"; [ -d "$APP" ] || APP="$HERE/package/RaineyUI"
[ -x "$APP/rainey" ] || { echo "RaineyUI binary not found in $APP (download a release or run ./build.sh)"; exit 1; }
DEST=/mnt/UDISK/Apps/RaineyUI
command -v adb >/dev/null || { echo "adb not found. Install Android platform-tools first."; exit 1; }
[ "$(adb get-state 2>/dev/null)" = "device" ] || { echo "No device found. Plug in the Brick with USB and try again."; exit 1; }

echo "==> Installing RaineyUI to $DEST"
adb shell "pidof rainey >/dev/null && echo quit > /tmp/rainey_input; mkdir -p $DEST /mnt/UDISK/Roms /mnt/UDISK/RaineyUI/bios/dc" || true
for f in rainey launch.sh config.json icon.png leds_watch.sh raineyleds.init game_fixes.txt; do adb push "$APP/$f" "$DEST/" >/dev/null; done
adb push "$APP/res" "$DEST/" >/dev/null
for d in FC SFC MD PS DC ARCADE GB GBA N64 NDS; do adb shell "mkdir -p /mnt/UDISK/Roms/$d"; done
# LED lights: re-apply the chosen preset at boot and whenever the TrimUI menu restarts
adb shell "chmod +x $DEST/rainey $DEST/launch.sh $DEST/leds_watch.sh; cp $DEST/raineyleds.init /etc/init.d/raineyleds; chmod +x /etc/init.d/raineyleds; ln -sf ../init.d/raineyleds /etc/rc.d/S98zraineyleds; /etc/init.d/raineyleds start; sync"

if [ "${1:-}" != "--app-only" ]; then
  T="$HERE/install_emulators.sh"; [ -f "$T" ] || T="$HERE/tools/install_emulators.sh"
  "$T"
fi
echo
echo "RaineyUI is installed. On the Brick open Apps → RaineyUI."
echo "Put games in /mnt/UDISK/Roms/<FC SFC MD PS DC ARCADE GB GBA N64 NDS> or on an SD card."
