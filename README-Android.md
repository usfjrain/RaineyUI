# Rainey for Android

Rainey is a native Android retro gaming frontend for Android 13+ devices, designed around Orange Pi 5 landscape TV and arcade setups.

It is a launcher and library manager only. It does not include emulator cores, BIOS files, ROMs, or copyrighted artwork.

## Features

- Kotlin Android app with Jetpack Compose
- Android 13 minimum SDK
- Landscape-first arcade UI
- Gamepad, keyboard, and basic touch navigation
- Optional native-emulator touch controls overlay
- Controller layout setting for Nintendo-style or Xbox-style A/B/X/Y mapping
- Storage Access Framework folder picking with persisted URI permissions
- Android Games category for installed Android apps marked as games
- Room database for systems, ROM folders, games, play history, and favorites
- DataStore settings for theme and scanline preference
- Standalone native libretro runtime integration
- Bundled arm64 SNES test core for Android 14 / Orange Pi 5 validation
- Optional RetroArch app launch mode with configurable package name and per-system core names/paths
- External emulator launch abstraction with placeholder package mappings as fallback
- Deterministic placeholder box art generator that keeps the first version functional
- Animated splash screen, smooth system carousel, controller focus glow, scanline toggle, recently played row, favorites row, and friendly empty/error states

## Build

1. Run `tools/fetch_android_cores.sh` once (downloads the bundled arm64 libretro cores; they are not stored in git).
1. Open this folder in Android Studio.
2. Install Android SDK 35 or newer.
3. Let Gradle sync.
4. Run the `app` configuration on an Android 13+ device.

Command-line build after Android SDK setup:

```bash
./gradlew assembleDebug
```

The debug APK is currently filtered to `arm64-v8a`, matching Orange Pi 5 and most modern Android 14 phones. This keeps the native emulator build smaller and avoids unnecessary x86 native builds.

The debug APK includes an arm64 SNES libretro core so SNES can be tested without picking a cores folder first. Other systems still require user-provided cores until additional compatible cores are intentionally added.

This repository does not check in a Gradle wrapper JAR. Android Studio can create one with `gradle wrapper` if your local Gradle installation is available.

## Mac Desktop Build

Rainey also includes a lightweight macOS desktop launcher build in `desktop/`.

This local build is tuned for the detected MacBook Pro 12,1 with a dual-core Intel Core i5, 8 GB RAM, Intel Iris Graphics 6100, and macOS 12.7.6. It adds launcher categories for GameCube, Wii, PlayStation 2, Xbox, Xbox 360, PlayStation 3, Nintendo DS, Nintendo 3DS, Sega Saturn, Neo Geo, and TurboGrafx-16 alongside the existing systems.

Build the desktop app:

```bash
clang desktop/RaineyMac/Sources/main.m \
  -o desktop/build/Rainey.app/Contents/MacOS/Rainey \
  -framework Cocoa \
  -framework WebKit \
  -fobjc-arc
```

Run it:

```bash
open desktop/build/Rainey.app
```

Rainey Desktop scans user-picked folders recursively, saves its library at `~/Library/Application Support/Rainey Desktop/library.json`, and opens games with the default macOS app for each ROM file type. It does not bundle emulators, BIOS files, ROMs, cores, or copyrighted art.

The desktop build also includes optional RetroArch support. In Settings, enable RetroArch, choose `RetroArch.app`, and set a per-system `.dylib` core name or path. If RetroArch or a core is missing, Rainey can show a friendly message and fall back to the default macOS launcher.

## Orange Pi 5 Notes

- Use Android 14 or newer.
- Run in landscape on a TV or monitor.
- Pair or connect a controller before launching.
- Use Settings to pick a ROM folder per system. Android will grant Rainey persisted access to those folders through the Storage Access Framework.

## Install and Test on Orange Pi 5 Android 13+

1. Build a debug APK from Android Studio or with `./gradlew assembleDebug`.
2. Copy `app/build/outputs/apk/debug/app-debug.apk` to the Orange Pi 5, or install over ADB:

```bash
adb connect ORANGE_PI_IP_ADDRESS:5555
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

### RG406V launcher button lock

Android can clear the selected Home app and accessibility key listener after an APK reinstall or some launcher/default-app changes. Rainey cannot permanently force those protected settings from inside the app, but the project includes a device-side ADB lock script for this RG406V setup:

```bash
./tools/lock_rainey_button.sh
```

For local development, use this instead of a plain APK install:

```bash
./tools/install_debug_and_lock.sh
```

That script installs the debug APK, disables `com.ylm.launcher`, sets `com.rainey.frontend/.HomeToggleActivity` as the Home handler, and re-enables `RaineyLauncherKeyService` so the hardware launcher button opens Rainey.

3. Launch Rainey from the Android launcher or TV launcher row.
4. Connect a controller and verify D-pad, A/Enter, B/Back, Start, and shoulder navigation.
5. Open Settings, choose a ROM folder for one system, then run Scan Games.
6. To add one ROM without scanning a folder, open a system Library and use Load Content.
7. For SNES native playback, the included `snes9x_libretro_android.so` core is used automatically. For other systems, pick a user-provided libretro cores folder in Settings and set a core name for the system.
8. Select a scanned or manually loaded game and press Play. Rainey tries native libretro playback first, then RetroArch app launch if enabled, then external emulator package launch. If nothing is configured or installed, Rainey shows a friendly error instead of failing silently.

Rainey only receives folder access through Android's Storage Access Framework. If games do not appear, re-pick the folder from Settings and make sure the files use supported extensions.

## Controls

- D-pad / left stick / arrow keys: move focus
- A / Enter / Center: select
- B / Back / Escape: back
- Start: settings
- Shoulder buttons: switch systems on the Home screen

## Native Gameplay Controls

Settings includes a Controls section:

- Touch overlay: shows or hides an on-screen D-pad, face buttons, L/R, Start, and Select during native gameplay.
- Opacity: adjusts touch overlay transparency.
- Button layout: choose Nintendo for direct RetroPad labels, or Xbox to swap A/B and X/Y for Xbox-style physical controllers.

Inside the native emulator, Android Back or Escape exits the game. Controller B is reserved for gameplay.

## ROM Extensions

- NES: `.nes`, `.zip`
- SNES: `.sfc`, `.smc`, `.zip`
- Genesis: `.gen`, `.md`, `.bin`, `.zip`
- PlayStation: `.cue`, `.bin`, `.chd`, `.iso`
- PSP: `.iso`, `.cso`
- Dreamcast: `.gdi`, `.chd`, `.cdi`
- Arcade: `.zip`, `.7z`
- Game Boy Advance: `.gba`, `.zip`
- Nintendo 64: `.z64`, `.n64`, `.v64`, `.zip`
- Android Games: installed Android launcher apps marked by Android as games

## Emulator Launching

Rainey uses a layered launch flow:

- `NativeEmulatorLauncher` starts a built-in LibretroDroid activity using bundled or user-provided `.so` libretro cores and optional user-provided BIOS/system files.
- `RetroArchLauncher` can hand off to an installed RetroArch app when enabled and configured.
- `ExternalIntentLauncher` sends a content URI to an installed emulator package when one is configured for the selected system.

RetroArch app support is optional. Enable it in Settings, enter the installed RetroArch package name, and set a core name or path for each system. If RetroArch is missing or not configured, Rainey shows a friendly message and attempts the next available fallback where possible.

Rainey does not bundle RetroArch, libretro cores, BIOS files, ROMs, or copyrighted artwork. Use Settings to pick user-provided folders for libretro cores and BIOS/system files. Rainey persists read access with Android's Storage Access Framework and validates configured core names against the selected cores folder where possible.

Native playback is powered by LibretroDroid, which is GPL-licensed. If you redistribute Rainey with this module, comply with the GPL license terms and provide corresponding source and notices for the native runtime.

For native playback, Rainey copies the selected ROM from Storage Access Framework into private app storage before launch. Cartridge `.zip` files are unpacked to the first supported ROM inside, while Arcade `.zip` sets remain zipped. This helps cores that need a real path or do not include ZIP support.

## TODO

- Add editable emulator package/activity mappings in Settings.
- Add per-game launch overrides.
- Add user-provided artwork import and local metadata editing.
- Add richer controller mapping UI.
- Add cloud-free local backup/export for the database and settings.
- Add release signing and GPL/source notice packaging for native emulator builds.
