---
name: free-sim-shortlist
type: decision
title: Live for Speed, Speed Dreams and RaceRoom — what the free-and-runs-on-this-Mac filter actually leaves, and what it rejects
area: games
tags: [games, steam, raceroom, lfs, speed-dreams]
status: active
updated: 2026-09-02
volatility: decays-with-code
provenance: 2026-09-01 session — prices and platforms pulled live from the Steam appdetails API, not from memory
---

# The candidate set is far smaller than it looks

**Context.** Requirement: the three best racing sims that are free to play, run on an M4 Pro Mac, and fully support a G29.

**The filter that does the work.** There is **no proper racing sim that runs natively on macOS**. A sweep of Steam's macOS + free + racing catalogue returns arcade and mobile ports only — the closest thing to a sim in the whole list is a telemetry tool. So every real candidate is a Windows title under translation.

**Decision.** Live for Speed (installed), Speed Dreams 2.4.2 (installed), RaceRoom Racing Experience (Steam client staged; blocked on a login — see [[raceroom-needs-steam-login]]).

**Rejections, with the reason each was cut** — all price/platform data pulled live, because recall was wrong here:

- **rFactor 2** — **$29.99, not free.** Widely believed to have gone free-to-play; the API says otherwise. This single check changed the shortlist.
- **Assetto Corsa** $19.99, **ACC** $39.99, **AC EVO** $39.99, **Automobilista 2** $39.99, **Le Mans Ultimate** $39.99, **BeamNG.drive** $24.99 — all paid.
- **Trackmania** — genuinely free, but arcade, and gated behind a Ubisoft launcher.
- **RACE 07 / RACE On demos** — free and real sims, but Windows-only demos of a dead product line; kept as fallbacks, not shipped.
- **Speed Dreams' own macOS build** — exists only for the old 2.3.0 release and explicitly not for Apple Silicon, so it runs as a Windows build under Wine like the others.

**Consequences and honesty.** Speed Dreams earned its slot by being free, installable with no account, and actually launching — not by being one of the three best sims in the world. Own that. RaceRoom is the strongest genuinely free-to-play sim available, but its model is a free base plus a largely paid car and track catalogue: a real sim, not a trial, but not the whole game either. Live for Speed is the pick with the strongest driving model, and its demo tier (Blackwood, three cars) is unlimited in time.
