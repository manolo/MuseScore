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

#pragma once

#include <QString>

#include "elements-note.h"

namespace mu::iex::encore {

struct EncChordSym : EncMeasureElem {
    quint8 toniko { 0 };
    quint8 tipo   { 0 };
    quint8 radiko { 0 };
    quint8 baso   { 0 };
    QString teksto;

    using EncMeasureElem::EncMeasureElem;

    bool read(QDataStream& ds) override;
    QString chordName() const;
};

struct EncLyric : EncMeasureElem {
    QString text;
    quint8 kie { 0 };                // location/anchor byte (similar to xoffset)
    quint8 textGapAfterKie { 9 };   // bytes to skip after kie before text; set from EncFormatReader

    using EncMeasureElem::EncMeasureElem;

    bool read(QDataStream& ds) override;
};

// TIE element: dir byte (+5) and startFlag (+6) encode arc direction. See ENCORE_FORMAT.md §TIE element.
struct EncTie : EncMeasureElem {
    bool isTieStart { false };      // true when dir byte has bit 7 or bit 1 set, or startFlag has bit 7 set
    quint8 arcX1         { 0 };     // arc start x (element offset +10); only valid for size >= 18
    quint8 arcX2         { 0 };     // arc end   x (element offset +12); only valid for size >= 18
    qint8 sourcePosition { -1 };    // staff position of source note (+14); -1 = all notes in chord

    using EncMeasureElem::EncMeasureElem;

    bool read(QDataStream& ds) override;
};

} // namespace mu::iex::encore
