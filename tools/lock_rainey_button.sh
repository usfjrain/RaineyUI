#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ADB="${ROOT_DIR}/.local/android-sdk/platform-tools/adb"

if [[ ! -x "${ADB}" ]]; then
  ADB="adb"
fi

RAINEY_PACKAGE="com.rainey.frontend"
RAINEY_ACCESSIBILITY="${RAINEY_PACKAGE}/.RaineyLauncherKeyService"
RETROID_LAUNCHER="com.ylm.launcher"
ANDROID_HOME="com.android.launcher3/com.android.searchlauncher.SearchLauncher"

echo "Locking RG406V launcher button to Rainey..."

"${ADB}" shell pm disable-user --user 0 "${RETROID_LAUNCHER}" >/dev/null || true
"${ADB}" shell cmd package set-home-activity --user 0 "${ANDROID_HOME}"
"${ADB}" shell settings --user 0 put secure enabled_accessibility_services "${RAINEY_ACCESSIBILITY}"
"${ADB}" shell settings --user 0 put secure accessibility_enabled 1

echo
echo "Home handler:"
"${ADB}" shell cmd package resolve-activity --brief --user 0 -a android.intent.action.MAIN -c android.intent.category.HOME

echo
echo "Accessibility service:"
"${ADB}" shell settings --user 0 get secure enabled_accessibility_services

echo
echo "Disabled launcher packages:"
"${ADB}" shell pm list packages -d | grep -E "com\\.ylm\\.launcher|${RAINEY_PACKAGE}" || true

echo
echo "Rainey launcher button lock applied."
