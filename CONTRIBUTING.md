# Contributing

Bug reports and patches are welcome, particularly from anyone with a Logitech wheel that is *not* a G29 — the protocol is shared across the family, but only the G29 has been tested here.

## Before you open an issue

Run the diagnostics and include their output:

```bash
make                    # build into bin/
./bin/lgwheel --detect  # is the wheel seen, and in which mode?
./bin/ffb-probe         # what does Apple's stack say about it?
./bin/lgwheel --verify  # does the motor respond? (let go of the wheel)
```

For a Wine problem, add `LG4FF_SHIM_DEBUG=1` and include the `[lg4ff-shim]` lines, plus the relevant `WINEDEBUG=+hid,+dinput` trace.

## Especially useful contributions

- **A real periodic-effect engine.** `SINE`/`TRIANGLE`/`SAWTOOTH` are currently held at peak magnitude instead of oscillating. This is the biggest functional gap. SDL3's `SDL_hidapi_lg4ff.c` is the reference to follow.
- **`DAMPER` / `INERTIA` / `FRICTION`.** The wheel protocol supports these natively on separate slots; the shim ignores them.
- **Other wheels.** Add the product ID and report what happens.
- **Other Wine runners.** CrossOver and Sikarugir ship their own SDL2; the shim needs building against those.
- **Corrections to the prior-art section.** If something here is already documented elsewhere, that is worth an issue — an accurate map beats a novelty claim.

## Ground rules

- Keep the tools dependency-free: system frameworks only, no package manager needed to run them. (Building the shim needs SDL2 headers; running it does not.)
- Sources live in `src/`, shell in `scripts/`, build output in `bin/`. `make` is the only build entry point.
- The shim must build `-arch x86_64` and must forward every non-wheel device untouched.
- Anything asserted about hardware behaviour should be measured, not assumed. `--verify` exists for exactly this reason.
