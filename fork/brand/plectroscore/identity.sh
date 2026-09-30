# The identity of the alternative build, kept alive to be shown beside the
# other one rather than argued about.
#
# The artwork is the same pick: the two brands differ in their name and in
# nothing else, which is the whole point of being able to see them together.

APP_NAME="PlectroScore"
APP_SLUG="plectroscore"
APP_ATTRIBUTION="based on MuseScore Studio"
APP_LINK="github.com/manolo/MuseScore"

# Two bundles sharing an identifier confuse LaunchServices: `open -b` becomes
# ambiguous and a double click on a score may start either one. This one gets
# a suffix, so the everyday build keeps the identifier it already has.
#
# It costs nothing else. Paths and settings come from the Qt application name,
# not from this, so both brands still share scores, plugins and preferences.
APP_BUNDLE_ID_SUFFIX=".plectroscore"
