#!/bin/sh
trap "" HUP
# Re-applies RaineyUI's LED preset whenever the stock MainUI (re)starts:
# MainUI turns the LED master switch off at startup when TrimUI's "LED switch" is off.
APP=/mnt/UDISK/Apps/RaineyUI
export LD_LIBRARY_PATH=/usr/trimui/lib
last=""
while true; do
    [ -x "$APP/rainey" ] || exit 0
    p=$(pidof MainUI)
    if [ -n "$p" ] && [ "$p" != "$last" ]; then
        # MainUI switches the LEDs off some seconds into its startup; win the race
        for t in 2 3 5 10; do
            sleep $t
            [ "$(cat /sys/class/led_anim/enable)" = "1" ] && [ $t -lt 10 ] && continue
            "$APP/rainey" --leds-apply
        done
        last=$p
    fi
    sleep 2
done
