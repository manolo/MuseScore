#!/usr/bin/env bash
#
# Replace upstream's README with the fork's.
#
# Somebody who lands on one of these branches, or downloads a release, needs
# to learn three things before anything else: that this is a fork, what it is
# for, and what it actually adds. Upstream's README answers none of them, and
# a fork that leaves it in place is a fork that looks like a stale mirror.
#
# It is assembled rather than stored whole: the name and the purpose come from
# the brand, what the line adds comes from fork/readme/<line>.md, and the
# component table comes from fork/CONTENTS.md, which the integration phase
# already wrote from the manifest. So the list of pull requests in the README
# cannot drift from the list that was actually merged.
#
# Upstream's build instructions are not copied. They change, and a copy would
# go stale; the link goes to the real thing.
#
# Idempotent: rewritten from its sources every time, so running it twice gives
# the same file.

set -o errexit
set -o nounset

[ -n "${BRAND_DIR:-}" ] || { echo "   $0: BRAND_DIR is not set" >&2; exit 1; }
[ -n "${LINE:-}" ]      || { echo "   $0: LINE is not set" >&2; exit 1; }
[ -n "${TOOLS_DIR:-}" ] || { echo "   $0: TOOLS_DIR is not set" >&2; exit 1; }
# shellcheck disable=SC1091
. "$BRAND_DIR/identity.sh"

FRAGMENT="$TOOLS_DIR/readme/$LINE.md"
[ -f "$FRAGMENT" ] || { echo "   $0: no $FRAGMENT; write what this line adds" >&2; exit 1; }
[ -f "README.md" ] || { echo "   $0: no README.md at the root; the fixup is stale" >&2; exit 1; }

UPSTREAM_README="https://github.com/musescore/MuseScore/blob/master/README.md"

{
    echo "# $APP_NAME"
    echo
    echo "$APP_TAGLINE. $APP_BLURB"
    echo
    echo "It is a fork of [MuseScore Studio](https://github.com/musescore/MuseScore), and it stays one: the same file formats, the same plugins, the same styles and the same settings directory, so a score moves between the two without noticing. What changes is that the things a plectrum ensemble needs are already in it, instead of waiting in a pull request."
    echo
    echo "## Why it exists"
    echo
    echo "Bandurria, laúd and guitar ensembles, the tuna and rondalla tradition and its Latin American relatives, are a small enough audience that notation software never quite gets to them. Their repertoire sits in file formats nobody reads any more, their instruments play techniques that general purpose playback approximates badly, and the fixes for both are the kind of change that is correct, narrow and of no interest to a maintainer with a thousand other issues open."
    echo
    echo "So the fixes get written, offered upstream, and wait. This is where they run in the meantime."
    echo
    echo "Everything here is offered upstream first and carried here second. When one is merged it leaves this fork, because at that point MuseScore does it and the fork should not."
    echo
    echo "## What this line adds"
    echo
    cat "$FRAGMENT"
    echo
    if [ -f "fork/CONTENTS.md" ]; then
        echo "## Where each piece comes from"
        echo
        tail -n +2 "fork/CONTENTS.md" | sed '/^Built from the manifest/,$d'
        echo "Generated from the manifest by \`fork/rebuild.sh\`."
        echo
    fi
    echo "## Installing"
    echo
    echo "Builds for macOS, Windows and Linux are published as [releases]($APP_LINK/releases). They are unsigned, because a fork holds no Apple or Microsoft certificate: macOS will refuse the first launch, so open it once from the context menu and choose Open, and Windows will show a SmartScreen warning."
    echo
    echo "$APP_NAME shares its scores, plugins, styles and preferences with MuseScore on the same machine. It is a MuseScore that calls itself something else, not a separate program, and installing it changes nothing about an existing MuseScore."
    echo
    echo "## Building, and everything else"
    echo
    echo "Unchanged from upstream: [MuseScore's README]($UPSTREAM_README) is the reference, and is not copied here because a copy goes stale."
    echo
    echo "How the fork is maintained, what each line carries and how a release is cut: the \`fork/tools\` branch, starting at \`fork/README.md\`."
    echo
    echo "## Licence"
    echo
    echo "GPL-3.0-only, the same as MuseScore Studio. See [LICENSE.txt](LICENSE.txt)."
    echo
    echo "MuseScore is a trademark of MuseScore Limited and is used here only to say what this is based on. The name is not licensed with the code, and this build claims no endorsement."
} > README.md.new

if cmp -s README.md.new README.md; then
    rm -f README.md.new
    echo "   nothing to do, the README is current"
else
    mv README.md.new README.md
    echo "   README: $(wc -l < README.md | tr -d ' ') lines, from the brand, fork/readme/$LINE.md and fork/CONTENTS.md"
fi
