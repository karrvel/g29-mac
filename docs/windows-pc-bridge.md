# Bridging the G29 to a Windows PC (VirtualHere + Steam Remote Play)

Use this page if Wine/Whisky isn't installed on this Mac, or you'd rather run the sim natively on a
Windows PC and just play from the Mac. The rest of `docs/` (`games.md`, `how-it-works.md`) covers the
Whisky/shim path instead; the two are independent — pick whichever machine actually has the game
installed. Why this approach and not Steam Remote Play carrying the wheel directly, or a Windows VM,
is in [`../_knowledge/decisions/bridge-wheel-to-windows-pc.md`](../_knowledge/decisions/bridge-wheel-to-windows-pc.md).

## The shape of it

```
G29 ──USB──► Mac ──VirtualHere (LAN)──► Windows PC ──runs the sim, real Logitech FFB──┐
                                                                                        │
Mac ◄──Steam Remote Play (video/audio/keyboard, LAN)── Windows PC ◄────────────────────┘
```

The wheel stays plugged into the Mac. VirtualHere tunnels its *raw USB protocol* to the Windows PC,
so Windows sees the actual G29 hardware and its native Logitech driver stack produces real force
feedback — no shim, no Wine, nothing translated. Steam Remote Play only ever carries the picture and
sound back; it never touches the wheel's data path. Needs both machines on the same LAN.

## Requirements

- The Windows PC must be **x64 (Intel/AMD)**, not ARM. Logitech's FFB drivers are x64 kernel-mode
  drivers and cannot load on Windows-on-ARM — this is a hard wall, not a config issue.
- Same local network on both machines. Wi-Fi works; wired Ethernet will feel crisper for FFB, since
  force updates are latency-sensitive.

## Setup

### 1. Mac — share the wheel

Download **VirtualHere Server** from [virtualhere.com/osx_server_software](https://www.virtualhere.com/osx_server_software)
(`VirtualHereServerUniversal.dmg`, Intel + Apple Silicon), drag it to Applications, and launch it with
the G29 plugged in. It should list the wheel as a shared device. The trial edition shares exactly one
device with no time limit — that's all this needs, so there's nothing to buy for a single wheel.

### 2. Windows PC — claim the wheel

Install the free **VirtualHere Client**. It should auto-discover the Mac's server over the LAN
(mDNS); if not, add it manually by the Mac's LAN IP. Right-click the G29 entry → **Use this device**.
Check Windows Device Manager: it should show the real Logitech G29 wheel, not a generic HID device.

### 3. Windows PC — install Logitech's wheel software

Install G HUB (or Logitech Gaming Software, whichever the current G29 support page points to) so FFB
profiles and calibration are available the normal Windows way.

### 4. Prove FFB below the game layer

Before touching any game: Windows Settings → **Set up USB Game Controllers** (`joy.cpl`) → select the
wheel → Properties → **Test** tab → **Force Feedback** sub-tab → fire a built-in test effect. If the
wheel doesn't resist here, stop and fix this layer first — nothing above it (Steam, the game) can
paper over a broken tunnel or a driver that didn't load.

### 5. Install and run the sim — natively, on Windows

No Wine, no shim. This also lifts the "must run under Wine on a Mac" constraint that shaped
[`../_knowledge/decisions/free-sim-shortlist.md`](../_knowledge/decisions/free-sim-shortlist.md) — any
sim the Windows PC can run is fair game now, not just that shortlist.

### 6. Steam Remote Play — carry the picture back

Install Steam on the Windows PC, add/launch the sim there, and turn on Remote Play in Steam's
settings. On the Mac, install Steam (or the Steam Link app); on the same LAN it should find the
Windows PC's session.

### 7. Two settings to get right before playing

- **Disable Steam Input for the wheel** on the Windows host (per-game, in Steam's controller layout
  settings). Left on, Steam tries to remap the G29 as a generic Xbox-shaped controller on top of the
  real DirectInput FFB the game wants to talk to directly.
- **Don't let the Mac's Steam/Steam Link client also forward the wheel.** VirtualHere already hands
  Windows the real physical device; Steam Remote Play forwarding the same wheel a second time (as a
  virtual pad) causes double or conflicting input. Remote Play should carry video/audio/keyboard
  here, nothing from the wheel.

## If it doesn't feel right

Force feedback that feels laggy, notchy, or delayed on Wi-Fi is expected to some degree — move to
wired Ethernet between the Mac and the Windows PC and re-judge. If the built-in Windows FFB test
(step 4) doesn't work at all, the problem is in the tunnel or the driver, not the game — re-check
steps 1–3 before going further.
