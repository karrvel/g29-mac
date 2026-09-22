#!/bin/bash
# RaceRoom Racing Experience — free to play, needs a one-time Steam login.
#
#   ./install-raceroom.sh login     open Steam in the bottle so you can sign in
#   ./install-raceroom.sh install   queue the RaceRoom download (after login)
#   ./install-raceroom.sh play      launch RaceRoom
#
# SUPERSEDED as of 2026-09-02 — this script no longer works, by design.
#
# Steam's UI never ran in this bottle: steamwebhelper.exe (a full Chrome 126)
# died during startup and Whisky's wine-7.7 relaunched it every ten seconds, so
# no login screen ever appeared. See
# _knowledge/gotchas/steam-webhelper-restart-loop-in-wine.md.
#
# Steam now lives in its own wine-staging 11.16 prefix instead — use
# scripts/steam-wine11.sh. The Steam install this script points at inside the
# bottle has been DELETED as redundant (it was 1.6 GiB of duplicate), so the
# commands below will fail on a missing Steam.exe. Kept only so the paths and
# the history are legible; do not revive it without reading the shard first.
set -euo pipefail
source "$(cd "$(dirname "$0")" && pwd)/common.sh"
STEAM="$WINEPREFIX/drive_c/Program Files (x86)/Steam/Steam.exe"
case "${1:-login}" in
  login)   echo "Opening Steam inside the bottle."
           echo
           echo "Expect this to fail: Steam's UI does not render here. It hangs about two"
           echo "minutes on the update check, then loops — steamwebhelper restarting every"
           echo "ten seconds — and no login screen appears. Watch it with:"
           echo "  grep -a 'webhelper launched' \"\$WINEPREFIX/drive_c/Program Files (x86)/Steam/logs/webhelper.txt\" | tail"
           echo "Why, and what to do instead: _knowledge/gotchas/steam-webhelper-restart-loop-in-wine.md"
           echo "Afterwards run scripts/reset-bottle.sh — the CEF helpers linger and wedge the bottle."
           echo
           exec "$WINE" "$STEAM" ;;
  install) prep_wheel; exec "$WINE" "$STEAM" -applaunch 211500 steam://install/211500 ;;
  play)    prep_wheel; exec "$WINE" "$STEAM" -applaunch 211500 ;;
  *) echo "usage: $0 {login|install|play}"; exit 1 ;;
esac
