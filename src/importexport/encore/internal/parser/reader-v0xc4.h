/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef MU_IMPORTEXPORT_ENC_PARSER_READER_V0XC4_H
#define MU_IMPORTEXPORT_ENC_PARSER_READER_V0XC4_H

#include <memory>
#include "reader.h"

namespace mu::iex::enc {

// Factory helper -- called from EncFormatReader::create().
// Full class definition is in reader-v0xc4.cpp.
std::unique_ptr<EncFormatReader> makeFormatReader_V0xC4();

} // namespace mu::iex::enc

#endif // MU_IMPORTEXPORT_ENC_PARSER_READER_V0XC4_H
