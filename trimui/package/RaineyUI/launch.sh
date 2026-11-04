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
        # RetroArch's per-folder overrides only match the internal N64/DC folder names, so games
        # from SD folders like n64/ or dreamcast/ fell back to integer scaling (a small box).
        # Give every N64/Dreamcast game the same full-screen settings, whatever folder it is in.
        APPEND=/tmp/rainey_ra.cfg
        printf '%s\n' 'auto_overrides_enable = "false"' 'game_specific_options = "false"' \
            'video_scale_integer = "false"' 'aspect_ratio_index = "22"' 'video_shader_enable = "false"' > $APPEND
        if [ "$SYS" = "dreamcast" ]; then
            CFG="$RA_DIR/.retroarch/config/Flycast"
            # DC.opt is the tuned template; Flycast.opt is the file RetroArch reads
            cp "$CFG/DC.opt" "$CFG/Flycast.opt"
            # use the real BIOS when present, otherwise Flycast's HLE BIOS
            if [ -f $DATA/bios/dc/dc_boot.bin ]; then HLE=disabled; else HLE=enabled; fi
            # internal resolution from RaineyUI settings (same order as dc_res_names in ui.c)
            case "$(sed -n 's/^dc_res=//p' $DATA/settings.cfg 2>/dev/null)" in
                1) RES=800x600 ;; 2) RES=960x720 ;; 3) RES=1024x768 ;; 4) RES=1280x960 ;; *) RES=640x480 ;;
            esac
            sed -i -e "s/^reicast_hle_bios = .*/reicast_hle_bios = \"$HLE\"/" \
                   -e "s/^reicast_internal_resolution = .*/reicast_internal_resolution = \"$RES\"/" "$CFG/Flycast.opt"
            printf '%s\n' 'input_player1_analog_dpad_mode = "0"' 'audio_max_timing_skew = "0.050000"' >> $APPEND
            echo "dreamcast: $RES, hle_bios=$HLE" >> "$LOG"
        fi
        [ "$SYS" = "n64" ] && swap_on
        cd "$RA_DIR"
        HOME="$RA_DIR" ./ra64.trimui -v -c "$RA_DIR/retroarch.cfg" --appendconfig $APPEND -L "$CORE" "$ROM" >> "$LOG" 2>&1
        cd "$progdir"
        [ "$SYS" = "n64" ] && swapoff $SWAP 2>/dev/null
        ;;
    ppsspp)
        cd "$progdir/emus/ppsspp"
        MS=$PWD/.config/ppsspp/PSP
        mkdir -p "$MS/SAVEDATA"
        # bring over saves from an SD card made on another device (read-only: the card is never changed)
        for d in /mnt/SDCARD/psp/SAVEDATA/* /mnt/SDCARD/PSP/SAVEDATA/*; do
            [ -d "$d" ] && [ ! -e "$MS/SAVEDATA/${d##*/}" ] && cp -r "$d" "$MS/SAVEDATA/" && echo "psp: imported save ${d##*/}" >> "$LOG"
        done
        swap_on
        HOME="$PWD" ./PPSSPPSDL_gl "$ROM" >> "$LOG" 2>&1
        swapoff $SWAP 2>/dev/null
        cd "$progdir"
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
