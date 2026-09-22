#!/bin/bash
# Steam under a modern Wine, in its own prefix — the route out of the CEF
# restart loop that makes Steam unusable on Whisky's wine-7.7.
# See _knowledge/gotchas/steam-webhelper-restart-loop-in-wine.md.
#
# STATUS as of 2026-09-02: Steam runs and is LOGGED IN under wine-staging
# 11.16, but **its window renders black** — Chromium falls back to software
# compositing here (ANGLE only exposes GLES 2.0 over MoltenVK) and that path
# never reaches the window. Everything already tried and ruled out is in the
# shard; do not re-run it.
#
# The black window is a cosmetic problem, not a blocker. Steam's UI is a
# Chromium app, so it is driven over the DevTools protocol instead: the client
# opens port 8080 when `.cef-enable-remote-debugging` exists next to it, and
# from there `Page.captureScreenshot` renders the login QR perfectly and
# `SteamClient.Installs.*` queues downloads. That is how the login was done.
# The `steam` verb below still opens the black window; it is the CDP channel
# that is useful. See the shard for the exact calls.
#
# RaceRoom itself is currently blocked on disk space, not on Wine: the install
# wizard wants 74.6 GiB and this machine has 56.1 GiB free.
#
#   ./steam-wine11.sh setup     download + unpack Wine, create the prefix
#   ./steam-wine11.sh steam     launch Steam (sign in here)
#   ./steam-wine11.sh install   queue the RaceRoom download (after login)
#   ./steam-wine11.sh play      launch RaceRoom
#   ./steam-wine11.sh kill      kill everything in this prefix
#   ./steam-wine11.sh status    what is installed and running
#
# This prefix is deliberately NOT the Whisky "Sim Racing" bottle. A newer Wine
# upgrades any prefix it opens and wine-7.7 cannot reliably reopen it, so
# pointing this at the bottle would take Live for Speed and Speed Dreams down
# with it. Keep them separate.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HOME_SUPPORT="${G29_WINE11_HOME:-$HOME/Library/Application Support/g29-mac}"
WINE_VER="${G29_WINE11_VERSION:-11.16}"
WINE_APP="$HOME_SUPPORT/wine-11/Wine Staging.app/Contents/Resources/wine"
WINE="$WINE_APP/bin/wine"
WINESERVER="$WINE_APP/bin/wineserver"
export WINEPREFIX="${G29_WINE11_PREFIX:-$HOME_SUPPORT/steam-prefix}"
export WINEDEBUG="${WINEDEBUG:--all}"
STEAM_DIR="$WINEPREFIX/drive_c/Program Files (x86)/Steam"
STEAM_EXE="$STEAM_DIR/Steam.exe"
RACEROOM_APPID=211500

need_wine() {
    [ -x "$WINE" ] || { echo "Wine $WINE_VER is not unpacked yet — run: $0 setup" >&2; exit 1; }
}

need_steam() {
    [ -f "$STEAM_EXE" ] || { echo "Steam is not in this prefix yet — run: $0 setup" >&2; exit 1; }
}

case "${1:-status}" in

setup)
    mkdir -p "$HOME_SUPPORT"
    TARBALL="$HOME_SUPPORT/wine-staging-$WINE_VER-osx64.tar.xz"
    URL="https://github.com/Gcenx/macOS_Wine_builds/releases/download/$WINE_VER/wine-staging-$WINE_VER-osx64.tar.xz"

    if [ ! -x "$WINE" ]; then
        [ -f "$TARBALL" ] || { echo "== downloading Wine $WINE_VER (~190 MB)"; curl -L --fail -o "$TARBALL" "$URL" || exit 1; }
        echo "== unpacking"
        mkdir -p "$HOME_SUPPORT/wine-11"
        tar -xJf "$TARBALL" -C "$HOME_SUPPORT/wine-11" || exit 1
        # Downloaded, so Gatekeeper would otherwise refuse every binary in it.
        xattr -dr com.apple.quarantine "$HOME_SUPPORT/wine-11" 2>/dev/null
    fi
    echo "== wine: $("$WINE" --version 2>/dev/null)"

    if [ ! -d "$WINEPREFIX/drive_c" ]; then
        echo "== creating the prefix (this takes a minute)"
        mkdir -p "$WINEPREFIX"
        "$WINE" wineboot -u >/dev/null 2>&1
    fi

    # Wine's WinHTTP otherwise runs WPAD auto-detection across every network
    # interface and Steam's updater times out — see
    # _knowledge/gotchas/wine-winhttp-wpad-stalls-steam.md. Per prefix, so a
    # fresh one needs it again.
    echo "== disabling WPAD proxy auto-detection"
    "$WINE" reg add "HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings\\Connections" \
        /v WinHttpSettings /t REG_BINARY /d 28000000000000000100000000000000000000 /f >/dev/null 2>&1
    "$WINE" reg add "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings" /v ProxyEnable /t REG_DWORD /d 0 /f >/dev/null 2>&1
    "$WINE" reg add "HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings" /v AutoDetect  /t REG_DWORD /d 0 /f >/dev/null 2>&1

    if [ ! -f "$STEAM_EXE" ]; then
        SETUP="$HOME_SUPPORT/SteamSetup.exe"
        [ -f "$SETUP" ] || { echo "== downloading SteamSetup.exe"; curl -L --fail -o "$SETUP" \
            "https://cdn.akamai.steamstatic.com/client/installer/SteamSetup.exe" || exit 1; }
        echo "== running SteamSetup.exe — accept the installer, then Steam self-updates (~1.6 GB)"
        "$WINE" "$SETUP"
    fi

    echo "done. Next: $0 steam"
    ;;

steam)
    need_wine; need_steam
    echo "Launching Steam in the Wine $WINE_VER prefix."
    echo "Sign in with the account you want RaceRoom on; approve Steam Guard on your phone."
    exec "$WINE" "$STEAM_EXE"
    ;;

install)
    need_wine; need_steam
    exec "$WINE" "$STEAM_EXE" -applaunch "$RACEROOM_APPID" "steam://install/$RACEROOM_APPID"
    ;;

play)
    need_wine; need_steam
    # Logitech wheels power up at a reduced rotation range until told otherwise.
    [ -x "$ROOT/bin/lgwheel" ] && { "$ROOT/bin/lgwheel" --range 900 >/dev/null 2>&1 \
        && echo "wheel: 900 degrees set" || echo "wheel: not detected (connect the G29 and its power brick)"; }
    exec "$WINE" "$STEAM_EXE" -applaunch "$RACEROOM_APPID"
    ;;

kill)
    # This prefix has its own wineserver; scripts/reset-bottle.sh only knows
    # about Whisky's, so it will not clean up after this one.
    need_wine
    "$WINESERVER" -k 2>/dev/null
    sleep 2
    pkill -9 -f "$WINEPREFIX" 2>/dev/null
    echo "prefix stopped."
    ;;

status)
    echo "prefix:   $WINEPREFIX"
    if [ -x "$WINE" ]; then echo "wine:     $("$WINE" --version 2>/dev/null)"; else echo "wine:     not installed — run: $0 setup"; fi
    if [ -f "$STEAM_EXE" ]; then echo "steam:    installed"; else echo "steam:    not installed — run: $0 setup"; fi
    if [ -d "$STEAM_DIR/steamapps/common/raceroom racing experience" ]; then
        echo "raceroom: installed"
    else
        echo "raceroom: not installed — run: $0 install (after signing in)"
    fi
    echo "running:  $(ps -eo command | grep -c "[c]ef.win64\|[S]team.exe") steam process(es)"
    if [ -f "$STEAM_DIR/logs/webhelper.txt" ]; then
        echo "webhelper launches this install: $(grep -ac 'Startup - webhelper launched' "$STEAM_DIR/logs/webhelper.txt")"
        echo "  (a number that climbs every 10s is the restart loop — see the gotcha shard)"
    fi
    ;;

*) echo "usage: $0 {setup|steam|install|play|kill|status}"; exit 1 ;;
esac
