---
name: shim-scoping-and-periodic-effects
type: task
title: Shim follow-ups — oscillate periodic effects instead of holding peak, and optionally scope the library override to one bottle
area: wine
tags: [shim, sdl, ffb, polish]
status: active
updated: 2026-09-02
volatility: decays-with-code
provenance: 2026-09-01 session — known limitations recorded at build time, not discovered later
---

# Two known limits of the FFB shim, in priority order

**1. Periodic effects are held at peak magnitude (do this one first).** `apply_effect()` maps `SDL_HAPTIC_SINE`, `TRIANGLE`, `SAWTOOTHUP` and `SAWTOOTHDOWN` to a single constant force at the effect's peak magnitude — there is no oscillator. Constant force, which is what sims actually steer with, is exact; periodic effects are what games use for kerb rumble and road texture. In practice that means a steady pull where there should be vibration, and it may read as the wheel fighting you. Speed Dreams was observed creating a type `0x2` (SINE) effect, so this path is live in real use.

The fix is a timer thread that re-sends `11 00 <force>` at the effect's period, sampling the waveform against `effect.periodic.period`, `magnitude`, `offset` and `phase`. lg4ff runs its own high-resolution timer for exactly this. Acceptance: driving over kerbs produces felt vibration rather than a constant offset.

Also unimplemented: `SDL_HAPTIC_DAMPER`, `INERTIA` and `FRICTION` are advertised nowhere and ignored, and `SPRING` is collapsed onto the wheel's autocentre rather than a true positional spring. Damper in particular is worth adding — the wheel protocol supports it natively on a separate slot.

**2. The override is global to Whisky.** Detail and blast-radius testing in [[sdl2-shim-is-global-to-whisky]]. It was verified not to disturb the existing "click" bottle, so this is polish rather than a defect. If it ever needs scoping: give the bottle its own copy of Wine's `lib` directory, or set `DYLD_LIBRARY_PATH` in the launcher scripts instead of replacing the shared dylib.

**Cheapest possible resolution of both.** If a future Wine or runner links SDL3 instead of SDL2, delete the shim — SDL3 ≥ 3.4.0 ships `SDL_hidapi_lg4ff.c` with a proper effect engine, default-enabled on macOS, and SDL3 3.4.14 is already installed here via Homebrew. Worth re-checking on any runner upgrade before investing in the oscillator.
