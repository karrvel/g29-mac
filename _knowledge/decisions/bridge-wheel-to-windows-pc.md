---
name: bridge-wheel-to-windows-pc
type: decision
title: Tunnel the G29's raw USB to a LAN Windows x64 PC over VirtualHere instead of reinstalling Wine — real FFB happens on Windows, Steam Remote Play only carries video/audio back
area: cross
tags: [ffb, usbip, virtualhere, windows, steam-remote-play, streaming]
status: active
updated: 2026-09-22
volatility: decays-with-code
provenance: 2026-09-22 session — macOS upgraded to Darwin 27 ("macOS 27"), Whisky/Wine deleted for storage, a LAN Windows x64 PC became available; VirtualHere pricing verified live against virtualhere.com and via search
---

# Context

Whisky/Wine (the entire runner this repo's `scripts/` and the SDL2 shim target) was deleted from this
Mac for storage reasons. Separately, macOS was upgraded to Darwin 27 ("macOS 27"). And separately
again, a Windows x64 PC is now reachable on the same LAN as this Mac. `lgwheel`/`ffb-probe` (the raw
HID path, no Wine dependency) still compile clean against Darwin 27's frameworks — see
`make tools` — so nothing here says macOS itself broke; the local FFB pipeline is gone because Wine
is gone, not because of the OS bump.

The user's first idea was to use Steam Remote Play ("Steam screen sharing") to reach the Windows PC.
[[ffb-delivery-paths-evaluated]] already closed that door for the wheel itself: Steam Link/Remote
Play forwards input as a **virtual Xbox controller**, which means rumble motors at best — the real
Logitech FFB protocol and the wheel's 900° rotation are both lost. That row is unchanged by anything
below; Remote Play still cannot be how the wheel talks to the game.

That same reference table separately ranks "stream from a Windows PC via USB-over-IP" as **the only
true-FFB streaming route**, marked at the time as "paid licence + a PC." That price was never
actually verified. It was, this session: **VirtualHere's Windows client is free (unlimited
installs), and the macOS server's trial edition shares exactly one device with no time limit** —
sources: virtualhere.com's server/purchase pages and a corroborating search result quoting the
trial-restriction table. This setup only ever needs to share one device (the G29), so for this exact
topology VirtualHere is free, not paid.

The table's other objection to any Windows route — "the killer that rules out every Windows VM" —
is that Logitech's FFB drivers (`logi_joy_bus_enum.sys` etc.) are **x64 kernel-mode drivers that
cannot load on Windows-on-ARM**. That killer is about the CPU architecture, not about VMs
specifically. The available PC is confirmed **x64 (Intel/AMD)**, so it does not apply here — a real
x64 Windows install loads Logitech's kernel drivers the ordinary way, no different from any other
Windows gamer's FFB wheel.

# Decision

Plug the G29 into **the Mac** (that's where the rig/desk is). Run a VirtualHere server on the Mac
sharing the G29, and a VirtualHere client on the Windows PC that claims it — this tunnels the real
USB protocol, so Windows sees the actual hardware device and its native Logitech driver stack does
real FFB, no shim and no Wine involved at all. The racing sim runs natively on Windows. Steam Remote
Play (or an equivalent) streams video/audio/keyboard from the Windows PC back to the Mac — display
only, never the wheel's data path.

Network link is Wi-Fi for now, wired Ethernet later. FFB updates are latency-sensitive, so expect
Wi-Fi to feel less crisp than wired; that's a tuning question, not a go/no-go one.

# Rationale (why this, not the alternatives)

- **Not reinstalling Whisky/Wine**: it was deleted for storage, and a real Windows PC makes the whole
  shim ([[build-sdl2-shim-not-buy-torqer]]) unnecessary — Windows already has correct native FFB, so
  there is nothing to substitute. The Whisky/shim path isn't wrong, it's just no longer the best
  available option now that a real Windows machine exists.
- **Not wheel-on-Windows-PC-directly**: works and is simpler (no VirtualHere at all), but means
  sitting at the Windows PC to drive, not at the Mac/rig. Rejected because the point of this bridge
  is to keep playing from the existing setup.
- **Not Steam Remote Play carrying the wheel**: already closed, see Context above — virtual-pad
  forwarding, rumble only, 900° lost.
- **VirtualHere over a free/open usbip stack**: no working macOS `usbip` server was found; VirtualHere
  is the one that ships a macOS server binary at all, and it's free for this exact 1-device case.

# Consequences

- The three sims picked in [[free-sim-shortlist]] were chosen partly because they're free *and* run
  under Wine on a Mac. Running natively on a real Windows PC removes that second constraint —
  anything the Windows PC can run is back on the table, including paid sims, if that's ever wanted.
- [[raceroom-needs-steam-login]] and [[shim-scoping-and-periodic-effects]] both depend on the
  Whisky/Wine-11 prefixes that no longer exist on this machine. They are not superseded (the work
  they describe is still real and still there if Wine ever comes back) but they are currently
  unactionable — parked, not closed. The active task now is [[wire-up-virtualhere-bridge]].
- `architecture.md`'s "two paths that work" diagram gains a third: `game → native Windows DirectInput
  → Logitech kernel driver → VirtualHere tunnel → G29 motor`. Worth folding in once the bridge is
  verified end-to-end on hardware.

## Re-check triggers

Re-verify VirtualHere's trial terms if this is revisited much later — pricing pages change. Re-open
this decision if the "LAN Windows PC" stops being available, or if it turns out to be ARM after all
(it was reported x64 but wasn't inspected directly from this session).
