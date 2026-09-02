#!/bin/bash
# Speed Dreams 2.4.2 — free and open source, full force feedback.
set -euo pipefail
source "$(cd "$(dirname "$0")" && pwd)/common.sh"
prep_wheel
cd "$WINEPREFIX/drive_c/SpeedDreams/bin"
exec "$WINE" speed-dreams-2.exe "$@"
