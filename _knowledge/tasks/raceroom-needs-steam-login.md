---
name: raceroom-needs-steam-login
type: task
title: RaceRoom is installed under Wine 11 — the only thing left is rebuilding the FFB shim against that Wine’s SDL2
area: games
tags: [raceroom, steam, cdp, whisky, wine11, ffb]
status: active
updated: 2026-09-02
volatility: one-shot
provenance: 2026-09-02 session — logged in through CDP against wine-staging 11.16; install wizard read back live from SteamClient.Installs.GetInstallManagerInfo()
---

# Installed. Force feedback is the last piece.

**State — installed 2026-09-03 00:51.** The Windows Steam client runs under **wine-staging 11.16** in its own prefix (`scripts/steam-wine11.sh`), is **logged in**, and RaceRoom is **fully installed**: `appmanifest_211500.acf` reads `StateFlags 4`, `SizeOnDisk 79914942897`, `steamapps/common/raceroom racing experience` holds 74 GB with `Game/RRRE64.exe` and `Game/RRRE.exe` present, and `steamapps/downloading` is empty. 28 GiB free afterwards. The download took about 2h40m and averaged well under its opening 156 Mbps — it sagged to 21 Mbps mid-way and recovered.

**The first half of the acceptance test is met.** What remains is that the game launches and a `LG4FF_SHIM_DEBUG=1` run logs `new effect … type 0x1`. It will not, yet — see the force-feedback note below, which is now the whole job.

Getting there needed 18.5 GiB that did not exist: the wizard wanted 74.6 GiB against 56.1 GiB free on the only volume. A read-only audit found 54.7 GiB of pure cache and dead leftovers; deleting the safe tier freed **49.9 GiB** (53.7 → 103.7 GiB). The single biggest item was 8.6 GiB of Chromium `BrowserMetrics` telemetry inside the bottle's `htmlcache.bak-192651` — spooled by the 2398 webhelper restarts documented in [[steam-webhelper-restart-loop-in-wine]]. The crash loop wrote itself 8 GB of crash reports.

**How the install was driven** (the client's window is black, so all of this goes through CDP — see the shard):

```javascript
SteamClient.Installs.OpenInstallWizard([211500])
SteamClient.Installs.GetInstallManagerInfo()   // check nDiskSpaceAvailable first
SteamClient.Installs.ContinueInstall()         // 7 → 8, then stalls
```

It stalls at `eInstallState: 8` with no error, and the reason is only visible in the main window's DOM: an **EULA** ("You must agree to the terms of the EULA to play RaceRoom Racing Experience"). That is a licence the user has to accept, not something to click on their behalf. Its buttons sit at `Accept` (746, 718) and `Cancel` (880, 718); once accepted the state goes to **14** and the download starts.

**Watch the disk.** Steam preallocates before it transfers — `steamapps/downloading` reached 50 GB within 25 minutes while only 145 MB had actually been downloaded, taking free space back down to 54 GiB. That is normal, but the margin is thinner than the headline numbers suggest.

**Acceptance, unchanged.** `steamapps/common/raceroom racing experience` exists, the game launches, and a `LG4FF_SHIM_DEBUG=1` run logs `new effect … type 0x1` — the constant-force signature Live for Speed and Speed Dreams produce.

**Still open after that.** Force feedback is not wired for this prefix. `install-shim.sh` hardcodes Whisky's `lib/libSDL2-2.0.0.dylib`; the Wine 11 tree has its own SDL2 and the shim must be rebuilt against it before RaceRoom can produce torque. That is a separate job from getting the game on disk.

An earlier version of this shard said the blocker was an interactive login. It was not — see [[steam-webhelper-restart-loop-in-wine]] for what the blocker actually was, and how the login was completed without a visible window.
