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

#include "reader.h"
#include "reader-v0xa6.h"
#include "reader-v0xc2.h"
#include "reader-v0xc4.h"

#include "log.h"

namespace mu::iex::enc {

// Maps magic byte to format reader; add new format cases here.
std::unique_ptr<EncFormatReader> EncFormatReader::create(quint8 magic)
{
    switch (magic) {
    case 0xA6:
        return std::make_unique<EncFormatReader_V0xA6>();
    case 0xC2:
        return makeFormatReader_V0xC2();
    case 0xC4:
        return makeFormatReader_V0xC4();
    default:
        LOGW() << QString("Encore: unsupported format version 0x%1 - import may fail")
                      .arg(magic, 2, 16, QChar('0'));
        return makeFormatReader_V0xC4();
    }
}

} // namespace mu::iex::enc
