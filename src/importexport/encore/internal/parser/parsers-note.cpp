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

#include "elem-note.h"

namespace mu::iex::enc {
bool EncMeasureElem::read(QDataStream& ds)
{
    quint8 rawStaff;
    ds >> size >> rawStaff;
    staffIdx    = rawStaff & 0x3F;
    staffWithin = rawStaff >> 6;
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
    // No trailing skip to the element end: the measure element loop reseeks to
    // elemStart + elemSpacing(size) after every read(), so any remaining bytes are
    // skipped there. The same applies to the other element readers below.
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
    if (static_cast<int>(size) > 15) {
        ds >> mrestCount;   // multi-measure rest count at element offset +15
        if (mrestCount < 1) {
            mrestCount = 1;
        }
    }
    return true;
}

bool EncKeyChange::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);
    ds >> tipo;
    return true;
}

bool EncClefChange::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);
    qint8 ct;
    ds >> ct;
    clefType = static_cast<EncClefType>(ct);
    return true;
}

bool EncGenericElem::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);
    return true;
}

bool EncMidiCc::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);   // consumes size (d[3]) + rawStaff (d[4]); ds now at d[5]
    // d[5] CC marker, d[6..9] zeros, d[10] controller, d[11] value. Only present when the
    // element is the full 12 bytes; the measure loop reseeks to elemStart+elemSpacing(size)
    // afterwards, so a short/garbage element stays aligned with controller/value left at 0.
    if (size >= 12) {
        ds.skipRawData(5);
        ds >> controller >> value;
    }
    return true;
}
} // namespace mu::iex::enc
