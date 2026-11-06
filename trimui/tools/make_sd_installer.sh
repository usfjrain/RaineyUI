#!/bin/bash
# Build the SD-card installer: RaineyUI-Installer-TrimUI-<version>.zip
# Users unzip it to the root of the SD card and open Apps → Install RaineyUI on the Brick.
#
#   tools/make_sd_installer.sh v0.3.0 [outdir]
#
# The zip holds RaineyUI itself, a few edited config files and a manifest. The emulators are
# NOT included: the Brick downloads them over Wi-Fi from their projects (CrossMix-OS files are
# checked against the SHA-256 recorded here; libretro cores come from the buildbot).
set -euo pipefail
VER=${1:?usage: make_sd_installer.sh <version> [outdir]}
HERE="$(cd "$(dirname "$0")/.." && pwd)"
OUTDIR="${2:-$HERE/../dist}"
mkdir -p "$OUTDIR"; OUTDIR="$(cd "$OUTDIR" && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
STAGE="${STAGE:-$WORK/stage}"   # reuse a previous `install_emulators.sh --stage DIR` with STAGE=DIR

"$HERE/build.sh" | tail -1
[ -d "$STAGE/RaineyUI/emus" ] || "$HERE/tools/install_emulators.sh" --stage "$STAGE"

INST="$WORK/zip/Apps/RaineyUI-Installer"
P="$HERE/package/RaineyUI"
mkdir -p "$INST/payload/RaineyUI"
cp "$HERE/installer/launch.sh" "$HERE/installer/install_worker.sh" "$HERE/installer/config.json" "$INST/"
cp "$P/icon.png" "$INST/icon.png"
cp -R "$P/rainey" "$P/launch.sh" "$P/config.json" "$P/icon.png" "$P/leds_watch.sh" "$P/raineyleds.init" "$P/game_fixes.txt" "$P/res" \
      "$INST/payload/RaineyUI/"
curl -fsSL -o "$INST/cacert.pem" https://curl.se/ca/cacert.pem   # Mozilla CA list (MPL-2.0), for HTTPS on the Brick

# PortMaster: RaineyUI's launcher/settings go in the payload; PortMaster, Python and bash are
# downloaded by the Brick from their releases (pinned here, checked by SHA-256)
cp -R "$HERE/portmaster" "$INST/payload/PortMaster"
PM_URL=https://github.com/PortsMaster/PortMaster-GUI/releases/download/2026.11.02-0433/trimui.portmaster.zip
PY_URL=https://github.com/astral-sh/python-build-standalone/releases/download/20261026/cpython-3.11.18%2B20261026-aarch64-unknown-linux-gnu-install_only_stripped.tar.gz
BASH_URL=https://github.com/robxu9/bash-static/releases/download/5.2.015-1.2.3-2/bash-linux-aarch64
PM_CACHE="$HERE/../.local/pm/dl"
mkdir -p "$PM_CACHE"
pm_line() { # kind url
    local f="$PM_CACHE/$(basename "${2//%2B/+}")"
    [ -s "$f" ] || curl -fsSL -o "$f" "$2"
    echo "$1|$(shasum -a 256 "$f" | cut -d' ' -f1)|$2"
}
PM_LINES="$(pm_line pm "$PM_URL")
$(pm_line python "$PY_URL")
$(pm_line bash "$BASH_URL")"

CROSSMIX_SHA=$(sed -n 's/^CROSSMIX_SHA=//p' "$HERE/tools/install_emulators.sh")
python3 -I - "$STAGE/RaineyUI" "$INST" "$CROSSMIX_SHA" <<'EOF'
import hashlib, os, shutil, sys, urllib.parse
stage, inst, sha = sys.argv[1:4]
RAW = f"https://raw.githubusercontent.com/cizia64/CrossMix-OS/{sha}/"
BUILDBOT = "https://buildbot.libretro.com/nightly/linux/aarch64/latest/"
SOURCES = {"emus/RetroArch/": "RetroArch/", "emus/drastic/": "Emus/NDS/drastic/",
           "emus/ppsspp/": "Emus/PSP/PPSSPP_1.17.1/"}

def edited(rel):  # config files install_emulators.sh rewrites: shipped in the payload
    return (rel == "emus/RetroArch/retroarch.cfg"
            or (rel.startswith("emus/RetroArch/.retroarch/config/") and rel.endswith(".cfg"))
            or rel == "emus/ppsspp/.config/ppsspp/PSP/SYSTEM/ppsspp.ini")

lines, smallest = [], None
for root, _, files in os.walk(stage):
    for f in sorted(files):
        full = os.path.join(root, f)
        rel = os.path.relpath(full, stage)
        if rel.startswith("cores/"):
            lines.append(f"core|-|{rel}|{BUILDBOT}{f}.zip")
            continue
        if edited(rel):
            dst = os.path.join(inst, "payload/RaineyUI", rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy(full, dst)
            continue
        prefix = next(p for p in SOURCES if rel.startswith(p))
        url = RAW + urllib.parse.quote(SOURCES[prefix] + rel[len(prefix):])
        size = os.path.getsize(full)
        digest = hashlib.sha256(open(full, "rb").read()).hexdigest()
        lines.append(f"file|{digest}|{rel}|{url}")
        if size > 0 and (smallest is None or size < smallest[0]):
            smallest = (size, url)
lines.sort(key=lambda l: (not l.startswith("core"), l))
with open(os.path.join(inst, "manifest.txt"), "w") as m:
    m.write(f"check|{smallest[1]}\n" + "\n".join(lines) + "\n")
print(f"manifest: {sum(l.startswith('core') for l in lines)} cores, {sum(l.startswith('file') for l in lines)} files")
EOF
echo "$PM_LINES" >> "$INST/manifest.txt"
cat > "$WORK/zip/README.txt" <<EOF
RaineyUI ${VER} installer for TrimUI Brick / Smart Pro (stock firmware)

1. Copy the Apps folder from this zip to the root of your SD card
   (it merges with an Apps folder that is already there).
2. Put the SD card in the Brick, connect it to Wi-Fi, and open Apps -> Install RaineyUI.
3. Press A. When it says "RaineyUI is installed!", open Apps -> RaineyUI.

The installer downloads the emulators and PortMaster from their original projects, so
Wi-Fi is needed. Everything is installed on the Brick's internal storage.
Run it again any time to update RaineyUI. Your games, saves and SD card are not changed.
https://github.com/usfjrain/RaineyUI
EOF
find "$WORK/zip" \( -name ".DS_Store" -o -name "._*" \) -delete
ZIP="$OUTDIR/RaineyUI-Installer-TrimUI-$VER.zip"
rm -f "$ZIP"
(cd "$WORK/zip" && zip -qrX "$ZIP" Apps README.txt)
ls -l "$ZIP"
