#!/usr/bin/env bash
#
# Generate the fork's icon in every format the three platforms want.
#
# The drawing belongs to no single brand: every brand uses this same pick,
# so the files are named for what they are and the fixup copies them under
# whatever the brand is called.
#
#   fork/brand/make-icon.sh [outdir]      default: fork/brand
#
# The design lives here, in the geometry below, rather than in a binary that
# nobody can edit. Re-running this reproduces the whole set.
#
# WHY NOT AN SVG MASTER. The obvious thing is to keep an SVG and rasterise it.
# It was tried and abandoned: the rsvg build ImageMagick delegates to on this
# machine silently drops <mask> and mishandles rotate() about a point, so the
# SVG came out blank, then black, then displaced. Drawing straight with
# ImageMagick is a format nobody has to guess about, and what you see here is
# what ships.
#
# THE SHAPE. A plectrum seen head on, with the rosette of a Spanish soundhole
# cut through it: the hole, the ring, and twelve spokes, one per string of a
# bandurria, six courses doubled.
#
# The first attempt was narrow with a long tip and read, unmistakably, as a
# map pin. A real 351 pick is nearly as wide as it is tall, with broad
# shoulders and a blunt tip, so that is what this draws. The rosette is
# heavier than a luthier would cut it, so that it survives being shrunk;
# below about 24 pixels it closes up anyway and only the silhouette and the
# colour identify the app, which is all a Dock icon needs.
#
# The holes are subpaths under the even-odd fill rule, not a mask. Masks are
# the first thing a rasteriser drops.

set -o errexit
set -o nounset
set -o pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# The generated files sit beside this script and are committed, so a build
# never needs ImageMagick; the script is here to change the design, not to
# be a build step.
OUT="${1:-$HERE/art}"

command -v magick   >/dev/null || { echo "need ImageMagick (magick)" >&2; exit 1; }
command -v iconutil >/dev/null || echo "note: no iconutil, the .icns will be skipped (macOS only)" >&2

mkdir -p "$OUT"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# ------------------------------------------------------------------ colour --
# Peninsular red, warmed towards the tortoiseshell of a real pick. Light at
# the shoulder, deep at the tip, so the form reads even as a silhouette.
TOP='#DD6A42'
BOTTOM='#8A2016'
TILT=-5          # degrees; enough to look held, not enough to look crooked

# --------------------------------------------------------------- geometry --
PATH_D="$(python3 - <<'PY'
import math

CX, CY = 512, 430          # centre of the rosette
SIZE    = 1024

def circle(r):
    return (f"M {CX-r},{CY} A {r},{r} 0 1,0 {CX+r},{CY} "
            f"A {r},{r} 0 1,0 {CX-r},{CY} Z")

def spoke(angle, r0=196, r1=262, half=17):
    a = math.radians(angle - 90)
    ca, sa = math.cos(a), math.sin(a)
    px, py = -sa, ca                      # perpendicular to the radius
    corners = [(r0, -half), (r1, -half), (r1, half), (r0, half)]
    return " ".join(
        ("M" if i == 0 else "L") + f" {CX+ca*r+px*h:.1f},{CY+sa*r+py*h:.1f}"
        for i, (r, h) in enumerate(corners)) + " Z"

# The pick: broad shoulders, flanks falling away, a blunt rounded tip.
plectrum = ("M 512,74 C 748,74 966,178 966,388 "
            "C 966,556 764,806 586,906 C 552,926 472,926 438,906 "
            "C 260,806 58,556 58,388 C 58,178 276,74 512,74 Z")

# Ordered by radius. Under even-odd this alternates hole and material
# outwards on its own: soundhole, material, ring, material, spokes.
rosette = [circle(118), circle(166), circle(196)]
rosette += [spoke(i * 30) for i in range(12)]

print(plectrum + " " + " ".join(rosette))
PY
)"

# ------------------------------------------------------------------ master --
# The shape as a stencil, then the gradient poured through it. Compositing
# beats a gradient-filled path here: ImageMagick's draw takes a flat fill.
magick -size 1024x1024 xc:black -fill white \
    -draw "fill-rule evenodd path '$PATH_D'" -alpha off "$WORK/stencil.png"

# A stencil that came out empty means the path was rejected; catch it here
# rather than shipping a blank icon.
if [ "$(magick identify -format '%[pixel:p{512,800}]' "$WORK/stencil.png")" != "gray(255)" ]; then
    echo "the stencil is empty under the rosette; the path was rejected" >&2
    exit 1
fi

magick -size 1024x1024 "gradient:${TOP}-${BOTTOM}" "$WORK/gradient.png"

magick "$WORK/gradient.png" \( "$WORK/stencil.png" -colorspace gray \) \
    -compose CopyOpacity -composite \
    -colorspace sRGB -background none -distort SRT "$TILT" \
    PNG32:"$OUT/icon-1024.png"

echo "master: $OUT/icon-1024.png"

# ------------------------------------------------------------------ sizes --
for sz in 512 256 128 64 48 32 16; do
    magick "$OUT/icon-1024.png" -resize ${sz}x${sz} PNG32:"$OUT/icon-$sz.png"
done
echo "pngs:   16 32 48 64 128 256 512 1024"

# -------------------------------------------------------------------- icns --
if command -v iconutil >/dev/null; then
    SET="$WORK/icon.iconset"
    mkdir -p "$SET"
    # Apple wants each size twice, once plain and once as the @2x of the half
    for pair in 16:16x16 32:16x16@2x 32:32x32 64:32x32@2x \
                128:128x128 256:128x128@2x 256:256x256 512:256x256@2x \
                512:512x512 1024:512x512@2x; do
        magick "$OUT/icon-1024.png" -resize "${pair%%:*}x${pair%%:*}" \
            PNG32:"$SET/icon_${pair##*:}.png"
    done
    iconutil -c icns "$SET" -o "$OUT/icon.icns"
    echo "icns:   $OUT/icon.icns"
fi

# --------------------------------------------------------------------- ico --
magick "$OUT/icon-1024.png" \
    -define icon:auto-resize=256,128,64,48,32,16 "$OUT/icon.ico"
echo "ico:    $OUT/icon.ico"

# ----------------------------------------------------------------- preview --
# Small sizes on both grounds, which is the only review that matters.
for bg_name in 'ece8e1:light' '1c1c1e:dark'; do
    bg="#${bg_name%%:*}"
    magick "$OUT/icon-128.png" "$OUT/icon-64.png" \
           "$OUT/icon-32.png" "$OUT/icon-16.png" \
        -background "$bg" -gravity center -extent 150x150 +append \
        -bordercolor "$bg" -border 10 PNG32:"$WORK/${bg_name##*:}.png"
done
magick "$WORK/light.png" "$WORK/dark.png" -append PNG32:"$OUT/preview.png"
echo "review: $OUT/preview.png"
