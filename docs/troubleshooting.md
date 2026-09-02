# Troubleshooting

Indexed by symptom. Each entry says what the symptom actually means before it says what to do, because most of the failures here look like something they are not — a permissions error that is really a report-length error, an input regression that was really caused by the force-feedback fix, a network error that has nothing to do with the network.

Where a fault has a full write-up, this page links to it rather than repeating it. The depth lives in [`_knowledge/`](../_knowledge/INDEX.md).

## Symptom index

**The wheel and the tools**

- [No Logitech device found at all](#no-logitech-device-found-at-all)
- [Logitech devices present but none is a known wheel](#logitech-devices-present-but-none-is-a-known-wheel)
- [IOHIDDeviceOpen failed, or a command says not permitted](#iohiddeviceopen-failed-or-a-command-says-not-permitted)
- [Every command prints ok but the wheel never moves](#every-command-prints-ok-but-the-wheel-never-moves)
- [A command prints FAILED with a USB pipe STALL 0xe0005000](#a-command-prints-failed-with-a-usb-pipe-stall-0xe0005000)
- [Verification cannot read the steering axis](#verification-cannot-read-the-steering-axis)
- [ffb-probe reports 0 force-feedback capable](#ffb-probe-reports-0-force-feedback-capable)

**Inside a Wine game**

- [The game sees the wheel but produces no force feedback](#the-game-sees-the-wheel-but-produces-no-force-feedback)
- [The shim logs FATAL cannot load real SDL2](#the-shim-logs-fatal-cannot-load-real-sdl2)
- [The wheel pulls steadily where it should vibrate](#the-wheel-pulls-steadily-where-it-should-vibrate)
- [Pedals cannot be bound, or the clutch acts as the accelerator](#pedals-cannot-be-bound-or-the-clutch-acts-as-the-accelerator)
- [wineserver crashed, please enable coredumps](#wineserver-crashed-please-enable-coredumps)
- [The game lags or stutters](#the-game-lags-or-stutters)
- [Steam in the bottle says it needs to be online to update](#steam-in-the-bottle-says-it-needs-to-be-online-to-update)

**Building**

- [The shim will not build, or exports the wrong number of symbols](#the-shim-will-not-build-or-exports-the-wrong-number-of-symbols)

Then: [gathering diagnostics before opening an issue](#gathering-diagnostics-before-opening-an-issue).

## Start here

Two commands separate a hardware or permissions problem from a game or Wine problem:

```bash
make check          # ffb-probe, then lgwheel --detect
./bin/lgwheel --verify   # let go of the wheel first
```

`--verify` applies a known force and polls the steering axis, so it answers "does the motor respond?" by measurement. If it reports `TORQUE CONFIRMED`, the wheel, the cable, the power brick, the permissions and the protocol are all fine, and anything still wrong is above that line — in Wine, in the shim, or in the game.

---

## No Logitech device found at all

`./bin/lgwheel --detect` prints `No Logitech device found at all` and a checklist.

**What it means.** The wheel is not enumerating on USB. This is upstream of everything in this repo; no software here can help until the device appears.

**Fix.** Work down the checklist the tool prints, in order:

1. USB cable connected to the Mac. A hub can work, but try a direct port first.
2. The wheel's external power brick is plugged in and switched on. Without it the wheel can enumerate over USB and never move — which is the same symptom as broken force feedback.
3. On a G29, the PS3/PS4 selector is set to **PS4**. Leave it there: on PS4 the wheel reports `046d:c24f` and force feedback is confirmed working in that position, contrary to advice you will find elsewhere — [../_knowledge/gotchas/keep-g29-selector-on-ps4.md](../_knowledge/gotchas/keep-g29-selector-on-ps4.md).
4. The wheel performs a full left-right calibration sweep at power-on. If it does not, it is not initialising.

## Logitech devices present but none is a known wheel

`--detect` lists one or more `046d:xxxx` devices, each annotated `=> not a known wheel (mouse/keyboard/headset?)`, and then stops.

**What it means.** The product ID does not match the device table transcribed from Linux's `hid-ids.h`. Either another Logitech peripheral is attached and the wheel is not, or the wheel has enumerated under a product ID the table does not carry.

**Fix.** Check the printed product IDs. If the wheel enumerated as `046d:c294` ("Driving Force / Formula EX") it is in a backward-compatibility mode where it impersonates an older model and gives up full force feedback and the 900° range; `--detect` flags that case with `=> run --native to unlock full FFB and 900 degrees`, and `./bin/lgwheel --native` sends the mode-switch command. A wheel on the PS4 setting should not need this. If the ID is genuinely absent from the table, that is a supported-hardware gap worth an issue.

## IOHIDDeviceOpen failed, or a command says not permitted

`--detect` finds and selects the wheel, then prints `IOHIDDeviceOpen failed (0x…)`. Or a command line ends in `FAILED` with `-> not permitted`.

**What it means.** macOS is refusing HID access to the process, not to the machine. This is the one failure in this list that is a genuine permissions problem — and it is worth stating, because two others in this list *look* like one and are not.

**Fix.** Grant your terminal Input Monitoring under System Settings → Privacy & Security → Input Monitoring, then re-run. If you launch the tools from an editor or IDE terminal, that application is the one that needs the grant, not Terminal.app.

## Every command prints ok but the wheel never moves

Each line of `--test` reports `ok`, and nothing happens.

**What it means.** The reports were accepted by *something*. Usually that something is the wrong HID interface. The G29 publishes two interfaces under the same `046d:c24f`: the joystick one (usage page `0x01`, usage `0x04`, 16-byte output reports) is where force feedback lives, and the vendor-defined one (usage page `0xff00`, 20-byte reports) is not. `IOHIDManagerCopyDevices` returns an unordered `CFSet`, so code that grabs the first Logitech device it finds picks the wrong interface roughly half the time — which reads as flaky hardware. `lgwheel` matches on usage page and usage explicitly and prints `=> selected (joystick interface)` when it has the right one, and the absence of that line from `--detect` output is the signal that the wrong interface was picked. Detail in [../_knowledge/gotchas/lg4ff-reports-must-be-16-bytes.md](../_knowledge/gotchas/lg4ff-reports-must-be-16-bytes.md).

**Fix.** Confirm `=> selected (joystick interface)` appears. Then rule out the mundane causes, which the `--verify` verdict names for you: the wheel was being held or was blocked against something, or the power brick is unplugged. A wheel with no external power accepts every command and produces no torque. Run `./bin/lgwheel --verify` with the wheel free to turn and read the measured swing rather than trusting feel.

## A command prints FAILED with a USB pipe STALL 0xe0005000

```
   stop-slot-0             13 00 00 00 00 00 00  (7b)  FAILED
      -> USB pipe STALL: the wheel rejected this report length (7).
```

**What it means.** `0xe0005000` decodes to `kUSBHostReturnPipeStalled` — the device's USB endpoint answered the transfer with a STALL. Every Logitech force-feedback command is a 7-byte block, but the wheel expects a report of the interface's declared output length. Linux never surfaces this because `hid_hw_request` pads to the report-descriptor length automatically; `IOHIDDeviceSetReport` passes your buffer through verbatim. On the G29's joystick interface `MaxOutputReportSize` is 16, so the command must be sent as 7 bytes followed by 9 zero bytes.

**Fix.** In this repo this is already handled — `send_cmd` in `src/lgwheel.c` reads `kIOHIDMaxOutputReportSizeKey` off the device and pads to it, and the report length is printed in brackets on every command line, so a healthy G29 shows `(16b)`. If you are seeing `(7b)`, you are running a hand-rolled tool or a modified build; pad to the interface's report length and read that length from the device rather than hardcoding 16, since it differs per interface. Full write-up: [../_knowledge/gotchas/lg4ff-reports-must-be-16-bytes.md](../_knowledge/gotchas/lg4ff-reports-must-be-16-bytes.md).

## Verification cannot read the steering axis

`--verify` prints `could not read the steering axis — cannot verify automatically`, or `warning: no steering (GD X) element found`.

**What it means.** The axis read failed, not the force. A stationary wheel is the awkward case: IOHID input-value callbacks fire only when a value *changes*, so a wheel sitting still delivers no events at all and a callback-based baseline read comes back empty — with no error anywhere, which is easy to misdiagnose as a permissions or device problem. `lgwheel` avoids this by copying the Generic Desktop X element once and polling it with `IOHIDDeviceGetValue`; see [../_knowledge/gotchas/hid-values-need-polling-not-callbacks.md](../_knowledge/gotchas/hid-values-need-polling-not-callbacks.md).

**Fix.** Confirm the joystick interface was selected (above) — the vendor-defined interface has no steering element to find. Failing that, fall back to `./bin/lgwheel --test`, which commands torque without needing to read anything back, and judge it by hand.

## ffb-probe reports 0 force-feedback capable

```
RESULT: 2 controller-class device(s) found, 0 force-feedback capable.
```

**This is the expected result and not a fault.** It is what `ffb-probe` exists to demonstrate.

**What it means.** `FFIsForceFeedback()` returns `0x80000003` for both of the G29's HID interfaces. Apple's `ForceFeedback.framework` only drives USB-HID **PID-class** hardware; the G29 exposes usage pages `0x01`, `0x09` and `0xFF00` and no `0x0F` PID page, and nothing on a stock macOS install registers `kIOForceFeedbackLibTypeID` in the first place. It speaks Logitech's proprietary protocol instead. This is a hardware class mismatch, not a missing driver, and there is no download that changes it — the full argument, including why `GCRacingWheel` and SDL2 are also dead ends, is in [../_knowledge/gotchas/macos-forcefeedback-rejects-g29.md](../_knowledge/gotchas/macos-forcefeedback-rejects-g29.md).

**Fix.** None needed, and none possible along that path. Torque is reached by writing the wheel's own protocol as raw HID output reports, which is what `./bin/lgwheel` does and what the shim extends into Wine. Do not attempt to "fix" this by installing Logitech G HUB — on macOS it is a firmware updater for this wheel, and its driver extension only ever loads for two mice ([../_knowledge/gotchas/ghub-macos-does-nothing-for-wheels.md](../_knowledge/gotchas/ghub-macos-does-nothing-for-wheels.md)).

The one line here that *would* be a real fault is `RESULT: no wheel/joystick/Logitech HID device found` — that is the enumeration failure covered [above](#no-logitech-device-found-at-all).

---

## The game sees the wheel but produces no force feedback

Steering, pedals and buttons all work in the game; no effect is ever felt.

**What it means.** Stock Wine on macOS enumerates the wheel correctly and can never produce torque, because `winebus.sys` only synthesises a DirectInput force-feedback device when its backend reports the joystick is haptic. The macOS-native `iohid` backend has no haptics entry points at all, and the `sdl` backend asks SDL2, which inherits `ForceFeedback.framework`'s refusal. The failure is at capability detection, so everything downstream is dead code and no registry key or `winecfg` toggle turns it on by itself — [../_knowledge/gotchas/wine-macos-has-no-ffb-path.md](../_knowledge/gotchas/wine-macos-has-no-ffb-path.md).

**Fix.** Both halves of the bridge have to be in place. Check them in this order:

1. **The shim is installed.** `scripts/install-shim.sh status` should report `state : SHIM INSTALLED`. If not, `make install-shim`. Reverting is one command and always safe: `scripts/install-shim.sh revert` restores the original library byte-for-byte.
2. **Wine's SDL backend is enabled in the bottle.** Without this the shim is never loaded at all:
   ```bash
   wine64 reg add "HKLM\System\CurrentControlSet\Services\winebus" /v "Enable SDL" /t REG_DWORD /d 1 /f
   ```
3. **The shim is x86_64.** Whisky's `winebus.so` runs x86_64 under Rosetta, and an arm64 dylib silently never loads. `make` builds with `-arch x86_64`; a hand-rolled `clang` command usually does not. Check with `file ./bin/libSDL2-2.0.0.dylib`.
4. **The shim is actually reached.** Run the game with `LG4FF_SHIM_DEBUG=1` (see [diagnostics](#gathering-diagnostics-before-opening-an-issue)). A working chain logs `initialised`, `wheel opened, output report 16 bytes`, `SDL_JoystickIsHaptic -> YES (wheel)`, `SDL_HapticOpenFromJoystick -> shim device`, and then `new effect 0 type 0x1` when the game creates a constant-force effect. Where that sequence stops tells you which link is broken.
5. **Nothing else is holding the wheel.** SDL takes no exclusive access on macOS, so a second force-feedback consumer — another game, or `lgwheel` still commanding a force — will fight the one you are testing.

Two further constraints worth knowing: the shim was built against Whisky's Wine 7.7, so another runner means rebuilding against its SDL2, and the override is global to the Whisky installation rather than per bottle ([../_knowledge/gotchas/sdl2-shim-is-global-to-whisky.md](../_knowledge/gotchas/sdl2-shim-is-global-to-whisky.md)).

## The shim logs FATAL cannot load real SDL2

```
[lg4ff-shim] FATAL: cannot load real SDL2 at …/libSDL2-2.0.0.real.dylib: …
```

**What it means.** The shim forwards 33 of its 52 symbols to the genuine SDL2, which it expects to find beside itself as `libSDL2-2.0.0.real.dylib`. That file is missing. It is created by `scripts/install-shim.sh install`, which moves the original aside *and* rewrites its install name — the rename matters, because dyld deduplicates by install name and would otherwise resolve the shim's own `dlopen` back to the shim and recurse.

**Fix.** Do not copy `bin/libSDL2-2.0.0.dylib` into Wine's `lib` directory by hand. Use `scripts/install-shim.sh install` (or `make install-shim`), which handles the backup, the rename and the ad-hoc code signature. If the tree is already in a mixed state, `scripts/install-shim.sh revert` first, then install again.

## The wheel pulls steadily where it should vibrate

Kerbs, rumble strips and road texture read as a constant offset, or the wheel feels like it is fighting you rather than shaking.

**What it means.** This is a known limitation, not a bug. `SINE`, `TRIANGLE`, `SAWTOOTHUP` and `SAWTOOTHDOWN` are mapped to a single constant force held at the effect's peak magnitude — there is no oscillator. Constant force, which is what sims actually steer with, is exact. `SPRING` is collapsed onto the wheel's autocentre rather than a true positional spring, and `DAMPER`, `INERTIA` and `FRICTION` are unimplemented. Speed Dreams has been observed creating a type `0x2` (SINE) effect, so this path is live in real use.

**Fix.** None available today; the intended fix and its acceptance test are recorded in [../_knowledge/tasks/shim-scoping-and-periodic-effects.md](../_knowledge/tasks/shim-scoping-and-periodic-effects.md). In the meantime, turning down the game's own rumble or road-texture effect and leaning on the constant-force channel gives a more honest feel. If a game leaves a force latched after crashing, `./bin/lgwheel --stop` clears it.

## Pedals cannot be bound, or the clutch acts as the accelerator

Two distinct faults produce this, and they can occur together. Identify which one you have before changing anything.

**Cause 1 — the axes rest inverted.** The G29 exposes four axes, and all three pedals read *full scale* when released and near zero when floored. A game that auto-binds them without inverting sees three pedals permanently held down, so it either ignores them or treats them as stuck. The confirmed mapping is `X` steering, `Z` accelerator, `Rz` brake, `Y` clutch. Measure it yourself with `scripts/identify-pedals.sh`, pressing one pedal at a time — it prints which axis moved, its range, and whether it rests high or low. Note that a pedal untouched since power-on can read at the wrong end until it is pressed once, so run every pedal through its full travel before trusting any calibration.

The fix is calibration, not configuration files: in Live for Speed use Options → Controls, and assign the pedals on the **Axes / FF** tab, not the Buttons tab — a pedal is an axis and the Buttons tab will wait forever for a button press. Set throttle/brake axes to `separate` and clutch to `axis`. Do not hand-edit LFS's `.csf`, which is an opaque binary. Full detail: [../_knowledge/gotchas/g29-pedal-axes-rest-inverted.md](../_knowledge/gotchas/g29-pedal-axes-rest-inverted.md).

**Cause 2 — Wine is emitting Simulation Controls axes.** Wine's SDL backend sets `desc.is_wheel` from `SDL_JoystickGetType()`, and for a wheel it builds a report descriptor with Simulation Controls usages (`0xC8` steering, `0xC4` accelerator, `0xC5` brake, `0xC6` clutch) instead of Generic Desktop `X`/`Y`/`Z`/`Rz`. Sims from the 2000s only bind Generic Desktop axes, so their pedals become unbindable outright. This appears only once the SDL backend is switched on, which means the force-feedback fix caused the input regression and the two look unrelated — [../_knowledge/gotchas/sdl-wheel-type-changes-axis-usages.md](../_knowledge/gotchas/sdl-wheel-type-changes-axis-usages.md).

The shim already overrides `SDL_JoystickGetType` to report a generic joystick, which restores the Generic Desktop axes while `SDL_JoystickIsHaptic` still claims haptics. If you are seeing the Simulation Controls axes anyway, check that the shim is installed and that `LG4FF_SHIM_WHEEL_TYPE=wheel` is not set in the environment — that variable deliberately restores the wheel descriptor for games that prefer it.

**Either way, clear the stale binding cache.** LFS caches per-device settings in a binary `.csf`, and a file written while Wine presented a *different* descriptor keeps stale axis bindings that no amount of re-assigning in the menu clears. `scripts/reset-lfs-controls.sh` backs the file up, removes it, and prints the assignment to make afterwards.

**Diagnostic note.** In a `+hid` trace, `HidP_GetSpecificValueCaps` lines are the application *querying* — a useful signal, not proof. The unambiguous read is `match_device_object` in a `+dinput` trace, which names the actual axis GUIDs.

## wineserver crashed, please enable coredumps

A game exits immediately with `wineserver crashed, please enable coredumps (ulimit -c unlimited) and restart`.

**What it means.** Every process in a bottle shares one `wineserver`. A wedged leftover takes the next game down with it, and the usual culprits are Steam and its CEF helpers, which linger after their window closes.

**Fix.**

```bash
scripts/reset-bottle.sh
```

It kills the wineserver, force-kills the survivors, reports how many processes were running before and after, and clears any force the dead game left latched on the wheel. Nothing is lost — bottles keep no runtime state worth saving. If it reports processes that survived, inspect them with `pgrep -fl wine64`.

## The game lags or stutters

Low or uneven frame rate, felt as lag even when the average frame rate looks survivable.

**What it means.** Whisky creates bottles with **DXVK off**, so D3D11 titles run on wined3d's own Vulkan backend. The launch log gives it away:

```
err:winediag:wined3d_adapter_create Using the Vulkan renderer for d3d10/11 applications.
```

That path is slower and, more importantly, paces frames badly. Measured on an M4 Pro in the same in-car LFS scene: 58 fps with heavy spikes (menu 39 fps, spikes to 38 ms) on the stock configuration, against a steady 100 fps at 8.5–11.5 ms frame times with DXVK plus trimmed quality settings. The tight frame-time spread is the real win.

**Fix.** Enable DXVK for the bottle (Whisky's own toggle, or the manual DLL copy documented in the shard) and trim the expensive in-game settings. Both, with the exact LFS settings that mattered, are in [../_knowledge/gotchas/lfs-performance-dxvk-and-quality.md](../_knowledge/gotchas/lfs-performance-dxvk-and-quality.md).

Before changing anything, measure: `LFS_HUD=1 scripts/play-lfs.sh` shows the DXVK frame-rate and frame-time overlay. If the frame rate is low while CPU sits near idle — around 12% on the machine this was diagnosed on — the bottleneck is presentation or GPU, not translation, and chasing Rosetta is a waste of time. Note also that MoltenVK generally offers only FIFO presentation, so vsync cannot really be disabled and the display refresh bounds the result; stable frame times, not a higher number, are the goal.

## Steam in the bottle says it needs to be online to update

The Windows Steam client in the bottle fails with `Steam needs to be online to update. Please confirm your network connection and try again`, while the host has working internet.

**What it means.** Not DNS, not TLS, not the firewall — all three were checked and all three were fine. Wine's WinHTTP performs WPAD proxy auto-detection over **every** network interface; on the machine where this was diagnosed there were 26 of them (VPN tunnels plus Docker, UTM and VirtualBox bridges). The probe takes long enough that Steam's updater times the manifest request out and reports `Download failed: http error 0`.

**Fix.** Disable proxy auto-detection in the bottle. The registry commands are in [../_knowledge/gotchas/wine-winhttp-wpad-stalls-steam.md](../_knowledge/gotchas/wine-winhttp-wpad-stalls-steam.md).

Two things to expect afterwards, or you will think it failed again: it is faster but not instant, and the first manifest attempt in a run may still log `http error 0` with only the retry succeeding — give it a few minutes rather than quitting at the first error. Quitting VPN clients removes most of the tunnel adapters and speeds it up considerably.

---

## The shim will not build, or exports the wrong number of symbols

```
ERROR: shim exports 51 SDL symbols, expected 52 — Wine dlsyms all of them
```

**What it means.** Wine resolves 52 symbols out of `libSDL2-2.0.0.dylib` by name. The Makefile counts the exported `SDL_*` symbols after linking and fails the build rather than installing a library that would break Wine's joystick support in a way that looks like a hardware fault. A missing symbol would surface later as a silently zeroed return — "wheel detected but no axes" rather than a crash.

**Fix.** This should not happen from a clean checkout; if it does after local edits, the trampoline list in `src/sdl2-lg4ff-shim.c` is where symbols go missing. A compile failure on `SDL2/SDL.h` is a different problem: the shim needs SDL2 headers, `brew install sdl2`, and the Makefile takes the include path from `brew --prefix`.

---

## Gathering diagnostics before opening an issue

Run these and attach the output. Between them they cover every layer of the chain.

```bash
# 1. Is the wheel there, on the right interface, with the right report length?
./bin/lgwheel --detect

# 2. What does Apple's own force-feedback stack say? (0 capable is expected)
./bin/ffb-probe

# 3. Objective proof of torque — let go of the wheel first.
./bin/lgwheel --verify

# 4. Is the bridge installed?
scripts/install-shim.sh status

# 5. Does the game reach the shim, and what effects does it create?
LG4FF_SHIM_DEBUG=1 scripts/play-lfs.sh 2>&1 | tee /tmp/shim.log

# 6. What descriptor and axes does Wine present to the game?
WINEDEBUG=+hid,+dinput scripts/play-lfs.sh 2>&1 | tee /tmp/wine.log
```

Notes on the last two. `LG4FF_SHIM_DEBUG=1` prints `[lg4ff-shim]` lines to stderr; the sequence to look for is listed under [no force feedback in a Wine game](#the-game-sees-the-wheel-but-produces-no-force-feedback), and `new effect … type 0x1` is the line that proves a constant-force effect reached the wheel. The launcher scripts set `WINEDEBUG=-all` by default but honour the variable if you export it, so prefixing the command as above is enough; `+hid,+dinput` traces are large, so capture them to a file. In a `+dinput` trace, `match_device_object` names the actual axis GUIDs and is the reliable read for a pedal-binding problem.

Please also include: your Mac model and macOS version, the wheel model and the `046d:xxxx` product ID from `--detect`, the runner and its Wine version (this repo was built against Whisky's Wine 7.7), and whether the shim was installed at the time. If the report is about prior art or attribution rather than a fault, say so — an accurate map of what already exists is more useful than a novelty claim.
