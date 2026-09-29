#!/usr/bin/env bash
#
# Two things the first branding pass got wrong.
#
# The about box still showed MuseScore's logo. Leaving it was a deliberate
# choice, on the reasoning that the logo says what the attribution line says,
# but a dialog titled About PlectroScore showing somebody else's mark just
# looks like the icon was forgotten. It now shows the pick.
#
# The loading screen said "based on MuseScore Studio" in plain type, which was
# easy to miss. Attribution that nobody reads is not attribution, so it now
# shows MuseScore's own wordmark, lifted from their loading screen and set
# smaller than the fork's name. Using a project's mark to say what a build is
# based on is what a mark is for; what a fork must not do is ship it as its own,
# which is why the fork's drawing replaced theirs everywhere else.
#
# Idempotent: silent once applied, loud if any of the four files move.

set -o errexit
set -o nounset

BRAND="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)/brand"
SPLASH_CPP="src/appshell/widgets/splashscreen/loadingscreenview.cpp"
APPSHELL_CMAKE="src/appshell/CMakeLists.txt"
QML_CMAKE="src/appshell/qml/MuseScore/AppShell/CMakeLists.txt"
ABOUT_QML="src/appshell/qml/MuseScore/AppShell/AboutDialog.qml"

WORDMARK="resources/musescore-wordmark.png"
ABOUT_LOGO="resources/plectroscore-logo.png"

for f in "$SPLASH_CPP" "$APPSHELL_CMAKE" "$QML_CMAKE" "$ABOUT_QML"; do
    [ -f "$f" ] || { echo "   $0: no $f; the fixup is stale" >&2; exit 1; }
done
[ -f "$BRAND/musescore-wordmark.png" ] || {
    echo "   $0: no wordmark in $BRAND" >&2; exit 1; }

changed=0

# ------------------------------------------------------- about dialog icon --
DEST="src/appshell/qml/MuseScore/AppShell/$ABOUT_LOGO"
if ! cmp -s "$BRAND/plectroscore-512.png" "$DEST" 2>/dev/null; then
    cp "$BRAND/plectroscore-512.png" "$DEST"
    echo "   about: logo asset"
    changed=1
fi

if ! grep -q "$ABOUT_LOGO" "$QML_CMAKE"; then
    # Listed in RESOURCES beside the logo it replaces, or QML cannot load it
    python3 - "$QML_CMAKE" "$ABOUT_LOGO" <<'PY'
import sys
path, asset = sys.argv[1], sys.argv[2]
s = open(path).read()
anchor = "        resources/mu_logo.svg\n"
if anchor not in s:
    print("   cannot find mu_logo.svg in the QML module; update this fixup", file=sys.stderr)
    sys.exit(1)
open(path, 'w').write(s.replace(anchor, anchor + "        %s\n" % asset, 1))
print("   about: logo listed in the QML module")
PY
    changed=1
fi

if grep -q 'resources/mu_logo.svg' "$ABOUT_QML"; then
    python3 - "$ABOUT_QML" "$ABOUT_LOGO" <<'PY'
import sys
path, asset = sys.argv[1], sys.argv[2]
s = open(path).read()
old = 'source: "resources/mu_logo.svg"'
if old not in s:
    print("   the about dialog no longer names mu_logo.svg", file=sys.stderr)
    sys.exit(1)
open(path, 'w').write(s.replace(old, 'source: "%s"' % asset, 1))
print("   about: dialog points at the pick")
PY
    changed=1
fi

# ------------------------------------------------------ splash attribution --
DEST="src/appshell/$WORDMARK"
if ! cmp -s "$BRAND/musescore-wordmark.png" "$DEST" 2>/dev/null; then
    cp "$BRAND/musescore-wordmark.png" "$DEST"
    echo "   splash: wordmark asset"
    changed=1
fi

if ! grep -q "$WORDMARK" "$APPSHELL_CMAKE"; then
    python3 - "$APPSHELL_CMAKE" "$WORDMARK" <<'PY'
import sys
path, asset = sys.argv[1], sys.argv[2]
s = open(path).read()
anchor = "        resources/LoadingScreen.svg\n"
if anchor not in s:
    print("   cannot find LoadingScreen.svg in the resource list", file=sys.stderr)
    sys.exit(1)
open(path, 'w').write(s.replace(anchor, anchor + "        %s\n" % asset, 1))
print("   splash: wordmark listed as a resource")
PY
    changed=1
fi

if ! grep -q 'musescore-wordmark' "$SPLASH_CPP"; then
    python3 - "$SPLASH_CPP" <<'PY'
import re, sys
path = sys.argv[1]
s = open(path).read()

# Replace the plain attribution line with their wordmark, drawn at a size that
# reads clearly but stays below the fork's own name.
old = re.search(
    r'\n *QFont basedFont = .*?QStringLiteral\("based on MuseScore Studio"\)\);\n',
    s, re.S)
if not old:
    print("   cannot find the attribution text to replace", file=sys.stderr)
    sys.exit(1)

block = '''
        // Their wordmark, not ours, and smaller than ours: this says what the
        // build is based on. Kept as an image because it is a mark, not type.
        QFont basedFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
        basedFont.setPixelSize(16);
        painter->setFont(basedFont);
        painter->setPen(QPen(QColor("#C79A87")));
        painter->drawText(QRectF(332, 208, 200, 22),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip,
                          QStringLiteral("based on"));

        QPixmap wordmark(QStringLiteral(":/resources/musescore-wordmark.png"));
        if (!wordmark.isNull()) {
            const int wordmarkWidth = 208;
            const int wordmarkHeight = wordmark.height() * wordmarkWidth / wordmark.width();
            painter->drawPixmap(QRect(412, 208 + (22 - wordmarkHeight) / 2,
                                      wordmarkWidth, wordmarkHeight),
                                wordmark);
        }
'''
s = s[:old.start()] + block + s[old.end():]

if "#include <QPixmap>" not in s:
    s = s.replace("#include <QPainter>", "#include <QPainter>\n#include <QPixmap>", 1)

open(path, 'w').write(s)
print("   splash: attribution now shows their wordmark")
PY
    changed=1
fi

# ----------------------------------------------------------- splash colour --
# The version number is painted in MuseScore's cyan, which was right on their
# navy and shouts on ours. Their wordmark keeps its own colour, because it is
# their mark; our own text should not borrow it.
if grep -q 'versionNumberColor("#19F3FF")' "$SPLASH_CPP"; then
    sed -i '' 's/versionNumberColor("#19F3FF")/versionNumberColor("#E8845C")/' "$SPLASH_CPP"
    echo "   splash: version number in the fork's own colour"
    changed=1
fi

[ "$changed" = 1 ] || echo "   nothing to do, already applied"
