---
name: ffb-delivery-paths-evaluated
type: reference
title: Every route to G29 force feedback on Apple Silicon, evaluated and ranked — so nobody re-derives this
area: cross
tags: [ffb, vm, cloud-gaming, streaming, landscape]
status: active
updated: 2026-09-22
volatility: decays-with-code
provenance: 2026-09-01 multi-agent research pass (12 agents) + local verification; vendor claims marked as such. VirtualHere pricing re-verified live 2026-09-22 against virtualhere.com — see [[bridge-wheel-to-windows-pc]]
---

# The landscape, so this is never re-researched from scratch

Ranked by (produces torque) × (game is playable) × (cost). The chosen path is #1; everything below it is recorded so the options aren't re-explored later.

| Path | Torque | Playable | Cost | Verdict |
|---|---|---|---|---|
| **Raw HID + SDL2 shim** (this repo) | ✅ verified | ✅ | free | **chosen** — [[build-sdl2-shim-not-buy-torqer]] |
| Torqer / CrossWheel + CrossOver or Sikarugir | ✅ vendor claim | ✅ | $12 / €20, + $74/yr if CrossOver | works, but paid and Whisky is unsupported |
| SDL3's built-in lg4ff driver | ✅ | ⚠️ | free | real, but almost no macOS *game* consumes it |
| Stream from a Windows PC via **VirtualHere** USB-over-IP | ✅ | ✅ | free for 1 device + a PC | the only true-FFB streaming route — **adopted**, see [[bridge-wheel-to-windows-pc]] |
| Moonlight / Sunshine or Steam Link | ❌ | ✅ | free | Sunshine presents a **virtual Xbox pad** — rumble, not FFB, and 900° is lost |
| Parallels / VMware Fusion / UTM + Windows-on-ARM | ❌ | varies | varies | see the killer below |
| UTM + Linux + new-lg4ff | ✅ likely | ❌ | free | torque yes, but no usable 3D acceleration |
| GeForce NOW free tier | ❓ | ✅ | free | unresolved on the macOS client; bring-your-own-library anyway |
| Boot Camp | — | — | — | does not exist on Apple Silicon |

## The killer that rules out every Windows VM

It is **not** USB passthrough. Logitech states the G29 "require[s] LGS to be installed on your PC for force feedback to function", and that software installs **x64 kernel-mode drivers** (`logi_joy_bus_enum.sys`, `logi_joy_vir_hid.sys`, `logi_joy_xlcore.sys`). **Windows on ARM cannot load x64 kernel drivers.** So passthrough succeeding tells you nothing — the driver above it can never load. This makes VMware Fusion being free since Nov 2024 irrelevant, and Parallels' good DirectX support irrelevant too.

This killer is about **CPU architecture**, not about VMs specifically — it applies equally to a physical Windows-on-ARM machine. It does **not** apply to a real x64 Windows PC (Intel/AMD), VM or otherwise: x64 kernel drivers load there the ordinary way. That's what makes the VirtualHere row above viable now that one is on the LAN — see [[bridge-wheel-to-windows-pc]].

**VirtualHere pricing, corrected 2026-09-22**: the earlier "paid licence" note undersold this. The Windows **client** is free with unlimited installs; the macOS **server** trial shares exactly **one device with no time limit** (verified against virtualhere.com's server/purchase pages and a corroborating trial-terms search result). A single-wheel bridge only ever needs to share one device, so this path is free for this exact case, not paid.

## Worth knowing

- **SDL3 ≥ 3.4.0** ships `SDL_hidapi_lg4ff.c` (merged 2025-03-17, ported from Linux `new-lg4ff`), **default-enabled on macOS**. SDL3 3.4.14 is already installed here via Homebrew. Nobody upstream tested this path on macOS before it merged, and SDL3 does not take exclusive HID access on macOS — so two consumers can contend.
- **Feral Interactive's** old Mac ports had working wheel FFB because they **embedded their own wheel driver**. That is the only way a native macOS game has ever done it, and it is not a general solution.
- **Apple's Game Porting Toolkit** is graphics-only. It changes nothing in the HID or FFB path — don't expect a GPTK upgrade to help here.
- The kext-era macOS workarounds (**FreeTheWheel**, `LogitechForceFeedback.kext`) are dead on Apple Silicon under SIP.

## Re-check triggers

Revisit this table if the runner moves to a Wine that links SDL3 (the shim becomes unnecessary), if a native macOS sim ships that uses SDL3 haptics, or if Logitech ever publishes a macOS wheel driver — see [[ghub-macos-does-nothing-for-wheels]] for why that last one is unlikely. Also revisit the VirtualHere row if its trial terms ever change, or if the LAN Windows x64 PC this session found stops being available.
