#!/bin/sh
# Build "Send to Brick.app" (drag-and-drop USB transfer for the Brick) from source.
set -e
cd "$(dirname "$0")"
rm -rf "Send to Brick.app"
osacompile -o "Send to Brick.app" SendToBrick.applescript
cp send.sh "Send to Brick.app/Contents/Resources/send.sh"
iconutil -c icns ../../desktop/RaineyIcon.iconset -o "Send to Brick.app/Contents/Resources/droplet.icns"
touch "Send to Brick.app"
echo "built mac/Send to Brick.app"
