#!/usr/bin/env bash
#
# The Encore importer (#34129) creates items on score->dummy()->segment().
#
# Main dropped the stand-in children of the dummy on 2026-10-05, in 0eb7651d62
# "Engraving: park objects on the dummy itself, not on stand-in children", and
# every importer now passes score->dummy() straight to the factory. The pull
# request predates that, so it merges cleanly and then fails to compile:
#
#   no member named 'segment' in 'mu::engraving::DummyParent'
#
# The real fix belongs on the pull request once it is rebased onto main. Until
# then this rewrites the calls the same way main rewrote its own importers.
#
# Idempotent: silent once the importer no longer asks for the segment, which is
# also what happens when the pull request catches up. Loud if the importer is
# gone or if the dummy grows its segment back.

set -o errexit
set -o nounset

DIR="src/importexport/encore"
DUMMY="src/engraving/dom/dummyparent.h"
NEEDLE='dummy()->segment()'

[ -d "$DIR" ]   || { echo "   $0: $DIR is gone; the fixup is stale" >&2; exit 1; }
[ -f "$DUMMY" ] || { echo "   $0: $DUMMY is gone; upstream moved, review this fixup" >&2; exit 1; }

if grep -q 'segment()' "$DUMMY"; then
    echo "   $0: DummyParent has a segment() again; review this fixup" >&2
    exit 1
fi

FILES=$(grep -rlF "$NEEDLE" "$DIR" || true)

if [ -z "$FILES" ]; then
    echo "   nothing to do, the importer already uses the dummy itself"
    exit 0
fi

for f in $FILES; do
    n=$(grep -cF "$NEEDLE" "$f")
    sed -i.bak 's/dummy()->segment()/dummy()/g' "$f" && rm -f "$f.bak"
    echo "   ${f#$DIR/}: $n call(s) now take the dummy itself"
done
