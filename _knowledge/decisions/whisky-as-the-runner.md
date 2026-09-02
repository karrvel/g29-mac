---
name: whisky-as-the-runner
type: decision
title: Run the games on Whisky 2.3.5 despite the project being archived, because it is free, installed, and verified working on macOS 26
area: wine
tags: [whisky, crossover, sikarugir, runner]
status: active
updated: 2026-09-02
volatility: decays-with-code
provenance: 2026-09-01 session — wine-7.7 smoke-tested on macOS 26.6.2 before committing to it
---

# Archived upstream, but it runs

**Context.** No proper racing sim runs natively on macOS ([[free-sim-shortlist]]), so the games must run under a Windows translation layer. Options: Whisky 2.3.5 (already installed, free, but the project was archived in 2025 with Wine pinned at 7.7), CrossOver (actively developed, $74/yr after a 14-day trial), Sikarugir (free, the Kegworks/Wineskin successor), or Apple's Game Porting Toolkit (graphics only — it changes nothing in the HID path).

**Decision.** Use Whisky, in a dedicated bottle named "Sim Racing" separate from the pre-existing "click" bottle.

**Why.** It was already on the machine, it costs nothing, and — the part that actually decided it — `wine-7.7` was smoke-tested on macOS 26.6.2 before anything was built on top of it, rather than trusted. Both games then launched and rendered: Live for Speed drives D3D11 through Vulkan/MoltenVK, Speed Dreams runs clean with no errors. "Archived" describes the project's maintenance status, not whether the binary works today. Paying $74/yr for a runner in order to then pay $12 for an FFB bridge was the wrong shape for a free-software brief.

**Consequences.**

- No upstream fixes are coming. If macOS 27 breaks wine-7.7, the migration target is Sikarugir (free) or CrossOver, and the shim must be rebuilt against that runner's SDL2.
- Wine 7.7 is old enough that newer DX12 titles are out of scope. It is fine for the DX9/DX11-era sims that are actually free.
- The commercial FFB bridges don't list Whisky as a supported host — which is moot here, since the shim was built rather than bought.
- The bottle needs `Enable SDL` set on `winebus` for the FFB path to exist at all; this is not Whisky's default.
