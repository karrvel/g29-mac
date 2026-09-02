#!/bin/bash
# Rebuild every tool in this folder.
set -euo pipefail
cd "$(dirname "$0")"
echo "building ffb-probe..."
clang -O2 -o ffb-probe ffb-probe.c -framework IOKit -framework ForceFeedback \
      -framework CoreFoundation -Wno-deprecated-declarations
echo "building lgwheel..."
clang -O2 -o lgwheel lgwheel.c -framework IOKit -framework CoreFoundation
echo "building SDL2 FFB shim (x86_64 for Wine)..."
clang -arch x86_64 -O2 -dynamiclib -o libSDL2-2.0.0.dylib sdl2-lg4ff-shim.c \
      -I/opt/homebrew/include -framework IOKit -framework CoreFoundation \
      -install_name @rpath/libSDL2-2.0.0.dylib -Wno-deprecated-declarations
echo "done. Run ./install-shim.sh install to activate force feedback in Wine."
