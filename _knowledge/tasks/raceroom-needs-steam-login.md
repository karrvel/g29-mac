---
name: raceroom-needs-steam-login
type: task
title: RaceRoom is staged but not installed — the Windows Steam client in the bottle needs a one-time interactive login
area: games
tags: [raceroom, steam, blocked]
status: active
updated: 2026-09-02
volatility: one-shot
provenance: 2026-09-01 session — Steam installed into the Sim Racing bottle; login could not be completed on the user's behalf
---

# The one part of the three-game brief that could not be finished unattended

**State (updated 2026-09-02).** The Windows Steam client is installed inside the Sim Racing bottle (`drive_c/Program Files (x86)/Steam`), has **successfully self-updated to a full 1.6 GB client**, and now runs to the login screen with `steamwebhelper` up and `connection_log.txt` reporting `[Logged Off]` — i.e. everything works and it is waiting for credentials. RaceRoom itself is still **not on disk**.

Getting there required fixing a fatal "Steam needs to be online to update" that had nothing to do with the network — see [[wine-winhttp-wpad-stalls-steam]]. If that error returns, the proxy registry keys are the first thing to re-check, and the first attempt of a run may fail before a retry succeeds.

A login in the macOS Steam app does not carry into the bottle — the Windows client keeps its own credential store and needs its own Steam Guard approval.

**Remaining work.**

```bash
cd ~/Projects/g29-mac
scripts/install-raceroom.sh login      # sign in, approve Steam Guard on your phone
scripts/install-raceroom.sh install    # queues app 211500
scripts/install-raceroom.sh play
```

**Acceptance.** `drive_c/Program Files (x86)/Steam/steamapps/common/raceroom racing experience` exists, the game launches, and a `LG4FF_SHIM_DEBUG=1` run logs `new effect … type 0x1` — the same constant-force signature Live for Speed and Speed Dreams produce, which is what proves FFB reached the wheel.

**Watch for.** Disk was 89 GB free at hand-off, comfortably enough. RaceRoom has no anti-cheat, so nothing there should fight Wine. Expect the free base game plus a paid content catalogue — see [[free-sim-shortlist]]. If Steam's in-bottle browser misbehaves during login, the fallback is copying the `ssfn*` files and `config.vdf` from the macOS Steam install to carry the machine authorisation, which still requires the password.
