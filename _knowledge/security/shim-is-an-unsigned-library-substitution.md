---
name: shim-is-an-unsigned-library-substitution
type: security
title: The FFB shim is an ad-hoc-signed library swapped into a shared Wine runtime — know the trust boundary before rebuilding or moving it
area: wine
tags: [trust, codesign, dylib, supply-chain]
status: active
updated: 2026-09-02
volatility: durable
provenance: 2026-09-01 session — built and installed here; recorded so the trust boundary is explicit rather than implied
---

# What installing the shim actually grants, and how to check it

`install-shim.sh install` replaces `libSDL2-2.0.0.dylib` in Whisky's **shared** Wine runtime with a locally built, **ad-hoc signed** (`codesign -s -`) library. Every Wine process on the machine then loads it, including bottles unrelated to sim racing ([[sdl2-shim-is-global-to-whisky]]). It opens the wheel's HID device and writes output reports to it.

That is a legitimate thing to run — it is built from source in this repo and its behaviour is bounded — but it is a library-substitution in a shared runtime, which is structurally the same shape as a supply-chain attack. So keep the boundary explicit:

- **Only ever install a shim you built from `sdl2-lg4ff-shim.c` in this repo** via `./build.sh`. Never drop in a prebuilt `libSDL2-2.0.0.dylib` from anywhere else — the filename is the whole attack surface, and a hostile one would be loaded by every bottle with no prompt.
- **Verify state before trusting it:** `./install-shim.sh status` should report the shim live and the original preserved as `libSDL2-2.0.0.real.dylib`. If the original is missing, the revert path is gone — restore from `libSDL2-2.0.0.dylib.orig-backup` or reinstall Whisky.
- **Ad-hoc signing means no notarisation and no team identity.** Gatekeeper does not vet it. That is acceptable for a locally built artefact and unacceptable for one received from someone else.
- **It grants HID write access to the wheel, nothing more.** No network, no filesystem writes, no privilege escalation; it runs as the user. The IOKit calls need no admin rights, and it deliberately requires no kext or DriverKit extension ([[raw-hid-not-apple-forcefeedback]]).

**Revert cleanly at any time:** `./install-shim.sh revert` restores the original byte-for-byte and re-signs it. Do that before uninstalling or upgrading Whisky, so its updater never sees a substituted library.
