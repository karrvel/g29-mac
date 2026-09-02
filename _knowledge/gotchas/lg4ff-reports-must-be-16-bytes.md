---
name: lg4ff-reports-must-be-16-bytes
type: gotcha
title: Logitech FFB commands are 7 bytes but must be sent padded to the interface's report length, or the USB endpoint STALLs
area: hardware
tags: [g29, hid, iokit, lg4ff, usb]
status: active
updated: 2026-09-02
volatility: durable
provenance: 2026-09-01 session — hit and fixed live against the G29 (046d:c24f) on macOS 26.6.2
---

# A 7-byte lg4ff command sent as 7 bytes gets rejected by the wheel

Every Logitech wheel FFB command is a 7-byte block (`11 00 <force> …` for constant force, `f8 81 <lo> <hi> …` for rotation range, and so on). Sending exactly those 7 bytes via `IOHIDDeviceSetReport` fails on every single command with `IOReturn 0xe0005000`, which decodes to `kUSBHostReturnPipeStalled` — the device's USB endpoint answered with a STALL handshake. Nothing moves, and the error is opaque enough to look like a permissions or driver problem.

The fix is to pad the buffer to the HID interface's declared output report length and send that. On the G29's joystick interface `MaxOutputReportSize` is **16**, so you write 7 command bytes followed by 9 zero bytes. Linux never surfaces this because `hid_hw_request` pads to the report-descriptor length automatically; `IOHIDDeviceSetReport` passes your buffer through verbatim. Read `kIOHIDMaxOutputReportSizeKey` off the device rather than hardcoding 16, since it differs per interface.

Second half of the same trap: the G29 publishes **two** HID interfaces under the same `046d:c24f`. The FFB path is the **joystick** interface — usage page `0x01` (Generic Desktop), usage `0x04` (Joystick), 16-byte output reports. The other is vendor-defined (usage page `0xff00`, 20-byte reports) and is not where force feedback lives. `IOHIDManagerCopyDevices` returns an unordered `CFSet`, so code that grabs "the first Logitech device it finds" picks the wrong interface roughly half the time and fails intermittently — which reads as flaky hardware. Match on usage page and usage explicitly.

Both halves are implemented in `lgwheel.c` (`send_cmd`, and the interface selection in `main`). See [[lg4ff-protocol]] for the command set and [[raw-hid-not-apple-forcefeedback]] for why this is the only route.
