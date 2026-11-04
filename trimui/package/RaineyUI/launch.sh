#!/bin/sh
# RaineyUI launcher for TrimUI MainUI (Brick / Smart Pro).
progdir=$(cd "$(dirname "$0")" && pwd)
cd "$progdir"
export LD_LIBRARY_PATH=/usr/trimui/lib:$progdir/lib:$LD_LIBRARY_PATH
DATA=/mnt/UDISK/RaineyUI
LOG=$DATA/rainey.log
mkdir -p $DATA/bios/dc
echo "=== RaineyUI start $(date)" > "$LOG"
echo 1 > /tmp/stay_awake
rm -f /tmp/rainey_launch /tmp/rainey_ui_state /tmp/rainey_input
GOV=/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
RA_DIR=$progdir/emus/RetroArch
SWAP=/mnt/UDISK/n64_swap

swap_on() {
    [ -f $SWAP ] || return
    swapon $SWAP 2>/dev/null || { mkswap $SWAP >/dev/null 2>&1 && swapon $SWAP 2>/dev/null; }
}

while true; do
    ./rainey >> "$LOG" 2>&1
    [ -f /tmp/rainey_launch ] || break
    SYS=$(sed -n 1p /tmp/rainey_launch)
    CORE=$(sed -n 2p /tmp/rainey_launch)
    ROM=$(sed -n 3p /tmp/rainey_launch)
    LAUNCHER=$(sed -n 5p /tmp/rainey_launch)
    echo "--- launch [$LAUNCHER] $SYS: $ROM" >> "$LOG"
    echo performance > $GOV 2>/dev/null
    case "$LAUNCHER" in
    retroarch)
        if [ "$SYS" = "dreamcast" ]; then
            # use the real BIOS when present, otherwise Flycast's HLE BIOS
            OPT="$RA_DIR/.retroarch/config/Flycast/DC.opt"
            if [ -f $DATA/bios/dc/dc_boot.bin ]; then HLE=disabled; else HLE=enabled; fi
            sed -i "s/^reicast_hle_bios = .*/reicast_hle_bios = \"$HLE\"/" "$OPT"
        fi
        [ "$SYS" = "n64" ] && swap_on
        cd "$RA_DIR"
        HOME="$RA_DIR" ./ra64.trimui -v -c "$RA_DIR/retroarch.cfg" -L "$CORE" "$ROM" >> "$LOG" 2>&1
        cd "$progdir"
        [ "$SYS" = "n64" ] && swapoff $SWAP 2>/dev/null
        ;;
    drastic)
        cd "$progdir/emus/drastic"
        echo "drastic output suppressed (very chatty)" >> "$LOG"
        HOME="$PWD" "$CORE" "$ROM" > /dev/null 2>&1
        cd "$progdir"
        ;;
    *)
        ./rainey --play /tmp/rainey_launch >> "$LOG" 2>&1
        ;;
    esac
    echo ondemand > $GOV 2>/dev/null
    rm -f /tmp/rainey_launch /tmp/rainey_rom/* 2>/dev/null
    sync
done
rm -f /tmp/rainey_ui_state /tmp/stay_awake
sync
