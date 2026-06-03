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

#ifndef MU_IMPORTEXPORT_ENC_PARSER_READER_V0XA6_H
#define MU_IMPORTEXPORT_ENC_PARSER_READER_V0XA6_H

#include "reader.h"

namespace mu::iex::encore {
// v0xA6 (Encore 2.x) format reader.
// Header ends at 0xA6, element blocks are 10 bytes (not 32 like v0xC4),
// MIDI pitch and tuplet at different offsets, REST dedup required.
struct EncFormatReader_V0xA6 final : EncFormatReader
{
    qint64 headerEnd() const override { return 0xA6; }

    quint32 elemBlockOffset() const override { return 0x1A; }

    bool postProcessElement(EncMeasureElem* elem, QDataStream& ds, qint64 rawElemStart) const override;

    bool deduplicateRest(std::vector<std::unique_ptr<EncMeasureElem> >& elements, EncMeasureElem* candidate) const override;

    qint64 elemSpacing(qint64 rawSize) const override { return rawSize * 2; }

    bool isMeasureNearEnd(QDataStream& ds, qint64 measEnd) const override;

    bool probeInstrumentEncoding() const override { return false; }

    void readKeyFromTKBlock(EncInstrument& instr, QDataStream& ds, qint64 contentStart) const override;

    bool hasGraceTimeBorrowing() const override { return true; }
};
} // namespace mu::iex::encore

#endif // MU_IMPORTEXPORT_ENC_PARSER_READER_V0XA6_H
