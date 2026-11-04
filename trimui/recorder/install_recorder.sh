#!/bin/sh
# Install the Screen Recorder app on a TrimUI Brick over adb (needs ../build.sh first).
set -e
cd "$(dirname "$0")"
DEST=/mnt/UDISK/Apps/ScreenRecorder
[ -x ../package/RaineyUI/rainey ] || ../build.sh
adb shell "mkdir -p $DEST/res"
adb push ScreenRecorder/config.json ScreenRecorder/launch.sh ScreenRecorder/recorder.init ScreenRecorder/icon.png $DEST/ >/dev/null
adb push ScreenRecorder/res/. $DEST/res/ >/dev/null
# restart a running recorder service so it uses the new binary
adb shell "P=\$(cat /tmp/rainey_rec.pid 2>/dev/null); [ -n \"\$P\" ] && kill \$P 2>/dev/null; sleep 1; true"
adb push ../package/RaineyUI/rainey $DEST/rainey >/dev/null
adb shell "chmod +x $DEST/rainey $DEST/launch.sh $DEST/recorder.init; grep -q '^enabled=1' /mnt/UDISK/RaineyUI/recorder.cfg 2>/dev/null && /etc/init.d/raineyrec start; sync"
echo "Installed to $DEST (Apps → Screen Recorder)"
