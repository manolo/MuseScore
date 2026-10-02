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

if (MUE_INSTALL_SOUNDFONT)


    # The fork's own voices. Nothing has to register them: the soundfont scan
    # is recursive over the resources directory and filters *.sf2 and *.sf3.
    file(GLOB PLECTRA_SOUNDFONTS "${CMAKE_CURRENT_SOURCE_DIR}/*-Con-Tremolo.sf2")
    if (PLECTRA_SOUNDFONTS)
        install(FILES ${PLECTRA_SOUNDFONTS}
            DESTINATION ${MUSE_APP_INSTALL_RESOURCES_LOCATION}/sound
            )
    endif ()

    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/MS Basic.sf3")
      install (FILES FluidR3Mono_GM.sf3
          DESTINATION ${MUSE_APP_INSTALL_RESOURCES_LOCATION}/sound
          RENAME "MS Basic.sf3"
          )
    else ()
      install(FILES
          "MS Basic.sf3"
          DESTINATION ${MUSE_APP_INSTALL_RESOURCES_LOCATION}/sound
          )
    endif ()

    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/MS Basic_License.md")
      install (FILES FluidR3Mono_License.md
          DESTINATION ${MUSE_APP_INSTALL_RESOURCES_LOCATION}/sound
          RENAME "MS Basic_License.md")
    else ()
      install(FILES
          "MS Basic_License.md"
          DESTINATION ${MUSE_APP_INSTALL_RESOURCES_LOCATION}/sound
          )
    endif ()

endif (MUE_INSTALL_SOUNDFONT)
