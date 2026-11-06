#!/bin/bash
# Install PortMaster for RaineyUI on a TrimUI Brick over ADB (developer shortcut; users get it
# from the SD installer). Everything goes to internal storage:
#   /mnt/UDISK/Apps/PortMaster   PortMaster + RaineyUI's launcher, Python 3 and bash
#   /mnt/UDISK/ports             installed ports
# Same pinned downloads as tools/make_sd_installer.sh.
set -euo pipefail
HERE="$(cd "$(dirname "$0")/.." && pwd)"
CACHE="$HERE/../.local/pm/dl"
PM_URL=https://github.com/PortsMaster/PortMaster-GUI/releases/download/2026.11.02-0433/trimui.portmaster.zip
PY_URL=https://github.com/astral-sh/python-build-standalone/releases/download/20261026/cpython-3.11.18%2B20261026-aarch64-unknown-linux-gnu-install_only_stripped.tar.gz
BASH_URL=https://github.com/robxu9/bash-static/releases/download/5.2.015-1.2.3-2/bash-linux-aarch64
mkdir -p "$CACHE"
get() { local f="$CACHE/$(basename "${1//%2B/+}")"; [ -s "$f" ] || curl -fsSL -o "$f" "$1"; echo "$f"; }
PM_ZIP=$(get "$PM_URL"); PY_TGZ=$(get "$PY_URL"); BASH_BIN=$(get "$BASH_URL")

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
S="$WORK/PortMaster"
cp -R "$HERE/portmaster" "$S"
mkdir -p "$WORK/zip" "$S/runtime/bin"
unzip -q "$PM_ZIP" -d "$WORK/zip"
mv "$WORK/zip/Apps/PortMaster/PortMaster" "$S/PortMaster"
cp "$WORK/zip/Apps/PortMaster/icon.png" "$S/icon.png"
tar -xzf "$PY_TGZ" -C "$S/runtime"
L="$S/runtime/python/lib"
rm -rf "$S/runtime/python/include" "$S/runtime/python/share" "$L"/libtcl* "$L"/libtk* "$L"/tcl* "$L"/tk* \
    "$L"/itcl* "$L"/thread* "$L/python3.11/test" "$L/python3.11/idlelib" "$L/python3.11/tkinter" \
    "$L/python3.11/turtledemo" "$L/python3.11/ensurepip" "$L/python3.11/lib2to3" "$L"/python3.11/config-3.11-* \
    "$L"/python3.11/site-packages/pip* "$L"/python3.11/lib-dynload/_tkinter*
cp "$BASH_BIN" "$S/runtime/bin/bash"
chmod +x "$S/runtime/bin/bash" "$S/launch.sh"
find "$S" \( -name ".DS_Store" -o -name "._*" \) -delete
(cd "$WORK" && COPYFILE_DISABLE=1 tar -czf pm.tgz PortMaster)

adb get-state >/dev/null
adb push "$WORK/pm.tgz" /mnt/UDISK/pm_install.tgz
# keep an installed PortMaster (it updates itself); refresh RaineyUI's files and the runtimes
adb shell 'set -e; cd /mnt/UDISK; rm -rf pm_new; mkdir pm_new; tar -xzf pm_install.tgz -C pm_new; rm pm_install.tgz
  mkdir -p Apps/PortMaster ports
  [ -f Apps/PortMaster/PortMaster/pugwash ] && rm -rf pm_new/PortMaster/PortMaster
  rm -rf Apps/PortMaster/runtime/python
  cp -r pm_new/PortMaster/. Apps/PortMaster/; rm -rf pm_new
  chmod -R +x Apps/PortMaster/PortMaster Apps/PortMaster/runtime/python/bin Apps/PortMaster/runtime/bin; sync
  du -sh Apps/PortMaster'
echo "PortMaster installed to /mnt/UDISK/Apps/PortMaster (ports: /mnt/UDISK/ports)"
