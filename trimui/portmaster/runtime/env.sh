# Environment for PortMaster and ports on the TrimUI Brick (sourced by the PortMaster app
# and by RaineyUI when it starts a port). Everything is on internal storage.
PM_APP=/mnt/UDISK/Apps/PortMaster
export controlfolder=$PM_APP/PortMaster
export HM_TOOLS_DIR=$PM_APP
export HM_PORTS_DIR=/mnt/UDISK/ports
export HM_SCRIPTS_DIR=/mnt/UDISK/ports
export XDG_DATA_HOME=$PM_APP
export RAINEY_PM_CONTROL=$PM_APP/runtime/control.txt
export PATH=$PM_APP/runtime/bin:$PM_APP/runtime/python/bin:$controlfolder:$PATH
export PYTHONPATH=$PM_APP/runtime/site
export PYTHONDONTWRITEBYTECODE=1
export PYSDL2_DLL_PATH=/usr/trimui/lib
export LD_LIBRARY_PATH=/usr/trimui/lib:$LD_LIBRARY_PATH
export SDL_GAMECONTROLLERCONFIG_FILE=/usr/trimui/gamecontrollerdb.txt
export SSL_CERT_FILE=$controlfolder/exlibs/certifi/cacert.pem

mkdir -p "$HM_PORTS_DIR"
# PortMaster's control.txt and mod_TrimUI.txt (sourced by every port) use SD card paths, and
# PortMaster updates bring them back: always use RaineyUI's versions
for f in control.txt mod_TrimUI.txt; do
    [ -d "$controlfolder" ] && ! cmp -s "$PM_APP/runtime/$f" "$controlfolder/$f" &&
        cp -f "$PM_APP/runtime/$f" "$controlfolder/$f"
done
# ports are bash scripts and some run other scripts directly (#!/bin/bash); ports that ship a
# libretro core run /usr/bin/retroarch. Linked only when the firmware has none.
[ -e /bin/bash ] || ln -s $PM_APP/runtime/bin/bash /bin/bash 2>/dev/null
[ -e /usr/bin/retroarch ] || ln -s $PM_APP/runtime/bin/retroarch /usr/bin/retroarch 2>/dev/null
