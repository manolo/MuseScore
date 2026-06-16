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

#include "elements-note.h"

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
        ds >> mrestCount;
        --toSkip;
        if (mrestCount < 1) {
            mrestCount = 1;
        }
        if (toSkip > 0) {
            ds.skipRawData(toSkip);
        }
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

} // namespace mu::iex::enc
