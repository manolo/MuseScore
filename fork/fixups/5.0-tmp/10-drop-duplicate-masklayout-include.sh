#!/usr/bin/env bash
#
# Merging #31200 leaves "#include \"masklayout.h\"" twice.
#
# This is not a conflict and rerere cannot help: main and the pull request each
# add the same include at a different line, git's three way merge sees two
# independent insertions, and keeps both. It compiles, because of the include
# guard, but it is wrong and it comes back on every rebuild.
#
# Idempotent: silent when there is one include, silent when upstream drops the
# duplication itself, loud only if the file stops looking the way we assume.

set -o errexit
set -o nounset

FILE="src/engraving/rendering/score/scorehorizontalviewlayout.cpp"
NEEDLE='#include "masklayout.h"'

[ -f "$FILE" ] || { echo "   $0: $FILE is gone; the fixup is stale" >&2; exit 1; }

COUNT=$(grep -c -F "$NEEDLE" "$FILE" || true)

case "$COUNT" in
    0) echo "   no masklayout include at all; upstream moved, review this fixup" >&2; exit 1 ;;
    1) echo "   nothing to do, the include appears once" ;;
    2) python3 - "$FILE" "$NEEDLE" <<'PY'
import sys
path, needle = sys.argv[1], sys.argv[2]
lines = open(path).read().split('\n')
hits = [i for i, l in enumerate(lines) if l.strip() == needle]
assert len(hits) == 2, hits
# Drop the later one: the earlier sits with the includes main already had.
del lines[hits[1]]
open(path, 'w').write('\n'.join(lines))
print("   dropped the duplicate include at line %d" % (hits[1] + 1))
PY
       ;;
    *) echo "   $COUNT copies of the include, expected at most 2; review this fixup" >&2; exit 1 ;;
esac
