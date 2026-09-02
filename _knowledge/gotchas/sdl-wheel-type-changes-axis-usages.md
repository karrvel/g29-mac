---
name: sdl-wheel-type-changes-axis-usages
type: gotcha
title: Enabling Wine's SDL backend makes it emit a wheel descriptor with Simulation Controls axes, and older sims cannot bind those pedals at all
area: wine
tags: [winebus, sdl, dinput, axes, lfs, shim]
status: active
updated: 2026-09-02
volatility: decays-with-code
provenance: 2026-09-02 session — diagnosed from HidP_GetSpecificValueCaps traces after LFS refused to bind pedals; fixed in the shim and re-verified
---

# Turning on force feedback silently relocated the pedals

Symptom: after enabling `Enable SDL` on `winebus` to get force feedback ([[wine-macos-has-no-ffb-path]]), Live for Speed detects the wheel and its buttons but **cannot bind the pedals at all** — the axes it offers do not correspond to anything the pedals do.

Cause: Wine's SDL backend sets `desc.is_wheel` from `SDL_JoystickGetType()`, and for a wheel it builds the HID report descriptor with **Simulation Controls** usages instead of Generic Desktop ones. Confirmed in a `+hid` trace:

```
usage_page 2, usage 200 (0xC8)  Steering
usage_page 2, usage 196 (0xC4)  Accelerator
usage_page 2, usage 197 (0xC5)  Brake
usage_page 2, usage 198 (0xC6)  Clutch
```

That is a *more* correct descriptor in the abstract, and it is exactly what breaks 2000s-era sims: they only bind Generic Desktop axes (`X`/`Y`/`Z`/`Rx`/`Ry`/`Rz`) and have no handling for page `0x02`. The iohid backend passes the device's real descriptor through, so this only appears once you switch to SDL — meaning **the FFB fix caused the input regression**, and the two looked unrelated.

**The fix, and why it is in the shim.** `SDL_JoystickGetType` is one of the 52 symbols Wine `dlsym`s, so the shim overrides it and returns `SDL_JOYSTICK_TYPE_UNKNOWN` for the wheel. Wine then builds a plain joystick descriptor while `SDL_JoystickIsHaptic` still reports haptics — so you keep both. Verified afterwards: dinput exposes exactly four axes, `GUID_XAxis` / `GUID_YAxis` / `GUID_ZAxis` / `GUID_RzAxis`, and LFS still creates a constant-force effect through the shim.

**Confirmed in-game 2026-09-02:** after the override plus a wipe of LFS's stale `.csf`, steering and all three pedals bind and work correctly in Live for Speed.

Set `LG4FF_SHIM_WHEEL_TYPE=wheel` to restore the wheel descriptor for a game that genuinely prefers Simulation Controls axes.

**Diagnostic tip:** `HidP_GetSpecificValueCaps` lines in a `+hid` trace are the app *querying*, so the set changes with the descriptor — useful as a signal but not proof. The unambiguous read is `match_device_object` in a `+dinput` trace, which names the actual axis GUIDs. See [[g29-pedal-axes-rest-inverted]] for which axis is which pedal.
