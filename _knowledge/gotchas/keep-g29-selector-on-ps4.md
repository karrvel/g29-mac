---
name: keep-g29-selector-on-ps4
type: gotcha
title: Leave the G29's console selector on PS4 — it reports 046d:c24f and force feedback works there; advice to switch to PS3 is wrong for this setup
area: hardware
tags: [g29, usb, modes]
status: active
updated: 2026-09-02
volatility: durable
provenance: 2026-09-01 session — measured on the physical wheel; contradicts a research claim, empirics win
---

# Don't touch the mode switch

The G29 has a physical PS3/PS4 selector. On this machine it sits on **PS4**, enumerates as `046d:c24f`, and force feedback is confirmed working in that position — `lgwheel --verify` drove the wheel across 100% of its travel without the switch being moved.

Worth recording because the guidance out there points the other way. One research pass concluded that `0xc24f` is "the PS3-mode variant" and that "in PS4 mode the descriptor differs and FFB is not driven", citing a Linux report. That does not describe this wheel on this Mac. The measurement supersedes the claim: the selector is on PS4, the device is `c24f`, and the motor responds. If you go switch it because a forum told you to, you will be changing a working configuration.

For context on what the switch actually does — the wheel can also enumerate as `046d:c294` ("Driving Force / Formula EX"), a backward-compatibility mode where it pretends to be an older model and gives up full FFB and the 900° range. `lgwheel --detect` flags that case explicitly and `lgwheel --native` sends the mode-switch command (`f8 0a …` then `f8 09 05 01 01 …`) to bring it back. You should not need either here.

Do check the boring things first when the wheel seems dead: the external power brick must be connected or the wheel enumerates over USB but never moves, and a healthy wheel does a full left-right calibration sweep at power-on.
