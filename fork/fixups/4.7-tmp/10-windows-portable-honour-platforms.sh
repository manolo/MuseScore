#!/usr/bin/env bash
#
# Make the Windows portable job honour the platforms input on this line.
#
# On 4.7 the job reads:
#
#   github.event_name != 'pull_request' &&
#   (github.event_name != 'workflow_dispatch' || contains(inputs.platforms, 'windows_portable'))
#
# Under push the first branch of the or is already true, so the job runs no
# matter what the caller asks for. It then builds fine and dies signing: it
# uploads to s3://muse-sign, which a fork cannot reach, and unlike the macOS
# signing it never checks whether the secret is empty. That one job failing
# takes the whole fork build down with it.
#
# The 5.0 line tests the input itself. This applies the same shape here, so
# passing platforms: windows_x64 skips the portable and leaves the x64 build,
# which is the one that produces the installer.
#
# Idempotent: silent once applied, silent if upstream adopts the same fix,
# loud if the condition stops looking the way it assumed.

set -o errexit
set -o nounset

FILE=".github/workflows/build_windows.yml"
OLD="(github.event_name != 'workflow_dispatch' || contains(inputs.platforms, 'windows_portable'))"
NEW="(inputs.platforms == '' || contains(inputs.platforms, 'windows_portable'))"

[ -f "$FILE" ] || { echo "   $0: $FILE is gone; the fixup is stale" >&2; exit 1; }

if grep -qF "$NEW" "$FILE"; then
    echo "   nothing to do, the portable job already honours the input"
    exit 0
fi

if ! grep -qF "$OLD" "$FILE"; then
    echo "   the portable job condition is neither the old nor the new shape;" >&2
    echo "   read it and update this fixup before trusting the build" >&2
    grep -n -A3 'windows_portable:' "$FILE" >&2 || true
    exit 1
fi

python3 - "$FILE" "$OLD" "$NEW" <<'PY'
import sys
path, old, new = sys.argv[1], sys.argv[2], sys.argv[3]
s = open(path).read()
assert s.count(old) == 1, "expected exactly one occurrence, found %d" % s.count(old)
open(path, 'w').write(s.replace(old, new))
print("   portable job now honours the platforms input")
PY
