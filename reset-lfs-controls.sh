#!/bin/bash
# Make LFS forget its wheel config and re-detect from scratch.
#
# LFS caches per-device controller settings in a binary .csf. A config written
# while Wine was presenting a DIFFERENT descriptor (see the KB shard
# sdl-wheel-type-changes-axis-usages) keeps stale axis bindings that no amount
# of re-assigning in the menu will clear. This backs it up and removes it.
set -euo pipefail
source "$(cd "$(dirname "$0")" && pwd)/common.sh"
CFG="$WINEPREFIX/drive_c/LFS/data/misc"
STAMP=$(date +%Y%m%d-%H%M%S)
shopt -s nullglob
found=0
for f in "$CFG"/*.csf; do
    cp "$f" "$f.backup-$STAMP"
    rm "$f"
    echo "reset: $(basename "$f")   (backup: $(basename "$f").backup-$STAMP)"
    found=1
done
[ "$found" = "1" ] || { echo "no .csf found — LFS has no saved wheel config, nothing to reset"; exit 0; }
echo
echo "Now start LFS, go to Options > Controls > 'wheel / joystick', and the"
echo "'Axes / FF' tab. Assign:  steering = X   throttle = Z   brake = Rz   clutch = Y"
