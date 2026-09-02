# Documentation

Task-oriented guides. For *why* something is the way it is — the measurements, the dead ends, the traps — see the knowledge base at [`../_knowledge/INDEX.md`](../_knowledge/INDEX.md); these pages link into it rather than restating it.

| Guide | Read it when |
|---|---|
| [Installation and setup](installation.md) | Starting from a fresh clone, or setting up a new machine. |
| [Troubleshooting](troubleshooting.md) | Something does not work. Indexed by symptom. |
| [How it works](how-it-works.md) | You want to understand or extend the design. |
| [Running sims](games.md) | Getting an actual game working with the wheel. |

## The short version

macOS cannot drive a Logitech wheel's force-feedback motor through any standard API — `ForceFeedback.framework` only handles PID-class hardware, `GCRacingWheel` is input-only, and SDL2's macOS haptics inherit the first failure. The motor is reachable anyway, from ordinary userspace, using Logitech's vendor protocol.

```bash
make                    # build into bin/
./bin/lgwheel --detect  # is the wheel seen?
./bin/lgwheel --verify  # objective proof of torque — let go of the wheel first
make install-shim       # force feedback inside Wine games
```

`--verify` is worth understanding: it applies a known force and watches the steering axis move on its own, so "does force feedback work?" is answered by measurement rather than by how the wheel feels in your hands. That distinction runs through the whole project.

## A note on scope

The force-feedback protocol here is not novel — it comes from Linux's `hid-lg4ff`, and SDL 3.4.0+ ships an equivalent userspace driver enabled by default on macOS. The [prior-art section of the README](../README.md#prior-art-and-credits) credits this properly and is worth reading before you assume anything here was invented. If you can depend on SDL3 in your own code, use it instead.
