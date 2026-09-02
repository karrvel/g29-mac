# Changelog

All notable changes to this project. Dates are ISO. This project does not yet publish tagged releases; entries are grouped by the day work landed.

## Unreleased

### Changed

- Restructured the repository: C sources moved to `src/`, shell scripts to `scripts/`, the SDL3 probe to `contrib/sdl3-probe/`, and build output to `bin/` (gitignored).
- Replaced `build.sh` with a `Makefile`. `make` builds everything; `make install-shim`, `make revert-shim`, `make shim-status`, `make check` and `make clean` cover the rest. The shim rule asserts that exactly 52 SDL symbols are exported and fails the build otherwise, since Wine `dlsym`s all of them.
- Added task-oriented guides under `docs/`. The knowledge base in `_knowledge/` remains the record of *why* each finding exists; `docs/` covers *how* to use the tools.
- `scripts/common.sh` finds the Whisky bottle by name rather than a hardcoded UUID, so the scripts work on any machine. Override with `G29_BOTTLE_NAME` or `WINEPREFIX`.

## 2026-09-02

### Added

- `docs/`, `CHANGELOG.md`, `CONTRIBUTING.md`, MIT `LICENSE`, and a macOS GitHub Actions build.
- `scripts/reset-bottle.sh` — clears a wedged Wine bottle, the usual cause of "wineserver crashed".
- `scripts/reset-lfs-controls.sh` — makes Live for Speed forget a stale binary wheel config.
- `lgwheel --axes` and `scripts/identify-pedals.sh` — identify which HID axis each pedal is by pressing them.
- `scripts/kb-sync.sh` — runs the knowledge-base maintenance loop and scrubs the absolute home path that `kb-sync.py` bakes into the generated index.

### Fixed

- The shim now reports the wheel as a generic joystick via `SDL_JoystickGetType`. Wine sets `desc.is_wheel` from that call and, for a wheel, emits Simulation Controls axes (`0xC4` accelerator, `0xC5` brake, `0xC6` clutch) instead of Generic Desktop `X`/`Y`/`Z`/`Rz`. Sims from the 2000s only bind Generic Desktop axes, so enabling force feedback had silently made their pedals unbindable. Set `LG4FF_SHIM_WHEEL_TYPE=wheel` to restore the previous behaviour.

## 2026-09-01

### Added

- `ffb-probe` — proves Apple's `ForceFeedback.framework` refuses the wheel (`FFIsForceFeedback()` returns `0x80000003`; the device exposes no `0x0F` PID usage page).
- `lgwheel` — userspace Logitech force-feedback driver over `IOHIDDeviceSetReport`, with `--verify`, which applies a known force and watches the steering axis move on its own rather than relying on feel.
- `sdl2-lg4ff-shim` — a drop-in `libSDL2-2.0.0.dylib` that reimplements SDL's haptic API over the wheel protocol, so Wine synthesises a DirectInput force-feedback device and Windows sims get real force feedback.

### Notes

- The 7-byte lg4ff command block must be padded to the HID interface's output report length (16 bytes on the G29's joystick interface) or the USB endpoint answers with a STALL (`0xe0005000`). Linux's `hid_hw_request` pads automatically; `IOHIDDeviceSetReport` does not.
- The wheel exposes two HID interfaces. Force feedback goes to the joystick one (usage page `0x01`, usage `0x04`), not the vendor-defined one.
