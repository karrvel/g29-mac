---
name: raceroom-dx9-effect-compile-fails
type: gotcha
title: RaceRoom’s DX9 effect compile fails until BOTH d3dx9_43 and d3dcompiler_43 are native — Wine’s ID3DXEffectCompiler is a stub
area: games
tags: [raceroom, dxvk, d3d9, d3dx9, moltenvk, wine11]
status: resolved
updated: 2026-09-03
volatility: durable
provenance: 2026-09-03 session — reproduced on wine-staging 11.16, MoltenVK 1.4.0, against RaceRoom app 211500 build of 2026-09-02
---

# Solved: two DLLs, not one

**The fix.** Both of these must be Microsoft's, not Wine's:

```bash
winetricks -q d3dx9 d3dcompiler_43     # then, in the prefix registry:
#   d3dx9_24..43     = native
#   d3dcompiler_43   = native
```

`d3dx9_43` alone is **not enough**, which is what made this confusing. Microsoft's d3dx9 delegates the actual shader compile to `d3dcompiler_43`, and with that still resolving to Wine's builtin the effect compiler cannot be created. `WINEDEBUG=+loaddll` is what shows it — look for `native` against both names:

```
Loaded L"C:\\windows\\system32\\d3dx9_43.dll" ... : native
Loaded L"C:\\windows\\system32\\d3dcompiler_43.dll" ... : native
```

Do not copy the DLLs the game ships. `Game/d3dcompiler_43.dll` **and** `Game/x64/d3dcompiler_43.dll` are both **PE32 (32-bit)** despite the directory name, so dropping the "x64" one into `system32` silently breaks the 64-bit process. Take them from the DirectX redist via winetricks.

The root cause, from `WINEDEBUG=+d3dx` with Wine's builtin in place:

```
fixme:d3dx:d3dx9_effect_compiler_init ID3DXEffectCompiler implementation is only a stub.
fixme:d3dx:ID3DXEffectCompilerImpl_CompileEffect ... stub!
```

The game calls `D3DXCreateEffectCompiler`, and Wine's implementation does nothing at all. That is also why **Retry and Cancel both do nothing** — there is no path forward through a stub.

## What was tried first and did not work

Symptom, as a Win32 modal with Retry/Cancel:

```
Failed to compile effect 'c:\program files (x86)\steam\steamapps\common\
raceroom racing experience\game\shaders\dx9\fx\postprocessing.fx'
```

Everything up to that point works: the client launches the app, Steam DRM is satisfied, `Game/x64/RRRE64.exe` starts, MoltenVK creates a device. `.fx` files are **D3DX9 effects**, compiled at runtime, and `D3DXCreateEffectFromFile` validates each technique against the D3D9 device's caps before it will hand one back.

**Two fixes that look obvious and are not.**

*Native d3dx9 does not help.* `winetricks -q d3dx9` installs Microsoft's real `d3dx9_24`–`_43` and sets them native — the override registers, the DLL loads (2.4 MB `d3dx9_43.dll`), and the effect still fails. So this is not Wine's d3dx9 reimplementation falling short. Worth knowing the game also ships its own `Game/d3dx9_43.dll`, though the x64 build runs from `Game/x64/` and so takes the system one.

*DXVK's D3D9 cannot run here at all.* RaceRoom ships a whole `Game/x64dxvk/` build with its own `d3d9.dll` for exactly this scenario, and Steam exposes it as a "RaceRoom with DXVK" launch option. It fails with `err: DxvkAdapter: Failed to create device`. Installing DXVK's `d3d9.dll` into the prefix instead fails the same way at **1.10.3** and, differently but fatally, at **3.1** (`warn: DXVK: No adapters found`). DXVK's D3D9 needs Vulkan features MoltenVK does not expose — note that this is specific to D3D9: DXVK's **d3d11** works fine on the same machine and is what gives Live for Speed 100 fps. So there is no way to get off wined3d for D3D9 here.

`-steam -dx11`, the third launch option in the app's launch config, does **not** avoid it either — the same dialog appears.

**What this leaves.** The DX9 renderer's postprocessing effects do not compile against wined3d's D3D9 caps on MoltenVK, and neither of the two escape routes is available. Untried, in rough order of promise:

1. **Press Cancel, not Retry.** Games frequently carry on without an optional postprocessing effect. Five seconds to test and nobody has tested it.
2. Get the actual reason: set `d3dx9_43` back to **builtin** and run with `WINEDEBUG=+d3dx,+d3d9` — Wine's own implementation logs which profile or cap it rejected, which native Microsoft code silently swallows.
3. Disable postprocessing in the game's config before first run, so the effect is never requested. RaceRoom keeps settings under `Documents/My Games/SimBin/…`; nothing is written yet because the game has never reached a menu.

**A method note, because it cost time here.** This failure is a **Win32 message box** — it writes nothing to stderr, so `grep`-ing the Wine log for "failed to compile" returns zero and reads as success. It was reported as fixed twice on that basis before the screenshot showed otherwise. The reliable signal without eyes on the screen is **process CPU**: a rendering game holds >5%, a game blocked on a modal sits at exactly `0.0`.

Unaffected by all of this: Live for Speed and Speed Dreams, which are D3D11/OpenGL and run in the Whisky bottle. See [[raceroom-needs-steam-login]] for how the game got installed and [[steam-webhelper-restart-loop-in-wine]] for the Steam side.
