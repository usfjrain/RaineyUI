#!/bin/sh
# Screen Recorder app (part of RaineyUI): settings, file browser, player.
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE"
export LD_LIBRARY_PATH=/usr/trimui/lib:$LD_LIBRARY_PATH
rm -f /tmp/rainey_input
./rainey --rec-app > /tmp/screenrec_app.log 2>&1
sync
