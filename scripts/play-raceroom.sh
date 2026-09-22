#!/bin/bash
# Launch RaceRoom in the Wine 11 prefix, doing the several non-obvious things
# that have to happen in order. Run it with no arguments:
#
#   scripts/play-raceroom.sh
#
# WARNING — as of 2026-09-03 the game crashes during load, usually within one to
# four minutes, at a fixed address in its own code. Every environmental variable
# within reach has been eliminated (graphics settings, input backend, the FFB
# shim, the wheel's presence, and the launch method). This script is correct; the
# game is not yet playable. See
# _knowledge/gotchas/raceroom-crashes-during-load.md before debugging further.
#
# What this handles that a bare `wine RRRE64.exe` does not:
#   * Steam must be running and logged in first — RaceRoom is a protected Steam
#     title and expects to be started by the client, not from a shell.
#   * Steam's own window renders black under Wine, so the launch is driven over
#     the DevTools protocol rather than clicked (scripts/steam-cdp.py).
#   * Steam interposes invisible modals that block the launch — "Grab a
#     controller" and, after any crash, "Cloud Out of Date". Both are dismissed
#     programmatically, choosing the affirmative button by name.
#   * The G29 powers up at a reduced rotation range until told otherwise.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CDP="$ROOT/scripts/steam-cdp.py"
HOME_SUPPORT="${G29_WINE11_HOME:-$HOME/Library/Application Support/g29-mac}"
WINE="$HOME_SUPPORT/wine-11/Wine Staging.app/Contents/Resources/wine/bin/wine"
export WINEPREFIX="${G29_WINE11_PREFIX:-$HOME_SUPPORT/steam-prefix}"
export WINEDEBUG="${WINEDEBUG:--all}"
STEAM_DIR="$WINEPREFIX/drive_c/Program Files (x86)/Steam"
APPID=211500

[ -x "$WINE" ] || { echo "Wine 11 not installed — run: scripts/steam-wine11.sh setup" >&2; exit 1; }
[ -d "$STEAM_DIR/steamapps/common/raceroom racing experience" ] || {
    echo "RaceRoom is not installed in this prefix — see scripts/steam-wine11.sh" >&2; exit 1; }

cdp() { python3 "$CDP" "$1" "$2" 2>/dev/null; }

# Logitech wheels come up at a reduced range; harmless if the wheel is absent.
if [ -x "$ROOT/bin/lgwheel" ]; then
    "$ROOT/bin/lgwheel" --range 900 >/dev/null 2>&1 \
        && echo "wheel: 900 degrees set" || echo "wheel: not detected (check the power brick)"
fi

# Remote debugging is how everything below talks to the client. The marker file
# only takes effect at client startup, so set it before Steam is launched.
touch "$STEAM_DIR/.cef-enable-remote-debugging"

# The DevTools endpoint answers well before the UI is usable. Until the client's
# React UI mounts, every steam:// URL silently no-ops with "BrowserBackstack:
# ... without a browser manager available!", so wait for the app store to be
# populated rather than merely for the port to respond.
# A half-mounted client populates appStore but never registers its window's
# browser manager, and then swallows every steam:// URL. The reliable signal is
# the UI's own CEF targets: a mounted client publishes a dozen or more
# ("Library Supernav", "Steam Root Menu", ...), a stuck one publishes two.
ui_ready() {
    local n
    n=$(curl -s --max-time 5 "http://localhost:${STEAM_CDP_PORT:-8080}/json/list" 2>/dev/null \
        | python3 -c 'import sys,json;print(sum(1 for t in json.load(sys.stdin) if "Supernav" in (t.get("title") or "")))' 2>/dev/null)
    [ -n "$n" ] && [ "$n" -ge 2 ]
}

start_steam() {
    ( "$WINE" "$STEAM_DIR/Steam.exe" >/dev/null 2>&1 & )
    for i in $(seq 1 36); do
        sleep 5
        ui_ready && return 0
    done
    return 1
}

if ! ui_ready; then
    echo "starting Steam..."
    pgrep -f "cef.win64" >/dev/null 2>&1 && { pkill -f "cef.win64" 2>/dev/null; pkill -f "Steam.exe" 2>/dev/null; sleep 3; }
    if ! start_steam; then
        # A client that comes up stuck on the loading spinner never mounts its UI;
        # restarting it clears that. Do not Page.reload the window — that makes it worse.
        echo "Steam came up without a usable UI — restarting it once..."
        pkill -f "cef.win64" 2>/dev/null; pkill -f "Steam.exe" 2>/dev/null; sleep 5
        start_steam || { echo "Steam UI never mounted; try scripts/steam-wine11.sh steam by hand" >&2; exit 1; }
    fi
fi
echo "Steam is up and logged in."

# A previous launch that never completed leaves a LaunchApp action wedged, and
# Steam will not start a second one while it exists.
cdp SharedJSContext '{"id":1,"method":"Runtime.evaluate","params":{"expression":"(async()=>{const a=await SteamClient.Apps.GetActiveGameActions();a.forEach(x=>SteamClient.Apps.CancelGameAction(x.nGameActionID));return a.length})()","awaitPromise":true,"returnByValue":true}}' >/dev/null 2>&1

echo "launching RaceRoom (app $APPID)..."
cdp SharedJSContext \
    '{"id":1,"method":"Runtime.evaluate","params":{"expression":"SteamClient.URL.ExecuteSteamURL(\"steam://run/'"$APPID"'\"); 1","returnByValue":true}}' >/dev/null

# Poll for the game, dismissing whatever modal Steam puts in the way. The
# "Grab a controller" notice appears once per session and blocks the launch
# entirely until something clicks its only button.
for i in $(seq 1 40); do
    sleep 3
    if pgrep -f "RRRE64.exe" >/dev/null 2>&1; then
        echo "RaceRoom is running."
        echo "  (if it dies in the next few minutes that is the known load crash,"
        echo "   not this script — see _knowledge/gotchas/raceroom-crashes-during-load.md)"
        exit 0
    fi
    # Two modals block the launch and neither is visible on a black window:
    #   "Grab a controller and kick back!"  -> OK
    #   "Cloud Out of Date"                 -> Play anyway   (every crashed session
    #                                          leaves an un-uploaded save, so after
    #                                          the first crash this shows every time)
    # They render either as their own "Steam Dialog" target or inside the main
    # window, so check both, and pick the affirmative button by name rather than
    # taking the first one — "Cancel" sits right next to it.
    for target in "Steam Dialog" "Steam"; do
        coords=$(cdp "$target" '{"id":1,"method":"Runtime.evaluate","params":{"expression":"(()=>{const want=/^(play anyway|ok|continue|accept|yes)$/i;const b=[...document.querySelectorAll(\"button,[class*=Button]\")].filter(x=>{const r=x.getBoundingClientRect();return r.width>0&&want.test((x.innerText||\"\").trim())});if(!b.length)return \"\";const r=b[0].getBoundingClientRect();return Math.round(r.x+r.width/2)+\" \"+Math.round(r.y+r.height/2)+\" \"+b[0].innerText.trim()})()","returnByValue":true}}' \
            | python3 -c 'import sys,re;m=re.search(r"\"value\":\s*\"(\d+) (\d+) ([^\"]*)\"",sys.stdin.read());print(f"{m.group(1)} {m.group(2)} {m.group(3)}" if m else "")' 2>/dev/null)
        if [ -n "$coords" ]; then
            set -- $coords
            echo "dismissing \"$3\" in $target at ($1,$2)"
            for ev in mousePressed mouseReleased; do
                cdp "$target" '{"id":1,"method":"Input.dispatchMouseEvent","params":{"type":"'"$ev"'","x":'"$1"',"y":'"$2"',"button":"left","clickCount":1}}' >/dev/null
            done
            break
        fi
    done

    # A launch parked on a task with bWaitingForUI:false needs a nudge before its
    # dialog is even created — SynchronizingCloud does exactly this.
    if [ $((i % 5)) -eq 0 ]; then
        cdp SharedJSContext '{"id":1,"method":"Runtime.evaluate","params":{"expression":"(async()=>{const a=await SteamClient.Apps.GetActiveGameActions();a.filter(x=>!x.bWaitingForUI).forEach(x=>SteamClient.Apps.ContinueGameAction(x.nGameActionID));return 1})()","awaitPromise":true,"returnByValue":true}}' >/dev/null 2>&1
    fi
done

echo "RaceRoom did not start within two minutes." >&2
exit 1
