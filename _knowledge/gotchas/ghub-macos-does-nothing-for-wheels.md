---
name: ghub-macos-does-nothing-for-wheels
type: gotcha
title: Logitech G HUB on macOS is a firmware updater for the G29 — its HID driver extension only ever loads for two mice
area: macos
tags: [logitech, ghub, driverkit, g29]
status: active
updated: 2026-09-02
volatility: durable
provenance: 2026-09-01 session — Logitech's own G29 spec sheet plus reading com.logi.ghub.hidfilter's Info.plist on this machine
---

# Installing G HUB looks like the fix and buys you nothing

`lghub.app` is installed here and `systemextensionsctl list` shows `com.logi.ghub.hidfilter` (1.1.23) activated and enabled, which strongly suggests Logitech ships wheel support on the Mac. It does not.

Logitech's own G29 technical specification states OS support as "Windows 10, Windows 11 / **Mac OS X 10.10.x or later (G HUB on Mac is used only to update Firmware)**". On macOS, G HUB is a firmware-flashing utility for this wheel and nothing else. The G920, the G923 Xbox/PC, the PRO Racing Wheel and the RS line don't list Mac at all.

The active driver extension is even narrower than that implies. `com.logi.ghub.hidfilter.dext`'s `Info.plist` declares exactly **two** `IOKitPersonalities`, both mice: G600 (`046d:c24a`) and G602 (`046d:c537`). Its maskable elements target the Button usage page and its scalable axes target Generic Desktop X/Y. There is no wheel product ID anywhere in it and it exposes no `IOForceFeedback` interface — for a G29 it never loads in the first place.

Useful corollary: because the extension never binds to the wheel, it also **cannot interfere** with userspace HID access. Third-party guides tell you to uninstall G HUB before doing FFB work; on this machine the raw `IOHIDManager` / `IOHIDDeviceSetReport` path was verified working with G HUB installed and both extensions enabled. Leave it alone unless you actually observe a problem — and if you do, quit the app and disable the extension under Login Items & Extensions → Driver Extensions rather than uninstalling.

Also dead as a fallback: Logitech Gaming Software's last macOS build is 9.02.22 (2020-01-16), targeting macOS 10.12–10.15, Intel, kext-era. It cannot install on macOS 26 on Apple Silicon.
