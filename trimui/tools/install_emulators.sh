#!/bin/bash
# Download the emulators RaineyUI uses on TrimUI Brick / Smart Pro from their original sources,
# adapt them for the stock firmware, and install them over USB (adb).
#
#   ./install_emulators.sh                 download + install to the connected device
#   ./install_emulators.sh --stage DIR     only download/prepare into DIR (no device needed)
#
# Sources:
#   libretro cores    https://buildbot.libretro.com/nightly/linux/aarch64/latest/   (GPL etc.)
#   RetroArch, Flycast, Mupen64Plus GLES2, DraStic, PPSSPP and their TrimUI configs:
#                     https://github.com/cizia64/CrossMix-OS (GPL-3.0), pinned commit below
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
CROSSMIX_SHA=206d612fdef83972ed54a17de955125d0c0b3528
RAW="https://raw.githubusercontent.com/cizia64/CrossMix-OS/$CROSSMIX_SHA"
BUILDBOT=https://buildbot.libretro.com/nightly/linux/aarch64/latest
CORES="fceumm snes9x genesis_plus_gx pcsx_rearmed fbneo gambatte mgba"
DEST=/mnt/UDISK/Apps/RaineyUI

STAGE=""
if [ "${1:-}" = "--stage" ]; then STAGE="${2:?--stage needs a directory}"; fi
WORK="${STAGE:-$(mktemp -d)}"
mkdir -p "$WORK"
PY="$(command -v python3)" || { echo "python3 is required"; exit 1; }
PATCHER="$HERE/glibc_downgrade.py"
[ -f "$PATCHER" ] || PATCHER="$HERE/tools/glibc_downgrade.py"

enc() { "$PY" -c 'import urllib.parse,sys;print(urllib.parse.quote(sys.argv[1]))' "$1"; }
get() { curl -fsSL --retry 3 -o "$2" "$1"; }

echo "==> libretro cores (buildbot nightlies, patched for glibc 2.33)"
mkdir -p "$WORK/dl" "$WORK/RaineyUI/cores"
for c in $CORES; do
  echo "    $c"
  get "$BUILDBOT/${c}_libretro.so.zip" "$WORK/dl/$c.zip"
  unzip -oq "$WORK/dl/$c.zip" -d "$WORK/dl"
  "$PY" -I "$PATCHER" "$WORK/dl/${c}_libretro.so" "$WORK/RaineyUI/cores/${c}_libretro.so" >/dev/null
done

echo "==> RetroArch + N64 / Dreamcast cores (CrossMix-OS ${CROSSMIX_SHA:0:7})"
RA="$WORK/RaineyUI/emus/RetroArch"
mkdir -p "$RA/.retroarch/cores" "$RA/.retroarch/autoconfig/sdl2" \
         "$RA/.retroarch/config/Flycast" "$RA/.retroarch/config/Mupen64Plus GLES2" \
         "$RA/.retroarch/config/remaps/Flycast"
get "$RAW/RetroArch/ra64.trimui" "$RA/ra64.trimui"
get "$RAW/RetroArch/retroarch.cfg" "$WORK/dl/retroarch.cfg"
for c in flycast_libretro.so mupen64plus_libretro.so; do
  get "$RAW/RetroArch/.retroarch/cores/$c" "$RA/.retroarch/cores/$c"
done
get "$RAW/RetroArch/.retroarch/autoconfig/sdl2/$(enc 'X360 Controller.cfg')" \
    "$RA/.retroarch/autoconfig/sdl2/X360 Controller.cfg"
for f in "Flycast/DC.cfg" "Flycast/DC.opt" "Mupen64Plus GLES2/N64.cfg" "Mupen64Plus GLES2/Mupen64Plus GLES2.opt" \
         "remaps/Flycast/Flycast.rmp"; do
  get "$RAW/RetroArch/.retroarch/config/$(enc "$f")" "$RA/.retroarch/config/$f"
done
# Point RetroArch at RaineyUI's folders on internal storage and drop CrossMix-only bits
"$PY" - "$WORK/dl/retroarch.cfg" "$RA/retroarch.cfg" "$RA/.retroarch/config" <<'EOF'
import re, sys, pathlib
src, dst, cfgdir = sys.argv[1:4]
s = open(src).read()
ov = {
  'system_directory': '/mnt/UDISK/RaineyUI/bios',
  'savefile_directory': '/mnt/UDISK/RaineyUI/saves/retroarch',
  'savestate_directory': '/mnt/UDISK/RaineyUI/states/retroarch',
  'rgui_browser_directory': '/mnt/UDISK/Roms',
  'screenshot_directory': '/mnt/UDISK/RaineyUI/screenshots',
  'menu_driver': 'rgui', 'input_overlay_enable': 'false',
  'input_overlay_enable_autopreferred': 'false', 'savestate_auto_load': 'false',
  'savestate_auto_save': 'false', 'config_save_on_exit': 'false', 'video_fullscreen': 'true',
  'menu_show_online_updater': 'false', 'menu_show_core_updater': 'false',
  'notification_show_autoconfig': 'false', 'video_font_enable': 'true',
  'network_cmd_enable': 'false',
}
for k, v in ov.items():
    pat = re.compile(r'^%s = ".*"$' % re.escape(k), re.M)
    line = '%s = "%s"' % (k, v)
    s = pat.sub(line, s) if pat.search(s) else s + line + '\n'
open(dst, 'w').write(s)
for p in pathlib.Path(cfgdir).rglob('*.cfg'):  # overlays aren't installed
    t = [l for l in p.read_text().splitlines() if 'input_overlay' not in l]
    p.write_text('\n'.join(t + ['input_overlay_enable = "false"']) + '\n')
EOF

echo "==> DraStic (Nintendo DS)"
curl -fsSL "https://api.github.com/repos/cizia64/CrossMix-OS/git/trees/$CROSSMIX_SHA:Emus/NDS/drastic?recursive=1" \
  | "$PY" -c "import json,sys; [print(x['path']) for x in json.load(sys.stdin)['tree'] if x['type']=='blob']" \
  > "$WORK/dl/drastic_files.txt"
while IFS= read -r f; do
  mkdir -p "$WORK/RaineyUI/emus/drastic/$(dirname "$f")"
  get "$RAW/Emus/NDS/drastic/$(enc "$f")" "$WORK/RaineyUI/emus/drastic/$f"
done < "$WORK/dl/drastic_files.txt"
echo "==> PPSSPP 1.17.1 (PSP, OpenGL ES build)"
PSP_SRC=Emus/PSP/PPSSPP_1.17.1
PSP="$WORK/RaineyUI/emus/ppsspp"
PSP_SYS="$PSP/.config/ppsspp/PSP/SYSTEM"
curl -fsSL "https://api.github.com/repos/cizia64/CrossMix-OS/git/trees/$CROSSMIX_SHA:$PSP_SRC/assets?recursive=1" \
  | "$PY" -c "import json,sys; [print(x['path']) for x in json.load(sys.stdin)['tree'] if x['type']=='blob']" \
  > "$WORK/dl/ppsspp_files.txt"
while IFS= read -r f; do
  mkdir -p "$PSP/assets/$(dirname "$f")"
  get "$RAW/$PSP_SRC/assets/$(enc "$f")" "$PSP/assets/$f"
done < "$WORK/dl/ppsspp_files.txt"
get "$RAW/$PSP_SRC/PPSSPPSDL_gl" "$PSP/PPSSPPSDL_gl"
mkdir -p "$PSP_SYS" "$PSP/.config/ppsspp/PSP/SAVEDATA" "$PSP/.config/ppsspp/PSP/PPSSPP_STATE"
for f in ppsspp.ini controls.ini; do get "$RAW/$PSP_SRC/.config/ppsspp/PSP/SYSTEM/$f" "$PSP_SYS/$f"; done
# RaineyUI's PSP folder, OpenGL backend; challenge-mode achievements would block save states
sed -i.bak -e 's|^CurrentDirectory = .*|CurrentDirectory = /mnt/UDISK/Roms/PSP|' \
    -e 's|^GraphicsBackend = .*|GraphicsBackend = 0 (OPENGL)|' \
    -e 's|^AchievementsEnable = .*|AchievementsEnable = False|' \
    -e 's|^AchievementsChallengeMode = .*|AchievementsChallengeMode = False|' "$PSP_SYS/ppsspp.ini"
rm -f "$PSP_SYS/ppsspp.ini.bak"

chmod +x "$RA/ra64.trimui" "$WORK/RaineyUI/emus/drastic/"drastic* "$PSP/PPSSPPSDL_gl"

if [ -n "$STAGE" ]; then
  echo "Staged in $WORK/RaineyUI (cores/ and emus/)."
  exit 0
fi

echo "==> Installing to the device ($DEST)"
command -v adb >/dev/null || { echo "adb not found (install Android platform-tools)"; exit 1; }
[ "$(adb get-state 2>/dev/null)" = "device" ] || { echo "No device: connect the Brick over USB with ADB enabled"; exit 1; }
adb shell "mkdir -p $DEST/cores $DEST/emus /mnt/UDISK/RaineyUI/bios/dc"
adb push "$WORK/RaineyUI/cores/." "$DEST/cores/"
adb push "$WORK/RaineyUI/emus/RetroArch" "$DEST/emus/"
adb push "$WORK/RaineyUI/emus/drastic" "$DEST/emus/"
adb push "$WORK/RaineyUI/emus/ppsspp" "$DEST/emus/"
adb shell "mkdir -p /mnt/UDISK/Roms/PSP; chmod +x $DEST/emus/RetroArch/ra64.trimui $DEST/emus/drastic/drastic* $DEST/emus/ppsspp/PPSSPPSDL_gl; sync"
rm -rf "$WORK"
echo "Done. Emulators installed."
