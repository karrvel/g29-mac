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

`~/Projects/g29-mac` — three C programs, a set of shell wrappers, and this vault. Not a git remote; local only.

| Path | Role |
|---|---|
| `ffb-probe.c` | Diagnostic. Probes Apple's `ForceFeedback.framework` and proves it refuses the wheel. Run first when something breaks. |
| `lgwheel.c` | The userspace driver. `--detect`, `--range`, `--autocentre`, `--force`, `--test`, and `--verify` (objective torque measurement). |
| `sdl2-lg4ff-shim.c` | The bridge that gives Wine-hosted Windows games real FFB. Builds to `libSDL2-2.0.0.dylib`, **x86_64 only**. |
| `install-shim.sh` | `install` / `revert` / `status` for the bridge. Reverting is always safe. |
| `play-lfs.sh`, `play-speeddreams.sh` | Launchers; each sets the wheel to 900° first via `common.sh`'s `prep_wheel`. |
| `install-raceroom.sh` | `login` / `install` / `play` for the blocked third game. |
| `build.sh` | Rebuilds all three binaries. |
| `common.sh` | Shared bottle path + `WINE` binary + `prep_wheel`. Holds the bottle UUID. |
| `sdl3-probe/` | An SDL3-based probe from a research pass, kept as an independent second opinion. Build: `clang g29probe.c -o g29probe $(pkg-config --cflags --libs sdl3)` (needs Homebrew `sdl3`, already installed). Binaries are gitignored. |
| `_knowledge/`, `_meta/` | This vault, and a **gitignored** disposable copy of the KB tooling — restore with `cp ~/Projects/kb-template/repos/carryover/tooling/*.py _meta/`. |

**The games live outside this folder**, in the Whisky bottle named "Sim Racing" (`~/Library/Containers/com.isaacmarovitz.Whisky/Bottles/<uuid>/drive_c/`): `LFS/` (3.7 GB) and `SpeedDreams/` (259 MB), plus a staged Windows Steam. The bottle UUID is recorded in `common.sh` — that file is the link between this repo and the bottle, so don't lose it.

**Rebuild note.** The shim must be compiled `-arch x86_64` even on Apple Silicon, because Whisky's `winebus.so` is x86_64 under Rosetta. `build.sh` handles it; a hand-rolled `clang` invocation will silently produce an arm64 dylib that Wine cannot load.

Start at [[architecture]] for how the pieces connect, or [[INDEX]] for everything.
