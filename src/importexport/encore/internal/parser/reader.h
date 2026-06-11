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
    // Byte offset where element block begins in a MEAS block. See ENCORE_FORMAT.md §Known quirks.
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

    // True if the candidate REST is a duplicate and should be dropped.
    virtual bool deduplicateRest(std::vector<std::unique_ptr<EncMeasureElem> >& elements,
                                 EncMeasureElem* candidate) const
    {
        (void)elements;
        (void)candidate;
        return false;
    }

    // Byte stride past one element block.
    virtual qint64 elemSpacing(qint64 rawSize) const { return rawSize; }

    // True when the stream is too close to measEnd for another element.
    virtual bool isMeasureNearEnd(QDataStream& ds, qint64 measEnd) const
    {
        (void)ds;
        (void)measEnd;
        return false;
    }

    // Byte offset where the file header ends; first block starts here.
    // See ENCORE_FORMAT.md §Known quirks for per-version values.
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

    // True when TK instrument names need UTF-16 probe.
    virtual bool probeInstrumentEncoding() const { return false; }

    // Reads Key transposition from TK content (v0xA6). See ENCORE_FORMAT.md §Instrument block.
    virtual void readKeyFromTKBlock(EncInstrument& /*instr*/,
                                    QDataStream& /*ds*/,
                                    qint64 /*contentStart*/) const {}

    // Format capability queries — see ENCORE_FORMAT.md §Known quirks for per-version details.
    virtual bool hasGraceTimeBorrowing() const { return false; }  // v0xA6: grace borrows rdur from next note
    virtual bool supportsImpliedTuplets() const { return false; }  // v0xC2: tuplet by rdur/fv mismatch
    virtual bool usesG1LowTieSender() const { return false; }      // v0xC2: grace1 low nibble = tie-sender
    virtual bool alMezuroIsReliable() const { return true; }       // v0xC2=false: alMezuro has no valid measure-count semantics
    // True in v0xC2: standalone ORN tipo 0xC4 encodes accent above (not up-bow).
    // In v0xC4, accent is in NOTE articulationByte 0x12; ORN 0xC4 = up-bow.
    virtual bool ornC4IsAccent() const { return false; }
    virtual const char* formatName() const { return "v0xC4"; }    // for logging
    // Bytes to skip between kie (byte +10) and text. v0xC4=9 (text at +20), v0xC2=7 (text at +18).
    virtual quint8 lyricTextGapAfterKie() const { return 9; }

    virtual ~EncFormatReader() = default;

    // Factory: returns the reader for the given magic byte (chuMagio).
    static std::unique_ptr<EncFormatReader> create(quint8 magic);
};
} // namespace mu::iex::encore

#endif // MU_IMPORTEXPORT_ENC_PARSER_READER_H
