# How it works

This is the design explanation: why force feedback for a Logitech wheel on Apple Silicon needs a substituted library at all, and what that library actually does. It is written for someone who wants to understand the system well enough to change it.

The vault holds the terse model — [`../_knowledge/architecture.md`](../_knowledge/architecture.md) is the one-page version, and the shards linked below hold the evidence for each claim. This page is the narrative that connects them.

## The shape of the problem

Four things have to be true before a wheel produces torque inside a game. The game has to be able to create a force-feedback effect through whatever input API it speaks; something has to accept that effect; something has to translate it into bytes the wheel understands; and the wheel has to be listening. On macOS the last of those is true and the first three are not, and the failures are quiet — the wheel enumerates, steering works, buttons work, pedals work, and no game ever produces torque.

Everything below is measured rather than assumed. `./bin/ffb-probe` exists specifically so the refusals can be reproduced rather than taken on trust.

## Why every standard route fails

**Apple's `ForceFeedback.framework` refuses the wheel, and will always refuse it.** The framework only drives USB-HID **PID-class** hardware — devices whose report descriptor carries usage page `0x0F`, the Physical Interface Device page, which is a standardised, self-describing effect protocol. The G29's descriptor exposes usage pages `0x01` (Generic Desktop), `0x09` (Button) and `0xFF00` (vendor-defined), and no `0x0F` page at all. `FFIsForceFeedback()` returns `0x80000003` for both of the wheel's HID interfaces; `./bin/ffb-probe` reports two controller-class devices found and zero force-feedback capable. This is a hardware class mismatch, not a missing driver: the wheel speaks Logitech's own protocol instead of PID. Independently, nothing on a stock macOS install registers `kIOForceFeedbackLibTypeID`, so even a genuinely PID-class wheel would find no plug-in to drive it. The framework is alive and current — arm64e, built against the 26.6 SDK, carrying no deprecation attributes — which is exactly what makes it misleading. Details: [`macos-forcefeedback-rejects-g29`](../_knowledge/gotchas/macos-forcefeedback-rejects-g29.md).

**Apple's `GCRacingWheel` (macOS 13+) is not a way out.** Its entire surface is discovery, acquire and relinquish, `wheelInput`, and capture. There is no haptics API, no effects API and no output API anywhere in it. It is input-only, so it cannot be the missing translation layer.

**SDL2's macOS haptic backend inherits the framework's failure.** It is built directly on `ForceFeedback.framework`, so when SDL2 asks macOS which devices are haptic it gets the same refusal and reports zero haptic devices for the wheel. Nothing about SDL2 is broken here; it is faithfully relaying a "no".

**Wine therefore never builds a force-feedback device.** `winebus.sys` only synthesises a DirectInput force-feedback (PID) device when its backend reports that the joystick is haptic, and Wine has two relevant backends on macOS, neither of which delivers. The **iohid** backend is registered as a `raw_device_vtbl`, a structure that has no haptics entry points at all — confirmed by symbols, since `winebus.so` imports only `IOHIDDeviceOpen`/`Close`/`GetReport`/`SetReport`/`RegisterInputReportCallback` and has no `ForceFeedback` linkage on that path. It passes the wheel's real report descriptor through verbatim, and because that descriptor is not PID-class, dinput never sets `DIDC_FORCEFEEDBACK` and `CreateEffect` fails before a single output report is attempted. The **sdl** backend does have the full PID vtbl (`sdl_device_physical_effect_control`, `…_effect_update`, `…_device_set_gain`, `sdl_device_haptics_start`) — but it asks SDL2, and SDL2 says no. This is a regression rather than a gap: CrossOver shipped working macOS force feedback through `joystick_osx.c` and `ForceFeedback.framework` until that file was deleted in the 2021 HID stack rewrite, and no macOS replacement followed. Details: [`wine-macos-has-no-ffb-path`](../_knowledge/gotchas/wine-macos-has-no-ffb-path.md).

Note where the chain actually breaks. It is not at effect playback, where a bug would be visible and fixable. It is at **capability detection**, one gate earlier, and everything downstream of it is dead code that never runs. There is no registry key, DLL override or `winecfg` toggle that turns it on, because the code path does not exist.

## The opening: the motor is reachable anyway

The wheel's motor answers to Logitech's vendor force-feedback protocol, sent as ordinary HID output reports. That protocol is the one Linux has implemented in `hid-lg4ff` for over a decade, and reaching it from macOS needs no kernel extension, no DriverKit extension, no entitlement and no admin rights — only `IOHIDDeviceSetReport` from an ordinary userspace process. On Apple Silicon under SIP that matters: the kext-era macOS workarounds are dead here, and this route cannot be taken away by a framework deprecation because it uses no framework beyond IOKit. See [`raw-hid-not-apple-forcefeedback`](../_knowledge/decisions/raw-hid-not-apple-forcefeedback.md) for the decision and [`lg4ff-protocol`](../_knowledge/reference/lg4ff-protocol.md) for the wire format.

Two properties of that protocol shape everything built on it. Commands are **7-byte blocks** — `11 00 <force> 00 00 00 00` for constant force in slot 0, where `0x80` is neutral and `0x00`/`0xff` are full lock either way — and they must be **padded to the HID interface's declared output report length** before sending, 16 bytes on the G29's joystick interface. Linux never surfaces this because `hid_hw_request` pads automatically; `IOHIDDeviceSetReport` passes the buffer through verbatim and the USB endpoint answers an unpadded write with a STALL (`0xe0005000`). And the wheel publishes **two** HID interfaces under the same `046d:c24f`: force feedback lives on the **joystick** interface (usage page `0x01`, usage `0x04`), not on the vendor-defined `0xff00` one. Both halves are in [`lg4ff-reports-must-be-16-bytes`](../_knowledge/gotchas/lg4ff-reports-must-be-16-bytes.md).

`./bin/lgwheel` is that protocol as a command-line tool, and `./bin/lgwheel --verify` is how the claim is checked rather than asserted: it stops all forces, samples the steering axis to establish a baseline, commands 80% left for two seconds, samples again, commands 80% right for two and a half seconds, samples again, and prints the swing as a fraction of full lock. On the reference machine the wheel drove itself across **100% of its travel** under command. "Does force feedback work?" is answered by measurement, not by feel.

## The gap the shim closes

A game does not speak raw HID. It speaks DirectInput, and DirectInput will only offer it a force-feedback effect if Wine built a PID device, and Wine only builds a PID device if SDL reports the joystick is haptic. That single boolean is the entire gate — and Wine loads SDL by `dlopen()` and `dlsym()`, which makes the library a clean, well-defined seam. Replacing it flips the gate without patching Wine and without patching any game. See [`build-sdl2-shim-not-buy-torqer`](../_knowledge/decisions/build-sdl2-shim-not-buy-torqer.md).

The full chain, once the shim is in place:

```
game → dinput8 → winebus PID device → SDL_Haptic → shim → lg4ff HID reports → motor
```

Read right to left and it is the standard-route failure list in reverse: the motor is reachable, the lg4ff reports reach it, the shim writes those reports, `SDL_Haptic` is the API Wine consults, the PID device is what Wine builds once that API answers yes, and dinput8 is what the game already speaks.

## How the shim is built

Wine `dlopen()`s `libSDL2-2.0.0.dylib` and `dlsym()`s **52 symbols** out of it. The shim *is* that library — `scripts/install-shim.sh install` moves Wine's original aside as `libSDL2-2.0.0.real.dylib` and drops the shim in its place — so it has to export all 52 or Wine's SDL backend fails to initialise. The build enforces that: the `Makefile` counts exported `T _SDL_` symbols with `nm` after linking and fails if the count is not exactly 52.

Of those 52, **17 are the haptic API** and are reimplemented against the wheel. The other 35 are non-haptic. **33 of them are one-instruction assembly trampolines** into the renamed real library:

```c
#define FWD(name)                                                        \
    void *p_##name = NULL;                                               \
    __asm__(".globl _" #name "\n"                                        \
            "_" #name ":\n"                                              \
            "  movq _p_" #name "@GOTPCREL(%rip), %rax\n"                 \
            "  movq (%rax), %rax\n"                                      \
            "  testq %rax, %rax\n"                                       \
            "  je 1f\n"                                                  \
            "  jmp *%rax\n"                                              \
            "1:\n"                                                       \
            "  xorl %eax, %eax\n"                                        \
            "  ret\n");
```

The point of writing these in assembly rather than C is the `jmp`. A naked jump never touches the argument registers, never sets up a frame and never returns to the shim — control transfers straight into the real SDL2 with the arguments exactly as the caller left them. That means **one macro covers every signature**, whatever the types and however many parameters, without declaring 33 prototypes and without the shim needing to know or care what any of those functions take. The `je 1f` branch is the fallback for a pointer that failed to resolve: it returns zero rather than jumping through a null pointer, which fails as "no data" rather than as a crash.

The remaining **two non-haptic symbols are written in C** rather than trampolined, because they are the two gates that decide what Wine builds: `SDL_JoystickIsHaptic`, which is the boolean the whole chain turns on, and `SDL_JoystickGetType`, which is discussed below.

Everything is wired up in a `__attribute__((constructor))` that runs at load: `dladdr()` finds the shim's own path on disk, the real SDL2 is `dlopen()`ed from the same directory, and every forwarding pointer is resolved once. The rename to `libSDL2-2.0.0.real.dylib` is load-bearing — dyld deduplicates by install name, so without rewriting the original's install name the shim's own `dlopen()` would resolve back to the shim and recurse. `install-shim.sh` handles that with `install_name_tool`, and `revert` restores the original byte for byte.

## How the shim decides what is its business

`joystick_is_our_wheel()` checks vendor `0x046d` plus a known wheel product ID, and **anything that is not a Logitech wheel is forwarded to the real SDL2 untouched**. The wheel itself is found by matching on usage page `0x01` and usage `0x04`/`0x05`, for the same reason `lgwheel` does — `IOHIDManagerCopyDevices` returns an unordered set, and "the first Logitech device found" picks the wrong interface about half the time.

The `SDL_Haptic` the shim hands back is not SDL's structure at all; it is the shim's own, starting with a magic value (`'LG4F'`) and holding a gain and 16 effect slots. Every haptic override begins with `is_ours(h)`, a magic check, so a pointer that came from the real SDL2 — a genuine haptic device on some other hardware — is passed straight through to the real implementation. That is why the shim being installed globally across every Whisky bottle has, in practice, no blast radius: it was tested against an unrelated pre-existing bottle, which launched normally on the real SDL2. See [`sdl2-shim-is-global-to-whisky`](../_knowledge/gotchas/sdl2-shim-is-global-to-whisky.md).

## From a DirectInput effect to a wheel command

`SDL_HapticQuery` advertises `CONSTANT`, the four periodic types, `RAMP`, `SPRING`, `GAIN` and `STATUS`. `SDL_HapticNewEffect` stores the descriptor in a free slot; `SDL_HapticRunEffect` marks it running and applies it; `SDL_HapticUpdateEffect` re-applies it immediately if it is already running, so that a title which steers by continuously updating one long-lived constant-force effect gets its new value written to the wheel rather than stored and ignored.

Applying an effect means mapping SDL's model onto the wheel's:

- **`CONSTANT`** — `level` is scaled from SDL's signed 16-bit range to a percentage, multiplied by a direction sign and by the current gain, and written as the `11 00 <force>` command. This is exact, and it is what sims actually steer with.
- **Periodic (`SINE`, `TRIANGLE`, `SAWTOOTHUP`, `SAWTOOTHDOWN`)** — **approximated**: there is no oscillator, so the effect is held at its peak magnitude. Kerbs and road texture read as a steady pull rather than as texture.
- **`SPRING`** — mapped to the wheel's **autocentre**, derived from `right_coeff[0]`. That is a self-centring spring around the wheel's own centre, not a true positional spring around an arbitrary point.
- **`RAMP`** — the start level is applied; it does not ramp over time.
- **`DAMPER`, `INERTIA`, `FRICTION`** — unimplemented, and not advertised in `SDL_HapticQuery`.

Direction comes from `SDL_HapticDirection`, reduced to a sign: for polar directions (`dir[0]` in hundredths of a degree, which is what dinput sends for wheels) the sign of the sine decides left or right; cartesian and spherical use the sign of the first component. A wheel has one axis, so a sign is all the information that survives.

The remaining work on this is written up in [`shim-scoping-and-periodic-effects`](../_knowledge/tasks/shim-scoping-and-periodic-effects.md) — an effect timer that oscillates periodics is the single largest improvement available, and SDL3's `SDL_hidapi_lg4ff.c` is the reference for how to build one.

## The two constraints that make or break it

**The shim must be built `-arch x86_64`.** Whisky's `winebus.so` is an x86_64 binary running under Rosetta, and a process cannot `dlopen()` a library of a different architecture. An arm64 build of the shim is a perfectly valid dylib that Wine never loads, with no error that points at the cause — you get stock behaviour and no force feedback. The `Makefile` pins this (`SHIM_ARCH := -arch x86_64`) and it is not optional; a hand-rolled `clang` invocation on an Apple Silicon Mac defaults to arm64 and silently produces an unloadable library.

**`SDL_JoystickGetType` is overridden to report a generic joystick.** Wine's SDL backend sets `desc.is_wheel` from that call, and for a wheel it builds the HID report descriptor with **Simulation Controls** usages — Steering `0xC8`, Accelerator `0xC4`, Brake `0xC5`, Clutch `0xC6` on usage page `0x02` — instead of Generic Desktop `X`/`Y`/`Z`/`Rz`. That descriptor is more correct in the abstract, and it is exactly what breaks sims of the 2000s: they only bind Generic Desktop axes and have no handling for page `0x02`, so on a "proper" wheel descriptor their pedals cannot be assigned at all. This is a trap worth stating plainly — the iohid backend passes the device's real descriptor through, so the problem only appears once you switch on the SDL backend, which means **the fix for force feedback caused the input regression**, and the two look unrelated. Returning `SDL_JOYSTICK_TYPE_UNKNOWN` gets the plain joystick descriptor back while `SDL_JoystickIsHaptic` still claims the device, so both work at once. Verified afterwards: dinput exposes exactly four axes, `GUID_XAxis`, `GUID_YAxis`, `GUID_ZAxis` and `GUID_RzAxis`, and Live for Speed still creates a constant-force effect through the shim. Set `LG4FF_SHIM_WHEEL_TYPE=wheel` to restore the wheel descriptor for a title that genuinely prefers Simulation Controls axes. Full account: [`sdl-wheel-type-changes-axis-usages`](../_knowledge/gotchas/sdl-wheel-type-changes-axis-usages.md).

One more thing is required outside the shim: the bottle needs Wine's SDL backend switched on, since it is not the default. Without `Enable SDL` on `winebus`, Wine uses the iohid backend and never loads SDL at all, so the shim is never reached.

## Evidence that the chain closes

- `./bin/lgwheel --verify` — the wheel drove itself across 100% of its travel under commanded force.
- With the shim installed, Wine builds the PID collections (`set constant force`, `effect control`, `effect update`) and dinput enumerates `GUID_ConstantForce`.
- Live for Speed creates effect type `0x1` (constant force); Speed Dreams creates `0x1` and `0x2` (periodic). Both arrive at the shim.
- `LG4FF_SHIM_DEBUG=1` in the environment makes the shim log its decisions to stderr, which is the quickest way to see whether a given title reaches it.

## Where this sits relative to existing work

Almost none of the underlying technique is new, and the [prior-art section of the README](../README.md#prior-art-and-credits) says so in detail. The protocol is Linux's `hid-lg4ff` / `berarma/new-lg4ff`, from which the command constants here were transcribed as documentation. **SDL3 already ships an equivalent userspace driver** — `SDL_hidapi_lg4ff.c`, merged in 2025 and enabled by default on macOS from 3.4.0 — so if you are writing new code and can depend on SDL3, use it and none of this is necessary. `contrib/sdl3-probe/` keeps two small SDL3 programs as an independent check on this repo's own tooling. On Linux the whole chain already works end to end with upstream components, via Wine and sdl2-compat backed by SDL3.

What is specific to this repo is the macOS build of that chain: a Wine runner that loads SDL2 rather than SDL3, a wheel that Apple's stack refuses, and the two constraints above, neither of which is documented anywhere else that could be found.

## Reading map for extending it

| To change | Read |
|---|---|
| the wire protocol or a new wheel model | [`../_knowledge/reference/lg4ff-protocol.md`](../_knowledge/reference/lg4ff-protocol.md), then `src/lgwheel.c` |
| effect mapping, periodics, scoping | `src/sdl2-lg4ff-shim.c` (`apply_effect`), then [`shim-scoping-and-periodic-effects`](../_knowledge/tasks/shim-scoping-and-periodic-effects.md) |
| whether a route was already ruled out | [`../_knowledge/reference/ffb-delivery-paths-evaluated.md`](../_knowledge/reference/ffb-delivery-paths-evaluated.md) |
| the system model in one page | [`../_knowledge/architecture.md`](../_knowledge/architecture.md) |
