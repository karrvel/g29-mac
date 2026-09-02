---
name: hid-values-need-polling-not-callbacks
type: gotcha
title: IOHID input-value callbacks only fire on change, so a stationary wheel reports nothing and a naive read returns "no data"
area: macos
tags: [iokit, hid, debugging]
status: active
updated: 2026-09-02
volatility: durable
provenance: 2026-09-01 session — hit while building lgwheel's --verify mode
---

# Reading an axis with a callback silently returns nothing until someone touches the wheel

`IOHIDDeviceRegisterInputValueCallback` + a run loop looks like the obvious way to read the steering axis. It fails for the case that matters: the callback fires only when a value **changes**, so a wheel sitting still delivers no events at all and your baseline read comes back empty. The symptom is a clean "could not read the steering axis" with no error anywhere — easy to misdiagnose as a permissions problem or a dead device.

Use `IOHIDDeviceCopyMatchingElements` to find the element once (Generic Desktop usage page `0x01`, usage `0x30` = X for steering), then poll it with `IOHIDDeviceGetValue` on an open, run-loop-scheduled device. Normalise with `IOHIDElementGetLogicalMin/Max` — the G29's steering is 16-bit, and if a device exposes several X-ish elements you want the widest one.

This is what makes objective verification possible, which is the real payoff: apply a known force to a free-standing wheel, poll the axis, and watch it move on its own. That turns "does force feedback work?" from something only a human hand can answer into a measurement. `lgwheel --verify` does exactly this and reported the wheel travelling across 100% of its lock under commanded force — which is how FFB was proven on this machine without anyone touching it.

If `IOHIDDeviceOpen` itself fails, that *is* a permissions problem: grant the terminal Input Monitoring under System Settings → Privacy & Security.
