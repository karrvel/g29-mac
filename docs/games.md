# Running sims

Three sims are known to run in the bottle this repo targets: Live for Speed, Speed Dreams and RaceRoom Racing Experience. All three are Windows titles running under Wine, and all three reach the wheel's motor through the same chain — the shim replaces the SDL2 that Wine loads, Wine then builds a DirectInput force-feedback device, and the game drives that. This page covers the setup that applies to any sim in the bottle, then each title in turn.

## Why none of this runs natively

There is no proper racing sim that runs natively on macOS. A sweep of Steam's macOS + free + racing catalogue returns arcade titles and mobile ports; the closest thing to a sim in the whole list is a telemetry tool. So every real candidate is a Windows title under translation, which is why everything below goes through Whisky. The full candidate list, including what was rejected and why — rFactor 2 is $29.99 and not free-to-play, Assetto Corsa and its successors are paid, Trackmania is free but arcade and launcher-gated — is in [`../_knowledge/decisions/free-sim-shortlist.md`](../_knowledge/decisions/free-sim-shortlist.md). The choice of Whisky as the runner, over CrossOver and the alternatives, is in [`../_knowledge/decisions/whisky-as-the-runner.md`](../_knowledge/decisions/whisky-as-the-runner.md).

## Setup common to every sim

### The bottle

The scripts find the bottle by name rather than by hardcoded path. The default name is **`Sim Racing`**, and the search looks at each bottle's `Metadata.plist` under `~/Library/Containers/com.isaacmarovitz.Whisky/Bottles`. Two overrides, both read by `scripts/common.sh`:

- `G29_BOTTLE_NAME` — use a differently named Whisky bottle.
- `WINEPREFIX` — an explicit prefix path, which skips the search entirely and lets a non-Whisky prefix be used.

```bash
G29_BOTTLE_NAME="My Bottle" scripts/play-lfs.sh
WINEPREFIX=/path/to/prefix scripts/play-lfs.sh
```

If neither matches anything, the scripts stop with an error naming the directory they searched rather than guessing.

### Enable SDL on winebus

Wine only synthesises a DirectInput force-feedback device when its SDL backend is active — the macOS `iohid` backend has no haptics entry points at all. This is not Whisky's default, and without it there is no force-feedback path to shim:

```bash
source scripts/common.sh
"$WINE" reg add "HKLM\System\CurrentControlSet\Services\winebus" /v "Enable SDL" /t REG_DWORD /d 1 /f
```

Sourcing `scripts/common.sh` sets `WINEPREFIX` and `WINE` for the bottle, so the key lands in the right prefix.

Enabling the SDL backend has a side effect worth knowing before it bites you: Wine builds the HID report descriptor from `SDL_JoystickGetType()`, and for a wheel it emits **Simulation Controls** axis usages (`0xC4` accelerator, `0xC5` brake, `0xC6` clutch) instead of Generic Desktop `X`/`Y`/`Z`/`Rz`. Sims from the 2000s only bind Generic Desktop axes, so their pedals become unbindable — the force-feedback fix causes an input regression that looks unrelated to it. The shim overrides `SDL_JoystickGetType` to report a generic joystick, which keeps both. Set `LG4FF_SHIM_WHEEL_TYPE=wheel` to restore the wheel descriptor for a game that genuinely wants the Simulation Controls axes. Detail in [`../_knowledge/gotchas/sdl-wheel-type-changes-axis-usages.md`](../_knowledge/gotchas/sdl-wheel-type-changes-axis-usages.md).

### Build and install the shim

```bash
make                 # builds ./bin/lgwheel, ./bin/ffb-probe and ./bin/libSDL2-2.0.0.dylib
make install-shim    # equivalently: scripts/install-shim.sh install
scripts/install-shim.sh status
```

`scripts/install-shim.sh revert` puts Wine's original SDL2 back, byte for byte, at any time. The shim is global to the Whisky installation rather than per bottle — see [`../_knowledge/gotchas/sdl2-shim-is-global-to-whisky.md`](../_knowledge/gotchas/sdl2-shim-is-global-to-whisky.md).

### Check the wheel before launching

```bash
./bin/lgwheel --detect
./bin/lgwheel --verify     # let go of the wheel first
```

`--verify` commands a known force and watches the steering axis to see whether the wheel moved itself. On the reference machine it drove across 100% of its travel, which answers "does force feedback work" by measurement rather than by feel. Do this before blaming a game.

### The 900-degree range

Logitech wheels power up at a reduced rotation range until told otherwise. Every launcher script calls `prep_wheel`, which runs `./bin/lgwheel --range 900` and prints either `wheel: 900 degrees set` or `wheel: not detected (connect the G29 and its power brick)`. If you launch a game by hand instead of through the scripts, set the range yourself first, or the in-game steering ratio will not match what the game thinks it is.

### One wineserver per bottle

Every process in a bottle shares a single `wineserver`, so a wedged leftover takes the next game down with it — usually reported as `wineserver crashed, please enable coredumps`. Steam and its CEF helpers are the usual culprits and they linger after their window closes. `scripts/reset-bottle.sh` kills everything in the bottle, starts fresh, and clears any force the dead process left latched on the wheel.

## Live for Speed

Free demo (Blackwood and three cars, unlimited in time), and the pick with the strongest driving model of the three. Constant-force feedback works: LFS creates a `type 0x1` effect that arrives at the shim.

The launcher expects the game at `$WINEPREFIX/drive_c/LFS/LFS.exe` — install LFS's own Windows installer into the bottle at that path. There is no install script for it in this repo.

```bash
scripts/play-lfs.sh              # play
LFS_HUD=1 scripts/play-lfs.sh    # play with the DXVK frame-rate and frame-time overlay
```

### Binding the wheel and the pedals

This is the part that wastes an afternoon if you go in cold. The G29 exposes four axes and no labels, and **all three pedals are inverted**: released reads full scale, floored reads about zero. A game that auto-binds them sees three pedals permanently held down. The symptom in LFS is that the clutch operates the throttle while the accelerator and brake do nothing.

Measured with `./bin/lgwheel --axes` on the attached G29:

| Axis | Pedal | Released | Floored |
|---|---|---|---|
| `X` | steering | — | 0.000 - 0.810 sweep |
| `Z` | accelerator | 1.000 | 0.000 |
| `Rz` | brake | 1.000 | 0.129 |
| `Y` | clutch | 1.000 | 0.000 |

Confirm the map on your own wheel with `scripts/identify-pedals.sh`, which prints which axis moved, its range, and whether it rests high or low. Press every pedal through its full travel before trusting any calibration — a pedal untouched since power-on can read at the wrong end until it is pressed once.

In LFS, go to **Options → Controls → "wheel / joystick"** and use the **`Axes / FF`** tab. Assign `steering = X`, `throttle = Z`, `brake = Rz`, `clutch = Y` by clicking each axis field and moving that control; LFS binds whatever moves and calibrates its range, which handles the inversion as a side effect.

Two things that make the difference between this working and not:

- **Do not use the `Buttons 1` tab.** Its prompt reads "Press button for : Accelerate", and a pedal is not a button, so the assignment can never complete. Pedals are axes and live on `Axes / FF`.
- Set **`Throttle / brake axes: separate`** (not `combined`) for a three-pedal set, and **`Clutch: axis`** rather than `button`.

LFS caches per-device settings in a binary `.csf` under `drive_c/LFS/data/misc`. Do not hand-edit it — it is opaque (`LFSCON` magic) and editing it is guesswork. If a config written while Wine was presenting a different descriptor has left stale bindings that re-assigning will not clear, `scripts/reset-lfs-controls.sh` backs the file up, removes it, and prints the axis map to re-enter. Background in [`../_knowledge/gotchas/g29-pedal-axes-rest-inverted.md`](../_knowledge/gotchas/g29-pedal-axes-rest-inverted.md).

### Performance

Whisky creates bottles with **DXVK off**, so D3D11 runs on wined3d's own Vulkan backend. That path paces frames badly, which is felt as lag even when the average frame rate looks survivable. The launch log gives it away:

```
err:winediag:wined3d_adapter_create Using the Vulkan renderer for d3d10/11 applications.
```

Turn DXVK on for the bottle (Whisky's own toggle does it; the manual DLL route is in the shard below), then trim the in-game quality. Measured on an M4 Pro with the DXVK overlay, same in-car scene:

| Configuration | FPS | Frame times |
|---|---|---|
| DXVK, stock quality | 58 | very spiky — 39 fps in the menu, spikes to 38 ms |
| DXVK + trimmed quality | 100 | 8.5 - 11.5 ms |

The tight frame-time spread is the real win; a 3 ms range is smooth, and the earlier spikes are what "lagging a lot" actually meant. The wined3d path was not measured in fps, only observed to pace badly, so no number is claimed for it.

The settings that mattered, in `drive_c/LFS/cfg.txt`:

```
Antialiasing 0 0
Mirror AA 0 0
Shadow Cascades 2      (was 4)
Dynamic Reflect 2 1    (was 8 4)
Mirror External 0
```

**Edit `cfg.txt` with LFS closed.** It rewrites the file on exit, so edits made while the game is running are lost.

`LFS_HUD=1` sets `DXVK_HUD=fps,frametimes` for the run. Read it this way: if the frame rate is low while the CPU sits near idle, the bottleneck is presentation or GPU, not translation, and chasing Rosetta is a dead end. `scripts/play-lfs.sh` also points `DXVK_CONFIG_FILE` at `drive_c/LFS/dxvk.conf`. That file sets `dxgi.syncInterval = 0`, but MoltenVK generally offers only FIFO presentation, so vsync cannot really be disabled and the display refresh still bounds the ceiling. Full working in [`../_knowledge/gotchas/lfs-performance-dxvk-and-quality.md`](../_knowledge/gotchas/lfs-performance-dxvk-and-quality.md).

## Speed Dreams

Version 2.4.2, free and open source, no account and no launcher. It runs clean under Wine and it is the easiest of the three to get to a first lap.

The launcher expects `$WINEPREFIX/drive_c/SpeedDreams/bin/speed-dreams-2.exe`. Install the Windows build into the bottle at that path; the project's own macOS build exists only for the older 2.3.0 release and is explicitly not for Apple Silicon, so it runs as a Windows build under Wine like the others.

```bash
scripts/play-speeddreams.sh
```

Speed Dreams creates both `type 0x1` (constant) and `type 0x2` (periodic) effects, so it is where the shim's main limitation is audible: **periodic effects are approximated by holding them at peak magnitude rather than oscillating them**, so kerb and road texture read as a steady pull instead of a vibration. Constant force — what a sim actually steers with — is exact. `SPRING` maps to the wheel's autocentre rather than a true positional spring, and `DAMPER`, `INERTIA` and `FRICTION` are unimplemented. The open work is tracked in [`../_knowledge/tasks/shim-scoping-and-periodic-effects.md`](../_knowledge/tasks/shim-scoping-and-periodic-effects.md).

Speed Dreams earned its place on the shortlist by being free, installable with no account, and actually launching — not by being one of the three best sims in the world.

## RaceRoom Racing Experience

Free to play, and the strongest genuinely free-to-play sim available. Be clear about the model, though: it is a **free base game plus a largely paid car and track catalogue**. That makes it a real sim rather than a trial, but not the whole game either.

It needs a one-time interactive Steam login inside the bottle. A login in the macOS Steam app does not carry across — the Windows client keeps its own credential store and needs its own Steam Guard approval, which is why this cannot be scripted.

```bash
scripts/install-raceroom.sh login      # sign in, approve Steam Guard on your phone
scripts/install-raceroom.sh install    # queues app 211500
scripts/install-raceroom.sh play
```

**Status here: staged, not finished.** The Windows Steam client is installed in the bottle and reaches the login screen with `steamwebhelper` running, but RaceRoom itself has not been installed and force feedback has not been confirmed end to end on it. The acceptance test is that `drive_c/Program Files (x86)/Steam/steamapps/common/raceroom racing experience` exists, the game launches, and a `LG4FF_SHIM_DEBUG=1` run logs `new effect … type 0x1` — the same constant-force signature Live for Speed and Speed Dreams produce. See [`../_knowledge/tasks/raceroom-needs-steam-login.md`](../_knowledge/tasks/raceroom-needs-steam-login.md).

Two things to expect on the way there:

- Steam may fail with **"Steam needs to be online to update"** while the host's network is fine. The cause is Wine's WinHTTP running WPAD proxy auto-detection across every network interface — 26 of them on the machine this was diagnosed on — until Steam's updater times out. The registry fix, and the reason quitting your VPN clients helps, are in [`../_knowledge/gotchas/wine-winhttp-wpad-stalls-steam.md`](../_knowledge/gotchas/wine-winhttp-wpad-stalls-steam.md). Even after the fix, the first manifest attempt in a run may still log `http error 0` and only the retry succeeds, so give it a few minutes before quitting.
- Steam's helper processes survive a polite shutdown and wedge the shared `wineserver`. Run `scripts/reset-bottle.sh` before launching another game.

## If force feedback is missing in a game

Work down the chain rather than changing settings at random. `./bin/lgwheel --verify` proves the hardware path independently of Wine; if that moves the wheel, the problem is above it. Then check `scripts/install-shim.sh status` for the shim, and that `Enable SDL` is set on `winebus` in the bottle you are actually launching. `LG4FF_SHIM_DEBUG=1` on the launch makes the shim log the effects a game creates, so you can see whether the game asked for anything at all. And do not run two force-feedback consumers at once — SDL takes no exclusive access on macOS, so a second one will fight the first.
