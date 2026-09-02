#!/bin/bash
# Kill everything running in the sim-racing bottle and start a fresh wineserver.
#
# Use this when a game reports "wineserver crashed, please enable coredumps
# (ulimit -c unlimited) and restart". Every process in a bottle shares ONE
# wineserver, so a wedged leftover — Steam and its CEF helpers are the usual
# culprits, and they linger after the window closes — takes the next game down
# with it. Nothing is lost: bottles keep no runtime state worth saving.

set -uo pipefail
source "$(cd "$(dirname "$0")" && pwd)/common.sh"

echo "processes in the bottle before: $(pgrep -f 'wine64|wineserver|steamwebhelper' 2>/dev/null | wc -l | tr -d ' ')"

"$(dirname "$WINE")/wineserver" -k 2>/dev/null
sleep 2

# CEF helpers routinely survive a polite shutdown.
for p in steamwebhelper Steam.exe wine64-preloader wineserver; do
    pkill -9 -f "$p" 2>/dev/null
done
sleep 1

left=$(pgrep -f 'wine64|wineserver|steamwebhelper' 2>/dev/null | wc -l | tr -d ' ')
echo "processes after: $left"

if [ "$left" = "0" ]; then
    echo "bottle is clean — launch your game normally."
else
    echo "warning: $left process(es) survived; check with: pgrep -fl wine64"
fi

# Forces can be left latched by a game that died mid-effect.
[ -x "$HERE/lgwheel" ] && "$HERE/lgwheel" --stop >/dev/null 2>&1 && echo "wheel forces cleared."
