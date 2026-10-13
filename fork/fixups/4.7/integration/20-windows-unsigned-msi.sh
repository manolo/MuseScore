#!/usr/bin/env bash
#
# Let a stable windows_x64 build package an unsigned .msi on this line.
#
# A stable build packages Windows as an .msi, and package.bat signs an .msi
# unless the key and the secret arrive as "". On 4.7 both Package steps, x64
# and portable, pass a missing secret as "''" instead:
#
#   IF ${{ secrets.WIN_SIGN_S3_SECRET != 0 }} == true ( SET S_S=... ) ELSE ( SET S_S="''" )
#
# so package.bat sees a key, uploads the binaries to s3://muse-sign, which a
# fork cannot reach, and the job fails with InvalidAccessKeyId. The 5.0 line
# passes the secrets straight through and an empty one arrives as "", which
# turns signing off with a warning. This makes the fallback "" here too.
#
# Idempotent: silent once applied, silent if upstream adopts the same fix,
# loud if the step stops looking the way it assumed.

set -o errexit
set -o nounset

FILE=".github/workflows/build_windows.yml"
OLD="ELSE ( SET S_S=\"''\" )"
NEW="ELSE ( SET S_S=\"\" )"
OLD_K="ELSE ( SET S_K=\"''\" )"
NEW_K="ELSE ( SET S_K=\"\" )"

[ -f "$FILE" ] || { echo "   $0: $FILE is gone; the fixup is stale" >&2; exit 1; }

if ! grep -qF "$OLD" "$FILE" && ! grep -qF "$OLD_K" "$FILE" && grep -qF "$NEW" "$FILE"; then
    echo "   nothing to do, a missing signing secret already arrives empty"
    exit 0
fi

if ! grep -qF "$OLD" "$FILE" || ! grep -qF "$OLD_K" "$FILE"; then
    echo "   the Package step is neither the old nor the new shape;" >&2
    echo "   read it and update this fixup before trusting the build" >&2
    grep -n -B1 -A4 'name: Package' "$FILE" >&2 || true
    exit 1
fi

python3 - "$FILE" "$OLD" "$NEW" "$OLD_K" "$NEW_K" <<'PY'
import sys
path, old, new, old_k, new_k = sys.argv[1:6]
s = open(path).read()
for a, b in ((old, new), (old_k, new_k)):
    # One per Package step, plus a commented out sign test that upstream keeps
    n = s.count(a)
    assert n >= 2, "expected one per Package step, found %d of %s" % (n, a)
    s = s.replace(a, b)
open(path, 'w').write(s)
print("   a missing signing secret now arrives empty, so the .msi is left unsigned")
PY
