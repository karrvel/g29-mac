#!/bin/bash
# Run the knowledge-base maintenance loop, then scrub machine-specific paths.
#
# kb-sync.py bakes the absolute path of the tier-1 memory dir into INDEX.md.
# That is fine for a private vault and a PII leak in a public repo, so this
# wrapper regenerates and then replaces it with a placeholder. Use this instead
# of calling kb-sync.py directly.
set -euo pipefail
cd "$(dirname "$0")"
[ -d _meta ] || { echo "_meta/ missing — see CLAUDE.md for how to restore it"; exit 1; }
[ -f .githooks/kb.env ] && source .githooks/kb.env

python3 _meta/kb-sync.py

# Scrub the absolute home path kb-sync writes into INDEX.md.
if [ -f _knowledge/INDEX.md ]; then
    python3 - <<'PY'
import re, pathlib
p = pathlib.Path("_knowledge/INDEX.md"); s = p.read_text()
s2 = re.sub(r"`/(?:Users|home)/[^`]*/memory/MEMORY\.md`",
            "`~/.claude/projects/<project>/memory/MEMORY.md`", s)
if s2 != s:
    p.write_text(s2); print("  ✓ INDEX.md — scrubbed absolute memory path")
PY
fi

python3 _meta/kb-fix.py
python3 _meta/kb-lint.py
python3 _meta/kb-links.py
python3 _meta/kb-staleness.py
