---
name: sdl2-shim-is-global-to-whisky
type: gotcha
title: install-shim.sh patches Whisky's shared Wine library, so it affects every bottle on the machine, not just the sim-racing one
area: wine
tags: [shim, whisky, blast-radius, sdl]
status: active
updated: 2026-09-02
volatility: decays-with-code
provenance: 2026-09-01 session — flagged in review, then tested against the pre-existing "click" bottle
---

# The FFB shim is installed once for all of Wine, not per bottle

`install-shim.sh` replaces `~/Library/Application Support/com.isaacmarovitz.Whisky/Libraries/Wine/lib/libSDL2-2.0.0.dylib`. That path is Whisky's **global** Wine installation — shared by every bottle. Installing the shim for the sim-racing bottle silently puts it in the load path of the pre-existing "click" bottle and anything else created later. It is easy to assume the change is scoped to one bottle; it is not.

In practice the blast radius is nil, because the shim gates hard on the device: `joystick_is_our_wheel()` checks vendor `0x046d` plus a known wheel product ID, and everything that isn't a Logitech wheel is forwarded to the real SDL2 untouched. That was tested, not assumed — the "click" bottle was launched with the shim installed and started normally, loading the real SDL2 with no behaviour change.

Two things to keep in mind:

- **The trampolines are the risk surface, not the haptic overrides.** 35 of the 52 exported symbols are one-instruction assembly jumps into the renamed real library, and the macro's fallback path returns 0 rather than crashing if a pointer failed to resolve. A silent zero would look like "wheel detected but no axes", not like a crash. Confirmed working: dinput traces show real data coming back through those trampolines (`vid 046d, pid c24f, version 8900`, full product name) — zeros would have been unmistakable.
- **Reverting is one command and always safe:** `scripts/install-shim.sh revert` restores the original library byte-for-byte. The original is kept alongside as `libSDL2-2.0.0.real.dylib` with its install name rewritten — that rename matters, because dyld deduplicates by install name and would otherwise resolve the shim's own `dlopen` back to the shim and recurse.

If per-bottle scoping is ever needed, the fix is to give the bottle its own copy of Wine's `lib` dir, or set `DYLD_LIBRARY_PATH` in the launcher scripts instead of replacing the shared dylib. See [[shim-scoping-and-periodic-effects]].
