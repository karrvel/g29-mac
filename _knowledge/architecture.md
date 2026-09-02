---
name: architecture
type: architecture
title: How torque actually reaches the G29 on this Mac — the whole chain, and where every standard route breaks
area: cross
tags: [architecture, ffb, wine, hid]
status: active
updated: 2026-09-02
volatility: decays-with-code
provenance: 2026-09-01 session — every link in the chain verified on hardware, M4 Pro / macOS 26.6.2 / G29 046d:c24f
---

# System model

Read this first on a cold start. The one-sentence version: **macOS cannot drive this wheel through any standard API, but the motor is reachable from ordinary userspace, and one substituted library extends that reach into Windows games running under Wine.**

## The two paths that work

```
DIRECT (tools)
  lgwheel ──► IOHIDDeviceSetReport (16-byte padded) ──► G29 motor

IN-GAME (Windows sims under Wine)
  game ──► dinput8 ──► winebus.sys PID device ──► SDL_Haptic ──► sdl2-lg4ff-shim ──┘
```

Both terminate in the same place: Logitech's vendor FFB protocol written as raw HID output reports. The shim exists solely to let a game reach that path, because a game speaks DirectInput and never raw HID.

## Where each standard route dies

| Route | Fails at | Detail |
|---|---|---|
| Apple `ForceFeedback.framework` | `FFIsForceFeedback()` → `0x80000003` | wheel has no `0x0F` PID usage page; nothing registers `kIOForceFeedbackLibTypeID` |
| Apple `GCRacingWheel` | no such API | input-only; no haptics or effects surface at all |
| SDL2 haptics | 0 devices | its macOS backend is built on `ForceFeedback.framework`, so it inherits the failure |
| Wine `iohid` backend | no entry points | registered as `raw_device_vtbl`; haptics live only on `hid_device_vtbl` |
| Wine `sdl` backend | capability check | has the full PID vtbl, but asks SDL2 — which says no |
| Logitech G HUB | never loads | on macOS it is a firmware updater; its dext targets two mice |

Details in [[macos-forcefeedback-rejects-g29]], [[wine-macos-has-no-ffb-path]] and [[ghub-macos-does-nothing-for-wheels]].

## The insight the design turns on

Wine synthesises a DirectInput force-feedback device **if and only if** SDL reports the joystick is haptic. That single boolean is the entire gate. Wine loads SDL by `dlopen` + `dlsym` on 52 symbols, so replacing that library flips the gate without patching Wine or any game — see [[build-sdl2-shim-not-buy-torqer]].

## Evidence this works end to end

- `lgwheel --verify` — wheel drove itself across **100% of its travel** under commanded force.
- Wine builds the PID collections (`set constant force`, `effect control`, `effect update`) and enumerates `GUID_ConstantForce`.
- Live for Speed creates `type 0x1` (constant force); Speed Dreams creates `0x1` and `0x2` (periodic). Both arrive at the shim.

## Load-bearing constraints

The shim is **x86_64** (Wine runs under Rosetta) and **global to Whisky**, not per bottle ([[sdl2-shim-is-global-to-whisky]]). The bottle needs `Enable SDL` on `winebus`. Constant force is exact; periodic effects are approximated ([[shim-scoping-and-periodic-effects]]). Don't run two FFB consumers at once — SDL takes no exclusive access on macOS.
