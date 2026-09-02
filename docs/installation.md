# Installation and setup

This gets you from a fresh clone to a wheel that provably produces torque, and then — optionally — to force feedback inside Windows sims running under Wine. Everything here is a userspace build: no kext, no DriverKit extension, no admin rights.

## Requirements

- An Apple Silicon Mac running macOS 13 or later. Developed and verified on an M4 Pro, macOS 26.6.2.
- Xcode command line tools: `xcode-select --install`.
- A supported Logitech wheel, connected by USB **and** to its external power brick.
- For the Wine bridge only: [Whisky](https://github.com/Whisky-App/Whisky) with a bottle, and SDL2 headers — `brew install sdl2`.

### Supported wheels

The product ID table lives in `src/lgwheel.c`. Anything with Logitech's vendor ID `0x046d` and one of these product IDs is recognised:

| Product ID | Wheel |
|---|---|
| `c24f` | G29 Driving Force Racing Wheel |
| `c260` | G29 (PS4 native mode) |
| `c262` | G920 Driving Force Racing Wheel |
| `c266` | G923 Racing Wheel (PC/PS) |
| `c267` | G923 Racing Wheel (PlayStation) |
| `c26e` | G923 Racing Wheel (Xbox) |
| `c295` | MOMO Force |
| `c298` | Driving Force Pro |
| `c299` | G25 Racing Wheel |
| `c29a` | Driving Force GT |
| `c29b` | G27 Racing Wheel |
| `ca03` | MOMO Racing |

One further ID, `c294` ("Driving Force / Formula EX"), is not a model but a backward-compatibility mode in which a newer wheel pretends to be an older one, giving up full force feedback and the 900° range. `./bin/lgwheel --detect` flags it explicitly and `./bin/lgwheel --native` sends the mode-switch command to leave it. On a G29 with the selector left alone you should not need either.

Only the G29 has been exercised on hardware here; the rest of the table is transcribed from Linux's `hid-ids.h` and shares the same command set.

## Clone and build

```bash
git clone https://github.com/karrvel/g29-mac.git
cd g29-mac
make
```

`make` builds three artefacts into `bin/`, which is a build directory and is gitignored:

| Path | What it is |
|---|---|
| `./bin/lgwheel` | The userspace driver — detection, rotation range, autocentre, constant force, verification. |
| `./bin/ffb-probe` | The diagnostic — asks macOS's own force-feedback stack whether it can drive the attached devices. |
| `./bin/libSDL2-2.0.0.dylib` | The Wine bridge, built `-arch x86_64`. Not used unless you install it. |

The default target builds the shim as well as the tools, and the shim needs SDL2's headers (`-I$(brew --prefix)/include`). If you only want direct control of the wheel and no Wine, `make tools` skips the shim and its Homebrew dependency entirely.

The `-arch x86_64` on the shim is not a preference: Whisky's `winebus.so` runs x86_64 under Rosetta, and an arm64 dylib is silently never loaded. The Makefile carries the flag. The shim build also counts its exported symbols and fails loudly unless it exports exactly 52 — Wine `dlsym()`s all of them, so a short build is a broken build.

`make help` lists every target; `make clean` removes `bin/` along with the `contrib/sdl3-probe` binaries if you built those separately.

## First run: does it work?

Run these three in order.

### 1. Is the wheel seen?

```bash
./bin/lgwheel --detect
```

This lists every Logitech HID device with its product ID, usage page and output report size, names the wheel from the table above, and selects the joystick interface (usage page `0x01`, usage `0x04`) — the vendor-defined interface exists too but the force-feedback path is not on it. If no Logitech device appears at all, the tool prints a checklist; work through it before anything else.

### 2. What does macOS itself think?

```bash
./bin/ffb-probe
```

Expect it to say **no**. On a stock system it reports `FFIsForceFeedback: NO` for the wheel and exits with "macOS cannot drive this wheel's motor". That is the correct, expected result, not a failed installation — the G29 is not PID-class hardware, so Apple's stack refuses it, and this whole repo exists because of that refusal. The probe is here to demonstrate the refusal rather than assert it. Background: [../_knowledge/gotchas/macos-forcefeedback-rejects-g29.md](../_knowledge/gotchas/macos-forcefeedback-rejects-g29.md).

`ffb-probe` never moves the wheel unless you pass `--spin`.

### 3. Objective proof of torque

```bash
./bin/lgwheel --verify
```

Let go of the wheel first, and have no game or other force-feedback consumer running — SDL takes no exclusive access on macOS, and two consumers at once will fight.

`--verify` is the answer to "does force feedback actually work", and it is measured rather than felt. It reads the steering axis to establish a baseline, applies 80% force left, samples the axis again, applies 80% force right, samples again, and prints the swing. On the reference machine the wheel drove itself across 100% of its travel under commanded force, and the verdict line says so. If the swing is under 5% it reports no movement and names the likely causes: the wheel was held or blocked, the power brick is unplugged, or reports are being accepted and ignored.

`make check` runs the probe and then `--detect` in one go. It deliberately does not command torque; `--verify` and `--test` are the ones that move the wheel.

## The Input Monitoring permission gate

Reading and writing HID reports needs your terminal to hold macOS's **Input Monitoring** permission. Without it the wheel enumerates normally — `--detect` will list it — and then opening it fails:

```
IOHIDDeviceOpen failed (0x........).
Grant your terminal Input Monitoring in
System Settings > Privacy & Security > Input Monitoring, then retry.
```

`lgwheel` exits 3 at that point. The same gate can show up in `ffb-probe` as `FFCreateDevice: FAILED` ("capable but not openable"), and inside `lgwheel` as a command that comes back `kIOReturnNotPermitted` — each of those prints the same instruction.

The fix is exactly what the message says: System Settings → Privacy & Security → Input Monitoring, add the terminal application you are running from, then retry. Note that the permission belongs to the terminal, not to `lgwheel`, so a different terminal or a different launcher needs granting separately.

A distinct failure worth not confusing with this one: `IOReturn 0xe0005000` is a USB pipe STALL from a wrongly sized report, not a permissions problem — see [../_knowledge/gotchas/lg4ff-reports-must-be-16-bytes.md](../_knowledge/gotchas/lg4ff-reports-must-be-16-bytes.md).

## Wheel hardware notes

Two physical things account for most "it detects but nothing happens" reports.

**The external power brick must be connected.** USB alone enumerates the wheel and gives you buttons and axes, but the motor never moves. A healthy wheel performs a full left-right calibration sweep at power-on; if you did not see that sweep, check the brick before debugging software.

**Leave a G29's PS3/PS4 selector on PS4.** In that position the wheel reports `046d:c24f`, and force feedback is confirmed working there — `--verify` drove it across its full travel without the switch being touched. Advice circulating elsewhere says to move it to PS3; that advice does not describe this hardware on macOS, and following it means changing a working configuration. The reasoning and the measurement are in [../_knowledge/gotchas/keep-g29-selector-on-ps4.md](../_knowledge/gotchas/keep-g29-selector-on-ps4.md).

Logitech G HUB does not help here and does not need to be installed — on macOS it is a firmware updater whose driver extension only ever loads for two mice ([../_knowledge/gotchas/ghub-macos-does-nothing-for-wheels.md](../_knowledge/gotchas/ghub-macos-does-nothing-for-wheels.md)).

## Optional: the Wine bridge

Everything above works without Wine. This section is only for force feedback *inside* Windows sims running under Whisky.

### Install the shim

```bash
make install-shim      # or: scripts/install-shim.sh install
scripts/install-shim.sh status
```

This moves Whisky's real `libSDL2-2.0.0.dylib` aside to `libSDL2-2.0.0.real.dylib` (with its install name rewritten, and a second copy kept as `libSDL2-2.0.0.dylib.orig-backup`), then drops the shim in its place and ad-hoc signs it. `status` should afterwards report the shim live and the original preserved.

Two things to understand before running it:

- **The substitution is global to your Whisky install, not scoped to one bottle.** Every Wine process on the machine loads the shim. In practice the blast radius is nil, because the shim gates on vendor and product ID and forwards everything that is not a Logitech wheel to the real SDL2 untouched — that was tested against an unrelated bottle, not assumed. Details: [../_knowledge/gotchas/sdl2-shim-is-global-to-whisky.md](../_knowledge/gotchas/sdl2-shim-is-global-to-whisky.md).
- **This is a library substitution in a shared runtime, and it is ad-hoc signed.** That is structurally the same shape as a supply-chain attack, so only ever install a shim you built yourself from `src/sdl2-lg4ff-shim.c` in this repo. Never drop in a prebuilt `libSDL2-2.0.0.dylib` from anywhere else. The trust boundary — what the shim is granted, and what it is not — is written out in [../_knowledge/security/shim-is-an-unsigned-library-substitution.md](../_knowledge/security/shim-is-an-unsigned-library-substitution.md). Read it before you install.

### Enable Wine's SDL backend

The shim is only reachable if `winebus` uses its SDL backend, which is not Whisky's default. Set it once per bottle:

```bash
wine64 reg add "HKLM\System\CurrentControlSet\Services\winebus" /v "Enable SDL" /t REG_DWORD /d 1 /f
```

Whisky's `wine64` is not on `PATH`. The scripts in this repo already know where it is, so the reliable form is:

```bash
source scripts/common.sh
"$WINE" reg add "HKLM\System\CurrentControlSet\Services\winebus" /v "Enable SDL" /t REG_DWORD /d 1 /f
```

`common.sh` is sourced, not executed. It locates the bottle by name — "Sim Racing" by default — and exports `WINEPREFIX`. Override `G29_BOTTLE_NAME` for a differently named bottle, or set `WINEPREFIX` yourself to skip the search. If neither matches it fails with the path it searched.

Enabling the SDL backend has a side effect worth knowing about in advance: `winebus` then emits a wheel descriptor with Simulation Controls axes, and sims from the 2000s can only bind Generic Desktop axes, so their pedals become unassignable. The shim already works around this by reporting a generic joystick type. The full account is in [../_knowledge/gotchas/sdl-wheel-type-changes-axis-usages.md](../_knowledge/gotchas/sdl-wheel-type-changes-axis-usages.md).

### What the bridge does and does not give you

Constant force — what sims actually steer with — is exact. `SINE`, `TRIANGLE` and `SAWTOOTH` are approximated: they are held at peak magnitude rather than oscillated, so kerbs and road texture read as a steady pull rather than a vibration. `SPRING` maps to the wheel's autocentre rather than a true positional spring, and `DAMPER`, `INERTIA` and `FRICTION` are unimplemented. The shim is built against Whisky's Wine 7.7; another runner means rebuilding against that runner's SDL2.

### Revert

```bash
make revert-shim       # or: scripts/install-shim.sh revert
```

The original library is restored byte-for-byte and re-signed, and Wine is back to stock — with no force feedback. This is safe to run at any time, and worth running before uninstalling or upgrading Whisky so its updater never sees a substituted library. One residue: `revert` does not delete `libSDL2-2.0.0.dylib.orig-backup`, so that spare copy stays in Whisky's `lib` directory until you remove it by hand.

If `libSDL2-2.0.0.real.dylib` has gone missing, `revert` has nothing to restore from — recover from the `.orig-backup` copy, or reinstall Whisky.

## Uninstalling

There is nothing to uninstall system-wide: the build writes only into `bin/`, and the tools need no installed component.

1. If you installed the Wine bridge, revert it first: `make revert-shim`, then delete `libSDL2-2.0.0.dylib.orig-backup` from Whisky's Wine `lib` directory if you want it gone.
2. Optionally undo the registry change with `"$WINE" reg delete "HKLM\System\CurrentControlSet\Services\winebus" /v "Enable SDL" /f`.
3. `make clean` to remove `bin/`.
4. Delete the clone.
5. Optionally remove your terminal from System Settings → Privacy & Security → Input Monitoring.

Games installed into a Whisky bottle live outside this repo and are unaffected by any of the above; remove the bottle in Whisky if you want those gone too.
