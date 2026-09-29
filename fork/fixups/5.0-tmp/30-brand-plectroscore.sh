#!/usr/bin/env bash
#
# Give the build its own identity: PlectroScore.
#
# Only the name the app shows. Everything that decides where files live is
# left exactly as upstream has it, on purpose: this is a MuseScore that calls
# itself something else, not a separate program.
#
# The two strings in version.cmake do very different jobs:
#
#   MUSE_APP_NAME_HUMAN_READABLE    the title bar, the about box, the DMG
#   MUSE_APP_NAME_MACHINE_READABLE  QCoreApplication::setApplicationName, and
#                                   through it the settings and every path
#
# So only the first is touched. The machine name and the bundle identifier
# stay as upstream, which keeps scores, plugins, styles, soundfonts, the
# extensions directory and the preferences shared with an official MuseScore
# on the same machine. Opening a score in one and then the other finds the
# same everything.
#
# The cost is known and accepted: the shared extensions directory is where a
# stale copy from an older layout once aborted the console builds. Sharing is
# what was asked for; if that ever stops being worth it, the machine name is
# the single line to change.
#
# Idempotent: silent once applied, loud if upstream restructures the file.

set -o errexit
set -o nounset

NAME_HUMAN="PlectroScore"

VERSION_CMAKE="version.cmake"
MACOS_PACKAGE="buildscripts/ci/macos/package.sh"

[ -f "$VERSION_CMAKE" ] || { echo "   $0: no $VERSION_CMAKE; the fixup is stale" >&2; exit 1; }

# ------------------------------------------------------------------ names --
if grep -q "MUSE_APP_NAME_HUMAN_READABLE \"$NAME_HUMAN\"" "$VERSION_CMAKE"; then
    echo "   names already branded"
else
    python3 - "$VERSION_CMAKE" "$NAME_HUMAN" <<'PY'
import re, sys
path, human = sys.argv[1], sys.argv[2]
s = open(path).read()

pattern = r'set\(MUSE_APP_NAME_HUMAN_READABLE\s+"[^"]*"\)'
if not re.search(pattern, s):
    print("   cannot find MUSE_APP_NAME_HUMAN_READABLE; read version.cmake "
          "and update this fixup", file=sys.stderr)
    sys.exit(1)
s = re.sub(pattern, 'set(MUSE_APP_NAME_HUMAN_READABLE "%s")' % human, s, count=1)

# Guard the promise: if the machine name ever drifts from upstream's, paths
# stop matching and nobody notices until a plugin goes missing.
m = re.search(r'set\(MUSE_APP_NAME_MACHINE_READABLE\s+"([^"]*)"\)', s)
if not m or m.group(1) != "MuseScoreStudio":
    print("   the machine readable name is %r, not MuseScoreStudio; paths would "
          "stop matching upstream" % (m.group(1) if m else None), file=sys.stderr)
    sys.exit(1)

open(path, 'w').write(s)
print("   version.cmake: display name only; paths left as upstream")
PY
fi

# -------------------------------------------------------- macOS packaging --
# The DMG names the .app from the build mode, so a devel build installs as
# "MuseScore 5.0.0 Development" however the binary calls itself. Name it after
# the fork instead, keeping the version so two lines can sit side by side.
if [ ! -f "$MACOS_PACKAGE" ]; then
    echo "   no macOS packaging script on this line, skipping"
    exit 0
fi

if grep -q "APP_NAME=\"$NAME_HUMAN" "$MACOS_PACKAGE"; then
    echo "   macOS package name already branded"
    exit 0
fi

python3 - "$MACOS_PACKAGE" "$NAME_HUMAN" <<'PY'
import re, sys
path, human = sys.argv[1], sys.argv[2]
s = open(path).read()

# Every branch that sets APP_NAME, whatever the build mode, becomes ours.
new, n = re.subn(r'APP_NAME="MuseScore ([^"]*)"',
                 lambda m: 'APP_NAME="%s %s"' % (human, m.group(1)), s)
if n == 0:
    print("   no APP_NAME assignment found; read the script and update this fixup",
          file=sys.stderr)
    sys.exit(1)
open(path, 'w').write(new)
print("   macOS packaging: %d app name assignments" % n)
PY
