---
name: wire-up-virtualhere-bridge
type: task
title: Wire up VirtualHere + Steam Remote Play end to end — G29 on the Mac, real FFB proven on the LAN Windows x64 PC
area: cross
tags: [virtualhere, usbip, windows, steam-remote-play, ffb, bridge]
status: active
updated: 2026-09-22
volatility: decays-with-code
provenance: 2026-09-22 session — see [[bridge-wheel-to-windows-pc]] for why this replaces the Whisky/shim path for now. Server-crash finding from direct hardware diagnosis the same session (`ps`, `lsof -iTCP:7575`, `~/vhusbd_log.txt`)
---

# Current blocker (unresolved as of 2026-09-22)

VirtualHere Server is installed on the Mac (`/Applications/VirtualHereServerUniversal.app`) and its
process (`vhusbdosx`, runs as root) stays alive in `ps`, but it is **not actually serving anything**:
`~/vhusbd_log.txt` shows it enumerate the full USB tree including `[046d:c24f] "Logitech, G29 Driving
Force Racing Wheel"` and successfully bind `Listening on ... TCP port 7575`, then **4 seconds later**
unmanage every device it found and log `>>> Shutdown <<<` — and the log has had no new lines since.
`lsof -iTCP:7575 -sTCP:LISTEN` confirms nothing is listening right now. So even though the process
object is technically alive, whatever the Windows client saw or "claimed" cannot be backed by a live
share on the Mac at that moment.

The immediate-shutdown-after-successful-enumeration pattern points at a permission wall rather than a
crash — most likely a macOS system prompt (Local Network access, or a code-signing/Gatekeeper
"cannot verify developer" dialog) that appeared and was dismissed, denied, or never answered, silently
killing the server's own event loop. Not yet confirmed — next step is to Quit the app from its menu
bar icon, relaunch it, and watch for exactly what dialog (if any) appears, then re-check
`lsof -iTCP:7575` and the log's tail before trying the Windows side again.

# What's left

Server bring-up on the Mac is the actual current blocker (see above) — nothing past step 2 below has
been meaningfully attempted yet, since a claim made against a server that immediately shuts down its
listener isn't a real claim. Concrete remaining steps, in order, each one a gate for the next:

1. **Mac**: install VirtualHere Server (`VirtualHereServerUniversal.dmg` from virtualhere.com),
   launch it with the G29 plugged in, confirm it lists the wheel as a shared device **and stays
   running** — `lsof -iTCP:7575 -sTCP:LISTEN` should show it listening, not just `ps` showing the
   process alive. Done except for "stays running" — see blocker above.
2. **Windows PC**: install VirtualHere Client (free — done), confirm it auto-discovers the Mac's
   server over the LAN (mDNS) or add it by IP, right-click the G29 → "Use this device". Confirm
   Windows Device Manager shows the real Logitech G29 entry, not a generic HID device. Attempted, but
   unverified given the blocker above — redo once step 1 is confirmed actually listening.
3. **Windows PC**: install Logitech's current Windows wheel software (G HUB or LGS, whichever the G29
   support page currently points to).
4. **Prove FFB below the game layer**: Windows `joy.cpl` → wheel → Properties → Test → Force Feedback
   tab → fire a built-in test effect. This has to work before any game is brought in — it isolates
   the tunnel + driver from anything sim-specific.
5. **Windows PC**: install and run a sim natively (no longer constrained to the Wine-and-Mac-free
   shortlist in [[free-sim-shortlist]] — anything the PC can run is fair game now). Confirm FFB in an
   actual game.
6. **Steam Remote Play**: Steam + the sim on the Windows PC, Remote Play enabled; Steam or Steam Link
   on the Mac, same LAN, launch and confirm video/audio/keyboard arrive with acceptable latency.
7. **Settle the two known collision risks** before calling this done:
   - Steam Input on the Windows host will try to wrap the G29 as a generic controller — needs
     disabling per-game so it doesn't fight the native DirectInput FFB the game wants directly.
   - The Mac's own Steam/Steam Link client may try to forward the same physical wheel a second time
     as a virtual pad. VirtualHere already hands Windows the real device; a second forwarding path
     for the same hardware causes double or conflicting input and needs to be turned off.
8. Once felt-good on Wi-Fi or not, move the Mac↔PC link to wired Ethernet (user's own stated next
   step) and re-judge whether FFB latency actually improves.

# Acceptance

Sitting at the Mac's rig, wheel plugged into the Mac, a racing sim running natively on the Windows PC
and displayed via Steam Remote Play on the Mac, produces felt force feedback that responds to the
game in real time — not rumble, not a fixed pull, actual torque that follows what's on screen.

# Not this task

Reinstalling Whisky/Wine, or fixing the SDL2 shim — that path is parked, see
[[bridge-wheel-to-windows-pc]], [[raceroom-needs-steam-login]], [[shim-scoping-and-periodic-effects]].
Also not this task: the SuperTuxKart-on-native-macOS axis issue the user hit while testing Darwin 27 —
that's a native-macOS-game input question, unrelated to this bridge, and nothing here has diagnosed
it yet.
