---
name: wine-winhttp-wpad-stalls-steam
type: gotcha
title: Steam in the bottle fails with "needs to be online to update" because Wine's WinHTTP runs WPAD proxy detection across all 26 network adapters
area: wine
tags: [steam, winhttp, wpad, proxy, vpn, networking]
status: active
updated: 2026-09-02
volatility: decays-with-code
provenance: 2026-09-02 session — diagnosed from Wine +winhttp/+winsock traces and Steam's own bootstrap_log.txt, then fixed
---

# The error blames your network, and your network is fine

Symptom: the Windows Steam client in the bottle throws a fatal **"Steam needs to be online to update. Please confirm your network connection and try again."** Meanwhile the host has perfectly good internet.

Do not chase DNS, TLS or the firewall. All three were checked and all three are fine: DNS resolves (`getaddrinfo client-update.steamstatic.com` succeeds), GnuTLS **is** bundled with Whisky's Wine (`lib/libgnutls.30.dylib`), and the host fetches the exact manifest Steam wants — `https://client-update.steamstatic.com/steam_client_win64` → HTTP 200, 7379 bytes.

The real cause is in the Wine trace: `winhttp:detect_autoproxyconfig_url_dhcp` repeating for adapter after adapter, interleaved with `getaddrinfo node "wpad.local"`. Wine's WinHTTP performs WPAD proxy auto-detection over **every** network interface. The machine this was found on has **26** (`ifconfig -l`): eight `utun` tunnels from VPN clients, plus Docker, UTM and VirtualBox bridges. The probe takes so long that Steam's updater times the manifest request out and reports `Download failed: http error 0` → `DownloadManifest - exhausted list of download hosts`.

**The fix** — disable proxy auto-detection in the bottle:

```bash
wine64 reg add "HKLM\Software\Microsoft\Windows\CurrentVersion\Internet Settings\Connections" \
  /v WinHttpSettings /t REG_BINARY /d 28000000000000000100000000000000000000 /f
wine64 reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings" /v ProxyEnable /t REG_DWORD /d 0 /f
wine64 reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings" /v AutoDetect  /t REG_DWORD /d 0 /f
```

That binary blob is what `netsh winhttp reset proxy` writes: direct connection, no proxy, no autodetect.

**Two things to expect afterwards, or you will think it failed again.** It is faster but still not instant — the first manifest attempt in a run may *still* log `http error 0` and only the retry succeeds (observed: failed 00:30:55, succeeded 00:32:14). Give it a few minutes rather than quitting at the first error. And the whole thing is much quicker with the VPNs quit, since that removes most of the `utun` adapters.

Verified fixed: Steam self-updated to a full 1.6 GB client and now reaches the login screen with `steamwebhelper` running. See [[raceroom-needs-steam-login]].
