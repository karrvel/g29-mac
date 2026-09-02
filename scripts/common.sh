# Shared environment for the sim-racing bottle. Sourced by every script here.
#
# The bottle is found by name rather than hardcoded, so this works on any
# machine. Override either of these if your layout differs:
#   G29_BOTTLE_NAME   the Whisky bottle to use   (default: "Sim Racing")
#   WINEPREFIX        an explicit bottle path    (skips the search entirely)

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
BIN="$ROOT/bin"
WHISKY_SUPPORT="$HOME/Library/Application Support/com.isaacmarovitz.Whisky"
BOTTLES_DIR="$HOME/Library/Containers/com.isaacmarovitz.Whisky/Bottles"
WINE="$WHISKY_SUPPORT/Libraries/Wine/bin/wine64"
G29_BOTTLE_NAME="${G29_BOTTLE_NAME:-Sim Racing}"
export WINEDEBUG="${WINEDEBUG:--all}"

# Find the bottle whose Metadata.plist declares $G29_BOTTLE_NAME.
find_bottle() {
    [ -d "$BOTTLES_DIR" ] || return 1
    local d name
    for d in "$BOTTLES_DIR"/*/; do
        [ -f "$d/Metadata.plist" ] || continue
        name=$(plutil -extract info.name raw -o - "$d/Metadata.plist" 2>/dev/null) || continue
        if [ "$name" = "$G29_BOTTLE_NAME" ]; then
            printf '%s' "${d%/}"
            return 0
        fi
    done
    return 1
}

if [ -z "${WINEPREFIX:-}" ]; then
    WINEPREFIX=$(find_bottle) || {
        echo "error: no Whisky bottle named '$G29_BOTTLE_NAME' found under" >&2
        echo "       $BOTTLES_DIR" >&2
        echo "Create one in Whisky, or set WINEPREFIX / G29_BOTTLE_NAME." >&2
        return 1 2>/dev/null || exit 1
    }
fi
export WINEPREFIX

[ -x "$WINE" ] || {
    echo "error: Whisky's Wine not found at $WINE" >&2
    echo "Install Whisky, or point WINE at another Wine build." >&2
    return 1 2>/dev/null || exit 1
}

# Logitech wheels power up at a reduced rotation range until told otherwise.
# Built tools live in bin/ (see the Makefile); scripts live in scripts/.
prep_wheel() {
    if [ -x "$BIN/lgwheel" ]; then
        "$BIN/lgwheel" --range 900 >/dev/null 2>&1 && echo "wheel: 900 degrees set" \
            || echo "wheel: not detected (connect the G29 and its power brick)"
    fi
}
