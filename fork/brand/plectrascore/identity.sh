# The identity of the fork.
#
# Sourced by the branding fixups and by nothing else. One file per brand, and
# this is the only place any of these strings is written down.
#
# What is NOT here, deliberately: MUSE_APP_NAME_MACHINE_READABLE. That string
# drives QCoreApplication::setApplicationName and through it every path, so it
# stays upstream's. See src/app/main.cpp.

APP_NAME="PlectraScore"
APP_SLUG="plectrascore"
APP_ATTRIBUTION="based on MuseScore Studio"
APP_LINK="github.com/manolo/MuseScore"

# What the build is for, shown on the loading screen and in the about box.
# One short line that fits beside the pick, and a sentence under it.
APP_TAGLINE="A MuseScore fork for pulso y púa ensembles"
APP_BLURB="Bandurria, laúd and guitar, in the Spanish and Latin American tradition."

# Appended to the bundle identifier so macOS can tell two installed brands
# apart. Empty here: this is the build people actually install, and keeping
# upstream's identifier keeps the permissions macOS has already granted it.
APP_BUNDLE_ID_SUFFIX=""
