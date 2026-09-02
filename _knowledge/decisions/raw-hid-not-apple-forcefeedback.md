---
name: raw-hid-not-apple-forcefeedback
type: decision
title: Drive the wheel with raw Logitech HID output reports from userspace rather than through any Apple or SDL force-feedback API
area: hardware
tags: [architecture, lg4ff, iokit, ffb]
status: active
updated: 2026-09-02
volatility: durable
provenance: 2026-09-01 session — decision taken after ffb-probe proved the standard stack refuses the wheel
---

# Bypass the entire standard stack and speak Logitech's protocol directly

**Context.** Four candidate routes to torque were evaluated: Apple's `ForceFeedback.framework`, Apple's `GCRacingWheel`, SDL2's haptic API, and raw HID. The first three were eliminated by measurement, not by reading — see [[macos-forcefeedback-rejects-g29]].

**Decision.** Write Logitech's vendor FFB protocol — the same command set Linux implements in `hid-lg4ff` — straight to the wheel as HID output reports via `IOHIDDeviceSetReport`, from ordinary userspace.

**Why this and not the alternatives.** It needs no kernel extension, no DriverKit extension, no entitlement and no admin rights, which on Apple Silicon with SIP is decisive: the kext-era workarounds (FreeTheWheel, `LogitechForceFeedback.kext`) are simply dead here. It also cannot be broken by Apple deprecating a framework it doesn't use. The protocol is stable, documented by a maintained Linux driver, and unchanged for a decade across G25/G27/DFGT/G29/G923.

**Consequences.**

- It works, and it is proven: the wheel moved across 100% of its travel under command.
- The same code carries over to the G920, G923, G27, G25, DFGT and Driving Force with only a product-ID table entry, because they share the protocol.
- No game can reach it directly — a game talks DirectInput or SDL, not raw HID. That gap is what forced the second decision, [[build-sdl2-shim-not-buy-torqer]].
- SDL does not take exclusive access on macOS, so two force-feedback consumers running at once will fight over the motor. Don't run `lgwheel` and a game at the same time.

**Validation.** Upstream agrees: SDL3 ≥ 3.4.0 solved the same problem the same way, in `SDL_hidapi_lg4ff.c`, ported from `new-lg4ff` and default-enabled on macOS. This is the sanctioned approach, not a hack.
