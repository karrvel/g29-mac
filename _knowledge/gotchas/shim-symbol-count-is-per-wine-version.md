---
name: shim-symbol-count-is-per-wine-version
type: gotcha
title: The SDL2 shim must export every symbol the host Wine resolves — 52 for wine-7.7, 55 for wine-staging 11.16 — and a missing one is a call to address zero
area: wine
tags: [shim, sdl2, wine11, whisky, ffb, symbols]
status: active
updated: 2026-09-03
volatility: durable
provenance: 2026-09-03 session — found by diffing winebus.so's SDL symbol strings against the shim's exports after crashes on wine-staging 11.16
---

# A shim built for one Wine will crash silently on another

Wine's `winebus` resolves the SDL2 entry points it needs **by name, at runtime**. The shim replaces that library, so it must export every one of them. If a name is missing the lookup returns NULL, Wine calls it anyway, and the process dies on a null instruction pointer — with no Wine error, no shim log line, and a backtrace that points into the *game*, not at us.

The set is not fixed across Wine versions:

| Wine | SDL symbols resolved |
|---|---|
| Whisky's wine-7.7 | 52 |
| wine-staging 11.16 | **55** — adds `SDL_HapticNumAxes`, `SDL_HapticSetAutocenter`, `SDL_JoystickGetSerial` |

Check it against any new runner before trusting the shim there:

```bash
WB=$(find "<wine-tree>" -name winebus.so | head -1)
strings -a "$WB" | grep -oE '^SDL_[A-Za-z0-9_]+$' | sort -u > /tmp/wants.txt
nm -gU bin/libSDL2-2.0.0.dylib | awk '{print $3}' | sed 's/^_//' \
  | grep -E '^SDL_' | sort -u > /tmp/have.txt
comm -23 /tmp/wants.txt /tmp/have.txt          # anything here is a latent crash
```

`SDL_GAMECONTROLLERCONFIG` shows up in that diff and is a false positive — it is an environment-variable name, not a function.

**Two of the three needed real implementations, not blind forwards.** The shim owns its haptic handles, so `SDL_HapticNumAxes` and `SDL_HapticSetAutocenter` must check `is_ours(h)` before touching the real SDL2 — forwarding our pointer into a library that never allocated it is its own crash. `NumAxes` returns 1 (the wheel has one force axis, steering) and `SetAutocenter` accepts and no-ops so a game can take centring over.

**And a NULL that only appears once the symbol exists:** the G29 reports no serial, so real SDL2 returns NULL from `SDL_JoystickGetSerial`. That NULL propagates outward to callers under no obligation to check it, so the shim substitutes `""`.

The Makefile asserts the export count — it was `52`, now `55`. Keep that assertion honest when moving to a new Wine, because it is the only thing that turns this from a three-minutes-in crash into a build failure. See [[sdl2-shim-is-global-to-whisky]] for the installation model and [[raceroom-crashes-during-load]] for a crash in the same period that these fixes did *not* explain.
