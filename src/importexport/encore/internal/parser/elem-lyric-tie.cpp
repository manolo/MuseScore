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

#include "elements-text.h"
#include "encoding.h"

namespace mu::iex::enc {

bool EncLyric::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);   // consumed: size + staffIdx (5 bytes from elemStart)

    // See ENCORE_FORMAT.md §Lyric element for field offsets and encoding detection.
    // textGapAfterKie is 9 for v0xC4 (text at +20) and 7 for v0xC2 (text at +18).
    const int fixedReads = 5 + 1 + static_cast<int>(textGapAfterKie);
    int remaining = static_cast<int>(size) - 5;
    if (remaining < fixedReads) {
        if (remaining > 0) {
            ds.skipRawData(remaining);
        }
        return true;
    }

    ds.skipRawData(5);
    ds >> kie;
    ds.skipRawData(textGapAfterKie);
    remaining -= fixedReads;

    text = readEncodedStringRemaining(ds, remaining);

    if (remaining > 0) {
        ds.skipRawData(remaining);
    }
    return true;
}

bool EncTie::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);   // reads size + staffIdx
    quint8 dirByte = 0;
    quint8 startFlag = 0;
    if (size > 5) {
        ds >> dirByte;          // tie arc direction byte at element offset +5
    }
    if (size > 6) {
        ds >> startFlag;        // tie-start flag at element offset +6
    }
    // Dir/startFlag bit layout: see ENCORE_FORMAT.md §TIE element.
    isTieStart = ((dirByte & 0x80) != 0) || ((startFlag & 0x80) != 0) || ((dirByte & 0x02) != 0);

    if (static_cast<int>(size) >= 18) {
        // Read arc x-positions at offsets +10 and +12. When arcX1 == arcX2 the arc has
        // zero horizontal extent. Two cases share this pattern:
        //   (a) intra-chord decorative arc (Encore connects two chord notes vertically,
        //       startFlag = 0x00) - must NOT become a forward tie.
        //   (b) cross-measure tie where the destination is in the next measure and Encore
        //       stores arcX2 = arcX1 as a placeholder (startFlag = 0x80) - IS a real tie.
        // Override isTieStart only when startFlag bit 7 is NOT set (case a).
        ds.skipRawData(3);          // skip offsets +7,+8,+9
        ds >> arcX1;                // offset +10
        ds.skipRawData(1);          // skip offset +11
        ds >> arcX2;                // offset +12
        if (arcX1 == arcX2 && (startFlag & 0x80) == 0) {
            isTieStart = false;
        }
        // Byte +13 unused; byte +14 = staff position of source note (see ENCORE_FORMAT.md §TIE element).
        ds.skipRawData(1);          // skip offset +13
        quint8 sp = 0;
        ds >> sp;                   // offset +14
        sourcePosition = static_cast<qint8>(sp);
        const int toSkip = static_cast<int>(size) - 15;  // skip offsets +15..end
        if (toSkip > 0) {
            ds.skipRawData(toSkip);
        }
    } else {
        const int consumed = (size > 5 ? 1 : 0) + (size > 6 ? 1 : 0);
        const int toSkip = static_cast<int>(size) - 5 - consumed;
        if (toSkip > 0) {
            ds.skipRawData(toSkip);
        }
    }
    return true;
}

} // namespace mu::iex::enc
