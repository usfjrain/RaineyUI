#!/bin/bash
# Send to Brick helper: copy one dropped item to the Brick's inbox over USB (adb).
# usage: send.sh push <path> | send.sh sort | send.sh check
ADB=""
for a in /usr/local/bin/adb /opt/homebrew/bin/adb "$HOME/Library/Android/sdk/platform-tools/adb" "$(command -v adb)"; do
  [ -x "$a" ] && ADB="$a" && break
done
INBOX=/mnt/UDISK/RaineyUI/inbox
case "$1" in
check)
  [ -n "$ADB" ] || { echo "NOADB"; exit 1; }
  [ "$("$ADB" get-state 2>/dev/null)" = "device" ] || { echo "NODEVICE"; exit 1; }
  "$ADB" shell "mkdir -p $INBOX" && echo OK ;;
push)
  p="${2%/}"; name="$(basename "$p")"; ext="$(echo "${name##*.}" | tr A-Z a-z)"
  if [ -d "$p" ]; then
    "$ADB" push "$p" "$INBOX/" >/dev/null
  elif [ "$ext" = "gdi" ] || [ "$ext" = "cue" ]; then
    # disc index: bring its track files (same folder, same name prefix) along
    stem="${name%.*}"; dir="$(dirname "$p")"
    "$ADB" shell "mkdir -p \"$INBOX/$stem\""
    for f in "$dir/$stem"*; do
      b="$(basename "$f")"; case "$b" in ._*) continue;; esac
      [ -f "$f" ] && "$ADB" push "$f" "$INBOX/$stem/" >/dev/null
    done
  else
    case "$name" in ._*|.DS_Store) exit 0;; esac
    "$ADB" push "$p" "$INBOX/" >/dev/null
  fi ;;
sort)
  "$ADB" shell "LD_LIBRARY_PATH=/usr/trimui/lib /mnt/UDISK/Apps/RaineyUI/rainey --sort-inbox" | tr -d '\r' ;;
esac
