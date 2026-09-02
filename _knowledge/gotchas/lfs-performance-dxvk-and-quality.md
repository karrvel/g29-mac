---
name: lfs-performance-dxvk-and-quality
type: gotcha
title: LFS stutters on Wine's builtin D3D11 renderer — DXVK plus trimmed quality took it from ~39-58 fps to a steady 100
area: wine
tags: [performance, dxvk, lfs, d3d11, whisky]
status: active
updated: 2026-09-02
volatility: decays-with-code
provenance: 2026-09-02 session — measured with the DXVK frame-rate overlay before and after, on the M4 Pro
---

# The bottle ships with the slow renderer selected

Whisky creates bottles with **DXVK off** (`Metadata.plist` → `dxvkConfig.dxvk = false`), so D3D11 titles run on wined3d's own Vulkan backend. The launch log gives it away:

```
err:winediag:wined3d_adapter_create Using the Vulkan renderer for d3d10/11 applications.
```

That path is much slower than DXVK and, more importantly, paces frames badly — which is felt as lag even when the average frame rate looks survivable.

**Measured on this machine** (DXVK overlay, same in-car scene):

| Configuration | FPS | Frame times |
|---|---|---|
| DXVK, stock quality | 58 | very spiky (menu 39 fps, spikes to 38 ms) |
| DXVK + trimmed quality | **100** | **8.5 - 11.5 ms** |

The tight frame-time spread is the real win. A 3 ms range is smooth; the earlier spikes are what "lagging a lot" actually meant.

**Enabling DXVK by hand** (Whisky's toggle does the same thing): copy `Libraries/DXVK/x64/{d3d11,dxgi,d3d10core}.dll` into the bottle's `system32` (and `x32` into `syswow64`), then set each to `native` under `HKCU\Software\Wine\DllOverrides`. Originals are backed up in the bottle at `dll-backup-builtin/`. A quick check that it took: builtin `d3d11.dll` is ~110 KB, DXVK's is ~3.9 MB.

**The quality settings that mattered**, edited in `drive_c/LFS/cfg.txt` with LFS closed (it rewrites the file on exit, so edits made while running are lost): `Antialiasing 0 0`, `Mirror AA 0 0`, `Shadow Cascades 2` (was 4), `Dynamic Reflect 2 1` (was 8 4), `Mirror External 0`. Backup at `cfg.txt.backup-preopt`.

**Diagnostics worth reusing.** `LFS_HUD=1 ./play-lfs.sh` shows the overlay. If frame rate is low but CPU sits near idle (~12% here), the bottleneck is presentation or GPU, not translation — do not go chasing Rosetta. Note LFS.exe is a 32-bit PE and runs through Wine's `x86_32on64` path, which was *not* the limiter here.

**Ceiling:** MoltenVK generally offers only FIFO presentation, so vsync cannot really be disabled — `dxgi.syncInterval = 0` in `dxvk.conf` is set but the display refresh still bounds things. Above ~100 fps is not the goal anyway; stable frame times are.
