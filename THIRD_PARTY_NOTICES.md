# Third-party components

## Included in this repository

| Component | Where | License |
| --- | --- | --- |
| LibretroDroid (Swordfish90) | `libretrodroid/` | GPL-3.0 — <https://github.com/Swordfish90/LibretroDroid> |
| Oboe (Google) | `libretrodroid/src/main/cpp/oboe/` | Apache-2.0 — <https://github.com/google/oboe> |
| libretro-common | `libretrodroid/src/main/cpp/libretro/` | MIT-style — <https://github.com/libretro/libretro-common> |
| libretro API header | `trimui/third_party/libretro.h` | MIT — <https://github.com/libretro/RetroArch> |
| SDL2, SDL2_image, SDL2_ttf headers | `trimui/third_party/include/` | zlib — <https://libsdl.org> |
| miniz | `trimui/third_party/miniz/` | MIT — <https://github.com/richgel999/miniz> |
| stb_image_write (Sean Barrett) | `trimui/third_party/stb/` | Public domain / MIT — <https://github.com/nothings/stb> |
| DejaVu Sans Mono | `trimui/package/RaineyUI/res/DejaVuSans*.ttf` | Bitstream Vera / DejaVu license (`res/DejaVu-LICENSE.txt`) |

## Downloaded at build or install time (not stored here)

| Component | Used by | Source | License |
| --- | --- | --- | --- |
| libretro cores: FCEUmm, Snes9x, Genesis Plus GX, PCSX ReARMed, FinalBurn Neo, Gambatte, mGBA | TrimUI, Android | <https://buildbot.libretro.com> (source: <https://github.com/libretro>) | GPL-2.0/GPL-3.0 and non-commercial licenses per core (see each project) |
| RetroArch (`ra64.trimui`), Flycast, Mupen64Plus GLES2 + TrimUI configs | TrimUI | [CrossMix-OS](https://github.com/cizia64/CrossMix-OS) (pinned in `trimui/tools/install_emulators.sh`) | GPL-3.0 (RetroArch, Flycast, Mupen64Plus: GPL-2.0+) |
| PPSSPP 1.17.1 (SDL build for TrimUI) + assets and config | TrimUI | [CrossMix-OS](https://github.com/cizia64/CrossMix-OS) (source: <https://github.com/hrydgard/ppsspp>) | GPL-2.0+ |
| DraStic | TrimUI | [CrossMix-OS](https://github.com/cizia64/CrossMix-OS) — DraStic by Exophase | Freeware, closed source |
| SDL2 runtime libraries | TrimUI (linking) | the TrimUI firmware on the device | zlib |
| zig toolchain | TrimUI build | <https://ziglang.org> | MIT |

The SD-card installer (`RaineyUI-Installer-TrimUI-*.zip`) also contains the Mozilla CA certificate list
from <https://curl.se/docs/caextract.html> (`cacert.pem`, MPL-2.0), used to verify downloads on the
device, and RaineyUI's edited copies of the CrossMix-OS RetroArch and PPSSPP config files
(GPL-3.0, from the pinned commit above).

`trimui/tools/glibc_downgrade.py` (and its C port, `rainey --patch-core`) adjusts ELF symbol-version requirements of the downloaded
libretro cores so they load on the TrimUI firmware's glibc 2.33; it does not change their code.

RaineyUI does not include or download games, BIOS files or copyrighted artwork.
