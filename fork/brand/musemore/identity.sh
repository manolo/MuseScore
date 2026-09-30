# The identity of the everyday build.
#
# Sourced by the branding fixups and by nothing else. One file per brand, and
# this is the only place any of these strings is written down: the name moved
# once already, from PlectroScore, and that cost an edit in eleven files.
#
# What is NOT here, deliberately: MUSE_APP_NAME_MACHINE_READABLE. That string
# drives QCoreApplication::setApplicationName and through it every path, so it
# stays upstream's for every brand. See src/app/main.cpp.

APP_NAME="MuseMore"
APP_SLUG="musemore"
APP_ATTRIBUTION="based on MuseScore Studio"
APP_LINK="github.com/manolo/MuseScore"

# Appended to the bundle identifier so macOS can tell two installed brands
# apart. Empty here on purpose: this is the build actually used, and keeping
# upstream's identifier keeps the permissions macOS has already granted it.
APP_BUNDLE_ID_SUFFIX=""
