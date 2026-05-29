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

// ---------------------------------------------------------------------------
// EncFormatReader: strategy interface for per-format binary parsing.
//
// Each format version implements this once. To add new format:
//   1. Create reader-vXXXX.cpp/h implementing EncFormatReader.
//   2. Register the magic byte in EncFormatReader::create().
// ---------------------------------------------------------------------------
struct EncFormatReader
{
    // -----------------------------------------------------------------------
    // Called from EncMeasure::read()
    // -----------------------------------------------------------------------

    // Byte offset from measure start where the element block begins.
    // v0xA6: 0x1A   |   v0xC2/v0xC4: 0x36
    virtual quint32 elemBlockOffset() const = 0;

    // Apply format-specific fixups to a just-parsed element.
    // Returns true if the element should be dropped (duplicate suppression).
    // raw_elem_start is the stream position at the start of this element.
    virtual bool postProcessElement(EncMeasureElem* elem,
                                    QDataStream& ds,
                                    qint64 rawElemStart) const
    {
        (void)elem; (void)ds; (void)rawElemStart;
        return false;
    }

    // Deduplicate the last two elements if the format requires it.
    // Returns true if the most-recently-added element was dropped.
    virtual bool deduplicateRest(std::vector<std::unique_ptr<EncMeasureElem> >& elements,
                                 EncMeasureElem* candidate) const
    {
        (void)elements; (void)candidate;
        return false;
    }

    // Byte stride to advance past one element block.
    // v0xA6: rawSize * 2   |   v0xC2/v0xC4: rawSize
    virtual qint64 elemSpacing(qint64 rawSize) const { return rawSize; }

    // True when the stream position is close enough to measEnd that the
    // element loop should stop early.
    virtual bool isMeasureNearEnd(QDataStream& ds, qint64 measEnd) const
    {
        (void)ds; (void)measEnd;
        return false;
    }

    // -----------------------------------------------------------------------
    // Called from EncFile::read() (instrument metadata post-processing)
    // -----------------------------------------------------------------------

    // Read metadata (MIDI programs, Key, name recovery) stored outside TK blocks.
    virtual bool readInstrumentMeta(std::vector<EncInstrument>& instruments,
                                    QDataStream& ds,
                                    const EncFile& file) const
    {
        (void)instruments; (void)ds; (void)file;
        return true;
    }

    // True when TK names need UTF-16 probe (v0xC4 only; v0xA6/v0xC2 use Latin-1).
    virtual bool probeInstrumentEncoding() const { return false; }

    // Read format-specific data from the TK block before EncInstrument::read().
    // v0xA6: Key transposition at content offset +42.
    // v0xC4: no per-block key reading (handled in readInstrumentMeta).
    virtual void readKeyFromTKBlock(EncInstrument& /*instr*/,
                                    QDataStream& /*ds*/,
                                    qint64 /*contentStart*/) const {}

    virtual ~EncFormatReader() = default;

    // Factory: returns the reader for the given magic byte (chuMagio).
    static std::unique_ptr<EncFormatReader> create(quint8 magic);
};

} // namespace mu::iex::encore

#endif // MU_IMPORTEXPORT_ENC_PARSER_READER_H
