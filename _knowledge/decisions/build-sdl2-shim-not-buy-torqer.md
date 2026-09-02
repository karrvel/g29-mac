---
name: build-sdl2-shim-not-buy-torqer
type: decision
title: Get in-game force feedback by substituting Wine's SDL2 with a shim, rather than paying for Torqer or CrossWheel
area: wine
tags: [architecture, shim, sdl, dinput, ffb]
status: active
updated: 2026-09-02
volatility: decays-with-code
provenance: 2026-09-01 session — built and verified end to end against Live for Speed and Speed Dreams
---

# Replace the one library that gates the whole chain

**Context.** Raw HID gives torque but no game can reach it ([[raw-hid-not-apple-forcefeedback]]), and Wine won't expose a force-feedback device because SDL2 reports no haptics ([[wine-macos-has-no-ffb-path]]). Two commercial products solve exactly this — Torqer ($12 lifetime, 5-day trial) and CrossWheel (€19.99) — both of which bridge FFB into Wine-hosted Windows sims. Neither supports Whisky as a host; both want CrossOver ($74/yr) or Sikarugir.

**Decision.** Build a drop-in replacement for `libSDL2-2.0.0.dylib` that reimplements SDL's haptic API on top of the wheel protocol, and leave everything else forwarding to the real library.

**Why.** The blocker is a single boolean: Wine synthesises a DirectInput PID device if and only if SDL says the joystick is haptic. Wine loads SDL by `dlopen` + `dlsym` on 52 symbols, so the library is a clean, well-defined seam that requires patching neither Wine nor the games. Cost is zero and nothing is added to the paid-software stack. The requirement was explicitly free, and a paid host runner would have been needed on top of the paid bridge.

**How it is built.** 35 non-haptic symbols become one-instruction assembly trampolines (`jmp` through a resolved pointer) into the renamed real SDL2 — a naked jump preserves every argument register, so one macro covers every signature without declaring 35 prototypes. The 17 haptic entry points are reimplemented against the wheel. Must be compiled `-arch x86_64`, because Whisky's `winebus.so` is x86_64 under Rosetta.

**Consequences.**

- Verified end to end: Wine builds the PID collections (`set constant force`, `effect control`, `effect update`), enumerates `GUID_ConstantForce`, and both installed games create real effects that arrive at the shim.
- It is global to Whisky, not per bottle — see [[sdl2-shim-is-global-to-whisky]].
- It is tied to Whisky's Wine build. Moving to CrossOver or Sikarugir means rebuilding against that runner's SDL2, and if a future Wine adopts SDL3 the shim becomes unnecessary — SDL3 already has this natively.
- Constant force is exact; periodic effects are approximated. See [[shim-scoping-and-periodic-effects]].
