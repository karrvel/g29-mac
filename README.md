# g29-mac

[![build](https://github.com/karrvel/g29-mac/actions/workflows/build.yml/badge.svg)](https://github.com/karrvel/g29-mac/actions/workflows/build.yml)
[![licence: MIT](https://img.shields.io/badge/licence-MIT-blue.svg)](LICENSE)
[![platform: macOS arm64](https://img.shields.io/badge/platform-macOS%20arm64-lightgrey.svg)](#requirements)

**Force feedback for Logitech racing wheels on Apple Silicon macOS — including inside Windows sims running under Wine.**

macOS ships no driver for Logitech's wheels, and every standard force-feedback API refuses them. This repo contains three small C programs that work around that: a diagnostic, a userspace driver, and a drop-in `libSDL2` replacement that makes Wine expose a real DirectInput force-feedback device to Windows games.

Verified on a MacBook Pro (M4 Pro, macOS 26.6.2) with a Logitech G29 Driving Force, running Live for Speed and Speed Dreams under Whisky.

> **Honest framing:** almost none of the underlying technique is new — see [Prior art](#prior-art-and-credits). The Logitech FFB protocol has been documented in the Linux kernel for over a decade, and SDL3 already implements it in userspace. What this repo adds is a macOS-specific implementation, an open-source route to force feedback in Wine-hosted sims on macOS, and a written record of several macOS-only pitfalls that cost real debugging time.

---

## The problem

Four things have to be true before a wheel produces torque in a game. On macOS, three of them are false:

| Layer | State | Why |
|---|---|---|
| Apple `ForceFeedback.framework` | ❌ | Only drives HID **PID-class** hardware. The G29 exposes usage pages `0x01`, `0x09`, `0xFF00` and **no `0x0F` PID page**, so `FFIsForceFeedback()` returns `0x80000003`. Nothing on a stock system registers `kIOForceFeedbackLibTypeID`. |
| Apple `GCRacingWheel` (macOS 13+) | ❌ | Input-only. No haptics, effects or output API. |
| SDL2 haptics on macOS | ❌ | Its backend is built on `ForceFeedback.framework`, so it inherits the failure and reports zero haptic devices. |
| Wine / DirectInput | ❌ | `winebus.sys` only synthesises a DirectInput force-feedback (PID) device when its SDL backend reports the joystick is haptic. It never does. Wine's macOS `iohid` backend has no haptics entry points at all — FFB was removed from Wine's macOS path in 2021 and never replaced. |
| **The wheel's motor itself** | ✅ | Speaks Logitech's vendor protocol. Reachable from ordinary userspace via HID output reports — no kext, no DriverKit, no admin rights. |

That last row is the entire opening.

## What's here

```
src/                  the three C programs
scripts/              launchers, installer, diagnostics
docs/                 guides — installation, troubleshooting, how it works, games
contrib/sdl3-probe/   an independent SDL3-based probe, kept as a second opinion
_knowledge/           the findings, as a small knowledge base
bin/                  build output (gitignored)
```

| Component | Purpose |
|---|---|
| `src/ffb-probe.c` | Diagnostic. Enumerates HID devices and asks Apple's stack whether it can drive them. Proves the refusal below rather than asserting it. |
| `src/lgwheel.c` | Userspace driver. `--detect`, `--range`, `--autocentre`, `--force`, `--test`, `--verify`, `--axes`. |
| `src/sdl2-lg4ff-shim.c` | The bridge. A drop-in `libSDL2-2.0.0.dylib` reimplementing SDL's haptic API over the wheel protocol, so Wine builds a real FFB device for Windows games. |
| `scripts/install-shim.sh` | `install` / `revert` / `status`. Always reversible. |
| `scripts/identify-pedals.sh` | Press pedals, learn which HID axis each one is. |
| `scripts/reset-bottle.sh` | Clears a wedged Wine bottle (`wineserver crashed`). |

## Documentation

- [Installation and setup](docs/installation.md)
- [Troubleshooting](docs/troubleshooting.md) — indexed by symptom
- [How it works](docs/how-it-works.md)
- [Running sims](docs/games.md)
- [The knowledge base](_knowledge/INDEX.md) — every finding, with provenance

## Quick start

```bash
git clone https://github.com/karrvel/g29-mac.git
cd g29-mac
make                    # builds into bin/

./bin/lgwheel --detect  # is the wheel seen?
./bin/lgwheel --verify  # objective proof of torque — let go of the wheel first
```

`make help` lists the rest.

`--verify` applies a known force and watches the steering axis move on its own, so "does force feedback work?" is answered by measurement rather than by feel.

### Force feedback inside Wine games

```bash
make install-shim   # back up the real SDL2, drop the shim in
make shim-status
make revert-shim    # undo, byte-for-byte, at any time
```

The bottle also needs Wine's SDL backend enabled:

```bash
wine64 reg add "HKLM\System\CurrentControlSet\Services\winebus" /v "Enable SDL" /t REG_DWORD /d 1 /f
```

## How the bridge works

Wine `dlopen()`s `libSDL2-2.0.0.dylib` and `dlsym()`s 52 symbols from it. The shim *is* that library:

- **35 non-haptic symbols** are one-instruction assembly trampolines into the real SDL2 (renamed alongside). A naked `jmp` preserves every argument register, so one macro covers every signature without declaring 35 prototypes.
- **17 haptic symbols** are reimplemented against the wheel.

```
game → dinput8 → winebus PID device → SDL_Haptic → shim → lg4ff HID reports → motor
```

Anything that is not a Logitech wheel is forwarded to the real SDL2 untouched.

Two details that make or break it:

- **Build `-arch x86_64`.** Whisky's `winebus.so` is x86_64 under Rosetta; an arm64 dylib silently won't load.
- **`SDL_JoystickGetType` is overridden to report a generic joystick.** Wine sets `desc.is_wheel` from it, and for a wheel it emits Simulation Controls axes (`0xC4` accelerator, `0xC5` brake, `0xC6` clutch) instead of Generic Desktop `X`/`Y`/`Z`/`Rz`. Sims from the 2000s only bind Generic Desktop axes, so on a "proper" wheel descriptor their pedals cannot be assigned at all. Set `LG4FF_SHIM_WHEEL_TYPE=wheel` to restore it.

## Known limitations

- **Constant force is exact; periodic effects are approximated.** `SINE`/`TRIANGLE`/`SAWTOOTH` are held at peak magnitude rather than oscillated, so kerb and road texture read as a steady pull. Constant force is what sims actually steer with, so this matters less than it sounds — but it is the main thing worth improving.
- **`SPRING` maps to the wheel's autocentre**, not a true positional spring. `DAMPER`, `INERTIA` and `FRICTION` are unimplemented.
- **The shim is global to the Wine install**, not per bottle. Harmless in practice (it gates on vendor/product ID), but worth knowing.
- **Built against Whisky's Wine 7.7.** Another runner means rebuilding against its SDL2.
- **Don't run two force-feedback consumers at once** — SDL takes no exclusive access on macOS.

## Prior art and credits

This project stands almost entirely on existing work. Specifically:

- **[berarma/new-lg4ff](https://github.com/berarma/new-lg4ff)** (GPL-2.0) — the Linux kernel driver. The command set in `lgwheel.c` was transcribed from `hid-lg4ff.c`, and the device ID table from Linux's `hid-ids.h`. Original authors include Simon Wood, Michal Malý and Katharine Chui.
- **[libsdl-org/SDL PR #11598](https://github.com/libsdl-org/SDL/pull/11598)** and **[#11591](https://github.com/libsdl-org/SDL/pull/11591)** by Kethen — G29 input and FFB over hidapi, now shipping as `SDL_hidapi_lg4ff.c` in **SDL 3.4.0+, enabled by default on macOS**. If you are writing new code, use SDL3 and you get all of this for free.
- **[Kethen/lg4ff_userspace](https://github.com/Kethen/lg4ff_userspace)** — a userspace port of the same driver for Linux.
- **[misarb/G29cpp](https://github.com/misarb/G29cpp)** — a C++ hidapi library for the G29 that documents macOS use.
- **[berarma/oversteer](https://github.com/berarma/oversteer)** and **[ffbtools](https://github.com/berarma/ffbtools)** — wheel management and FFB protocol documentation for Linux.

**On Linux this problem is already solved end to end**: Wine + sdl2-compat backed by SDL3 gives DirectInput force feedback in games — the same chain this repo builds by hand for macOS (see [sdl2-compat#306](https://github.com/libsdl-org/sdl2-compat/issues/306)).

**Commercial macOS products** already deliver Wine-hosted FFB: [Torqer](https://torqer.app) (~$12) and CrossWheel (~€20). Their mechanisms are undocumented; this repo is an independent, free alternative rather than a reimplementation of either.

### What appears to be new here

Modest, and stated carefully — I could not find these documented anywhere:

1. **An open-source SDL2 shim giving Wine-on-macOS force feedback.** The chain is standard on Linux with upstream components; this is a macOS build of it that needs no newer Wine and no newer SDL.
2. **The 16-byte output report requirement.** The lg4ff command block is 7 bytes. Linux's `hid_hw_request` pads to the report-descriptor length automatically; `IOHIDDeviceSetReport` does not, and the USB endpoint answers with a **STALL (`0xe0005000`)**. Also: the wheel exposes two HID interfaces, and the FFB path is the joystick one (usage page `0x01`, usage `0x04`), not the vendor-defined one.
3. **`SDL_JoystickGetType` → Wine descriptor → old sims.** Reporting a wheel makes Wine emit Simulation Controls axes, which silently makes pedals unbindable in older titles.

If any of this *is* documented elsewhere, please open an issue — an accurate map of prior art is more useful than a novelty claim.

## Licence

MIT — see [LICENSE](LICENSE).

The lg4ff command constants in `lgwheel.c` and `sdl2-lg4ff-shim.c` were transcribed from GPL-2.0 Linux sources as **protocol documentation**; no source code was copied. This follows the precedent set by SDL, which ships its own port of the same driver under the permissive zlib licence with attribution to the original authors. If you believe the attribution here is insufficient, please open an issue.

Not affiliated with or endorsed by Logitech. "Logitech", "G29" and related marks belong to their owners.
