#!/bin/sh
# PortMaster for RaineyUI on the TrimUI Brick (stock firmware).
# PortMaster lives here and installs ports to /mnt/UDISK/ports; the SD card is never written.
. /mnt/UDISK/Apps/PortMaster/runtime/env.sh
cd "$controlfolder" || exit 1
echo 1 > /tmp/stay_awake
exec > "$controlfolder/log.txt" 2>&1
chmod -R +x . 2>/dev/null
rm -f .pugwash-reboot
while true; do
    python3 ./pugwash --debug
    [ -f .pugwash-reboot ] || break
    rm -f .pugwash-reboot
done
# back in RaineyUI the Ports list is read again
touch /tmp/rainey_rescan
sync
