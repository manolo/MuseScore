# SPDX-License-Identifier: GPL-3.0-only
# MuseScore-Studio-CLA-applies
#
# MuseScore Studio
# Music Composition & Notation
#
# Copyright (C) 2026 MuseScore Limited and others
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
# along with this program. If not, see <https://www.gnu.org/licenses/>.

set(MODULE_TEST muse_vst_test)

# The vst headers include their siblings unprefixed (e.g. vstsequencer.h -> "vsttypes.h"), so the
# module directory must be on the include path, like it is when the module itself is built.
set(MODULE_TEST_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/..)

set(MODULE_TEST_SRC
    ${CMAKE_CURRENT_LIST_DIR}/vstsequencertest.cpp
    ${MUSE_FRAMEWORK_SRC_PATH}/mpe/tests/utils/articulationutils.h
    )

set(MODULE_TEST_LINK muse_vst muse_audio muse_uicomponents muse_mpe vst3sdk)

include(SetupGTest)
