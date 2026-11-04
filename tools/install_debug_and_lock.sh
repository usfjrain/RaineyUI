#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
JDK_HOME="/private/tmp/vsgodot-jdk/Contents/Home"
GRADLE="${ROOT_DIR}/../RaineyPack companion/.local/gradle/gradle-8.10.2/bin/gradle"
ADB="${ROOT_DIR}/.local/android-sdk/platform-tools/adb"

if [[ ! -x "${ADB}" ]]; then
  ADB="adb"
fi

export JAVA_HOME="${JDK_HOME}"
export PATH="${JDK_HOME}/bin:${PATH}"
export GRADLE_USER_HOME="${ROOT_DIR}/.local/gradle-user"

cd "${ROOT_DIR}"

"${GRADLE}" assembleDebug --no-daemon
"${ADB}" install -r app/build/outputs/apk/debug/app-debug.apk
"${ROOT_DIR}/tools/lock_rainey_button.sh"
