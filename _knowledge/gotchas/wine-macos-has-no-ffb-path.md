---
name: wine-macos-has-no-ffb-path
type: gotcha
title: Wine on macOS gives games a wheel with working axes and no force feedback — the failure is at capability detection, not at effect playback
area: wine
tags: [wine, winebus, dinput, sdl, ffb]
status: active
updated: 2026-09-02
volatility: decays-with-code
provenance: 2026-09-01 session — verified against Whisky 2.3.5 (wine-7.7) by inspecting winebus.so symbols and running +hid,+dinput traces
---

# Steering works, pedals work, buttons work, and no game ever produces torque

This looks like a game configuration problem and is not. Stock Wine on macOS enumerates the wheel fine — `+dinput` traces show the device found by name, `usage 0001:0004` — but no game can create a force-feedback effect on it.

The mechanism: Wine's `winebus.sys` only synthesises a DirectInput force-feedback (PID) device when its backend reports the joystick is **haptic**. Wine has two relevant backends and neither delivers on macOS out of the box:

- The **iohid** backend (macOS-native) is registered as a `raw_device_vtbl`, which has no haptics entry points at all. Confirmed by symbols: `winebus.so` imports only `IOHIDDeviceOpen/Close/GetReport/SetReport/RegisterInputReportCallback` — there is no `ForceFeedback` linkage in that path. It passes the wheel's real report descriptor through verbatim, and because the G29 is not PID-class ([[macos-forcefeedback-rejects-g29]]), dinput never sets `DIDC_FORCEFEEDBACK` and `CreateEffect` fails before a single output report is attempted.
- The **sdl** backend does have the full PID vtbl (`sdl_device_physical_effect_control`, `…_effect_update`, `…_device_set_gain`, `sdl_device_haptics_start`) — but it asks SDL2, and SDL2 on macOS reports no haptic device for this wheel.

So the capability check fails at the very first gate and everything downstream is dead code. This is a **regression, not a gap**: CrossOver shipped working macOS force feedback via `joystick_osx.c` and `ForceFeedback.framework` until that file was deleted in the 2021 HID stack rewrite, with no macOS replacement. Every CrossOver changelog from 22.0.0 through 26.3.0 mentions force feedback zero times.

Practical notes: the SDL backend is not on by default — set `HKLM\System\CurrentControlSet\Services\winebus` → `Enable SDL` (DWORD 1), which is what makes the shim reachable. There is no registry key, DLL override or winecfg toggle that turns FFB on by itself; the code path genuinely does not exist. The fix is [[build-sdl2-shim-not-buy-torqer]].
