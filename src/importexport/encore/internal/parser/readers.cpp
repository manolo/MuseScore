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

#include "readers.h"
#include "readers-v0xa6.h"
#include "readers-v0xc2.h"
#include "readers-v0xc4.h"

#include "log.h"

namespace mu::iex::enc {

// Selects a format reader. SCO5 (macOS Encore 5) is identified by its magic string rather than
// chuMagio (which is not 0xC4 in those files); it shares the v0xC4 binary format and differs
// only in page-margin handling. Otherwise the format byte (chuMagio) picks the reader; add new
// format cases here.
std::unique_ptr<EncFormatReader> EncFormatReader::create(quint8 chuMagio, const QString& magic)
{
    if (magic == "SCO5") {
        return makeFormatReader_SCO5();
    }
    switch (chuMagio) {
    case static_cast<quint8>(EncFormatVersion::V2_X):
        return std::make_unique<EncFormatReader_V0xA6>();
    case static_cast<quint8>(EncFormatVersion::V3_4_X):
        return makeFormatReader_V0xC2();
    case static_cast<quint8>(EncFormatVersion::V5_X):
        return makeFormatReader_V0xC4();
    default:
        LOGW() << QString("Encore: unsupported format version 0x%1 - import may fail")
                      .arg(chuMagio, 2, 16, QChar('0'));
        return makeFormatReader_V0xC4();
    }
}

} // namespace mu::iex::enc
