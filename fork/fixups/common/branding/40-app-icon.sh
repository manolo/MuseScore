#!/usr/bin/env bash
#
# Put the PlectroScore icon on the app.
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

BRAND="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)/brand"
ICON_DIR="share/icons"

[ -d "$ICON_DIR" ] || { echo "   $0: no $ICON_DIR; the fixup is stale" >&2; exit 1; }
[ -f "$BRAND/PlectroScore.icns" ] || {
    echo "   $0: no icon in $BRAND; run fork/brand/make-icon.sh" >&2
    exit 1
}

changed=0

# ------------------------------------------------------------------ macOS --
# CMake copies whatever MACOSX_BUNDLE_ICON_FILE names into Resources, so the
# file has to be a source of the target as well as named in the bundle.
if [ ! -f "$ICON_DIR/AppIcon/PlectroScore.icns" ] \
   || ! cmp -s "$BRAND/PlectroScore.icns" "$ICON_DIR/AppIcon/PlectroScore.icns"; then
    cp "$BRAND/PlectroScore.icns" "$ICON_DIR/AppIcon/PlectroScore.icns"
    echo "   macOS: PlectroScore.icns"
    changed=1
fi

APP_CMAKE="src/app/CMakeLists.txt"
if [ -f "$APP_CMAKE" ] && ! grep -q 'PlectroScore.icns' "$APP_CMAKE"; then
    python3 - "$APP_CMAKE" <<'PY'
import re, sys
path = sys.argv[1]
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
    f'{indent}set(PLECTRO_ICNS ${{PROJECT_SOURCE_DIR}}/share/icons/AppIcon/PlectroScore.icns)\n'
    f'{indent}target_sources(MuseScoreStudio PRIVATE ${{PLECTRO_ICNS}})\n'
    f'{indent}set_source_files_properties(${{PLECTRO_ICNS}} PROPERTIES\n'
    f'{indent}    MACOSX_PACKAGE_LOCATION Resources)\n'
    f'{indent}set_target_properties(MuseScoreStudio PROPERTIES\n'
    f'{indent}    MACOSX_BUNDLE_ICON_FILE PlectroScore.icns)\n'
)
s = s[:needle.end()] + "\n" + block + s[needle.end():]
open(path, 'w').write(s)
print("   macOS: bundle points at PlectroScore.icns")
PY
    changed=1
fi

# ---------------------------------------------------------------- Windows --
# windows_icons.rc names MS4_AppIcon.ico, so replacing that file is enough.
WIN_ICO="$ICON_DIR/AppIcon/MS4_AppIcon.ico"
if [ -f "$WIN_ICO" ] && ! cmp -s "$BRAND/PlectroScore.ico" "$WIN_ICO"; then
    cp "$BRAND/PlectroScore.ico" "$WIN_ICO"
    echo "   Windows: MS4_AppIcon.ico"
    changed=1
fi

# ------------------------------------------------------------------ Linux --
# Installed by size under the upstream names; replace each one we have.
for sz in 16 32 48 64 128 256 512; do
    src="$BRAND/plectroscore-$sz.png"
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
