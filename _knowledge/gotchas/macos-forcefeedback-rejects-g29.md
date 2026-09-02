---
name: macos-forcefeedback-rejects-g29
type: gotcha
title: Apple's ForceFeedback.framework refuses the G29 — it is not PID-class hardware, so every standard FFB API returns nothing
area: macos
tags: [g29, forcefeedback, iokit, sdl, hid]
status: active
updated: 2026-09-02
volatility: durable
provenance: 2026-09-01 session — measured with ffb-probe.c on macOS 26.6.2, M4 Pro, G29 attached
---

# The whole standard force-feedback stack is a dead end for this wheel, and it fails silently

With the G29 physically attached and working, `FFIsForceFeedback()` returns `0x80000003` (unsupported) for **both** of its HID interfaces. Run `./ffb-probe` to reproduce: it reports 2 controller-class devices found, 0 force-feedback capable.

The cause is not a missing driver you can install — it is a hardware class mismatch. `ForceFeedback.framework` only drives USB-HID **PID-class** (Physical Interface Device) hardware. The G29's report descriptor exposes usage pages `0x01` (Generic Desktop), `0x09` (Button) and `0xFF00` (vendor-defined) and **no `0x0F` PID page**. It speaks Logitech's proprietary FFB protocol instead — the one Linux implements in `hid-lg4ff`. Independently confirmed: nothing on a stock macOS install registers `kIOForceFeedbackLibTypeID`, so even a PID-class wheel would find no plug-in.

The framework itself is alive and current — arm64e, built against the 26.6 SDK, no deprecation attributes — which makes this especially misleading. It is present, it is maintained, and it will never drive this wheel.

Two consequences that cost real time if you don't know them:

- **SDL2 inherits the failure.** Its macOS haptic backend is built directly on `ForceFeedback.framework`, so SDL2 reports zero haptic devices for the wheel. That is what breaks Wine — see [[wine-macos-has-no-ffb-path]].
- **Apple's `GCRacingWheel` (macOS 13+) is not a way out.** Its entire API surface is discovery, acquire/relinquish, `wheelInput` and capture. There is no haptics, effect or output API anywhere in it. It is input-only.

The motor is still perfectly reachable — just not through any of this. See [[raw-hid-not-apple-forcefeedback]].
