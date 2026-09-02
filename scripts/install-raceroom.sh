#!/bin/bash
# RaceRoom Racing Experience — free to play, needs a one-time Steam login.
#
#   ./install-raceroom.sh login     open Steam in the bottle so you can sign in
#   ./install-raceroom.sh install   queue the RaceRoom download (after login)
#   ./install-raceroom.sh play      launch RaceRoom
set -euo pipefail
source "$(cd "$(dirname "$0")" && pwd)/common.sh"
STEAM="$WINEPREFIX/drive_c/Program Files (x86)/Steam/Steam.exe"
case "${1:-login}" in
  login)   echo "Signing in to Steam inside the bottle (one time only)."
           echo "Use the same account as the Mac Steam app; approve on your phone."
           exec "$WINE" "$STEAM" ;;
  install) prep_wheel; exec "$WINE" "$STEAM" -applaunch 211500 steam://install/211500 ;;
  play)    prep_wheel; exec "$WINE" "$STEAM" -applaunch 211500 ;;
  *) echo "usage: $0 {login|install|play}"; exit 1 ;;
esac
