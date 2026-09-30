#!/usr/bin/env bash
#
# Say the fork's name on the loading screen and in the about box, and say what
# it is based on.
#
# The attribution is not decoration. The code is MuseScore's under the GPL,
# and the honest thing for a renamed build is to name its origin where anyone
# can see it, not only in a licence file nobody opens. Both surfaces get the
# same line: based on MuseScore Studio.
#
# The loading screen background is replaced with the fork's own drawing rather
# than edited: upstream's carries the musescore studio wordmark as vector
# paths, and shipping that under another name is the one thing a fork really
# should not do. The name and the attribution are painted by the code, beside
# the version and the link it already draws, which keeps type out of an SVG
# that Qt renders with a limited subset.
#
# Idempotent: silent once applied, loud if either file moves.

set -o errexit
set -o nounset

[ -n "${BRAND_DIR:-}" ] || { echo "   $0: BRAND_DIR is not set" >&2; exit 1; }
# shellcheck disable=SC1091
. "$BRAND_DIR/identity.sh"

ART="$(cd "$BRAND_DIR/.." && pwd)/art"
art() { if [ -f "$BRAND_DIR/$1" ]; then echo "$BRAND_DIR/$1"; else echo "$ART/$1"; fi; }

SPLASH_SVG="src/appshell/resources/LoadingScreen.svg"
SPLASH_CPP="src/appshell/widgets/splashscreen/loadingscreenview.cpp"
ABOUT_QML="src/appshell/qml/MuseScore/AppShell/AboutDialog.qml"

NAME="$APP_NAME"
BASED_ON="$APP_ATTRIBUTION"
LINK="$APP_LINK"

for f in "$SPLASH_SVG" "$SPLASH_CPP" "$ABOUT_QML"; do
    [ -f "$f" ] || { echo "   $0: no $f; the fixup is stale" >&2; exit 1; }
done
[ -f "$(art LoadingScreen.svg)" ] || {
    echo "   $0: no splash art; run fork/brand/make-splash.sh" >&2; exit 1; }

changed=0

# ---------------------------------------------------------- splash artwork --
if ! cmp -s "$(art LoadingScreen.svg)" "$SPLASH_SVG"; then
    cp "$(art LoadingScreen.svg)" "$SPLASH_SVG"
    echo "   splash: artwork"
    changed=1
fi

# ------------------------------------------------------------- splash text --
if ! grep -q "$NAME" "$SPLASH_CPP"; then
    python3 - "$SPLASH_CPP" "$NAME" "$BASED_ON" "$LINK" <<'PY'
import re, sys
path, name, based_on, link = sys.argv[1:5]
s = open(path).read()

# The link upstream shows is its own; ours points at where these builds live.
s2, n = re.subn(r'static const QString website\("[^"]*"\);',
                'static const QString website("%s");' % link, s)
if n != 1:
    print("   cannot find the website string in the loading screen", file=sys.stderr)
    sys.exit(1)
s = s2

# Paint the name and the attribution over the empty middle of the artwork,
# which was left empty for them.
anchor = "    // Draw message\n"
if anchor not in s:
    print("   cannot find the message block in the loading screen", file=sys.stderr)
    sys.exit(1)

block = '''    // Draw the fork's name, and what it is based on. The artwork leaves this
    // area empty on purpose; the attribution belongs where people can see it.
    {
        QFont nameFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
        nameFont.setPixelSize(44);
        nameFont.setWeight(QFont::DemiBold);
        painter->setFont(nameFont);
        painter->setPen(QPen(QColor("#F3E7E1")));
        painter->drawText(QRectF(330, 150, 430, 56),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip,
                          QStringLiteral("%s"));

        QFont basedFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
        basedFont.setPixelSize(15);
        painter->setFont(basedFont);
        painter->setPen(QPen(QColor("#C79A87")));
        painter->drawText(QRectF(332, 206, 430, 22),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip,
                          QStringLiteral("%s"));
    }

''' % (name, based_on)

s = s.replace(anchor, block + anchor, 1)

# 4.7 draws its message with a plain QFont and never includes QFontDatabase;
# 5.0 already uses it. Add it where it is missing rather than assuming either.
if "#include <QFontDatabase>" not in s:
    s = s.replace("#include <QPainter>", "#include <QPainter>\n#include <QFontDatabase>", 1)

open(path, 'w').write(s)
print("   splash: name, attribution and link")
PY
    changed=1
fi

# ------------------------------------------------------------------- about --
if ! grep -q "$BASED_ON" "$ABOUT_QML"; then
    python3 - "$ABOUT_QML" "$NAME" "$BASED_ON" <<'PY'
import re, sys
path, name, based_on = sys.argv[1:4]
s = open(path).read()

# The dialog title
s2, n = re.subn(r'title: qsTrc\("appshell/about", "About [^"]*"\)',
                'title: qsTrc("appshell/about", "About %s")' % name, s)
if n != 1:
    print("   cannot find the about dialog title", file=sys.stderr)
    sys.exit(1)
s = s2

# A line under the version saying what this is. Not translated through qsTrc:
# the name is a name, and the attribution has to read the same in every
# language for it to be worth anything.
anchor = re.search(
    r'( *)StyledTextLabel \{\n\s*anchors\.horizontalCenter: parent\.horizontalCenter\n'
    r'\s*text: qsTrc\("appshell/about", "Version:"\)[^\n]*\n[^\n]*\n\s*\}\n', s)
if not anchor:
    print("   cannot find the version label in the about dialog", file=sys.stderr)
    sys.exit(1)

indent = anchor.group(1)
block = (
    f'\n{indent}StyledTextLabel {{\n'
    f'{indent}    anchors.horizontalCenter: parent.horizontalCenter\n'
    f'{indent}    text: "{based_on}"\n'
    f'{indent}    opacity: 0.7\n'
    f'{indent}}}\n'
)
s = s[:anchor.end()] + block + s[anchor.end():]
open(path, 'w').write(s)
print("   about: title and attribution")
PY
    changed=1
fi

# ------------------------------------------------------------- about menu --
# The dialog title lives in QML, but what people actually click is an action
# title in C++. Changing only the QML leaves the menu still saying About
# MuseScore Studio, which is exactly what it looks like when nothing has
# changed at all.
for f in src/appshell/internal/applicationuiactions.cpp \
         src/appshell/internal/appshellcommandsregister.cpp; do
    [ -f "$f" ] || continue
    grep -q "About $NAME" "$f" && continue
    if ! grep -q 'About MuseScore Studio' "$f"; then
        echo "   $f no longer names the about action; read it and update this fixup" >&2
        exit 1
    fi
    n=$(grep -c 'About MuseScore Studio' "$f")
    sed -i '' "s/About MuseScore Studio/About $NAME/g" "$f"
    echo "   about menu: $(basename "$f"), $n string(s)"
    changed=1
done

# The about dialog's logo stays MuseScore's on purpose: it sits beside a line
# that now says this is based on MuseScore Studio, which is exactly what the
# logo is there to convey.

[ "$changed" = 1 ] || echo "   nothing to do, already branded"
