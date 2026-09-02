# sdl3-probe

Two small probes written against SDL3 rather than this repo's own driver, kept as an independent check on the main tooling.

SDL 3.4.0 and later ship `SDL_hidapi_lg4ff.c` — a userspace Logitech force-feedback driver ported from Linux's `new-lg4ff`, enabled by default on macOS. That means SDL3 can drive the wheel without anything from this repo. These probes confirm that independently, which is useful when you want a second opinion on whether a problem is in the hardware, in macOS, or in this project's code.

- `g29probe.c` — opens the wheel through SDL3, reports joystick and haptic capabilities, and runs a constant-force effect.
- `ffcheck.c` — counts the haptic devices Apple's `ForceFeedback.framework` exposes. Expected answer with a G29 attached: zero. That is the whole reason this repo exists.

## Building

Not built by the top-level `Makefile`, because they need SDL3 from Homebrew:

```bash
brew install sdl3
cd contrib/sdl3-probe
clang g29probe.c -o g29probe $(pkg-config --cflags --libs sdl3)
clang ffcheck.c  -o ffcheck  -framework ForceFeedback -framework IOKit -framework CoreFoundation
```

If you are writing new code and can depend on SDL3, prefer `SDL_OpenHapticFromJoystick` over anything in this repo. See the prior-art section of the top-level [README](../../README.md).
