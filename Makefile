# g29-mac — force feedback for Logitech wheels on Apple Silicon macOS
#
#   make              build everything into bin/
#   make install-shim install the Wine force-feedback bridge
#   make revert-shim  put Wine's original SDL2 back
#   make check        run the diagnostic against an attached wheel
#   make clean        remove build output

CC      ?= clang
CFLAGS  ?= -O2 -Wall
SRC     := src
BIN     := bin

# ForceFeedback is deprecated-adjacent but still the only way to ask macOS
# whether it can drive a device at all — which is precisely what ffb-probe tests.
QUIET_DEPRECATION := -Wno-deprecated-declarations

FW_COMMON := -framework IOKit -framework CoreFoundation
FW_PROBE  := $(FW_COMMON) -framework ForceFeedback

# The shim replaces the SDL2 that Wine loads. Wine's winebus.so is x86_64 under
# Rosetta, so an arm64 build would silently never load. This is not optional.
SHIM_ARCH := -arch x86_64
SDL_INC   := $(shell brew --prefix 2>/dev/null)/include

TOOLS := $(BIN)/ffb-probe $(BIN)/lgwheel
SHIM  := $(BIN)/libSDL2-2.0.0.dylib

.PHONY: all tools shim check install-shim revert-shim shim-status clean help

all: tools shim

tools: $(TOOLS)

shim: $(SHIM)

$(BIN):
	@mkdir -p $(BIN)

$(BIN)/ffb-probe: $(SRC)/ffb-probe.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $< $(FW_PROBE) $(QUIET_DEPRECATION)

$(BIN)/lgwheel: $(SRC)/lgwheel.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $< $(FW_COMMON)

$(SHIM): $(SRC)/sdl2-lg4ff-shim.c | $(BIN)
	$(CC) $(SHIM_ARCH) $(CFLAGS) -dynamiclib -o $@ $< \
		-I$(SDL_INC) $(FW_COMMON) \
		-install_name @rpath/libSDL2-2.0.0.dylib $(QUIET_DEPRECATION)
	@n=$$(nm -gU $@ | grep -c " T _SDL_"); \
	  if [ "$$n" -ne 55 ]; then \
	    echo "ERROR: shim exports $$n SDL symbols, expected 55 — Wine dlsyms all of them"; \
	    exit 1; \
	  else echo "  shim exports $$n SDL symbols ✓"; fi

check: $(TOOLS)
	@./$(BIN)/ffb-probe || true
	@echo
	@./$(BIN)/lgwheel --detect || true

install-shim: $(SHIM)
	@scripts/install-shim.sh install

revert-shim:
	@scripts/install-shim.sh revert

shim-status:
	@scripts/install-shim.sh status

clean:
	rm -rf $(BIN)
	rm -f contrib/sdl3-probe/g29probe contrib/sdl3-probe/ffcheck

help:
	@echo "g29-mac"
	@echo "  make               build tools + shim into bin/"
	@echo "  make check         probe Apple's FFB stack, then detect the wheel"
	@echo "  make install-shim  install the Wine force-feedback bridge"
	@echo "  make revert-shim   restore Wine's original SDL2"
	@echo "  make shim-status   is the bridge installed?"
	@echo "  make clean         remove build output"
	@echo
	@echo "Then:  ./bin/lgwheel --verify        objective proof of torque"
	@echo "       scripts/play-lfs.sh           launch Live for Speed"
