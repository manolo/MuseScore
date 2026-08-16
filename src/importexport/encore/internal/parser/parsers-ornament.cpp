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

// Read ORNAMENT elements (slurs, wedges, staff text, etc.): geometry, spanning measure counts, tind.

#include "elem-ornament.h"

namespace mu::iex::enc {
bool EncOrnament::read(QDataStream& ds)
{
    const qint64 elemPos = ds.device()->pos();   // first byte after the type/voice byte; see ENCORE_FORMAT.md §Ornament subtypes
    EncMeasureElem::read(ds);
    ds >> tipo;
    // Everything from +8 onward moved two bytes later in Encore 4.0; bodyShift folds the older
    // layout into this one read. See ENCORE_FORMAT.md §Ornament element.
    ds.skipRawData(4 + bodyShift);
    ds >> xoffset;
    ds.skipRawData(1);
    ds >> yoffset;
    ds.skipRawData(2);
    ds >> altMezuro;   // +16: v0xC2 spanning measure-count
    ds.skipRawData(1);
    ds >> alMezuro;    // +18: v0xC4 spanning measure-count
    ds.skipRawData(1);
    ds >> xoffset2;
    ds.skipRawData(5);
    ds >> speguleco;
    speguleco &= 3;
    ds.skipRawData(1);
    ds >> noto;
    ds.skipRawData(1);
    ds >> tempo;
    // The text index has its own slot only in a long enough element; otherwise it shares the tempo
    // byte. The threshold moves with the body layout, so a pre-4.0 size-32 staff text still reaches
    // its own slot. See ENCORE_FORMAT.md §Ornament subtypes.
    if (static_cast<int>(size) >= 33 + bodyShift) {
        ds.skipRawData(1);
        ds >> tind;
    } else {
        tind = tempo;
    }
    // A compact ornament (v0xA6) does not follow the field order read above, so the fields it
    // places elsewhere are re-read from their own offsets. Scope each seek to the device so an
    // ornament near EOF cannot desync the element loop.
    const qint64 elemStart = elemPos - 3;   // elemPos sits just past the type/voice byte, at +3
    auto readAt = [&](int elemOffset, auto& dest) {
        const qint64 pos = elemStart + elemOffset;
        if (pos >= 0 && pos < ds.device()->size()) {
            ds.device()->seek(pos);
            ds >> dest;
        }
    };

    // The staff-text TEXT index is the one compact field that is subtype-specific.
    if (tindOffset >= 0 && ornType() == EncOrnamentType::STAFFTEXT) {
        readAt(tindOffset, tind);
    }
    // Vertical placement is a signed byte in the compact ornament, not the s16 read above, and it
    // applies to every subtype: dynamics placement and the fermata above/below pair need it too.
    if (yByteOffset >= 0) {
        qint8 y = 0;
        readAt(yByteOffset, y);
        yoffset = y;
    }
    // Forward measure count, likewise for every subtype.
    if (measCountOffset >= 0) {
        readAt(measCountOffset, alMezuro);
    }
    // No trailing skip: the element loop reseeks to the element end after read().
    return true;
}
} // namespace mu::iex::enc
