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

#include <memory>
#include <vector>

#include <QString>

#include "elements-enums.h"
#include "elements-note.h"     // MeasureElemVec, EncMeasureElem
#include "elements-measure.h"  // EncMeasure (used in EncFile::measures)
#include "elements-text.h"     // EncLyric, EncTie, EncChordSym

namespace mu::iex::encore {

struct EncFormatReader;   // defined in reader.h

// ---------------------------------------------------------------------------
// Instrument / part
// ---------------------------------------------------------------------------

struct EncInstrument {
    QString name;
    quint32 offset    { 0 };
    qint64 contentFilePos { -1 };   // byte offset of TK content start (after 8-byte header); -1 for compact
    int nstaves   { 0 };
    int midiProgram { 0 };   // 1-indexed GM program (0 = not configured)
    bool showStaff { true }; // false = hidden in score (Encore "Show" flag)
    // Signed chromatic offset from Encore's Staff Sheet "Key" field.
    // 0=written, -12=octave lower, +12=octave higher. v0xC4 only.
    qint8 keyTransposeSemitones { 0 };

    EncCharSize charSize() const { return (offset > 250) ? EncCharSize::TWO_BYTES : EncCharSize::ONE_BYTE; }

    bool read(QDataStream& ds, quint32 vs, bool probeEncoding = false);
};

// ---------------------------------------------------------------------------
// Staff data within a system line
// ---------------------------------------------------------------------------

struct EncLineStaffData {
    EncClefType clef       { EncClefType::G };
    quint8 key        { 0 };
    quint8 pageIdx    { 0 };
    EncStaffType staffType  { EncStaffType::MELODY };
    quint8 instrStaffIdx { 0 };
    bool showStaff { true }; // byte +19 of LINE staff entry: 0x01 = visible, 0x00 = hidden.

    unsigned int instrumentIndex() const { return instrStaffIdx & 0x3F; }
    unsigned int staffIndex() const { return instrStaffIdx >> 6; }

    bool read(QDataStream& ds);
};

struct EncLine {
    quint32 offset       { 0 };
    quint16 start        { 0 };
    quint8 measureCount { 0 };
    std::vector<EncLineStaffData> staffData;

    bool read(QDataStream& ds, quint32 vs, int staffPerSystem);
};

// ---------------------------------------------------------------------------
// Title block
// ---------------------------------------------------------------------------

QString readTextItem(QDataStream& ds, EncCharSize cs);

struct EncHeaderFooter {
    QString text;
    EncTextAlign align { EncTextAlign::LEFT };
};

struct EncTitle {
    QString title;
    std::vector<QString> subtitle;
    std::vector<QString> instruction;
    std::vector<QString> author;
    std::vector<EncHeaderFooter> header;
    std::vector<EncHeaderFooter> footer;
    std::vector<QString> copyright;

    bool read(QDataStream& ds, quint32 vs, EncCharSize cs);
};

// ---------------------------------------------------------------------------
// File header
// ---------------------------------------------------------------------------

struct EncHeader {
    QString magic;
    quint8 chuMagio       { 0 };
    quint16 chuVersio      { 0 };
    quint16 nekon1         { 0 };
    quint16 fiksa1         { 0 };
    qint16 lineCount      { 0 };
    qint16 pageCount      { 0 };
    qint8 instrumentCount{ 0 };
    qint8 staffPerSystem { 0 };
    qint16 measureCount   { 0 };
    quint8 scoreSize      { 4 };  // staff-size selector 1-4 at header offset 0x52; 4 = default

    bool readMagicAndVersion(QDataStream& ds);
    bool read(QDataStream& ds, const EncFormatReader& fmt);
};

// ---------------------------------------------------------------------------
// EncFile - top-level container
// ---------------------------------------------------------------------------

bool isInstrumentMagic(const QString& magic);
bool isKnownMagic(const QString& magic);
QString findNextKnownMagic(QDataStream& ds);
void addSpannerEnds(std::vector<EncMeasure>& measures);

// TEXT block: N-th entry referenced by ORN tind byte (+32). See ENCORE_FORMAT.md §TEXT block.
struct EncTextBlock {
    std::vector<QString> entries;

    bool read(QDataStream& ds, quint32 varSize);
};

// WINI block: margins in points (1/72 inch). See ENCORE_FORMAT.md §WINI block.
struct EncPageSetup {
    bool hasData      { false };
    qint32 top        { 0 };   // top margin in pts
    qint32 left       { 0 };   // left margin in pts
    qint32 bottomEdge { 0 };   // pageHeight_pts - bottomMargin_pts
    qint32 rightEdge  { 0 };   // pageWidth_pts  - rightMargin_pts
};

struct EncFile {
    EncHeader header;
    std::vector<EncInstrument> instruments;
    std::vector<EncLine> lines;
    std::vector<EncMeasure> measures;
    EncTitle titleBlock;
    EncTextBlock textBlock;
    EncPageSetup pageSetup;
    std::unique_ptr<struct EncFormatReader> fmt;  // set during read()

    bool read(QDataStream& ds);
};

} // namespace mu::iex::encore
