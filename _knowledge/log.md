---
type: log
title: "g29-mac knowledge — append-only build/session log"
updated: 2026-09-02
---

# log

**Append-only** chronological record. One line per session/event:
`## [YYYY-MM-DD] <kind> | <summary>`. Recent last. Find recent: `grep '^## \[' log.md | tail`.

## [2026-09-01] build | G29 force feedback made to work on Apple Silicon, two of three sims installed
Proved Apple's ForceFeedback stack refuses the G29 (`FFIsForceFeedback` -> 0x80000003, no PID usage page) and that SDL2 and Wine inherit that failure. Reached the motor instead with raw Logitech HID output reports; `lgwheel --verify` measured the wheel driving itself across 100% of its travel. Built `sdl2-lg4ff-shim` to substitute Wine's SDL2 so Windows sims get a real DirectInput FFB device — verified end to end, Live for Speed emits a constant-force effect and Speed Dreams emits constant + periodic. Installed Live for Speed and Speed Dreams 2.4.2 into a new Whisky bottle ("Sim Racing"); RaceRoom staged but blocked on an interactive Steam login. Hard-won details: 7-byte commands must be padded to the interface's 16-byte report or the endpoint STALLs, and the FFB interface is the joystick one (usage 01:04), not the vendor-defined one.

## [2026-09-02] kb-init | Vault created from the carryover kit
Seeded from `repos/carryover/template`, then distilled this session into 15 shards rather than leaving the structure empty. Tooling copied to `_meta/`, health gate wired in `.githooks/`.

## [2026-09-02] fix | Steam bootstrap unblocked; G29 pedal mis-binding diagnosed
Steam in the bottle was failing fatally with "needs to be online to update" while the host had working internet. Cause was Wine's WinHTTP running WPAD proxy auto-detection across all 26 network interfaces on this machine (8 VPN tunnels + Docker/UTM/VirtualBox bridges), stalling the manifest request until the updater gave up. Disabled proxy autodetect via the `WinHttpSettings` registry blob; Steam then self-updated to a full 1.6 GB client and now reaches the login screen. Separately, measured the G29's axes: only four exist, and `Z`/`Rz` rest at full scale while `Y` rests near zero — which is why LFS auto-bound the clutch (`Y`) as the throttle and ignored the real pedals. Added `identify-pedals.sh` and `lgwheel --axes` to measure this directly. Fix is in-game re-calibration, not editing the binary `.csf`.

## [2026-09-02] fix | Pedals and steering working in LFS — the FFB change had broken input
Enabling Wine's SDL backend for force feedback also made winebus emit a wheel descriptor with Simulation Controls axes (Steering 0xC8 / Accelerator 0xC4 / Brake 0xC5 / Clutch 0xC6). LFS only binds Generic Desktop axes, so its pedals became unbindable — an input regression caused by the FFB fix, which read as two unrelated faults. Fixed by overriding `SDL_JoystickGetType` in the shim to report the wheel as a generic joystick; dinput then exposes GUID_X/Y/Z/Rz while `SDL_JoystickIsHaptic` still claims haptics, so both work. Confirmed by the user: steering and all three pedals now correct. Added `reset-bottle.sh` (wedged wineserver) and `reset-lfs-controls.sh` (stale binary .csf).

## [2026-09-02] perf | LFS lag fixed: DXVK + trimmed quality, ~39-58 fps spiky -> 100 fps at 8.5-11.5 ms
Whisky creates bottles with DXVK disabled, so LFS was running on wined3d's own Vulkan backend — slow and, worse, badly paced. CPU sat at ~12% while the frame rate was low, which ruled out Rosetta/translation and pointed at presentation. Installed DXVK into the bottle (native DLL overrides, builtin originals kept in dll-backup-builtin/) and trimmed the expensive LFS settings (AA off, mirror AA off, shadow cascades 4->2, dynamic reflect 8 4 -> 2 1, external mirror off). Result measured with the DXVK overlay: 100 fps with an 8.5-11.5 ms frame-time spread, versus 58 fps with heavy spikes before. `LFS_HUD=1 ./play-lfs.sh` shows the overlay on demand.

