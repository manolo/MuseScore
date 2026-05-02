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
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

muse_create_module(iex_encore)

target_sources(iex_encore PRIVATE
    encoremodule.cpp
    encoremodule.h
    internal/encoreelements.cpp
    internal/encoreelements.h
    internal/encoremapping.cpp
    internal/encoremapping.h
    internal/encorerhythm.cpp
    internal/encorerhythm.h
    internal/encoretuplets.cpp
    internal/encoretuplets.h
    internal/notationencorereader.cpp
    internal/notationencorereader.h
    internal/importencore.cpp
    internal/importencore.h
)

target_link_libraries(iex_encore PRIVATE engraving)

if (MUE_BUILD_IMPORTEXPORT_TESTS)
    add_subdirectory(tests)
endif()
