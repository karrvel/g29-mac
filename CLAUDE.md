# g29-mac — CLAUDE.md

> **First action every session:** `cat _knowledge/INDEX.md` — the vault is the authoritative source. This file is quick-boot orientation; the vault has the depth.

Logitech G29 force feedback on Apple Silicon (M4 Pro, macOS 26.6.2), plus three free racing sims. Everything here is free software; the tooling is written in this repo.

## The one thing to know before touching anything

**macOS cannot drive this wheel through any standard API — and that is settled, not a lead to chase.** Apple's `ForceFeedback.framework` returns `0x80000003` for the G29 (it has no PID usage page), `GCRacingWheel` is input-only, SDL2 inherits the framework's failure, and Wine therefore never exposes a force-feedback device. Torque is reached by writing Logitech's vendor HID protocol from userspace, and games reach it through a substituted SDL2 library. Read [[architecture]] before proposing any change to the FFB path.

Do not "fix" this by installing Logitech G HUB, a kext, or Apple's Game Porting Toolkit. All three were evaluated and none of them touch this problem — [[ghub-macos-does-nothing-for-wheels]], [[ffb-delivery-paths-evaluated]].

## Layout

```
g29-mac/
├── Makefile             build entry point — `make`, `make install-shim`, `make check`, `make help`
├── src/                 ffb-probe.c  lgwheel.c  sdl2-lg4ff-shim.c
├── scripts/             launchers, installer, diagnostics (common.sh is sourced by all of them)
├── bin/                 BUILD OUTPUT, gitignored — ./bin/lgwheel, ./bin/ffb-probe, ./bin/libSDL2-2.0.0.dylib
├── docs/                task-oriented guides (how to use it)
├── contrib/sdl3-probe/  independent SDL3 probe — second opinion when debugging
├── _knowledge/          the vault (why we learned it) — start at INDEX.md
└── _meta/               disposable copy of the KB tooling — never hand-edit, it's a copy
```

`docs/` and `_knowledge/` are deliberately different things: docs are *how do I do this*, the vault is *why is it like this*. Put new findings in the vault as shards and link to them from docs, rather than restating them.

The games live **outside** this folder, in the Whisky bottle named "Sim Racing". `scripts/common.sh` finds it by name, so nothing here hardcodes a machine-specific path; override with `G29_BOTTLE_NAME` or `WINEPREFIX`.

## Gotchas that will cost you an hour each

- Build the shim **`-arch x86_64`** — Whisky's `winebus.so` is x86_64 under Rosetta. The `Makefile` gets this right; a hand-rolled `clang` silently produces an unloadable arm64 dylib.
- lg4ff commands are 7 bytes but must be **padded to the interface's 16-byte report** or the USB endpoint STALLs — [[lg4ff-reports-must-be-16-bytes]].
- The FFB interface is the **joystick** one (usage page `0x01`, usage `0x04`), not the vendor-defined `0xff00` one.
- **Leave the wheel's selector on PS4.** It reports `046d:c24f` and FFB works there — [[keep-g29-selector-on-ps4]].
- The shim is **global to Whisky**, not per bottle — [[sdl2-shim-is-global-to-whisky]].

### 🔴 LIVE — open security findings
<!-- BEGIN:sync:live-security -->
_none open_
<!-- END:sync:live-security -->

### 🟠 LIVE — open work
<!-- BEGIN:sync:open-work -->
- [[raceroom-needs-steam-login]] — RaceRoom is installed under Wine 11 — the only thing left is rebuilding the FFB shim against that Wine’s SDL2
- [[shim-scoping-and-periodic-effects]] — Shim follow-ups — oscillate periodic effects instead of holding peak, and optionally scope the library override to one bottle
- [[wire-up-virtualhere-bridge]] — Wire up VirtualHere + Steam Remote Play end to end — G29 on the Mac, real FFB proven on the LAN Windows x64 PC
<!-- END:sync:open-work -->

**How the LIVE blocks above are filled** (non-obvious, and it decides whether a finding you file is ever seen): `open-work` lists every `tasks/` shard with `status: active`. `live-security` lists a `security/` shard only when it is **both** `status: active` **and** `volatility: decays-with-code`. A `durable` security shard is treated as a standing rule, not an open finding, and deliberately does not appear — which is why the count reads 0 while `security/` holds a shard. File a genuine open finding as `decays-with-code` or it will stay invisible here.

## Maintenance loop (after editing shards)

```bash
scripts/kb-sync.sh
```

Use the wrapper, not `kb-sync.py` directly: it runs the whole loop *and* scrubs the absolute home path that kb-sync bakes into `INDEX.md`. This repo is public, so that path is a PII leak — the wrapper is what keeps it from coming back on every sync.

Append events to `_knowledge/log.md`. Keep the always-loaded core tiny. Write generated markdown **un-hard-wrapped** — single-line paragraphs, so Obsidian doesn't render broken line breaks.

**Two things that do not survive a fresh clone.** `_meta/` is gitignored (it is a disposable copy, and this repo has no vendored kit), so restore it by copying `tooling/*.py` from the [carryover](https://github.com/karrvel/carryover) kit into `_meta/` — without it the loop above and the pre-commit hook both fail on missing scripts. And `git config core.hooksPath .githooks` is local repo config, not committed; re-run it to re-arm the health gate.
