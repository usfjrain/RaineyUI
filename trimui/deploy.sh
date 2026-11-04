#!/bin/sh
# Build RaineyUI and install it to a TrimUI Brick/Smart Pro over ADB (internal storage).
#   ./deploy.sh          build + push binary/res/config
#   ./deploy.sh --full   also download + install all emulators (tools/install_emulators.sh)
#   ./deploy.sh --run    launch it on the device afterwards
set -e
cd "$(dirname "$0")"
./build.sh
DEST=/mnt/UDISK/Apps/RaineyUI
adb get-state >/dev/null
adb shell "pidof rainey >/dev/null && echo quit > /tmp/rainey_input; mkdir -p $DEST/cores $DEST/res /mnt/UDISK/Roms"
adb push package/RaineyUI/rainey package/RaineyUI/launch.sh package/RaineyUI/config.json package/RaineyUI/icon.png \
  package/RaineyUI/leds_watch.sh package/RaineyUI/raineyleds.init $DEST/
adb push package/RaineyUI/res/. $DEST/res/
if [ "$1" = "--full" ]; then
  ./tools/install_emulators.sh
fi
adb shell "mkdir -p /mnt/UDISK/Roms/N64 /mnt/UDISK/Roms/DC /mnt/UDISK/Roms/NDS /mnt/UDISK/RaineyUI/bios/dc"
# LED boot hook: re-applies the LED preset at power-on and whenever MainUI restarts
adb shell "chmod +x $DEST/leds_watch.sh; cp $DEST/raineyleds.init /etc/init.d/raineyleds; chmod +x /etc/init.d/raineyleds; ln -sf ../init.d/raineyleds /etc/rc.d/S98zraineyleds; /etc/init.d/raineyleds start"
adb shell "chmod +x $DEST/rainey $DEST/launch.sh; sync"
echo "Installed to $DEST"
if [ "$1" = "--run" ] || [ "$2" = "--run" ]; then
  adb shell "echo $DEST/launch.sh > /tmp/cmd_to_run.sh; chmod +x /tmp/cmd_to_run.sh; killall -9 MainUI"
fi
