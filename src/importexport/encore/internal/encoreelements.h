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

#ifndef MU_IMPORTEXPORT_ENCOREELEMENTS_H
#define MU_IMPORTEXPORT_ENCOREELEMENTS_H

#include <memory>
#include <map>
#include <vector>

#include <QDataStream>
#include <QString>

namespace mu::iex::encore {

// ---------------------------------------------------------------------------
// Encore binary format data structures
// Ported from Enc2MusicXML (https://github.com/lvinken/Enc2MusicXML)
// by Leon Vinken, GPL v3+
// ---------------------------------------------------------------------------

enum class EncCharSize : char {
    ONE_BYTE,
    TWO_BYTES
};

enum class EncClefType : qint8 {
    ALIA = -1,
    G    = 0,
    F    = 1,
    C3L  = 2,
    C4L  = 3,
    G8P  = 4,
    G8M  = 5,
    F8M  = 6,
    PERC = 7,
    TAB  = 8
};

enum class EncStaffType : quint8 {
    MELODY  = 0,
    TAB     = 1,
    RHYTHM  = 2
};

enum class EncElemType : quint8 {
    NONE      = 0,
    CLEF      = 1,
    KEYCHANGE = 2,
    TIE       = 3,
    BEAM      = 4,
    ORNAMENT  = 5,
    LYRIC     = 6,
    CHORD     = 7,
    REST      = 8,
    NOTE      = 9,
    UNKNOWN1  = 10,
    UNKNOWN2  = 11
};

enum class EncBarlineType : quint8 {
    NORMAL      = 0,
    REPEATSTART = 2,
    DOUBLEL     = 3,
    REPEATEND   = 4,
    FINAL       = 5,
    DOUBLER     = 6
};

enum class EncRepeatType : quint8 {
    NONE     = 0,
    DCALCODA = 0x80,
    DSALCODA = 0x81,
    DCALFINE = 0x82,
    DSALFINE = 0x83,
    DS       = 0x84,
    CODA1    = 0x85,
    FINE     = 0x86,
    DC       = 0x87,
    SEGNO    = 0x88,
    CODA2    = 0x89
};

enum class EncOrnamentType : quint8 {
    NONE       = 0,
    WEDGESTART = 0x1D,
    STAFFTEXT  = 0x1E,
    SLURSTART  = 0x21,
    TEMPO      = 0x32,
    SLURSTOP   = 0x41,
    WEDGESTOP  = 0x4D
};

enum class EncAccidentalType : quint8 {
    NONE    = 0,
    SHARP   = 1,
    FLAT    = 2,
    NATURAL = 3
};

enum class EncGraceType : char {
    NORMAL        = 0,
    ACCIACCATURA  = 1,
    APPOGGIATURA  = 2
};

// ---------------------------------------------------------------------------
// Base class for all measure elements
// ---------------------------------------------------------------------------

struct EncMeasureElem {
    quint16 tick  { 0 };
    quint8 type  { 0 };
    quint8 voice { 0 };
    quint8 size  { 0 };
    quint8 staffIdx { 0 };
    quint8 xoffset  { 0 };
    qint16 realDuration { -1 };

    // Returns the tuplet byte (0 = no tuplet). Used to sort tuplet notes before
    // non-tuplet notes at the same tick, so the tuplet note creates the chord.
    virtual quint8 tupletByte() const { return 0; }

    EncMeasureElem() = default;
    EncMeasureElem(quint16 t, quint8 tp, quint8 v)
        : tick(t), type(tp), voice(v) {}
    virtual ~EncMeasureElem() = default;

    virtual bool read(QDataStream& ds);
};

struct EncNote : EncMeasureElem {
    quint8 faceValue       { 0 };
    quint8 grace1          { 0 };
    quint8 grace2          { 0 };
    qint8 position        { 0 };
    quint8 tuplet          { 0 };
    quint8 dotControl      { 0 };
    quint8 semiTonePitch   { 0 };
    quint16 playbackDurTicks{ 0 };
    quint8 velocity        { 0 };
    quint8 options         { 0 };
    quint8 alterationGlyph { 0 };
    quint8 articulationUp  { 0 };
    quint8 articulationDown{ 0 };

    using EncMeasureElem::EncMeasureElem;

    quint8 tupletByte() const override { return tuplet; }
    int actualNotes() const { return tuplet >> 4; }
    int normalNotes() const { return tuplet & 0x0F; }

    EncGraceType graceType() const;

    bool read(QDataStream& ds) override;
};

struct EncRest : EncMeasureElem {
    quint8 faceValue  { 0 };
    quint8 tuplet     { 0 };
    quint8 dotControl { 0 };

    using EncMeasureElem::EncMeasureElem;

    quint8 tupletByte() const override { return tuplet; }
    int actualNotes() const { return tuplet >> 4; }
    int normalNotes() const { return tuplet & 0x0F; }

    bool read(QDataStream& ds) override;
};

struct EncChordSym : EncMeasureElem {
    quint8 toniko { 0 };
    quint8 tipo   { 0 };
    quint8 radiko { 0 };
    quint8 baso   { 0 };
    QString teksto;

    using EncMeasureElem::EncMeasureElem;

    bool read(QDataStream& ds) override;
};

struct EncOrnament : EncMeasureElem {
    // Field names follow the Encore binary format notation used throughout the spec
    quint8 tipo      { 0 };
    quint8 alMezuro  { 0 };
    quint8 xoffset2  { 0 };
    quint8 speguleco { 0 };
    quint8 noto      { 0 };
    quint8 tempo     { 0 };
    quint8 tind      { 0 };

    using EncMeasureElem::EncMeasureElem;

    EncOrnamentType ornType() const { return static_cast<EncOrnamentType>(tipo); }
    void setOrnType(EncOrnamentType t) { tipo = static_cast<quint8>(t); }

    bool read(QDataStream& ds) override;
};

struct EncLyric : EncMeasureElem {
    using EncMeasureElem::EncMeasureElem;

    bool read(QDataStream& ds) override;
};

struct EncKeyChange : EncMeasureElem {
    quint8 tipo { 0 };

    using EncMeasureElem::EncMeasureElem;

    bool read(QDataStream& ds) override;
};

struct EncGenericElem : EncMeasureElem {
    using EncMeasureElem::EncMeasureElem;

    bool read(QDataStream& ds) override;
};

// TIE element: marks that the note(s) at (staffIdx, voice, tick) tie forward.
// The element has no pitch field; ties are matched by (staffIdx, voice, tick).
//
// The byte at elemStart+2 encodes the tie arc direction:
//   0xfe  — outgoing tie: the note at this tick SENDS a tie to the next same-pitch note
//            (TIE-START).  Added to tieStartSet during the pre-scan.
//   other — arc-only marker: visual ornament for an incoming tie arc endpoint; the note
//            at this tick receives a tie from a previous note but does NOT start a new
//            outgoing tie.  NOT added to tieStartSet.
struct EncTie : EncMeasureElem {
    bool isTieStart { false };   // true when elemStart+2 == 0xfe (outgoing tie)

    using EncMeasureElem::EncMeasureElem;

    bool read(QDataStream& ds) override;
};

// ---------------------------------------------------------------------------
// Chord-cluster threshold: notes within this many Encore ticks of each other are
// treated as simultaneous (live-recorded MIDI timing drift).  Used in both
// calculateRealDurations() and the tie/chord-extension logic in buildScore().
inline constexpr int CHORD_CLUSTER_THRESHOLD = 4;   // Encore ticks (~8ms at 120bpm)

// ---------------------------------------------------------------------------
// Measure
// ---------------------------------------------------------------------------

using MeasureElemVec = std::vector<std::unique_ptr<EncMeasureElem> >;
using MeasureElemRefVec = std::vector<const EncMeasureElem*>;

struct EncMeasure {
    QString m_id;
    qint32 varsize           { 0 };
    quint16 bpm               { 0 };
    quint8 timeSigGlyph      { 0 };
    quint16 beatTicks         { 0 };
    quint16 durTicks          { 0 };
    quint8 timeSigNum        { 0 };
    quint8 timeSigDen        { 0 };
    quint8 barTypeStart      { 0 };
    quint8 barTypeEnd        { 0 };
    quint8 repeatAlternative { 0 };
    quint32 coda              { 0 };
    MeasureElemVec elements;

    EncMeasure() = default;
    EncMeasure(const EncMeasure&) = delete;
    EncMeasure& operator=(const EncMeasure&) = delete;
    EncMeasure(EncMeasure&&) noexcept = default;
    EncMeasure& operator=(EncMeasure&&) noexcept = default;

    ~EncMeasure() = default;

    EncBarlineType startBarline() const { return static_cast<EncBarlineType>(barTypeStart); }
    EncBarlineType endBarline() const { return static_cast<EncBarlineType>(barTypeEnd); }
    EncRepeatType repeatMark() const { return static_cast<EncRepeatType>((coda >> 8) & 0xFF); }

    bool read(QDataStream& ds, const quint32 vs, bool oldFormat, bool veryOldFormat);
    void calculateRealDurations();
};

// ---------------------------------------------------------------------------
// Instrument / part
// ---------------------------------------------------------------------------

struct EncInstrument {
    QString name;
    quint32 offset    { 0 };
    int nstaves   { 0 };
    int midiProgram { 0 };   // 1-indexed GM program (0 = not configured)
    bool showStaff { true }; // false = hidden in score (Encore "Show" flag)

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
    // Staff visibility flag: bit 0 set (0x01) = visible; 0x00 = hidden from score.
    // Stored at byte +19 of the 30-byte staff entry (3rd byte of the 3-byte skip
    // block that follows pageIdx).  Verified by binary-diffing pachbel-shown.enc
    // vs pachbel-hiden.enc: only the hidden staff has this byte == 0x00.
    bool showStaff { true };

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

struct EncTitle {
    QString title;
    std::vector<QString> subtitle;
    std::vector<QString> instruction;
    std::vector<QString> author;
    std::vector<QString> header;
    std::vector<QString> footer;
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

    bool isOldFormat() const { return chuMagio == 0xC2; }
    bool isVeryOldFormat() const { return chuMagio == 0xA6; }

    bool read(QDataStream& ds);
};

// ---------------------------------------------------------------------------
// EncFile - top-level container
// ---------------------------------------------------------------------------

bool isInstrumentMagic(const QString& magic);
bool isKnownMagic(const QString& magic);
QString findNextKnownMagic(QDataStream& ds);
void addSpannerEnds(std::vector<EncMeasure>& measures);

struct EncFile {
    EncHeader header;
    std::vector<EncInstrument> instruments;
    std::vector<EncLine> lines;
    std::vector<EncMeasure> measures;
    EncTitle titleBlock;

    bool read(QDataStream& ds);
};

} // namespace mu::iex::encore

#endif // MU_IMPORTEXPORT_ENCOREELEMENTS_H
