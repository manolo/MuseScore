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

#include "elements.h"

#include "encoding.h"
#include "ticks.h"

namespace mu::iex::encore {
// ---------------------------------------------------------------------------
// EncMeasureElem and derived element types
// ---------------------------------------------------------------------------

bool EncMeasureElem::read(QDataStream& ds)
{
    ds >> size >> staffIdx;
    staffIdx &= 0x3F;
    return true;
}

EncGraceType EncNote::graceType() const
{
    quint8 g1 = grace1 & 0x30;
    quint8 g2 = grace2 & 0x05;
    if (g1 == 0x20 && g2 == 0x04) {
        return EncGraceType::ACCIACCATURA;
    }
    if (g1 > 0x10 && g2 != 0x01) {
        return EncGraceType::APPOGGIATURA;
    }
    return EncGraceType::NORMAL;
}

bool EncNote::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);

    ds >> faceValue >> grace1 >> grace2;
    ds.skipRawData(2);
    ds >> xoffset;
    ds.skipRawData(1);
    ds >> position >> tuplet >> dotControl >> semiTonePitch >> playbackDurTicks;
    ds.skipRawData(1);
    ds >> velocity >> options >> alterationGlyph;
    ds.skipRawData(2);
    ds >> articulationUp;
    ds.skipRawData(1);
    ds >> articulationDown;
    int toSkip = static_cast<int>(size) - 27;
    if (toSkip > 0) {
        ds.skipRawData(toSkip);
    }
    return true;
}

bool EncRest::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);
    ds >> faceValue;
    ds.skipRawData(4);
    ds >> xoffset;
    ds.skipRawData(2);
    ds >> tuplet >> dotControl;
    int toSkip = static_cast<int>(size) - 10 - 5;
    if (toSkip > 0) {
        ds.skipRawData(toSkip);
    }
    return true;
}

bool EncChordSym::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);
    ds >> toniko >> tipo;
    ds.skipRawData(3);
    ds >> xoffset;
    ds.skipRawData(1);
    ds >> radiko >> baso;
    const bool hasText = (tipo & 1);
    if (hasText) {
        // 36-byte slot; UTF-16 LE in modern files, Latin-1 in legacy scores.
        teksto = readEncodedStringFixed(ds, 36);
        int toSkip = static_cast<int>(size) - 5 - 9 - 36;
        if (toSkip > 0) {
            ds.skipRawData(toSkip);
        }
    } else {
        int toSkip = static_cast<int>(size) - 5 - 9;
        if (toSkip > 0) {
            ds.skipRawData(toSkip);
        }
    }
    return true;
}

bool EncOrnament::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);
    ds >> tipo;
    ds.skipRawData(4);
    ds >> xoffset;
    ds.skipRawData(1);
    ds >> yoffset;
    ds.skipRawData(4);
    ds >> alMezuro;
    ds.skipRawData(1);
    ds >> xoffset2;
    ds.skipRawData(5);
    ds >> speguleco;
    speguleco &= 3;
    ds.skipRawData(1);
    ds >> noto;
    ds.skipRawData(1);
    ds >> tempo;
    // v0xC2 size-32: tind overlaps tempo at byte 30. See ENCORE_FORMAT.md §Ornament subtypes.
    if (static_cast<int>(size) >= 33) {
        ds.skipRawData(1);
        ds >> tind;
    } else {
        tind = tempo;
    }
    int toSkip = static_cast<int>(size) - 5 - (static_cast<int>(size) >= 33 ? 28 : 26);
    if (toSkip > 0) {
        ds.skipRawData(toSkip);
    }
    return true;
}

bool EncLyric::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);   // consumed: size + staffIdx (5 bytes from elemStart)

    // See ENCORE_FORMAT.md §Lyric element for field offsets and encoding detection.
    int remaining = static_cast<int>(size) - 5;
    if (remaining < 15) {
        if (remaining > 0) {
            ds.skipRawData(remaining);
        }
        return true;
    }

    ds.skipRawData(5);
    ds >> kie;
    ds.skipRawData(9);
    remaining -= 15;

    text = readEncodedStringRemaining(ds, remaining);

    if (remaining > 0) {
        ds.skipRawData(remaining);
    }
    return true;
}

bool EncKeyChange::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);
    ds >> tipo;
    int toSkip = static_cast<int>(size) - 5 - 1;
    if (toSkip > 0) {
        ds.skipRawData(toSkip);
    }
    return true;
}

bool EncGenericElem::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);
    int toSkip = static_cast<int>(size) - 5;
    if (toSkip > 0) {
        ds.skipRawData(toSkip);
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
    int consumed = (size > 5 ? 1 : 0) + (size > 6 ? 1 : 0);
    int toSkip = static_cast<int>(size) - 5 - consumed;
    if (toSkip > 0) {
        ds.skipRawData(toSkip);
    }
    return true;
}
} // namespace mu::iex::encore
