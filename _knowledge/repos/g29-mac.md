---
name: g29-mac
type: repo
title: g29-mac — the tooling that makes a G29 produce torque on Apple Silicon, plus the sim-racing bottle
area: hardware
tags: [component-map, tooling]
status: active
updated: 2026-09-02
volatility: decays-with-code
provenance: 2026-09-01 session — built in this folder; verified against the attached wheel
---

# What is in this folder and what each piece is for

`~/Projects/g29-mac` — three C programs, a set of shell wrappers, and this vault. **Public** at github.com/karrvel/g29-mac (MIT); use the `karrvel` gh account for anything touching it.

| Path | Role |
|---|---|
| `Makefile` | The only build entry point. `make`, `make install-shim`, `make revert-shim`, `make shim-status`, `make check`, `make clean`, `make help`. The shim rule asserts 52 exported SDL symbols and fails the build otherwise. |
| `src/ffb-probe.c` | Diagnostic. Probes Apple's `ForceFeedback.framework` and proves it refuses the wheel. Run first when something breaks. |
| `src/lgwheel.c` | The userspace driver. `--detect`, `--range`, `--autocentre`, `--force`, `--test`, `--axes`, and `--verify` (objective torque measurement). |
| `src/sdl2-lg4ff-shim.c` | The bridge that gives Wine-hosted Windows games real FFB. Builds to `bin/libSDL2-2.0.0.dylib`, **x86_64 only**. |
| `scripts/install-shim.sh` | `install` / `revert` / `status` for the bridge. Reverting is always safe. |
| `scripts/play-lfs.sh`, `scripts/play-speeddreams.sh` | Launchers; each sets the wheel to 900° first via `common.sh`'s `prep_wheel`. `LFS_HUD=1` shows the frame-rate overlay. |
| `scripts/install-raceroom.sh` | `login` / `install` / `play` for the blocked third game. |
| `scripts/identify-pedals.sh` | Press pedals, learn which axis each one is. |
| `scripts/reset-bottle.sh` | Clears a wedged wineserver — the fix for "wineserver crashed". |
| `scripts/reset-lfs-controls.sh` | Wipes LFS's stale binary wheel config so it re-detects. |
| `scripts/common.sh` | Shared bottle discovery (**by name, not a hardcoded UUID**), `WINE` path, and `prep_wheel`. Sourced by every script. |
| `scripts/kb-sync.sh` | Runs the KB maintenance loop, then scrubs the absolute home path kb-sync bakes into `INDEX.md`. Use this, not `kb-sync.py`. |
| `bin/` | Build output. Gitignored; created by `make`. |
| `docs/` | Task-oriented guides — installation, troubleshooting, how it works, running sims. |
| `contrib/sdl3-probe/` | An SDL3-based probe kept as an independent second opinion. Not built by the Makefile. |
| `_knowledge/`, `_meta/` | This vault, and a **gitignored** disposable copy of the KB tooling — restore by copying `tooling/*.py` from the carryover kit. |

**The games live outside this folder**, in the Whisky bottle named "Sim Racing" (`~/Library/Containers/com.isaacmarovitz.Whisky/Bottles/<uuid>/drive_c/`): `LFS/` (3.7 GB) and `SpeedDreams/` (259 MB), plus a staged Windows Steam. `scripts/common.sh` locates the bottle by its Metadata.plist name, so the link between repo and bottle is by name rather than a fragile UUID. Override with `G29_BOTTLE_NAME` or `WINEPREFIX`.

**Rebuild note.** The shim must be compiled `-arch x86_64` even on Apple Silicon, because Whisky's `winebus.so` is x86_64 under Rosetta. The `Makefile` handles it; a hand-rolled `clang` invocation will silently produce an arm64 dylib that Wine cannot load.

Start at [[architecture]] for how the pieces connect, or [[INDEX]] for everything.
