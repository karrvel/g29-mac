---
name: example-slug-kebab-case
type: gotcha
title: One-line, specific, human-readable title (this is what shows in the MOC + memory)
area: backend
tags: [tag1, tag2]
status: active
updated: YYYY-MM-DD
volatility: durable
provenance: 2026-03-19 session ab12cd34 — or a code path / doc name (for decays-with-code prefer git-anchored, e.g. verified vs main@a1b2c3d on 2026-03-19)
---

# A short heading restating the trap/decision/finding/task

2–5 tight sentences. For a **gotcha**: what breaks, the symptom, and the fix/why. For a
**decision**: context → decision → rationale (why X not Y) → consequences. For **security**: the
rule or the open finding + how to verify. For a **task**: the concrete remaining work + acceptance.

Be actionable. Cite specific files/functions when you can. Link related shards with [[other-slug]].
Prefer 0 shards over noise — generic best-practice everyone knows does not belong here.

<!-- Copy this file, rename to <slug>.md under the right collection dir, fill it in, delete these comments, run kb-sync.py -->
