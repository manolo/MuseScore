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

#include "elements-text.h"
#include "encoding.h"

namespace mu::iex::enc {

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

} // namespace mu::iex::enc
