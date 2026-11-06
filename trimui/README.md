# RaineyUI for TrimUI Brick / Smart Pro

Native SDL2 port of the Rainey frontend for TrimUI's stock Linux firmware
(TinaLinux, aarch64, glibc 2.33, 1024x768, SDL2 2.30 on the Mali/PowerVR GLES2 driver).
The Android app can't run there, so this port reimplements the same UI in C
(Home carousel, Recently Played, Library grid/list, Detail, Library collections,
Settings, and the in-game *Rainey Options* menu) with a built-in libretro player.

## Install location (internal storage)

| What | Path on device |
| --- | --- |
| App (shows in MainUI → Apps) | `/mnt/UDISK/Apps/RaineyUI/` |
| ROMs | `/mnt/UDISK/Roms/<FC SFC MD PS DC ARCADE GB GBA N64 NDS PSP>/` (also scans `/mnt/SDCARD/Roms/` if an SD card is inserted) |
| BIOS / system files | `/mnt/UDISK/RaineyUI/bios/` (Dreamcast: `bios/dc/dc_boot.bin` + `dc_flash.bin`, optional) |
| Saves (.srm) / states | `/mnt/UDISK/RaineyUI/saves/<system>/`, `/mnt/UDISK/RaineyUI/states/<system>/` |
| Box art (optional) | `<rom folder>/Imgs/<game>.png` or `/mnt/UDISK/Imgs/<FOLDER>/<game>.png` |
| Core option overrides | `/mnt/UDISK/RaineyUI/options/<core>.opt` (`key = "value"`) |
| Log | `/mnt/UDISK/RaineyUI/rainey.log` |
| Extra ROM folders | `/mnt/UDISK/RaineyUI/rom_folders.txt` (written by *Settings → ROM folders*) |
| PortMaster | `/mnt/UDISK/Apps/PortMaster/` (also in MainUI → Apps); ports in `/mnt/UDISK/ports/` |

Copy ROMs with `adb push game.zip /mnt/UDISK/Roms/SFC/`, then *Settings → Rescan games* (or SELECT in a library).

## Systems and emulators

Built-in RaineyUI player (libretro, software rendering, *Rainey Options* menu):
NES (fceumm), SNES (snes9x), Genesis/Master System/Game Gear (genesis_plus_gx),
PlayStation (pcsx_rearmed, HLE BIOS; real `scph*.bin` in `bios/` improves compatibility),
Arcade/Neo Geo (fbneo), Game Boy/Color (gambatte), Game Boy Advance (mgba).
These are libretro buildbot nightlies patched by `tools/glibc_downgrade.py` so they load on
glibc 2.33 / GCC 10 libstdc++.

Handed off to external emulators (GPU-accelerated / heavier systems), taken from
[CrossMix-OS](https://github.com/cizia64/CrossMix-OS), which builds them for this hardware:

| System | Emulator | Notes |
| --- | --- | --- |
| Nintendo 64 | RetroArch (`ra64.trimui`) + Mupen64Plus GLES2 | 512 MB `/mnt/UDISK/n64_swap` is enabled while playing |
| Dreamcast | RetroArch + Flycast (GLES) | uses `bios/dc/dc_boot.bin` if present, otherwise Flycast HLE BIOS |
| Nintendo DS | DraStic r2.5.2.2 | free DraStic BIOS built in; saves in `emus/drastic/backup` |
| PSP | PPSSPP 1.17.1 (SDL, OpenGL ES) | `.iso` / `.cso` / `.pbp`; saves in `emus/ppsspp/.config/ppsspp/PSP/SAVEDATA` (saves found in an SD card's `psp/SAVEDATA` are copied there, the card is not changed) |

They live in `emus/` inside the app. RetroArch config: `emus/RetroArch/retroarch.cfg`
(core options: `.retroarch/config/Mupen64Plus GLES2/Mupen64Plus GLES2.opt`; Dreamcast uses
`Flycast/DC.opt` as a template and *Settings → Dreamcast resolution* picks the internal resolution).
PPSSPP config: `emus/ppsspp/.config/ppsspp/PSP/SYSTEM/ppsspp.ini`. Android Games from the Android build are not included.

## Controls

UI: D-pad move, A select, B back, L/R page or switch system, START settings, Y favorite, X grid/list.
In game (built-in systems): MENU (or SELECT+START) opens Rainey Options: resume, reset,
save/load state (L/R picks slot), video scale, scanlines, quit.
N64 / Dreamcast (RetroArch): MENU opens the TrimUI in-game menu (save, load, exit); MENU+START quits.
DS (DraStic) and PSP (PPSSPP): MENU opens the emulator's menu; choose Exit to return to RaineyUI. If A/B feel swapped, set *Settings → Button layout → Xbox*.

## SD cards

An inserted card is mounted by the firmware at `/mnt/SDCARD` (FAT32/exFAT, no formatting).
RaineyUI only **reads** from it:

- games in the TrimUI layout (`Roms/FC`, `Roms/SFC`, …) and in the EmulationStation / Batocera /
  Anbernic layout (`nes`, `snes`, `megadrive`, `mastersystem`, `gamegear`, `psx`, `dreamcast`,
  `n64`, `nds`, `psp`, `gb`, `gbc`, `gba`, `arcade`, `fbneo`, `neogeo`, `cps1-3`, `mame` at the card root);
  `images/`, `manuals/`, `videos/` folders are skipped
- box art from `images/<game>.png|jpg` (or `-image` suffix), `boxart/`, `Imgs/<FOLDER>/`
- a `.srm` save next to a game is used on first play; new saves go to internal storage
- BIOS files the emulators need (PS1 `scph*`, Dreamcast `dc_boot.bin`/`dc_flash.bin`, `gba_bios.bin`,
  `neogeo.zip`) are copied from the card's `bios/` folder to `/mnt/UDISK/RaineyUI/bios` when missing,
  because emulators write VMU/flash data next to their BIOS
- a game that exists both internally and on the card is listed once (the complete / internal copy)
- `.gdi`/`.cue` discs whose track files are missing are marked "incomplete" and won't launch

## ROM folders (Settings → ROM folders)

*Add a folder* opens a folder browser (SD card or internal storage); X picks the folder you are
in. The folder is only read, nothing on it is moved or changed. With **Auto** (the default):

- a folder named after a system (`snes`, `SFC`, `psx`, `Dreamcast`, …) is read as that system
- other game files get their system from the file type; `.iso`, `.pbp` and `.chd` are opened to
  tell PSP from PS1 and Dreamcast from PS1, and `.zip` files are looked inside
- folders named after systems RaineyUI can't play (`saturn`, `ps2`, `3do`, `amiga`, …) are skipped,
  and `.zip` files with no recognisable game inside (arcade sets) are only read from arcade-named
  folders (`arcade`, `mame`, `fbneo`, `neogeo`, `cps1-3`)

← → on a folder sets its system instead (every supported file type of that system, two folder
levels deep). X removes it from the list. Up to 32 folders; a folder on a card that isn't
inserted is shown as "not found".

## Ports (PortMaster)

The SD installer also installs [PortMaster](https://portmaster.games) to
`/mnt/UDISK/Apps/PortMaster`. Open it from the **Ports** system (START) or from MainUI → Apps,
download ports, and they appear under Ports in RaineyUI with their pictures.

PortMaster's own TrimUI setup writes to the SD card (`Roms/PORTS`, `Imgs/PORTS`, `Data/ports`),
so RaineyUI runs it differently, keeping everything on internal storage:

- `portmaster/launch.sh` + `runtime/env.sh`: paths (`HM_TOOLS_DIR`, `HM_PORTS_DIR=/mnt/UDISK/ports`),
  `runtime/control.txt` (RaineyUI's version of PortMaster's TrimUI `control.txt`: `directory=mnt/UDISK`)
- `runtime/mod_TrimUI.txt`: replaces PortMaster's file of the same name, which every port runs
  and which puts `HOME` (where ports mount their save folders) on the SD card; RaineyUI's puts it
  in `/mnt/UDISK/ports/.home`
- `runtime/site/sitecustomize.py`: swaps PortMaster's TrimUI platform for one that never touches
  the SD card, and restores the two files above when PortMaster updates itself (`env.sh` also
  restores them before every launch)
- `runtime/bin/`: `pkill`/`pgrep` (missing from the firmware's BusyBox; ports use them to stop
  `gptokeyb` on exit) and `retroarch`, which runs RaineyUI's RetroArch for ports that ship a
  libretro core; it is linked to `/usr/bin/retroarch` when the firmware has none
- the stock firmware has no Python or bash, so the installer downloads a standalone Python 3.11
  ([python-build-standalone](https://github.com/astral-sh/python-build-standalone), glibc 2.17)
  for PortMaster's menu and a static bash ([bash-static](https://github.com/robxu9/bash-static))
  for the ports; `/bin/bash` is linked to it the first time PortMaster or a port runs
- ports are started by `launch.sh` (`bash <port>.sh` with `env.sh`); ports found on an SD card are
  not listed, because they would save to the card
- in a port, SELECT+START quits back to RaineyUI (PortMaster's `gptokeyb` hotkey)

Tested on a Brick (firmware 1.1.1): PortMaster's menu, installing ports, OpenTyrian (native
SDL2) and 2048 (libretro core) started from RaineyUI, and nothing written to the SD card.

Developer install over ADB: `tools/install_portmaster.sh`.

## Moving files from your computer

Everything you send lands in `/mnt/UDISK/RaineyUI/inbox` and is sorted automatically
(`rainey --sort-inbox`): games by file type into `Roms/<system>` (zips are peeked into; `.chd` is
told apart as Dreamcast or PS1; `.gdi`/`.cue` discs keep their tracks together in their own folder),
BIOS files into `bios/` (`dc_boot.bin`/`dc_flash.bin` into `bios/dc/`), pictures into
`Imgs/<system>/` when the name matches a game (box art) or `backgrounds/` otherwise, and anything
unknown into `/mnt/UDISK/Downloads`. A running RaineyUI rescans by itself.

**USB (fast, ~4.5 MB/s): Send to Brick app.** `mac/Send to Brick.app` (a copy is on the Desktop):
drag files or folders onto its icon with the Brick plugged in. Dropping a `.gdi`/`.cue` also sends
its track files. Build it with `mac/build_app.sh`.

**Wi-Fi (any computer or phone): Settings → Wi-Fi transfer.** Shows an address like
`http://192.168.4.250`; open it in a browser on the same network and drag files in. The server
only runs while that screen is open (it also turns Wi-Fi power-saving off meanwhile). The Brick's
Wi-Fi is 2.4 GHz only, so this is best for small files (measured ~0.25 MB/s on a busy channel);
use USB for disc games.

## Backgrounds

*Settings → Background* picks the menu background (applies instantly); *Background brightness*
(Dim / Normal / Bright) sets how strongly it shows behind the menus.

Built in: Neon grid (no image), Rainey, Rainey (animated), Lightning (animated) — the animated ones
are the project's `background 2.mp4` / `background.mp4` converted to 15 fps JPEG frames in
`res/backgrounds/<name>/` (decoded on a background thread).

Your own: copy images (`.png`, `.jpg`) into `/mnt/UDISK/RaineyUI/backgrounds/`; the file name is
the display name. For an animated one, make a folder there holding numbered frames
(`f0001.jpg`, `f0002.jpg`, …) and optionally `fps.txt` (default 15). To convert a video on the Mac:

```sh
mkdir "My Clip" && ffmpeg -i clip.mp4 -vf "fps=15,scale=640:-2" -q:v 6 "My Clip/f%04d.jpg"
adb push "My Clip" /mnt/UDISK/RaineyUI/backgrounds/
```

## LED lights

*Settings → LED lights* controls the Brick's six LED zones (Left, Right, Middle, F1, F2, Rear)
through the firmware's `/sys/class/led_anim` driver:

- per light: color (24-color palette or custom RGB), animation (Off, Static, Breath, Pulse,
  Sniff, Blink 1-3) and speed (0.3-5 s); SELECT copies one light's settings to all
- master on/off and brightness (driver limit 60 = 100%)
- 5 presets: Y saves the current lights to the selected slot, A loads it, X clears it
- changes show on the device live and are saved to `/mnt/UDISK/RaineyUI/leds.cfg`

TrimUI's MainUI switches the LEDs off whenever it starts if its own "LED switch" setting is off,
so a small boot hook keeps your preset applied: `/etc/init.d/raineyleds` (linked as
`/etc/rc.d/S98zraineyleds`) starts `leds_watch.sh`, which re-applies the preset (`rainey --leds-apply`)
at power-on and each time MainUI restarts. To remove it:
`rm /etc/rc.d/S98zraineyleds /etc/init.d/raineyleds` and reboot.

## Screen Recorder app

`recorder/ScreenRecorder` is a second app (Apps → Screen Recorder, same `rainey` binary):

- Turn the recorder on, then press **SELECT + L1 + R1** in any game or app to start and stop.
  The LEDs pulse red while recording. Video only (no sound), 1024x768 or 512x384 at 15/20/30 fps,
  saved as Motion-JPEG `.mov` (plays in QuickTime, VLC, phones) in `/mnt/UDISK/Recordings`
  (~65 MB per minute at 1024x768 / 20 fps; it stops by itself when storage runs low).
- The service is `rainey --rec-daemon` (screen from `/dev/fb0`, the buffer on display), started at
  boot by `/etc/init.d/raineyrec` while it's switched on. Log: `/mnt/UDISK/RaineyUI/recorder.log`.
- File browser for Recordings, internal storage and the SD card: play recordings, view pictures,
  delete files.

Install over adb: `recorder/install_recorder.sh`.

## Install (from a release)

**Easiest — SD card, no computer tools:** unzip `RaineyUI-Installer-TrimUI-<version>.zip` to the
root of the SD card (its `Apps` folder merges with the card's), put the card in the Brick, connect
to Wi-Fi and open **Apps → Install RaineyUI**. The installer (`installer/`, run by
`rainey --install-ui`) copies the app to internal storage, downloads the emulators listed in its
`manifest.txt` from their original projects (CrossMix-OS files are checked against SHA-256 hashes
recorded at build time; libretro cores are unzipped and patched on the device with
`rainey --unzip` / `rainey --patch-core`), installs the LED hook and a swap file, and shows the
progress. Running it again updates RaineyUI and only downloads what changed. Log:
`/mnt/UDISK/RaineyUI/install.log`. Build it with `tools/make_sd_installer.sh <version>`.

**From a computer over USB:** unzip `RaineyUI-TrimUI-<version>.zip`, connect the Brick over USB (ADB on) and run `./install.sh`
(needs `adb`, `python3`, `curl`). It copies the app to `/mnt/UDISK/Apps/RaineyUI`, installs the
LED-lights boot hook, and runs `install_emulators.sh`, which downloads the emulators from their
original sources (libretro buildbot, CrossMix-OS at a pinned commit) and installs them.
`./install.sh --app-only` skips the emulator download.

## Build & deploy (macOS)

Requires the zig toolchain at `~/.local/share/rainey-toolchain/zig-macos-x86_64-0.13.0/zig`
(or set `ZIG=`), and the device connected over ADB.

```sh
./build.sh            # cross-compile to package/RaineyUI/rainey
./deploy.sh --run     # push to /mnt/UDISK/Apps/RaineyUI and launch it
./deploy.sh --full    # also downloads + installs the emulators (tools/install_emulators.sh)
```

`devlibs/` (created on first build) holds the SDL2 libraries pulled from the device over adb, used
only for linking; `third_party/` holds the SDL headers, libretro.h and miniz.

Debugging: `adb shell 'echo "down a shot:/tmp/s.png" > /tmp/rainey_input'` injects buttons and
saves a screenshot.
