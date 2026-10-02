#!/usr/bin/env bash
#
# Teach the build about the two destinations it does not already cover.
#
# share/plugins and share/extensions are installed wholesale, and anything
# reaching the installed tree reaches the DMG, the MSI and the AppImage,
# because none of the three packaging paths has a file list. So only two
# things need rules:
#
# **The soundfonts.** share/sound/CMakeLists.txt installs one file by name,
# with no glob, so extra fonts have to be named. Once installed they are found
# without any registration: the scan is recursive over the whole resources
# directory and filters *.sf2 and *.sf3.
#
# **The VST.** This one cannot go under share/ at all. The VST module never
# looks at the resources directory; what it uses is the Steinberg SDK's own
# list of module paths, which already includes one folder per platform at
# application level. Those three folders are the only places a bundled plugin
# is found without the user installing it system wide:
#
#   macOS    mscore.app/Contents/VST3      module_mac.mm
#   Windows  VST3 beside the executable    module_win32.cpp
#   Linux    vst3 beside the executable    module_linux.cpp
#
# Each rule is guarded by EXISTS so a platform with no published build, today
# Linux on arm64, simply ships without the plugin instead of failing to
# install.
#
# Idempotent: silent once applied, loud if either file stops looking like this.

set -o errexit
set -o nounset

SOUND_CMAKE="share/sound/CMakeLists.txt"
SHARE_CMAKE="share/CMakeLists.txt"

for f in "$SOUND_CMAKE" "$SHARE_CMAKE"; do
    [ -f "$f" ] || { echo "   $0: no $f; the fixup is stale" >&2; exit 1; }
done

changed=0

# --------------------------------------------------------------- soundfonts --
if ! grep -q 'PLECTRA_SOUNDFONTS' "$SOUND_CMAKE"; then
    python3 - "$SOUND_CMAKE" <<'PY'
import re, sys
path = sys.argv[1]
s = open(path).read()

# Do NOT write the destination from memory. The two lines name it with
# different variables, MUSE_APP_INSTALL_RESOURCES_LOCATION on 5.0 and
# Mscore_SHARE_NAME plus Mscore_INSTALL_NAME on 4.7, and one of them ends in a
# slash while the other does not. Getting it wrong sends the install to /sound
# at the root of the disk, which is exactly what happened. Copy the line that
# is already there instead.
dest = re.search(r'^\s*DESTINATION\s+(\S*sound)\s*$', s, re.M)
if not dest:
    print("   cannot find where MS Basic is installed; read "
          "share/sound/CMakeLists.txt and update this fixup", file=sys.stderr)
    sys.exit(1)

m = re.search(r'^if \(MUE_INSTALL_SOUNDFONT\)\s*$', s, re.M)
if not m:
    print("   cannot find the MUE_INSTALL_SOUNDFONT guard", file=sys.stderr)
    sys.exit(1)

block = '''
    # The fork's own voices. Nothing has to register them: the soundfont scan
    # is recursive over the resources directory and filters *.sf2 and *.sf3.
    file(GLOB PLECTRA_SOUNDFONTS "${CMAKE_CURRENT_SOURCE_DIR}/*-Con-Tremolo.sf2")
    if (PLECTRA_SOUNDFONTS)
        install(FILES ${PLECTRA_SOUNDFONTS}
            DESTINATION %s
            )
    endif ()
''' % dest.group(1)

s = s[:m.end()] + "\n" + block + s[m.end():]
open(path, 'w').write(s)
print("   sound: the fork's soundfonts, into %s" % dest.group(1))
PY
    changed=1
fi

# ---------------------------------------------------------------------- VST --
if ! grep -q 'PLECTRA_VST3' "$SHARE_CMAKE"; then
    python3 - "$SHARE_CMAKE" <<'PY'
import re, sys
path = sys.argv[1]
s = open(path).read()

anchor = re.search(r'^install \(DIRECTORY\n    workspaces\n.*?\n    \)\n', s, re.S | re.M)
if not anchor:
    print("   cannot find the workspaces install rule; read share/CMakeLists.txt "
          "and update this fixup", file=sys.stderr)
    sys.exit(1)

# Where the resources go, taken from the rule that is already there. 5.0 and
# 4.7 name this with different variables and only one of them ends in a
# slash, so it is read rather than written from memory.
res = re.search(r'^install \(DIRECTORY\n    plugins\n\s*DESTINATION (\S+)\s*$', s, re.M)
if not res:
    print("   cannot find where the plugins directory is installed", file=sys.stderr)
    sys.exit(1)

block = '''
# The bundled VST3, which cannot live under the resources directory: the VST
# module never looks there. These three destinations are the application level
# folders the Steinberg SDK already scans, so the plugin is found without the
# user installing anything.
#
# Guarded by EXISTS because not every platform has a published build. Linux on
# arm64 has none today and ships without it.
if (APPLE)
    set(PLECTRA_VST3 "${CMAKE_CURRENT_SOURCE_DIR}/vst3/macos")
    # A sibling of Resources inside the bundle, derived from wherever the
    # resources go rather than written out, because the two lines name that
    # place with different variables.
    string(REGEX REPLACE "Resources/?$" "VST3" PLECTRA_VST3_DEST "RESOURCES_DEST")
elseif (WIN32)
    set(PLECTRA_VST3 "${CMAKE_CURRENT_SOURCE_DIR}/vst3/windows")
    set(PLECTRA_VST3_DEST "VST3")
elseif (UNIX AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(PLECTRA_VST3 "${CMAKE_CURRENT_SOURCE_DIR}/vst3/linux_x86_64")
    set(PLECTRA_VST3_DEST "bin/vst3")
endif ()

if (PLECTRA_VST3 AND EXISTS "${PLECTRA_VST3}")
    file(GLOB PLECTRA_VST3_BUNDLES "${PLECTRA_VST3}/*.vst3")
    foreach (bundle ${PLECTRA_VST3_BUNDLES})
        install (DIRECTORY "${bundle}"
            DESTINATION ${PLECTRA_VST3_DEST}
            USE_SOURCE_PERMISSIONS
            )
    endforeach ()
endif ()
'''
s = s[:anchor.end()] + block.replace("RESOURCES_DEST", res.group(1)) + s[anchor.end():]
open(path, 'w').write(s)
print("   share: the bundled VST3, per platform, mac under %s" % res.group(1))
PY
    changed=1
fi

[ "$changed" = 1 ] || echo "   nothing to do, the rules are in place"
