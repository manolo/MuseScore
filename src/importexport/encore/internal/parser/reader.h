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

#ifndef MU_IMPORTEXPORT_ENC_PARSER_READER_H
#define MU_IMPORTEXPORT_ENC_PARSER_READER_H

#include <memory>
#include <vector>

#include <QtGlobal>
#include "elements.h"

class QDataStream;

namespace mu::iex::encore {
struct EncMeasureElem;
struct EncInstrument;
struct EncFile;

// EncFormatReader: per-format binary parsing strategy. Register a new version in EncFormatReader::create().
struct EncFormatReader
{
    // Byte offset from measure start where the element block begins.
    // v0xA6: 0x1A   |   v0xC2/v0xC4: 0x36
    virtual quint32 elemBlockOffset() const = 0;

    // Apply format-specific fixups; return true to drop the element (duplicate suppression).
    virtual bool postProcessElement(EncMeasureElem* elem,
                                    QDataStream& ds,
                                    qint64 rawElemStart) const
    {
        (void)elem;
        (void)ds;
        (void)rawElemStart;
        return false;
    }

    // Return true if the candidate REST was a duplicate and should be dropped.
    virtual bool deduplicateRest(std::vector<std::unique_ptr<EncMeasureElem> >& elements,
                                 EncMeasureElem* candidate) const
    {
        (void)elements;
        (void)candidate;
        return false;
    }

    // Byte stride to advance past one element block.
    // v0xA6: rawSize * 2   |   v0xC2/v0xC4: rawSize
    virtual qint64 elemSpacing(qint64 rawSize) const { return rawSize; }

    // True when the stream is too close to measEnd for another element.
    virtual bool isMeasureNearEnd(QDataStream& ds, qint64 measEnd) const
    {
        (void)ds;
        (void)measEnd;
        return false;
    }

    // -----------------------------------------------------------------------
    // Called from EncHeader::read()
    // -----------------------------------------------------------------------

    // Byte offset where the file header ends (first block starts here).
    // v0xA6: 0xA6   |   v0xC2/v0xC4: 0xC2
    virtual qint64 headerEnd() const { return 0xC2; }

    // Read MIDI program, Key, and name metadata stored outside TK blocks.
    virtual bool readInstrumentMeta(std::vector<EncInstrument>& instruments,
                                    QDataStream& ds,
                                    const EncFile& file) const
    {
        (void)instruments;
        (void)ds;
        (void)file;
        return true;
    }

    // True when TK names need UTF-16 probe (v0xC4 only; v0xA6/v0xC2 use Latin-1).
    virtual bool probeInstrumentEncoding() const { return false; }

    // v0xA6: reads Key transposition from TK content offset +42. See ENCORE_FORMAT.md §Instrument block.
    virtual void readKeyFromTKBlock(EncInstrument& /*instr*/,
                                    QDataStream& /*ds*/,
                                    qint64 /*contentStart*/) const {}

    // -----------------------------------------------------------------------
    // Format capability queries (used by importer, not parser)
    // -----------------------------------------------------------------------

    // v0xA6 only: grace notes borrow real duration from the next note.
    virtual bool hasGraceTimeBorrowing() const { return false; }

    // v0xC2 only: tuplet membership implied by rdur/faceValue mismatch (no explicit tup byte).
    virtual bool supportsImpliedTuplets() const { return false; }

    // v0xC2 only: grace1 low nibble encodes tie-sender for live-recording scores.
    virtual bool usesG1LowTieSender() const { return false; }

    virtual ~EncFormatReader() = default;

    // Factory: returns the reader for the given magic byte (chuMagio).
    static std::unique_ptr<EncFormatReader> create(quint8 magic);
};
} // namespace mu::iex::encore

#endif // MU_IMPORTEXPORT_ENC_PARSER_READER_H
