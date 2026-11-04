#!/bin/sh
# Apps → Install RaineyUI: shows the installer screen, which runs install_worker.sh.
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE"
export LD_LIBRARY_PATH=/usr/trimui/lib:$LD_LIBRARY_PATH
echo 1 > /tmp/stay_awake
rm -f /tmp/rainey_input
"$HERE/payload/RaineyUI/rainey" --install-ui "$HERE/install_worker.sh" > /tmp/rainey_installer_ui.log 2>&1
rm -f /tmp/stay_awake
sync
