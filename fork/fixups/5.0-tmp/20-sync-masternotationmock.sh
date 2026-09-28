#!/usr/bin/env bash
#
# Keep MasterNotationMock in step with the excerpt interface that #31738 changes.
#
# The pull request is based on a main from before masternotationmock.h existed,
# so it never touched the mock. Merged onto today's main the two disagree: the
# interface gains deinitExcerpts, which the mock does not implement, and loses
# potentialExcerpts, which the mock still declares as an override. The mock
# ends up abstract and the release build dies on it.
#
# Derived from the interface rather than hardcoded, so it keeps working when
# #31738 is finally rebased and the mismatch changes shape or disappears.

set -o errexit
set -o nounset

IFACE="src/notation/imasternotation.h"
MOCK="src/notation/tests/mocks/masternotationmock.h"

[ -f "$IFACE" ] || { echo "   $0: $IFACE is gone; the fixup is stale" >&2; exit 1; }
if [ ! -f "$MOCK" ]; then
    echo "   no mock on this line, nothing to sync"
    exit 0
fi

python3 - "$IFACE" "$MOCK" <<'PY'
import re, sys

iface_path, mock_path = sys.argv[1], sys.argv[2]
iface = open(iface_path).read()
mock = open(mock_path).read()

# Pure virtuals the interface declares, by name.
declared = set(re.findall(r'virtual\s+[^;=]*?\b(\w+)\s*\([^;]*?\)\s*(?:const\s*)?=\s*0\s*;', iface))
# Methods the mock claims to override, by name.
mocked = dict()
for m in re.finditer(r'^\s*MOCK_METHOD\((?:[^()]|\([^()]*\))*\)\s*;\s*$', mock, re.M):
    name = re.match(r'\s*MOCK_METHOD\(\s*(?:[^,()]|\([^()]*\))+,\s*(\w+)\s*,', m.group(0))
    if name:
        mocked[name.group(1)] = m.group(0)

stale = [n for n in mocked if n not in declared]
missing = sorted(declared - set(mocked))

changed = False

for name in stale:
    mock = mock.replace(mocked[name] + '\n', '')
    print("   removed %s from the mock; the interface no longer declares it" % name)
    changed = True

for name in missing:
    sig = re.search(
        r'virtual\s+([^;=]*?)\b%s\s*\(([^;]*?)\)\s*(const\s*)?=\s*0\s*;' % re.escape(name), iface)
    if not sig:
        continue
    ret, args, is_const = sig.group(1).strip(), sig.group(2).strip(), bool(sig.group(3))
    quals = '(const, override)' if is_const else '(override)'
    # MOCK_METHOD splits its arguments on commas, so a multi argument list has
    # to be parenthesised or the macro miscounts.
    if ',' in args:
        args = '(%s)' % args
    line = '    MOCK_METHOD(%s, %s, (%s), %s);\n' % (ret, name, args, quals)
    anchor = '    MOCK_METHOD(void, setExcerpts,'
    idx = mock.find(anchor)
    if idx == -1:
        print("   cannot place %s, no anchor in the mock; add it by hand" % name, file=sys.stderr)
        sys.exit(1)
    mock = mock[:idx] + line + mock[idx:]
    print("   added %s to the mock" % name)
    changed = True

if changed:
    open(mock_path, 'w').write(mock)
else:
    print("   nothing to do, the mock already matches the interface")
PY
