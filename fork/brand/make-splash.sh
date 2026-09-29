#!/usr/bin/env bash
#
# Draw the PlectroScore loading screen.
#
#   fork/brand/make-splash.sh [outdir]      default: fork/brand
#
# Upstream's is 800x380: the musescore studio wordmark on the left, a fan of
# triangles on the right, over navy. None of that is reused. Borrowing another
# project's visual identity for a differently named build is exactly the thing
# a fork should not do, so this is its own drawing in the pick's own colours,
# at the same size so the window code needs no changes.
#
# Flat fills, no gradients. This rasteriser renders a gradient referenced by
# url() as black in this file, and Qt's QSvgRenderer supports an even smaller
# subset than that. The real consumer is Qt, which cannot be checked from here,
# so the drawing uses only what every renderer agrees on.
#
# Text is NOT drawn here. loadingscreenview.cpp already paints the version and
# the link with QPainter, and the fork adds its name the same way. Qt renders
# this file with QSvgRenderer, whose SVG support is a subset: paths and linear
# gradients are safe, and leaving type to the code avoids depending on a font
# being present inside an SVG.
#
# The pick geometry is the icon's, so the two never drift apart.

set -o errexit
set -o nounset
set -o pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${1:-$HERE}"
mkdir -p "$OUT"

W=800
H=380

python3 - "$W" "$H" > "$OUT/LoadingScreen.svg" <<'PY'
import math, sys

W, H = int(sys.argv[1]), int(sys.argv[2])

# The pick, same shape as the icon, scaled into the left of the panel.
SCALE = 0.30
OX, OY = 56, 78          # where the 1024 box lands
def P(x, y):
    return f"{OX + x*SCALE:.1f},{OY + y*SCALE:.1f}"

CX, CY = 512, 430

def circle(r):
    pts = []
    for i in range(65):                      # arcs are fine in Qt, but a
        a = 2*math.pi*i/64                   # polyline is fine everywhere
        pts.append(P(CX + r*math.cos(a), CY + r*math.sin(a)))
    return "M " + " L ".join(pts) + " Z"

def spoke(angle, r0=196, r1=262, half=17):
    a = math.radians(angle - 90)
    ca, sa = math.cos(a), math.sin(a)
    px, py = -sa, ca
    corners = [(r0,-half), (r1,-half), (r1,half), (r0,half)]
    return "M " + " L ".join(P(CX+ca*r+px*h, CY+sa*r+py*h) for r, h in corners) + " Z"

plectrum = ("M " + P(512,74) +
            " C " + P(748,74) + " " + P(966,178) + " " + P(966,388) +
            " C " + P(966,556) + " " + P(764,806) + " " + P(586,906) +
            " C " + P(552,926) + " " + P(472,926) + " " + P(438,906) +
            " C " + P(260,806) + " " + P(58,556) + " " + P(58,388) +
            " C " + P(58,178) + " " + P(276,74) + " " + P(512,74) + " Z")

pick = plectrum + " " + " ".join(
    [circle(118), circle(166), circle(196)] + [spoke(i*30) for i in range(12)])

# A few strings sweeping across the panel: the doubled courses of a bandurria,
# receding. Decorative, and nothing anybody else is using.
strings = []
for i in range(12):
    t = i / 11.0
    y0 = 40 + t * 300
    y1 = y0 + 26 + t * 40
    op = 0.05 + 0.16 * (1.0 - t)
    strings.append(
        f'  <path d="M {W*0.42:.0f},{y0:.0f} C {W*0.62:.0f},{y0-10:.0f} '
        f'{W*0.80:.0f},{y1:.0f} {W+10},{y1+18:.0f}" '
        f'stroke="#E8845C" stroke-opacity="{op:.3f}" stroke-width="2" fill="none"/>')

print(f'''<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">
  <rect width="{W}" height="{H}" fill="#2B1A15"/>
{chr(10).join(strings)}
  <path fill-rule="evenodd" fill="#D4552F" d="{pick}"/>
</svg>''')
PY

echo "splash:  $OUT/LoadingScreen.svg"

# A PNG beside it, only so the drawing can be reviewed without launching the app
if command -v magick >/dev/null; then
    magick -background none "$OUT/LoadingScreen.svg" PNG32:"$OUT/splash-preview.png"
    echo "review:  $OUT/splash-preview.png"
fi

# --------------------------------------------------- MuseScore's own wordmark --
# Used on the loading screen to say what the build is based on. Lifted from
# upstream's loading screen rather than redrawn, because a mark has to be the
# real one to mean anything.
#
# Taken from a high resolution render and not by pulling paths out of the SVG:
# that was tried, and the fifty eight paths interleave the wordmark with the
# decorative fan, so filtering by coordinates lost letters or kept triangles.
#
# The crop carries upstream's navy background, opaque, which showed as a dark
# box on ours. Luminance becomes the alpha instead, which keeps the antialiased
# edges clean, and the colour is then painted back on flat.
UPSTREAM_SPLASH="${UPSTREAM_SPLASH:-}"
if [ -n "$UPSTREAM_SPLASH" ] && [ -f "$UPSTREAM_SPLASH" ] && command -v magick >/dev/null; then
    TMP="$(mktemp -d)"
    magick -background none "$UPSTREAM_SPLASH" -resize 3200x1520 "$TMP/big.png"
    magick "$TMP/big.png" -crop 1320x290+140+540 +repage -trim +repage "$TMP/crop.png"
    SIZE=$(magick identify -format '%wx%h' "$TMP/crop.png")
    magick "$TMP/crop.png" -colorspace gray -level 12%,62% "$TMP/mask.png"
    magick -size "$SIZE" xc:'#19F3FF' \( "$TMP/mask.png" \) -alpha off \
        -compose CopyOpacity -composite -resize 900x PNG32:"$OUT/musescore-wordmark.png"
    rm -rf "$TMP"
    echo "wordmark: $OUT/musescore-wordmark.png"
else
    echo "wordmark: kept as is (set UPSTREAM_SPLASH to regenerate from upstream's)"
fi
