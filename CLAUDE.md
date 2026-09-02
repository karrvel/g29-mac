# g29-mac — CLAUDE.md

> **First action every session:** `cat _knowledge/INDEX.md` — the vault is the authoritative source. This file is quick-boot orientation; the vault has the depth.

Logitech G29 force feedback on Apple Silicon (M4 Pro, macOS 26.6.2), plus three free racing sims. Everything here is free software; the tooling is written in this repo.

## The one thing to know before touching anything

**macOS cannot drive this wheel through any standard API — and that is settled, not a lead to chase.** Apple's `ForceFeedback.framework` returns `0x80000003` for the G29 (it has no PID usage page), `GCRacingWheel` is input-only, SDL2 inherits the framework's failure, and Wine therefore never exposes a force-feedback device. Torque is reached by writing Logitech's vendor HID protocol from userspace, and games reach it through a substituted SDL2 library. Read [[architecture]] before proposing any change to the FFB path.

Do not "fix" this by installing Logitech G HUB, a kext, or Apple's Game Porting Toolkit. All three were evaluated and none of them touch this problem — [[ghub-macos-does-nothing-for-wheels]], [[ffb-delivery-paths-evaluated]].

## Layout

```
g29-mac/
├── ffb-probe.c            diagnostic — proves Apple's FF stack refuses the wheel
├── lgwheel.c              the userspace driver (--detect --verify --range --autocentre --force)
├── sdl2-lg4ff-shim.c      the bridge that gives Wine-hosted Windows sims real FFB (x86_64 only!)
├── install-shim.sh        install / revert / status for the bridge — revert is always safe
├── play-*.sh              game launchers (set the wheel to 900° first)
├── _knowledge/            the vault — start at INDEX.md
└── _meta/                 disposable copy of the KB tooling — never hand-edit, it's a copy
```

The games live **outside** this folder, in the Whisky bottle named "Sim Racing". Its UUID is in `common.sh` — that file is the only link between this repo and the bottle.

## Gotchas that will cost you an hour each

- Build the shim **`-arch x86_64`** — Whisky's `winebus.so` is x86_64 under Rosetta. `build.sh` gets this right; a hand-rolled `clang` silently produces an unloadable arm64 dylib.
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
- [[raceroom-needs-steam-login]] — RaceRoom is staged but not installed — the Windows Steam client in the bottle needs a one-time interactive login
- [[shim-scoping-and-periodic-effects]] — Shim follow-ups — oscillate periodic effects instead of holding peak, and optionally scope the library override to one bottle
<!-- END:sync:open-work -->

**How the LIVE blocks above are filled** (non-obvious, and it decides whether a finding you file is ever seen): `open-work` lists every `tasks/` shard with `status: active`. `live-security` lists a `security/` shard only when it is **both** `status: active` **and** `volatility: decays-with-code`. A `durable` security shard is treated as a standing rule, not an open finding, and deliberately does not appear — which is why the count reads 0 while `security/` holds a shard. File a genuine open finding as `decays-with-code` or it will stay invisible here.

## Maintenance loop (after editing shards)

```bash
./kb-sync.sh
```

Use the wrapper, not `kb-sync.py` directly: it runs the whole loop *and* scrubs the absolute home path that kb-sync bakes into `INDEX.md`. This repo is public, so that path is a PII leak — the wrapper is what keeps it from coming back on every sync.

Append events to `_knowledge/log.md`. Keep the always-loaded core tiny. Write generated markdown **un-hard-wrapped** — single-line paragraphs, so Obsidian doesn't render broken line breaks.

**Two things that do not survive a fresh clone.** `_meta/` is gitignored (it is a disposable copy, and this repo has no vendored kit), so restore it by copying `tooling/*.py` from the [carryover](https://github.com/karrvel/carryover) kit into `_meta/` — without it the loop above and the pre-commit hook both fail on missing scripts. And `git config core.hooksPath .githooks` is local repo config, not committed; re-run it to re-arm the health gate.
