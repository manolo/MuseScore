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
    enc-module.cpp
    enc-module.h
    internal/parser/elem-note.cpp
    internal/parser/elem-chord.cpp
    internal/parser/elem-ornament.cpp
    internal/parser/elem-lyric-tie.cpp
    internal/parser/elem-measure.cpp
    internal/parser/elem-metadata.cpp
    internal/parser/elem-text.cpp
    internal/parser/elem-file.cpp
    internal/parser/encoding.h
    internal/parser/encoding.cpp
    internal/parser/reader.h
    internal/parser/reader.cpp
    internal/parser/reader-v0xa6.h
    internal/parser/reader-v0xa6.cpp
    internal/parser/reader-v0xc4.h
    internal/parser/reader-v0xc4.cpp
    internal/parser/elements.h
    internal/importer/mapping.cpp
    internal/importer/mapping.h
    internal/parser/ticks.cpp
    internal/parser/ticks.h
    internal/importer/tuplets.cpp
    internal/importer/tuplets.h
    internal/notationencreader.cpp
    internal/notationencreader.h
    internal/importer/import.cpp
    internal/importer/ctx.h
    internal/importer/builders.h
    internal/importer/builders.cpp
    internal/importer/noteloop-internal.h
    internal/importer/noteloop-note.cpp
    internal/importer/noteloop-rest.cpp
    internal/importer/noteloop-orn.cpp
    internal/importer/noteloop.cpp
    internal/importer/resolvers.h
    internal/importer/resolvers.cpp
    internal/importer/resolvers-slur.cpp
    internal/importer/resolvers-hairpin.cpp
    internal/importer/resolvers-ornaments.cpp
    internal/importer/resolvers-fingering.cpp
    internal/importer/import.h
)

target_link_libraries(iex_encore PRIVATE engraving)

if (MUE_BUILD_IMPORTEXPORT_TESTS)
    add_subdirectory(tests)
endif()
