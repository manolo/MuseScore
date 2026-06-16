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

#include "elements-ornament.h"

namespace mu::iex::encore {

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

} // namespace mu::iex::encore
