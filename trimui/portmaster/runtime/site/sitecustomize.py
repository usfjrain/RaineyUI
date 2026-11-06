# RaineyUI: run PortMaster in its generic mode on the TrimUI Brick.
#
# PortMaster's own TrimUI mode copies scripts and pictures to the SD card (Roms/PORTS,
# Imgs/PORTS, Ports, Data/ports). RaineyUI never writes to the SD card, so when PortMaster
# loads its platform list, the "trimui" entry is replaced with a mode that keeps everything
# on internal storage (HM_TOOLS_DIR / HM_PORTS_DIR from env.sh). Python runs this file on
# start-up because it is on PYTHONPATH, so it keeps working after PortMaster updates itself.
import importlib.abc
import importlib.util
import os
import shutil
import sys
from pathlib import Path


def _patch(module):
    base = module.PlatformBase

    class PlatformRaineyUI(base):
        WANT_XBOX_FIX = False

        def first_run(self):
            self.portmaster_install([])

        def portmaster_install(self, bash_files):
            super().portmaster_install(bash_files)
            # a PortMaster update brings the stock control.txt and mod_TrimUI.txt (SD card
            # paths): put RaineyUI's back
            runtime = Path(os.environ.get("RAINEY_PM_CONTROL", "")).parent
            for name in ("control.txt", "mod_TrimUI.txt"):
                if (runtime / name).is_file():
                    shutil.copy(runtime / name, self.hm.tools_dir / "PortMaster" / name)

    module.PlatformRaineyUI = PlatformRaineyUI
    module.HM_PLATFORMS["trimui"] = PlatformRaineyUI


class _Finder(importlib.abc.MetaPathFinder):
    def find_spec(self, name, path, target=None):
        if name != "harbourmaster.platform":
            return None
        sys.meta_path.remove(self)
        try:
            spec = importlib.util.find_spec(name)
        finally:
            sys.meta_path.insert(0, self)
        if spec is None or spec.loader is None:
            return spec
        run = spec.loader.exec_module

        def exec_module(module):
            run(module)
            _patch(module)

        spec.loader.exec_module = exec_module
        return spec


sys.meta_path.insert(0, _Finder())
