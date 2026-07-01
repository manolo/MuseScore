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

#include "elem-text.h"
#include "parsers-encoding.h"

namespace mu::iex::enc {
bool EncLyric::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);   // consumes size + rawStaff; 5 bytes read from elemStart (tick + tv too)

    // kie at +preKieSkip, text +textGapAfterKie later, null-terminated within the size*spacingFactor slot.
    const int textOffset = 5 + static_cast<int>(preKieSkip) + 1 + static_cast<int>(textGapAfterKie);
    const int slot = static_cast<int>(size) * static_cast<int>(spacingFactor);
    int remaining = slot - textOffset;
    if (remaining <= 0) {
        return true;   // too short to carry text; element loop reseeks past it
    }

    ds.skipRawData(preKieSkip);
    ds >> kie;
    ds.skipRawData(textGapAfterKie);

    text = readEncodedStringRemaining(ds, remaining);
    // No trailing skip: the element loop reseeks to the element end after read().
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
        // Read arc x-positions at offsets +10 and +12. The arc span is the authoritative
        // forward-tie signal when the element carries it (18-byte form):
        //   - arcX1 <  arcX2: genuine left-to-right horizontal span - a real forward tie,
        //       regardless of the +5 byte. Byte +5 is a signed arc-curvature value
        //       (0x02/0x04 curve down, 0xFC/0xFE curve up), NOT a bitfield; the old
        //       (+5 & 0x80) || (+5 & 0x02) rule caught 0x02/0xFC/0xFE but silently
        //       dropped the equally-valid 0x04 ties.
        //   - arcX1 == arcX2: zero horizontal extent. Two cases share this pattern:
        //       (a) intra-chord decorative arc (Encore connects two chord notes vertically,
        //           startFlag = 0x00) - must NOT become a forward tie.
        //       (b) cross-measure tie where the destination is in the next measure and
        //           Encore stores arcX2 = arcX1 as a placeholder (startFlag = 0x80) - a
        //           real tie. Override isTieStart only when startFlag bit 7 is NOT set (a).
        ds.skipRawData(3);          // skip offsets +7,+8,+9
        ds >> arcX1;                // offset +10
        ds.skipRawData(1);          // skip offset +11
        ds >> arcX2;                // offset +12
        if (arcX1 < arcX2) {
            isTieStart = true;
        } else if (arcX1 == arcX2 && (startFlag & 0x80) == 0) {
            isTieStart = false;
        }
        // Byte +13 unused; byte +14 = staff position of source note (see ENCORE_FORMAT.md §TIE element).
        ds.skipRawData(1);          // skip offset +13
        quint8 sp = 0;
        ds >> sp;                   // offset +14
        sourcePosition = static_cast<qint8>(sp);
    }
    // No trailing skip: the element loop reseeks to the element end after read().
    return true;
}
} // namespace mu::iex::enc
