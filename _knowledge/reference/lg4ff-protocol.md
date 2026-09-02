---
name: lg4ff-protocol
type: reference
title: Logitech wheel FFB wire protocol — the command set, transcribed from new-lg4ff
area: hardware
tags: [lg4ff, hid, protocol, g29]
status: active
updated: 2026-09-02
volatility: durable
provenance: transcribed from berarma/new-lg4ff hid-lg4ff.c + linux hid-ids.h, fetched 2026-09-01; every command exercised against the physical G29
---

# The wire format

Commands are **7-byte blocks**, padded to the interface's output report length before sending (16 bytes on the G29's joystick interface — see [[lg4ff-reports-must-be-16-bytes]]). Report ID 0. Send with `IOHIDDeviceSetReport(dev, kIOHIDReportTypeOutput, 0, buf, len)`.

## Commands in use

| Purpose | Bytes | Notes |
|---|---|---|
| Constant force, slot 0 | `11 00 F 00 00 00 00` | `F = ((clamp_s16(level) + 0x8000) >> 8)`; **`0x80` is neutral**, `0x00`/`0xff` full lock either way |
| Stop slot 0 | `13 00 00 00 00 00 00` | |
| Set rotation range | `f8 81 <lo> <hi> 00 00 00` | degrees, 40–900, little-endian |
| Autocentre off | `f5 00 00 00 00 00 00` | |
| Autocentre set | `fe 0d <k1> <k2> <clip> 00 00` | then `14 00 00 00 00 00 00` to activate |
| Switch to G29 native | `f8 0a 00 00 00 00 00` then `f8 09 05 01 01 00 00` | first reverts on USB reset; wheel re-enumerates |
| Spring, slot N | `<(0x10<<N)+op> 0b <d1>>3> <d2>>3> …` | full packing in `lg4ff_update_slot` |

Slot command byte is `(0x10 << slot_id) + op`, where op is `1` = download-and-play, `3` = stop, `0x0c` = refresh.

## Autocentre magnitude expansion

```
magnitude = 65535 * pct / 100
if magnitude <= 0xaaaa:  a = 0x0c*magnitude;                       b = 0x80*magnitude
else:                    a = 0x0c*0xaaaa + 0x06*(magnitude-0xaaaa); b = 0x80*0xaaaa + 0xff*(magnitude-0xaaaa)
a >>= 1                        # all non-MOMO wheels
k1 = k2 = a / 0xaaaa;  clip = b / 0xaaaa
```

## Product IDs (Logitech VID `0x046d`)

`c24f` G29 · `c260` G29 PS4-native · `c262` G920 · `c266`/`c267`/`c26e` G923 PC/PS/Xbox · `c294` Driving Force — **compatibility mode**, the wheel pretending to be an older model with reduced FFB and range · `c295` MOMO Force · `c298` Driving Force Pro · `c299` G25 · `c29a` Driving Force GT · `c29b` G27 · `ca03` MOMO Racing.

Because all of these share the protocol, `lgwheel` supports the whole family from one product-ID table.

## Source and further reading

`berarma/new-lg4ff` (`hid-lg4ff.c`) is the reference implementation; `lg4ff_update_slot`, `lg4ff_set_autocenter_default` and `lg4ff_set_range_g25` are the functions that matter. SDL3's `SDL_hidapi_lg4ff.c` is the same protocol ported to userspace and is worth reading for the effect-timer design that [[shim-scoping-and-periodic-effects]] still needs.
