---
name: g29-pedal-axes-rest-inverted
type: gotcha
title: Two of the G29's pedal axes rest at full scale, so a game auto-binding them reads the pedals as permanently floored
area: hardware
tags: [g29, pedals, axes, lfs, calibration]
status: active
updated: 2026-09-02
volatility: decays-with-code
provenance: 2026-09-02 session — measured with `lgwheel --axes` on the attached G29; Z-vs-Rz ordering still unconfirmed
---

# Why a game grabs the clutch as the accelerator

Symptom in Live for Speed: the **clutch** operates the throttle, and the real accelerator and brake do nothing at all.

The G29 exposes exactly **four** axes, and their resting values are the explanation:

**Confirmed mapping** (measured by pressing each pedal in turn):

| Axis | Pedal | Released | Floored |
|---|---|---|---|
| `X` | steering | — | 0.000 - 0.810 sweep |
| `Z` | **accelerator** | 1.000 | 0.000 |
| `Rz` | **brake** | 1.000 | 0.129 |
| `Y` | **clutch** | 1.000 | 0.000 |

**All three pedals are inverted**: released reads *full scale*, floored reads ~zero. Any game that binds them without inverting sees three pedals permanently held down, so it either ignores them or treats them as stuck — which is why auto-binding produced nonsense in LFS.

(An earlier snapshot caught `Y` resting at 0.063 rather than 1.000; a pedal that has not been touched since power-on can read at the wrong end until it is pressed once. Press every pedal through its full travel before trusting any calibration.)

**The fix is calibration, not configuration files.** In LFS use **Options → Controls**, click each axis field and press that pedal; LFS binds whatever moves and calibrates its range, which handles the inversion as a side effect. Do **not** hand-edit `data/misc/Logitech_G29_Driving_Force_Racing_Wheel.csf` — it is an opaque binary (`LFSCON` magic) and editing it is guesswork. Back it up before re-binding.

To see the mapping for yourself on any game: `scripts/identify-pedals.sh`, then press one pedal at a time. It prints which axis moved, its range, and whether it rests high or low.

**The other half of the LFS trap:** pedals are **axes**, not buttons, and LFS's control screen separates the two. Assigning them on the `Buttons 1` tab — where the prompt reads "Press button for : Accelerate" — can never work, because LFS is waiting for a button press and a pedal is not one. Axes live on the **`Axes / FF`** tab. Also set `Throttle / brake axes: separate` (not `combined`) for a three-pedal set, and `Clutch: axis` rather than `button`.
