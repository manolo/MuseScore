#!/usr/bin/env bash
#
# Put the fork's icon on the app.
#
# The drawing is shared by every brand and lives under brand/art; only the
# name it is installed under comes from the brand. A brand that ever wants
# its own drawing drops an icon.icns in its own directory and it wins.
#
# Upstream builds its macOS icon with Icon Composer: share/icons/AppIcon.icon
# is a bundle that actool compiles into Assets.car and an .icns at build time.
# That needs Xcode 26 or newer, and where actool is missing the setup function
# warns and returns, leaving the bundle with no icon of its own at all. So
# rather than authoring an Icon Composer bundle that only some machines can
# compile, this drops a ready made .icns in and points the bundle at it. The
# file is committed, so every machine and CI get the same icon.
#
# Windows reads share/icons/windows_icons.rc, which names the .ico files, and
# Linux installs a set of PNGs by size. Both take our files under their
# existing names, so no build file has to learn a new one.
#
# Only the application icon changes. The document icons, the ones on a .mscz
# and a .mscx in the Finder, stay as MuseScore's: those files belong to
# MuseScore, not to this fork, and a score is the same score either way.
#
# Idempotent: silent once applied, loud if the icons move.

set -o errexit
set -o nounset

[ -n "${BRAND_DIR:-}" ] || { echo "   $0: BRAND_DIR is not set" >&2; exit 1; }
# shellcheck disable=SC1091
. "$BRAND_DIR/identity.sh"

ART="$(cd "$BRAND_DIR/.." && pwd)/art"
ICON_DIR="share/icons"
ICNS_NAME="$APP_NAME.icns"

# A brand may override any piece of the drawing with its own copy.
art() { if [ -f "$BRAND_DIR/$1" ]; then echo "$BRAND_DIR/$1"; else echo "$ART/$1"; fi; }

[ -d "$ICON_DIR" ] || { echo "   $0: no $ICON_DIR; the fixup is stale" >&2; exit 1; }
[ -f "$(art icon.icns)" ] || {
    echo "   $0: no icon for $APP_NAME; run fork/brand/make-icon.sh" >&2
    exit 1
}

changed=0

# ------------------------------------------------------------------ macOS --
# CMake copies whatever MACOSX_BUNDLE_ICON_FILE names into Resources, so the
# file has to be a source of the target as well as named in the bundle.
if [ ! -f "$ICON_DIR/AppIcon/$ICNS_NAME" ] \
   || ! cmp -s "$(art icon.icns)" "$ICON_DIR/AppIcon/$ICNS_NAME"; then
    cp "$(art icon.icns)" "$ICON_DIR/AppIcon/$ICNS_NAME"
    echo "   macOS: $ICNS_NAME"
    changed=1
fi

APP_CMAKE="src/app/CMakeLists.txt"
if [ -f "$APP_CMAKE" ] && ! grep -q "$ICNS_NAME" "$APP_CMAKE"; then
    python3 - "$APP_CMAKE" "$ICNS_NAME" <<'PY'
import re, sys
path, icns = sys.argv[1], sys.argv[2]
s = open(path).read()

needle = re.search(r'^(\s*)target_setup_iconcomposer_icon\([^)]*\)\s*$', s, re.M)
if not needle:
    print("   cannot find the icon setup call in src/app/CMakeLists.txt;"
          " read it and update this fixup", file=sys.stderr)
    sys.exit(1)

indent = needle.group(1)
block = (
    f'{indent}# The fork ships a ready made icon rather than an Icon Composer bundle:\n'
    f'{indent}# actool is absent on most machines, and where it is the upstream setup\n'
    f'{indent}# warns and leaves the bundle with no icon at all.\n'
    f'{indent}set(FORK_APP_ICNS ${{PROJECT_SOURCE_DIR}}/share/icons/AppIcon/{icns})\n'
    f'{indent}target_sources(MuseScoreStudio PRIVATE ${{FORK_APP_ICNS}})\n'
    f'{indent}set_source_files_properties(${{FORK_APP_ICNS}} PROPERTIES\n'
    f'{indent}    MACOSX_PACKAGE_LOCATION Resources)\n'
    f'{indent}set_target_properties(MuseScoreStudio PROPERTIES\n'
    f'{indent}    MACOSX_BUNDLE_ICON_FILE {icns})\n'
)
s = s[:needle.end()] + "\n" + block + s[needle.end():]
open(path, 'w').write(s)
print("   macOS: bundle points at %s" % icns)
PY
    changed=1
fi

# ---------------------------------------------------------------- Windows --
# windows_icons.rc names MS4_AppIcon.ico, so replacing that file is enough.
WIN_ICO="$ICON_DIR/AppIcon/MS4_AppIcon.ico"
if [ -f "$WIN_ICO" ] && ! cmp -s "$(art icon.ico)" "$WIN_ICO"; then
    cp "$(art icon.ico)" "$WIN_ICO"
    echo "   Windows: MS4_AppIcon.ico"
    changed=1
fi

# ------------------------------------------------------------------ Linux --
# Installed by size under the upstream names; replace each one we have.
for sz in 16 32 48 64 128 256 512; do
    src="$(art icon-$sz.png)"
    dst="$ICON_DIR/AppIcon/MS4_AppIcon_${sz}x${sz}.png"
    if [ -f "$src" ] && [ -f "$dst" ] && ! cmp -s "$src" "$dst"; then
        cp "$src" "$dst"
        changed=1
    fi
done
[ "$changed" = 1 ] && echo "   Linux: the PNG set" || true

# The larger sizes upstream ships have no counterpart in our set; scaling the
# 512 up would look worse than leaving MuseScore's, and nothing on Linux asks
# for them by default.

[ "$changed" = 1 ] || echo "   nothing to do, the icon is already in place"
