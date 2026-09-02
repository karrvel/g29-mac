#!/bin/bash
# Live for Speed — free demo (Blackwood + 3 cars), full force feedback.
#
#   ./play-lfs.sh            play
#   LFS_HUD=1 ./play-lfs.sh  play with the DXVK frame-rate overlay
#
# D3D11 goes through DXVK, not Wine's builtin renderer — that plus trimmed
# in-game quality took this from ~39-58 fps with visible stutter to a steady
# 100 fps at 8.5-11.5 ms frame times.
set -euo pipefail
source "$(cd "$(dirname "$0")" && pwd)/common.sh"
prep_wheel
cd "$WINEPREFIX/drive_c/LFS"
export DXVK_CONFIG_FILE="$WINEPREFIX/drive_c/LFS/dxvk.conf"
[ -n "${LFS_HUD:-}" ] && export DXVK_HUD=fps,frametimes
exec "$WINE" LFS.exe "$@"
