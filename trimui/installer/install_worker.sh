#!/bin/sh
# RaineyUI on-device installer worker (started by `rainey --install-ui`).
# Installs RaineyUI from payload/ to internal storage and downloads the emulators listed in
# manifest.txt from their original projects. Progress goes to /tmp/rainey_install.status
# (state / percent / title / detail), which the installer screen shows.
HERE=$(cd "$(dirname "$0")" && pwd)
ST=/tmp/rainey_install.status
DEST=/mnt/UDISK/Apps/RaineyUI
DATA=/mnt/UDISK/RaineyUI
DL=$DATA/.download
CA=$HERE/cacert.pem
MANIFEST=$HERE/manifest.txt
RAINEY=$HERE/payload/RaineyUI/rainey
export LD_LIBRARY_PATH=/usr/trimui/lib:$LD_LIBRARY_PATH

status() { printf '%s\n%s\n%s\n%s\n' "$1" "$2" "$3" "$4" > $ST.tmp && mv $ST.tmp $ST; }
fail() {
    echo "FAIL: $1 / $2"
    status FAIL 0 "$1" "$2"
    cp /tmp/rainey_install.log $DATA/install.log 2>/dev/null
    rm -rf "$DL"
    exit 1
}
fetch() { curl -fsSL --retry 3 --connect-timeout 20 --cacert "$CA" -o "$2" "$1"; }
sha() { sha256sum "$1" 2>/dev/null | cut -d' ' -f1; }

echo "=== RaineyUI installer $(date)"
[ -f "$MANIFEST" ] && [ -x "$RAINEY" ] || fail "The installer folder is incomplete." "Copy the whole RaineyUI-Installer folder to the SD card again."

# 1. internet (a wrong clock breaks HTTPS, so say so)
status RUN 1 "Checking the internet connection" ""
CHECK=$(sed -n 's/^check|//p' "$MANIFEST")
if ! fetch "$CHECK" /dev/null; then
    if [ "$(date +%Y)" -lt 2025 ]; then
        fail "The Brick's clock is wrong ($(date +%Y-%m-%d))." "Set the date and time in Settings, then run the installer again."
    fi
    fail "No internet connection." "Connect to Wi-Fi in Settings → Network, then run the installer again."
fi

# 2. space: app + emulators need ~250 MB
FREE_MB=$(( $(df -k /mnt/UDISK | awk 'NR==2 {print $4}') / 1024 ))
[ "$FREE_MB" -ge 300 ] || fail "Not enough internal storage (${FREE_MB} MB free)." "RaineyUI needs about 300 MB. Delete some files and try again."

# 3. the app itself
status RUN 4 "Copying RaineyUI" "to internal storage"
mkdir -p "$DEST" "$DATA/bios/dc" "$DL" || fail "Could not write to internal storage." ""
rm -rf "$DEST/res"
cp -r "$HERE/payload/RaineyUI/." "$DEST/" || fail "Copying RaineyUI failed." "Is the SD card OK? Try copying the installer again."
chmod +x "$DEST/rainey" "$DEST/launch.sh" "$DEST/leds_watch.sh"
for d in FC SFC MD PS DC ARCADE GB GBA N64 NDS PSP; do mkdir -p /mnt/UDISK/Roms/$d; done
mkdir -p "$DEST/emus/ppsspp/.config/ppsspp/PSP/SAVEDATA" "$DEST/emus/ppsspp/.config/ppsspp/PSP/PPSSPP_STATE"

# 4. emulators: "file|<sha256>|<path>|<url>" (CrossMix-OS, verified) or "core|-|<path>|<url>" (buildbot)
TOTAL=$(grep -cE '^(file|core)[|]' "$MANIFEST")
N=0
while IFS='|' read -r KIND SUM REL URL; do
    case "$KIND" in file|core) ;; *) continue ;; esac
    N=$((N + 1))
    PCT=$((8 + 84 * N / TOTAL))
    OUT="$DEST/$REL"
    NAME=${REL##*/}
    mkdir -p "${OUT%/*}"
    if [ "$KIND" = file ]; then
        [ -f "$OUT" ] && [ "$(sha "$OUT")" = "$SUM" ] && continue
        status RUN $PCT "Downloading emulators ($N of $TOTAL)" "$NAME"
        : > "$DL/f"   # curl writes nothing for an empty file
        fetch "$URL" "$DL/f" || fail "Download failed: $NAME" "Check the Wi-Fi connection and run the installer again."
        [ "$(sha "$DL/f")" = "$SUM" ] || fail "A download was damaged: $NAME" "Run the installer again."
        mv "$DL/f" "$OUT" || fail "Could not save $NAME" ""
    else
        [ -f "$OUT" ] && continue
        status RUN $PCT "Downloading emulators ($N of $TOTAL)" "$NAME"
        rm -rf "$DL/core"
        fetch "$URL" "$DL/core.zip" || fail "Download failed: $NAME" "Check the Wi-Fi connection and run the installer again."
        "$RAINEY" --unzip "$DL/core.zip" "$DL/core" || fail "Could not unpack $NAME" "Run the installer again."
        "$RAINEY" --patch-core "$DL/core/$NAME" "$OUT" || fail "Could not prepare $NAME" ""
    fi
done < "$MANIFEST"
chmod +x "$DEST/emus/RetroArch/ra64.trimui" "$DEST/emus/drastic/"drastic* "$DEST/emus/ppsspp/PPSSPPSDL_gl" 2>/dev/null

# 5. LED lights: re-apply the chosen preset at boot and when the TrimUI menu restarts
status RUN 93 "Finishing up" "LED lights"
cp "$DEST/raineyleds.init" /etc/init.d/raineyleds && chmod +x /etc/init.d/raineyleds &&
    ln -sf ../init.d/raineyleds /etc/rc.d/S98zraineyleds && /etc/init.d/raineyleds start

# 6. swap file used while N64 and PSP games run (skipped when storage is tight)
if [ ! -f /mnt/UDISK/n64_swap ]; then
    FREE_MB=$(( $(df -k /mnt/UDISK | awk 'NR==2 {print $4}') / 1024 ))
    if [ "$FREE_MB" -ge 1200 ]; then
        status RUN 95 "Finishing up" "Setting aside memory for N64 and PSP games"
        dd if=/dev/zero of=/mnt/UDISK/n64_swap bs=1M count=512 2>/dev/null && mkswap /mnt/UDISK/n64_swap >/dev/null 2>&1 ||
            rm -f /mnt/UDISK/n64_swap
    fi
fi

rm -rf "$DL"
sync
cp /tmp/rainey_install.log $DATA/install.log 2>/dev/null
echo "=== done"
status DONE 100 "Games go in Roms/ or on the SD card." ""
