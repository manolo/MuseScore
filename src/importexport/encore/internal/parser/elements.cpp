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

#include "elements.h"

#include "encoding.h"
#include "ticks.h"

namespace mu::iex::encore {
// ---------------------------------------------------------------------------
// EncMeasureElem and derived element types
// ---------------------------------------------------------------------------

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

bool EncChordSym::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);
    ds >> toniko >> tipo;
    ds.skipRawData(3);
    ds >> xoffset;
    ds.skipRawData(1);
    ds >> radiko >> baso;
    const bool hasText = (tipo & 1);
    if (hasText) {
        // 36-byte slot; UTF-16 LE in modern files, Latin-1 in legacy scores.
        teksto = readEncodedStringFixed(ds, 36);
        int toSkip = static_cast<int>(size) - 5 - 9 - 36;
        if (toSkip > 0) {
            ds.skipRawData(toSkip);
        }
    } else {
        int toSkip = static_cast<int>(size) - 5 - 9;
        if (toSkip > 0) {
            ds.skipRawData(toSkip);
        }
    }
    return true;
}

// Chord quality suffixes indexed by toniko value (0-62).
// Derived from Enc2MusicXML's tnk[] table (textfile.cpp).
// Empty entries denote undefined/reserved chord types; they degrade to major.
static const char* const kChordQuality[] = {
    "",            //  0: major (no suffix)
    "m",           //  1: minor
    "+",           //  2: augmented
    "dim",         //  3: diminished
    "7",           //  4: dominant 7
    "5",           //  5: power chord
    "6",           //  6: major 6
    "6/9",         //  7
    "(add2)",      //  8
    "(add9)",      //  9
    "(omit3)",     // 10
    "(omit5)",     // 11
    "maj7",        // 12
    "maj7(b5)",    // 13
    "maj7(6/9)",   // 14
    "maj7(#5)",    // 15
    "",            // 16: undefined
    "maj9",        // 17
    "maj9(b5)",    // 18
    "maj9(#5)",    // 19
    "",            // 20: undefined
    "maj13",       // 21
    "maj13(b5)",   // 22
    "",            // 23: undefined
    "7",           // 24: dominant 7 (alternate encoding)
    "7(b5)",       // 25
    "7(b9)",       // 26
    "7(#9)",       // 27
    "",            // 28: undefined
    "",            // 29: undefined
    "",            // 30: undefined
    "",            // 31: undefined
    "9",           // 32
    "9(b5)",       // 33
    "11",          // 34
    "13",          // 35
    "13(b5)",      // 36
    "13(b9)",      // 37
    "13(#9)",      // 38
    "",            // 39: undefined
    "+7",          // 40: augmented 7
    "+7(b9)",      // 41
    "+7(#9)",      // 42
    "+9",          // 43
    "sus2",        // 44
    "sus2sus4",    // 45 (Encore "sus2,sus4"; comma removed for MuseScore parser)
    "sus4",        // 46
    "7sus4",       // 47
    "9sus4",       // 48
    "13sus4",      // 49
    "m(add2)",     // 50
    "m(add9)",     // 51
    "m6",          // 52
    "m6/9",        // 53
    "m7",          // 54
    "m(maj7)",     // 55
    "m7(b5)",      // 56
    "m7(add4)",    // 57
    "m7(add11)",   // 58
    "m9",          // 59
    "m(maj9)",     // 60
    "m11",         // 61
    "m13",         // 62
};
static constexpr int kChordQualityCount = static_cast<int>(sizeof(kChordQuality) / sizeof(kChordQuality[0]));

// Note names for the lower nibble of radiko/baso (0=C, 1=D, 2=E, 3=F, 4=G, 5=A, 6=B).
static const char* const kNoteNames[] = { "C", "D", "E", "F", "G", "A", "B" };
static constexpr int kNoteNameCount   = static_cast<int>(sizeof(kNoteNames) / sizeof(kNoteNames[0]));

static QString encRootToString(quint8 field)
{
    const int noteIdx = field & 0x0F;
    if (noteIdx >= kNoteNameCount) {
        return {};
    }
    QString root = QString::fromLatin1(kNoteNames[noteIdx]);
    const int acc = (field & 0xF0) >> 4;
    if (acc == 1) {
        root += u'#';
    } else if (acc == 2) {
        root += u'b';
    }
    return root;
}

QString EncChordSym::chordName() const
{
    if (!teksto.isEmpty()) {
        return teksto;
    }

    const QString root = encRootToString(radiko);
    if (root.isEmpty()) {
        return {};
    }

    const QString quality = (toniko < kChordQualityCount)
                            ? QString::fromLatin1(kChordQuality[toniko])
                            : QString{};

    QString name = root + quality;

    if (tipo & 0x02) {
        const QString bass = encRootToString(baso);
        if (!bass.isEmpty()) {
            name += u'/' + bass;
        }
    }

    return name;
}

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

bool EncLyric::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);   // consumed: size + staffIdx (5 bytes from elemStart)

    // See ENCORE_FORMAT.md §Lyric element for field offsets and encoding detection.
    // textGapAfterKie is 9 for v0xC4 (text at +20) and 7 for v0xC2 (text at +18).
    const int fixedReads = 5 + 1 + static_cast<int>(textGapAfterKie);
    int remaining = static_cast<int>(size) - 5;
    if (remaining < fixedReads) {
        if (remaining > 0) {
            ds.skipRawData(remaining);
        }
        return true;
    }

    ds.skipRawData(5);
    ds >> kie;
    ds.skipRawData(textGapAfterKie);
    remaining -= fixedReads;

    text = readEncodedStringRemaining(ds, remaining);

    if (remaining > 0) {
        ds.skipRawData(remaining);
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

bool EncTie::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);   // reads size + staffIdx
    quint8 dirByte = 0;
    quint8 startFlag = 0;
    if (size > 5) {
        ds >> dirByte;          // tie arc direction byte at element offset +5
    }
    if (size > 6) {
        ds >> startFlag;        // tie-start flag at element offset +6
    }
    // Dir/startFlag bit layout: see ENCORE_FORMAT.md §TIE element.
    isTieStart = ((dirByte & 0x80) != 0) || ((startFlag & 0x80) != 0) || ((dirByte & 0x02) != 0);

    if (static_cast<int>(size) >= 18) {
        // Read arc x-positions at offsets +10 and +12. When arcX1 == arcX2 the arc has
        // zero horizontal extent. Two cases share this pattern:
        //   (a) intra-chord decorative arc (Encore connects two chord notes vertically,
        //       startFlag = 0x00) — must NOT become a forward tie.
        //   (b) cross-measure tie where the destination is in the next measure and Encore
        //       stores arcX2 = arcX1 as a placeholder (startFlag = 0x80) — IS a real tie.
        // Override isTieStart only when startFlag bit 7 is NOT set (case a).
        ds.skipRawData(3);          // skip offsets +7,+8,+9
        ds >> arcX1;                // offset +10
        ds.skipRawData(1);          // skip offset +11
        ds >> arcX2;                // offset +12
        if (arcX1 == arcX2 && (startFlag & 0x80) == 0) {
            isTieStart = false;
        }
        // Byte +13 unused; byte +14 = staff position of source note (see ENCORE_FORMAT.md §TIE element).
        ds.skipRawData(1);          // skip offset +13
        quint8 sp = 0;
        ds >> sp;                   // offset +14
        sourcePosition = static_cast<qint8>(sp);
        const int toSkip = static_cast<int>(size) - 15;  // skip offsets +15..end
        if (toSkip > 0) {
            ds.skipRawData(toSkip);
        }
    } else {
        const int consumed = (size > 5 ? 1 : 0) + (size > 6 ? 1 : 0);
        const int toSkip = static_cast<int>(size) - 5 - consumed;
        if (toSkip > 0) {
            ds.skipRawData(toSkip);
        }
    }
    return true;
}
} // namespace mu::iex::encore
