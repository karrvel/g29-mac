#!/bin/bash
# Install (or remove) the SDL2 force-feedback shim into Whisky's Wine.
#
#   ./install-shim.sh install    put the shim in place (backs up the real SDL2)
#   ./install-shim.sh revert     restore the original SDL2, unchanged
#   ./install-shim.sh status     show what is currently installed
#
# The shim is what makes a Windows racing sim running under Wine produce real
# torque on a Logitech wheel. Reverting is always safe: the original library is
# kept untouched next to it.

set -euo pipefail

WINE_LIB="$HOME/Library/Application Support/com.isaacmarovitz.Whisky/Libraries/Wine/lib"
LIVE="$WINE_LIB/libSDL2-2.0.0.dylib"
REAL="$WINE_LIB/libSDL2-2.0.0.real.dylib"
BACKUP="$WINE_LIB/libSDL2-2.0.0.dylib.orig-backup"
SHIM="$(cd "$(dirname "$0")" && pwd)/libSDL2-2.0.0.dylib"

case "${1:-status}" in
install)
    [ -f "$SHIM" ] || { echo "build the shim first: see build.sh"; exit 1; }

    if [ ! -f "$REAL" ]; then
        echo "== backing up the original SDL2"
        cp "$LIVE" "$BACKUP"
        mv "$LIVE" "$REAL"
        # Give the real library its own install name, or dyld will resolve the
        # shim's dlopen() back to the shim itself and recurse.
        install_name_tool -id "@rpath/libSDL2-2.0.0.real.dylib" "$REAL"
        codesign --force --sign - "$REAL" 2>/dev/null || true
        echo "   original kept at: $REAL"
    else
        echo "== original already set aside at $REAL"
    fi

    echo "== installing shim"
    cp "$SHIM" "$LIVE"
    codesign --force --sign - "$LIVE" 2>/dev/null || true
    echo "done. Wine will now report the wheel as force-feedback capable."
    ;;

revert)
    if [ -f "$REAL" ]; then
        echo "== restoring original SDL2"
        rm -f "$LIVE"
        mv "$REAL" "$LIVE"
        install_name_tool -id "@rpath/libSDL2-2.0.0.dylib" "$LIVE"
        codesign --force --sign - "$LIVE" 2>/dev/null || true
        echo "done. Wine is back to stock (no force feedback)."
    else
        echo "nothing to revert: no $REAL"
    fi
    ;;

status)
    echo "wine lib dir : $WINE_LIB"
    if [ -f "$REAL" ]; then
        echo "state        : SHIM INSTALLED"
        echo "  live       : $(ls -la "$LIVE" | awk '{print $5" bytes"}')  (shim)"
        echo "  original   : $(ls -la "$REAL" | awk '{print $5" bytes"}')"
    else
        echo "state        : stock SDL2 (no shim)"
        [ -f "$LIVE" ] && echo "  live       : $(ls -la "$LIVE" | awk '{print $5" bytes"}')"
    fi
    ;;

*)
    echo "usage: $0 {install|revert|status}"; exit 1;;
esac
