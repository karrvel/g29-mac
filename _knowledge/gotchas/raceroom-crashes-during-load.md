---
name: raceroom-crashes-during-load
type: gotcha
title: RaceRoom null-derefs during loading at rrre64+0xc1ddad in every configuration tried — wheel or not, shim or not, SDL backend on or off — and is not yet playable
area: games
tags: [raceroom, winebus, sdl, ffb, wine11, crash]
status: active
updated: 2026-09-03
volatility: decays-with-code
provenance: 2026-09-03 session — reproduced four times on wine-staging 11.16, including a control run with the shim removed entirely
---

# One address, every configuration — cause still unknown

RaceRoom reaches its "Loading RaceRoom…" splash and dies there. **Do not repeat the eliminations below**; each cost a full launch-and-wait cycle.

Force feedback in this project needs `winebus` → **`Enable SDL = 1`**, because the shim works by substituting the SDL2 that Wine's joystick backend loads. With that set, RaceRoom dies roughly **three minutes** after launch, every time:

```
Unhandled exception: page fault on read access to 0x0000000000000000
  in 64-bit code (0x00000140c1ddad)
=>0 0x140c1ddad in rrre64   movq (%rax), %rdx     rax = 0
  1 0x140adef34 in rrre64
  2 0x141042979 in rrre64
  3 kernel32+0x27a49                              (BaseThreadInitThunk)
  4 ntdll+0x5787f
```

A game worker thread dereferences a null pointer it never checked. Always the same address.

**The shim is not the cause.** A control run with the real `libSDL2-2.0.0.dylib` restored — shim entirely out of the process, zero `[lg4ff-shim]` lines in the log — crashes at the **identical address**. It is Wine's SDL joystick backend and RaceRoom that do not get along.

| Configuration | Outcome |
|---|---|
| `Enable SDL = 1`, shim v1 (52 symbols) | crash |
| `Enable SDL = 1`, shim v2 (55 symbols + serial fix) | crash |
| `Enable SDL = 1`, real SDL2, shim removed entirely | crash — **same address** |
| `Enable SDL = 0` | crash (survived 4:23 once, which misled) |
| `Enable SDL = 0` + `winebus DisableInput` for 046d:c24f | crash |
| **G29 physically unplugged** | **crash — same address** |
| RetinaMode y / n | no difference |
| **launched properly via Steam** (`steam://run/211500`, real client, real ticket) | **crash — died inside 1 minute** |

**Nothing in our stack is implicated.** The wheel is not the trigger, the shim is not the trigger, the SDL backend is not the trigger. The game reached its portal UI exactly once (screenshot at ~02:20) and probably only because the observation window closed before the crash fired.

There is no config to clear — the game never writes one, since it dies before reaching a menu. No module fails to load; the only Wine errors present are the benign clipboard and kerberos ones.

## RaceRoom is anti-debug protected, and that reframes every test above

`winedbg` is **not available for this game**. Attaching it produces the game's own refusal:

> The application cannot run under a debugger. Deactivate all active debugging and run the application again.

The game ships `Game/protect.exe` and `Game/protect.dll` and polices its execution environment. So no live debugging, and the saved `backtrace-*.txt` dumps are all the fault detail obtainable.

**This is the best-fitting explanation for the crash so far**, because the profile matches an integrity check rather than a rendering or input bug:

- a background thread off `BaseThreadInitThunk`, not the main or render thread
- fires at irregular intervals between one and four minutes, never at a fixed point
- one constant address across every configuration
- completely indifferent to graphics settings, input backends, the shim, and whether the wheel is even connected

**And it invalidates the launch method behind every test in the table above.** All of them ran `Game/x64/RRRE64.exe` directly from a shell with `SteamAppId`/`SteamGameId` faked as environment variables. That is convenient for iterating, but it is not how a protected Steam title expects to start — no real client parentage, no real ownership ticket. Launch it the way it is designed to be launched:

```bash
scripts/steam-wine11.sh play          # or, with the client already up:
#   SteamClient.URL.ExecuteSteamURL("steam://run/211500")   via CDP
```

**Launching correctly does not fix it.** The table's last row is a real Steam launch — client running and logged in, ownership ticket genuine, process parented by Steam — and the game still died inside a minute. So the direct-exe shortcut used for the earlier tests was not what caused those crashes, and the protection theory, while still the best fit for the crash *profile*, has no fix attached to it. Nothing about the protection itself should be touched; the point was only ever to start the game the way it expects.

## Launching it at all: two invisible modals and a wedged cloud sync

Independent of the crash, getting the game to *start* is unreliable by hand, because Steam blocks the launch behind dialogs that cannot be seen on a black window. `scripts/play-raceroom.sh` handles all of this; the detail is here in case it changes.

**"Cloud Out of Date"** is the one that bites repeatedly. Every crashed session leaves an un-uploaded save, so after the first crash this appears on *every* launch:

> You played RaceRoom Racing Experience on the PC "" (Today at 1:45 PM), and that save is not yet in the cloud. (upload not started)
> **[Play anyway]** [Cancel]

It renders **inside the main "Steam" window**, not as a separate `Steam Dialog` target, and "Cancel" sits immediately beside the affirmative button — so pick the button by matching its text, never by taking the first one found.

**Before that dialog even exists**, the launch parks on `strTaskName: "SynchronizingCloud"`, `strTaskDetails: "pendingcloudsessions"` with `bWaitingForUI: false`, and nothing happens. `GetCloudPendingRemoteOperations` returns empty — there is no actual work pending. `SteamClient.Apps.ContinueGameAction(id)` is what flips it to `bWaitingForUI: true` and causes the dialog to be created. Without that nudge the launch simply times out, which is what "RaceRoom did not start within two minutes" means.

A launch that never completes also leaves a `LaunchApp` action wedged, and Steam will not start another while it exists — cancel any stale ones before launching.

**Remaining leads:** retry the `-dx11` and `Game/x64dxvk/` variants now that `d3dx9`/`d3dcompiler_43` are correct (both were tested *before* that fix and their failures may have been the shader bug, not this one); and study how Proton runs this title, since it reportedly does — its DLL override set would say which component matters.

Live for Speed and Speed Dreams are untouched: Whisky bottle, wine-7.7, full force feedback. Nothing here costs anything outside RaceRoom.

**Method note that cost real time here.** The failure takes ~3.3 minutes. Two separate "it's fixed" calls were made off observation windows of 2:06 and 3:26 — one shorter than the failure, one barely longer — and both were wrong. Any stability claim about this crash needs **six minutes or more** before it means anything. See also [[shim-symbol-count-is-per-wine-version]].
