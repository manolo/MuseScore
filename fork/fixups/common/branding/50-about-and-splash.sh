#!/usr/bin/env bash
#
# Say what the build is, on the loading screen and in the about box.
#
# Three things go on both surfaces: the name, what the build is for, and what
# it is based on. The last one is not decoration. The code is MuseScore's
# under the GPL, and the honest thing for a renamed build is to name its
# origin where anyone can see it, not only in a licence file nobody opens.
#
# The attribution is MuseScore's own wordmark rather than type, because a mark
# has to be the real one to mean anything. Using it to say what a build is
# based on is what a mark is for; what a fork must not do is ship it as its
# own, which is why the loading screen artwork is the fork's own drawing and
# not an edit of upstream's.
#
# None of the strings are translated through qsTrc. The name is a name, and
# the attribution has to read the same in every language for it to be worth
# anything.
#
# Idempotent: silent once applied, loud if any of these files move.

set -o errexit
set -o nounset

[ -n "${BRAND_DIR:-}" ] || { echo "   $0: BRAND_DIR is not set" >&2; exit 1; }
# shellcheck disable=SC1091
. "$BRAND_DIR/identity.sh"

ART="$(cd "$BRAND_DIR/.." && pwd)/art"
art() { if [ -f "$BRAND_DIR/$1" ]; then echo "$BRAND_DIR/$1"; else echo "$ART/$1"; fi; }

SPLASH_SVG="src/appshell/resources/LoadingScreen.svg"
SPLASH_CPP="src/appshell/widgets/splashscreen/loadingscreenview.cpp"
APPSHELL_CMAKE="src/appshell/CMakeLists.txt"
QML_CMAKE="src/appshell/qml/MuseScore/AppShell/CMakeLists.txt"
ABOUT_QML="src/appshell/qml/MuseScore/AppShell/AboutDialog.qml"

WORDMARK="resources/musescore-wordmark.png"
ABOUT_LOGO="resources/$APP_SLUG-logo.png"

for f in "$SPLASH_SVG" "$SPLASH_CPP" "$APPSHELL_CMAKE" "$QML_CMAKE" "$ABOUT_QML"; do
    [ -f "$f" ] || { echo "   $0: no $f; the fixup is stale" >&2; exit 1; }
done
for a in LoadingScreen.svg musescore-wordmark.png icon-512.png; do
    [ -f "$(art $a)" ] || { echo "   $0: no $a; run fork/brand/make-splash.sh" >&2; exit 1; }
done

changed=0

# ------------------------------------------------------- loading screen art --
if ! cmp -s "$(art LoadingScreen.svg)" "$SPLASH_SVG"; then
    cp "$(art LoadingScreen.svg)" "$SPLASH_SVG"
    echo "   splash: artwork"
    changed=1
fi

if ! cmp -s "$(art musescore-wordmark.png)" "src/appshell/$WORDMARK" 2>/dev/null; then
    cp "$(art musescore-wordmark.png)" "src/appshell/$WORDMARK"
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

# ------------------------------------------------------ loading screen text --
if ! grep -q "$APP_NAME" "$SPLASH_CPP"; then
    python3 - "$SPLASH_CPP" "$APP_NAME" "$APP_TAGLINE" "$APP_ATTRIBUTION" "$APP_LINK" <<'PY'
import re, sys
path, name, tagline, based_on, link = sys.argv[1:6]
s = open(path).read()

# The link upstream shows is its own; ours points at where these builds live.
s2, n = re.subn(r'static const QString website\("[^"]*"\);',
                'static const QString website("%s");' % link, s)
if n != 1:
    print("   cannot find the website string in the loading screen", file=sys.stderr)
    sys.exit(1)
s = s2

anchor = "    // Draw message\n"
if anchor not in s:
    print("   cannot find the message block in the loading screen", file=sys.stderr)
    sys.exit(1)

# The artwork leaves the right half empty on purpose. Three rows go in it:
# the name, one line saying what the build is for, and the attribution.
block = '''    // The name, what this build is for, and what it is based on. The artwork
    // leaves this area empty on purpose.
    {
        QFont nameFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
        nameFont.setPixelSize(44);
        nameFont.setWeight(QFont::DemiBold);
        painter->setFont(nameFont);
        painter->setPen(QPen(QColor("#F3E7E1")));
        painter->drawText(QRectF(330, 126, 440, 56),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip,
                          QStringLiteral("%s"));

        QFont taglineFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
        taglineFont.setPixelSize(15);
        painter->setFont(taglineFont);
        painter->setPen(QPen(QColor("#D9B3A2")));
        painter->drawText(QRectF(332, 180, 440, 24),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip,
                          QStringLiteral("%s"));

        // Their wordmark, not ours, and smaller than ours: this says what the
        // build is based on. Kept as an image because it is a mark, not type.
        QFont basedFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
        basedFont.setPixelSize(16);
        painter->setFont(basedFont);
        painter->setPen(QPen(QColor("#C79A87")));
        painter->drawText(QRectF(332, 228, 200, 22),
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextDontClip,
                          QStringLiteral("based on"));

        QPixmap wordmark(QStringLiteral(":/resources/musescore-wordmark.png"));
        if (!wordmark.isNull()) {
            const int wordmarkWidth = 208;
            const int wordmarkHeight = wordmark.height() * wordmarkWidth / wordmark.width();
            painter->drawPixmap(QRect(412, 228 + (22 - wordmarkHeight) / 2,
                                      wordmarkWidth, wordmarkHeight),
                                wordmark);
        }
    }

''' % (name, tagline)

s = s.replace(anchor, block + anchor, 1)

# 4.7 draws its message with a plain QFont and includes neither of these;
# 5.0 already has QFontDatabase. Add whichever is missing.
for header in ("QFontDatabase", "QPixmap"):
    if "#include <%s>" % header not in s:
        s = s.replace("#include <QPainter>", "#include <QPainter>\n#include <%s>" % header, 1)

open(path, 'w').write(s)
print("   splash: name, tagline, attribution and link")
PY
    changed=1
fi

# The version number is painted in MuseScore's cyan, which was right on their
# navy and shouts on ours.
if grep -q 'versionNumberColor("#19F3FF")' "$SPLASH_CPP"; then
    sed -i '' 's/versionNumberColor("#19F3FF")/versionNumberColor("#E8845C")/' "$SPLASH_CPP"
    echo "   splash: version number in the fork's own colour"
    changed=1
fi

# -------------------------------------------------------------- about logo --
# A dialog titled About <fork> showing somebody else's mark looks like the
# icon was forgotten. The attribution below it is what names the origin.
DEST="src/appshell/qml/MuseScore/AppShell/$ABOUT_LOGO"
if ! cmp -s "$(art icon-512.png)" "$DEST" 2>/dev/null; then
    cp "$(art icon-512.png)" "$DEST"
    echo "   about: logo asset"
    changed=1
fi

if ! grep -q "$ABOUT_LOGO" "$QML_CMAKE"; then
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

if grep -q 'source: "resources/mu_logo.svg"' "$ABOUT_QML"; then
    python3 - "$ABOUT_QML" "$ABOUT_LOGO" <<'PY'
import sys
path, asset = sys.argv[1], sys.argv[2]
s = open(path).read()
open(path, 'w').write(s.replace('source: "resources/mu_logo.svg"', 'source: "%s"' % asset, 1))
print("   about: dialog points at the pick")
PY
    changed=1
fi

# -------------------------------------------------------------- about text --
if ! grep -q "$APP_ATTRIBUTION" "$ABOUT_QML"; then
    python3 - "$ABOUT_QML" "$APP_NAME" "$APP_TAGLINE" "$APP_BLURB" "$APP_ATTRIBUTION" <<'PY'
import re, sys
path, name, tagline, blurb, based_on = sys.argv[1:6]
s = open(path).read()

s2, n = re.subn(r'title: qsTrc\("appshell/about", "About [^"]*"\)',
                'title: qsTrc("appshell/about", "About %s")' % name, s)
if n != 1:
    print("   cannot find the about dialog title", file=sys.stderr)
    sys.exit(1)
s = s2

anchor = re.search(
    r'( *)StyledTextLabel \{\n\s*anchors\.horizontalCenter: parent\.horizontalCenter\n'
    r'\s*text: qsTrc\("appshell/about", "Version:"\)[^\n]*\n[^\n]*\n\s*\}\n', s)
if not anchor:
    print("   cannot find the version label in the about dialog", file=sys.stderr)
    sys.exit(1)

i = anchor.group(1)
def label(text, opacity, wrap=False):
    out = (f'\n{i}StyledTextLabel {{\n'
           f'{i}    anchors.horizontalCenter: parent.horizontalCenter\n'
           f'{i}    width: parent.width\n'
           f'{i}    horizontalAlignment: Text.AlignHCenter\n'
           f'{i}    text: "{text}"\n'
           f'{i}    opacity: {opacity}\n')
    if wrap:
        out += f'{i}    wrapMode: Text.WordWrap\n'
    return out + f'{i}}}\n'

block = label(tagline, "0.9")
if blurb:
    block += label(blurb, "0.7", wrap=True)
block += label(based_on, "0.7")

s = s[:anchor.end()] + block + s[anchor.end():]
open(path, 'w').write(s)
print("   about: title, what the build is for, and attribution")
PY
    changed=1
fi

# -------------------------------------------------------------- about menu --
# The dialog title lives in QML, but what people actually click is an action
# title in C++. Changing only the QML leaves the menu still saying About
# MuseScore Studio, which looks exactly like nothing changed at all.
for f in src/appshell/internal/applicationuiactions.cpp \
         src/appshell/internal/appshellcommandsregister.cpp; do
    [ -f "$f" ] || continue
    grep -q "About $APP_NAME" "$f" && continue
    if ! grep -q 'About MuseScore Studio' "$f"; then
        echo "   $f no longer names the about action; read it and update this fixup" >&2
        exit 1
    fi
    n=$(grep -c 'About MuseScore Studio' "$f")
    sed -i '' "s/About MuseScore Studio/About $APP_NAME/g" "$f"
    echo "   about menu: $(basename "$f"), $n string(s)"
    changed=1
done

[ "$changed" = 1 ] || echo "   nothing to do, already branded"
