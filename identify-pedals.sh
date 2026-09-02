#!/bin/bash
# Which physical pedal is which HID axis? Press them and find out.
#
# The G29 reports four axes and no labels: X is the steering, and three more
# are the pedals. Games map them by axis name, so you need to know which is
# which before you can fix a mis-assigned throttle.
#
#   ./identify-pedals.sh          30 second run
#   ./identify-pedals.sh 60       longer, if you want to take your time

set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SECS="${1:-30}"

cat <<'EOF'

  PEDAL IDENTIFICATION
  ====================
  When the movement log starts, press ONE pedal at a time, all the way to the
  floor, holding about two seconds each, with a pause in between:

      1.  ACCELERATOR      (rightmost)
      2.  BRAKE            (middle)
      3.  CLUTCH           (leftmost)

  Then turn the wheel fully left and right.

  Watch which axis name moves for each one — that mapping is what you enter
  in the game. Starting in 3 seconds...

EOF
sleep 3
exec "$HERE/lgwheel" --axes "$SECS"
