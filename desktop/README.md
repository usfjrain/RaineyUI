# Rainey Desktop for Mac

This is a lightweight macOS desktop build of Rainey for the local MacBook Pro:

- MacBookPro12,1
- Dual-core Intel Core i5 2.9 GHz
- 8 GB RAM
- Intel Iris Graphics 6100
- macOS 12.7.6

Rainey Desktop is a launcher and library manager. It does not bundle emulators, BIOS files, ROMs, or copyrighted artwork.

## Systems

Great targets on this Mac:

- NES, SNES, Genesis, TurboGrafx-16, Neo Geo, PlayStation, Game Boy Advance

Good targets with the right emulator:

- Sega Saturn, Nintendo 64, Nintendo DS, Dreamcast, PSP, Arcade

Heavy or experimental targets:

- GameCube, Wii, PlayStation 2, Xbox, Xbox 360, PlayStation 3, Nintendo 3DS

Xbox 360 and PS3 are included so the frontend can organize and launch them, but this Mac's Intel Iris 6100 graphics and 8 GB RAM make them experimental. Expect external emulator compatibility and performance to vary.

## Build

From the repository root:

```bash
clang desktop/RaineyMac/Sources/main.m \
  -o desktop/build/Rainey.app/Contents/MacOS/Rainey \
  -framework Cocoa \
  -framework WebKit \
  -fobjc-arc
```

The app bundle is generated at:

```text
desktop/build/Rainey.app
```

## Run

Open the app:

```bash
open desktop/build/Rainey.app
```

If macOS blocks the unsigned app, right-click `Rainey.app` and choose Open.

## Usage

1. Pick a system.
2. Choose `Pick Folder` or `Load Content`.
3. Rainey scans matching file extensions recursively.
4. Open a game detail page and press `Play`.
5. Rainey launches through RetroArch if enabled and configured, otherwise macOS opens the ROM with the default app for that file type.

Install and configure emulator apps separately, then associate ROM file types with the emulator you prefer. Examples include OpenEmu, Dolphin, PPSSPP, PCSX2, RPCS3, Xemu, and other macOS emulators.

## RetroArch Support

Rainey Desktop can optionally launch games through RetroArch for macOS.

In Settings:

- Enable `Use RetroArch first`.
- Choose `RetroArch.app`.
- Set a core for each system by typing a core name/path or choosing a local `.dylib` core.
- Keep `Fallback launcher` enabled if you want Rainey to open the ROM with the default macOS app when RetroArch or a core is missing.

Rainey launches RetroArch with the standard command shape:

```text
RetroArch -L /path/to/core.dylib /path/to/game
```

Rainey does not bundle RetroArch, cores, BIOS files, or ROMs.

## Data

The desktop library is saved locally at:

```text
~/Library/Application Support/Rainey Desktop/library.json
```
