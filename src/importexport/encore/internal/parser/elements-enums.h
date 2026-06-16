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

#include <QtGlobal>

namespace mu::iex::encore {

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
    // See ENCORE_FORMAT.md §Ornament subtypes for tipo values, sizes, and quirks.
    TRILL_END    = 0x35,
    TRILL_START  = 0x36,
    TRILL_ALT    = 0x37,
    // 0xB0: standalone 16-byte "tr" ornament; places ornamentTrill glyph.
    TRILL_TR     = 0xB0,
    // 0xB6: standalone 16-byte short-trill ornament; places ornamentShortTrill glyph.
    TRILL_SHORT  = 0xB6,
    SEGNO       = 0xA2,
    TO_CODA     = 0xA5,
    CODA        = 0xA6,
    // 0xC9 staccato: Encore's MusicXML exporter drops it; we import it.
    STACCATO    = 0xC9,
    TEMPO      = 0x32,
    // 0xAF standard, 0xEF alternate (half notes at tick >= durTicks).
    TREMOLO_32 = 0xAF,
    TREMOLO_32B = 0xEF,
    SLURSTOP   = 0x41,
    WEDGESTOP  = 0x4D,
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
    DYN_FZ     = 0xAA,
    DYN_SF     = 0xAB,
    // Fingering: 0xB8 + finger number (1..5).
    FINGER_1   = 0xB9,
    FINGER_2   = 0xBA,
    FINGER_3   = 0xBB,
    FINGER_4   = 0xBC,
    FINGER_5   = 0xBD,
    ACCENT        = 0xBE,  // standalone accent (>) in v0xC4; maps to articAccentAbove
    UPBOW         = 0xC4,
    DOWNBOW       = 0xC5,
    FERMATA_ABOVE = 0xCC,  // standalone fermata above (size-16 ORN; yoffset > 0)
    FERMATA_BELOW = 0xCD,  // standalone fermata below (size-16 ORN; yoffset < 0)
    REPEAT_MEASURE = 0xA3, // "%" repeat-last-bar glyph (size-16 ORN)
    CAESURA       = 0xA7,  // caesura // (size-16 ORN, placed before note at tick)
    BREATH_COMMA  = 0xA8   // breath mark comma (size-16 ORN, placed before note at tick)
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

// See ENCORE_FORMAT.md §TITL block for header/footer alignment byte values.
enum class EncTextAlign : quint8 {
    LEFT   = 0x04,
    CENTER = 0x06,
    RIGHT  = 0x02
};

} // namespace mu::iex::encore
