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

#ifndef MU_IMPORTEXPORT_ENC_PARSER_ELEMENTS_H
#define MU_IMPORTEXPORT_ENC_PARSER_ELEMENTS_H

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
    DOUBLER     = 6,
    DOTTED      = 8
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
    ARPEGGIO   = 0x22,
    // Trill-line ornaments (size-28): 0x36 starts first trill, 0x37 starts second,
    // 0x35 is the end marker (adds no trill mark itself).
    TRILL_END   = 0x35,
    TRILL_START = 0x36,
    TRILL_ALT   = 0x37,
    // Section navigation markers (size-16): 0xA2=segno, 0xA5=to coda, 0xA6=coda.
    SEGNO       = 0xA2,
    TO_CODA     = 0xA5,
    CODA        = 0xA6,
    // Per-chord staccato (size-16). Encore's own MusicXML exporter drops 0xC9 entirely;
    // we import it because it accounts for nearly all staccatos in plectro scores.
    STACCATO    = 0xC9,
    TEMPO      = 0x32,
    // Single-chord tremolo (plectro), size-16. Both map to TremoloSingleChord/R32.
    // 0xAF: standard encoding (248 occurrences in Beethoven Plectro).
    // 0xEF: alternate encoding seen on half notes at tick >= measure durTicks.
    TREMOLO_32 = 0xAF,
    TREMOLO_32B = 0xEF,
    SLURSTOP   = 0x41,
    WEDGESTOP  = 0x4D,
    // Dynamic markings (size-16), confirmed against encore-symbols.xml:
    //   ppp=0x80, pp=0x81, p=0x82, mp=0x83, mf=0x84
    //   f=0x85, ff=0x86, fff=0x87, sfz=0x88, sffz=0x89, fp=0x8A
    DYN_PPP    = 0x80,
    DYN_PP     = 0x81,
    DYN_P      = 0x82,
    DYN_MP     = 0x83,
    DYN_MF     = 0x84,
    DYN_F      = 0x85,
    DYN_FF     = 0x86,
    DYN_FFF    = 0x87,
    DYN_SFZ    = 0x88,
    DYN_SFFZ   = 0x89,
    DYN_FP     = 0x8A,
    // High-range: fp=0x8A, fz=0xAA, sf=0xAB (from encore-symbols.enc m3).
    DYN_FZ     = 0xAA,
    DYN_SF     = 0xAB,
    // Fingering numbers 1..5 as stand-alone ORN elements (size-16).
    // Encoded as 0xB8 + finger (1=0xB9, 2=0xBA, 3=0xBB, 4=0xBC, 5=0xBD).
    FINGER_1   = 0xB9,
    FINGER_2   = 0xBA,
    FINGER_3   = 0xBB,
    FINGER_4   = 0xBC,
    FINGER_5   = 0xBD,
    // Plectro stroke direction (size-16): down stroke (П) and up stroke (V).
    UPBOW      = 0xC4,
    DOWNBOW    = 0xC5
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
    qint16 yoffset   { 0 };  // signed 16-bit Cartesian y (positive = upward in Encore)
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
    QString text;
    quint8 kie { 0 };   // location/anchor byte (similar to xoffset)

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

// TIE element: marks notes at (staffIdx, voice, tick) that tie forward.
// Dir byte (elemStart+5): bit 7 (0x80) = arc-above outgoing; bit 1 (0x02) = arc-below outgoing.
//   0xfe/0x80+: arc-above; 0x02/0x03: arc-below. Endpoint-only TIEs have dir=0x00/0x01.
// StartFlag byte (elemStart+6): high bit set = additional outgoing tie marker.
struct EncTie : EncMeasureElem {
    bool isTieStart { false };   // true when dir byte has bit 7 or bit 1 set, or startFlag has bit 7 set

    using EncMeasureElem::EncMeasureElem;

    bool read(QDataStream& ds) override;
};

// Notes within this many Encore ticks treated as simultaneous (MIDI timing drift).
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
    EncRepeatType repeatMark() const { return static_cast<EncRepeatType>(coda & 0xFF); }

    bool read(QDataStream& ds, const quint32 vs, const struct EncFormatReader& fmt);
    void calculateRealDurations();
};

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

// Header/footer line in the TITL block. The 30-byte line prefix includes a
// horizontal-alignment byte at +14 (0x02 = right, 0x04 = left, 0x06 = center)
// that picks which page corner the text lands on.
enum class EncTextAlign : quint8 {
    LEFT   = 0x04,
    CENTER = 0x06,
    RIGHT  = 0x02
};

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

// TEXT block: indexed text payload for STAFFTEXT 0x1E ornaments (and possibly
// other free-text annotations). The N-th entry is referenced by an ornament's
// `tind` field (+32). Block layout:
//   +0..+1: 0x0000 sync
//   +2..+3: entry count (2 bytes)
//   +4..+7: content size (4 bytes, total of all entries)
//   then, for each entry:
//     +0..+1: payload size (S)
//     +2..+S+1: payload
//       +0..+13: 14 bytes of header (not fully decoded)
//       +14..+S-5: UTF-16 LE text
//       +S-4..+S-1: 0x04 0x00 0x00 0x00 terminator
struct EncTextBlock {
    std::vector<QString> entries;

    bool read(QDataStream& ds, quint32 varSize);
};

struct EncFile {
    EncHeader header;
    std::vector<EncInstrument> instruments;
    std::vector<EncLine> lines;
    std::vector<EncMeasure> measures;
    EncTitle titleBlock;
    EncTextBlock textBlock;

    bool read(QDataStream& ds);
};
} // namespace mu::iex::encore

#endif // MU_IMPORTEXPORT_ENC_PARSER_ELEMENTS_H
