---
name: steam-webhelper-restart-loop-in-wine
type: gotcha
title: Steam's UI never opens in the bottle — steamwebhelper dies and Steam relaunches it every 10 seconds, which is the "not responding" dialog you see
area: wine
tags: [steam, cef, crashpad, chromium, whisky, raceroom]
status: active
updated: 2026-09-02
volatility: decays-with-code
provenance: 2026-09-02 session — diagnosed live against Whisky's wine-7.7 and Steam client 1788291500 (CEF/Chrome 126.0.6478.183); logs kept in Steam/logs and a WINEDEBUG=err+all capture
---

# Steam starts, shows nothing, and complains that a component stopped responding

Symptom: `scripts/install-raceroom.sh login` puts a Steam process on screen, no UI ever paints, and Steam eventually offers its own "not responding / restart Steam" box. The give-away in the logs is `Steam/logs/webhelper.txt`: a new `Startup - webhelper launched pid: …` line **every ten seconds**, with `-startcount=` climbing without bound (it had reached 2398 before this was looked at). Steam.exe is alive and healthy the whole time — it is the CEF child, `bin/cef/cef.win64/steamwebhelper.exe`, that never survives startup, and Steam's watchdog restarts it forever.

There are **two** distinct failure modes, one behind the other, and fixing the first only exposes the second.

**1. The crash handler kills the browser process.** In a `WINEDEBUG=err+all` capture, every cycle contains exactly this pair (22 occurrences, 22 restarts, 1:1):

```
C:\Program Files (x86)\Steam\bin\cef\cef.win64\steamwebhelper.exe: invalid option -- `-n'
[ERROR:crashpad_client_win.cc(144)] crash server failed to launch, self-terminating
```

The first line is crashpad's bundled getopt rejecting `-n…`, i.e. Steam's own `-nocrashdialog`. Steam launches the webhelper with single-dash flags (`-nocrashdialog -lang=en_US -cachedir=… -steampid=…`); under Wine those flags reach the crashpad **handler** process, whose parser only accepts crashpad's own `--long-options`. The handler exits, the client's registration pipe breaks (`registration_protocol_win.cc(136) TransactNamedPipe: Broken pipe`), and Chromium deliberately self-terminates when the crash server won't start. Steam ships this same command line on Windows and works there, so the argument leak is Wine's doing rather than a Steam bug — an inference from that, not something observed on Windows here.

No launch option turns it off. `strings` on `Steam.exe`/`steamui.dll` gives the complete accepted set — `-cef-disable-gpu`, `-cef-disable-gpu-sandbox`, `-cef-disable-sandbox`, `-cef-disable-seccomp-sandbox`, `-cef-force-accessibility`, `-cef-force-gpu`, `-no-cef-sandbox` — and none of them touch crash reporting. What does work is CEF's own rule that crash reporting is enabled **only** when a config file sits next to the binary:

```bash
mv "$WINEPREFIX/drive_c/Program Files (x86)/Steam/bin/cef/cef.win64/crash_reporter.cfg"{,.disabled}
```

That takes both log lines above to zero. Steam rewrites the file when the client version changes, so re-check it after an update.

**2. The webhelper then crashes anyway.** With crash reporting disabled the browser process gets further — it reaches `browser_management_service.cc` in `cef_log.txt`, spawns its network and storage utility children — and then dies on `wine: Unhandled exception 0x80000003 … at address 00000001859EF905` (STATUS_BREAKPOINT: a Chromium `CHECK`/`IMMEDIATE_CRASH`, same address every time, inside libcef). The 10-second loop continues, unchanged from the user's point of view. Whisky ships no `winedbg`, so there is no backtrace to be had from this bottle.

**Where that leaves it.** Whisky's Wine is **7.7** (CrossOver-derived, 2023) and the Steam client now embeds Chrome 126. That combination does not run here, and it is not a configuration problem to be flagged away. See [[raceroom-needs-steam-login]]. Live for Speed and Speed Dreams are unaffected: they do not involve Steam, CEF, or any of this.

## A newer Wine kills the loop — and leaves a black window

Tested the same Steam install (client 1788291500) under **wine-staging 11.16** (the official WineHQ macOS build) in a fresh prefix, driven by `scripts/steam-wine11.sh`. Everything above goes away: `--type=crashpad-handler` stays alive as a process, `webhelper.txt` records **one** launch instead of one every ten seconds, the GPU and renderer processes come up, `Failed creating offscreen shared JS context` never appears, the two-minute WPAD stall does not happen, and `connection_log.txt` reaches `ConnectionCompleted()` against a Steam CM. That is the crashpad argument leak confirmed as a Wine bug and fixed upstream.

**The window is still black.** Steam itself is healthy — `steamui_html.txt` shows `CreateBrowser` → `BrowserReady` → two `PopupHTMLWindow`s, and `cef_log.txt` shows `library.js` executing the login poller — so the client is running and only the pixels are missing. Every GPU report in every run carries `problems[6]: Gpu compositing has been disabled`, because ANGLE only ever exposes **OpenGL ES 2.0** (over MoltenVK) where Chromium wants 3.0; Chromium therefore falls back to software compositing, and that is what is failing to reach the window.

Tried and did not help, so do not repeat them: `-cef-disable-gpu` (adds `--disable-gpu --disable-gpu-compositing`; still black), installing DXVK into the prefix so ANGLE could use a real D3D11 (ANGLE still fell back to Vulkan — `Renderer11.cpp:1108 Error querying driver version from DXGI Adapter`), `ANGLE_DEFAULT_PLATFORM=swiftshader` (ignored, ANGLE stayed on MoltenVK), and `WINEDLLOVERRIDES="dcomp=d"` to force Chromium off DirectComposition. A Wine **virtual desktop** (`explorer /desktop=`) is actively worse: it brings the ten-second webhelper restart loop back.

The complete `-cef-*` option set is listed above and none of it reaches the presentation path.

## The black window does not actually block anything — drive the client over CDP

Steam's UI is a Chromium app, so it can be operated with no visible pixels at all. Drop a marker file next to the client and it opens a DevTools endpoint on port 8080:

```bash
touch "$STEAM_DIR/.cef-enable-remote-debugging"   # then restart Steam
curl -s http://localhost:8080/json/list           # every UI window, with a webSocketDebuggerUrl
```

That is how the login was completed on a machine where the window was solid black. `Runtime.evaluate` on the login page returns `title: "Sign in to Steam"` with the full form text, which proves the page is laid out and only the pixels are missing; `Page.captureScreenshot` renders it from the renderer's own compositor and comes back **perfect**, QR code and all. Screenshot it, hand the image to the user to scan with the Steam Mobile App, and the client logs in. `Page.captureScreenshot` takes `clip` and `scale`, so `{x:449,y:114,width:220,height:220,scale:4}` yields a large, sharp QR rather than a 200 px one. Steam's QR expires every couple of minutes — clicking the refresh glyph at (559, 224) with `Input.dispatchMouseEvent` regenerates it.

Once logged in, the same channel installs games: `SteamClient.Installs.OpenInstallWizard([appid])`, `GetInstallManagerInfo()`, `ContinueInstall()`, all evaluated in the **SharedJSContext** target — see [[raceroom-needs-steam-login]].

Two traps found doing this. The main window can come up stuck on the loading spinner, and any `steam://` URL then no-ops with `BrowserBackstack: Attempted to show a URL in the main window browser without a browser manager available!` — **restart the client**; do not `Page.reload` the main window, which leaves it a blank `about:blank` and is worse. And when testing whether Chromium's network works, use `mode:"no-cors"`: a plain cross-origin `fetch` from `steamloopback.host` fails on CORS and reads exactly like a dead network stack, which sent this session chasing a networking bug that did not exist.

Two things that are **not** the cause, both checked: DXVK's native `dxgi`/`d3d11`/`d3d10core` overrides (the webhelper runs `--disable-gpu --in-process-gpu`, and the same failure predates the DXVK install), and a poisoned CEF profile under `AppData/Local/Steam/htmlcache` (moved aside, the loop came back on a freshly created one).

One thing that could **not** be tested and is therefore still open: the GPU fallback. Steam adds `--disable-gpu --in-process-gpu` to the webhelper on its own, which means any fault in ANGLE/SwiftShader lands in the browser process. `-cef-force-gpu` does not override it — `webhelper_gpu.txt` still logs `Disabling GPU acceleration: Disabled/CommandLine` and the loop was unchanged — and there is no persisted GPU key in the bottle's `user.reg` to flip either, so the launch options simply cannot reach it. The separate two-minute hang on `Downloading manifest: …/steam_client_win64` at every launch is [[wine-winhttp-wpad-stalls-steam]] and is cosmetic — it times out, logs `http error 0`, and Steam carries on.
