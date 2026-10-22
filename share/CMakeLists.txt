# SPDX-License-Identifier: GPL-3.0-only
# MuseScore-Studio-CLA-applies
#
# MuseScore Studio
# Music Composition & Notation
#
# Copyright (C) 2021 MuseScore Limited and others
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

subdirs(testflowscripts icons templates wallpapers locale styles sound manual instruments)

install (DIRECTORY
    plugins
    DESTINATION ${MUSE_APP_INSTALL_RESOURCES_LOCATION}
    PATTERN ".*" EXCLUDE
    )

install (DIRECTORY
    extensions
    DESTINATION ${MUSE_APP_INSTALL_RESOURCES_LOCATION}
    PATTERN ".*" EXCLUDE
    )

install (DIRECTORY
    workspaces
    DESTINATION ${MUSE_APP_INSTALL_RESOURCES_LOCATION}
    PATTERN ".*" EXCLUDE
    )

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
    string(REGEX REPLACE "Resources/?$" "VST3" PLECTRA_VST3_DEST "${MUSE_APP_INSTALL_RESOURCES_LOCATION}")
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
