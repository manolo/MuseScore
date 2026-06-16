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

// Unit tests for EncChordSym::chordName().
// These tests exercise the numeric chord decoding logic in isolation
// (no score, no .enc file loading) by constructing EncChordSym directly.

#include <gtest/gtest.h>

#include "../internal/parser/elements.h"

using namespace mu::iex::enc;

// Helper: build a numeric-only chord (tipo bit 0 clear = no text).
static EncChordSym makeNumeric(quint8 toniko, quint8 radiko, quint8 baso = 0, quint8 tipo = 0)
{
    EncChordSym cs;
    cs.toniko = toniko;
    cs.tipo   = tipo;
    cs.radiko = radiko;
    cs.baso   = baso;
    return cs;
}

// Helper: build a text-mode chord (tipo bit 0 set).
static EncChordSym makeText(const QString& teksto)
{
    EncChordSym cs;
    cs.tipo   = 1;
    cs.teksto = teksto;
    return cs;
}

// ---------------------------------------------------------------------------
// Root note encoding (radiko lower nibble = note name 0-6, upper nibble = acc)
// ---------------------------------------------------------------------------

TEST(Tst_EncChordSym, all_natural_roots)
{
    // Lower nibble 0-6 maps to C D E F G A B with no accidental (upper nibble 0).
    EXPECT_EQ(makeNumeric(0, 0x00).chordName(), "C");
    EXPECT_EQ(makeNumeric(0, 0x01).chordName(), "D");
    EXPECT_EQ(makeNumeric(0, 0x02).chordName(), "E");
    EXPECT_EQ(makeNumeric(0, 0x03).chordName(), "F");
    EXPECT_EQ(makeNumeric(0, 0x04).chordName(), "G");
    EXPECT_EQ(makeNumeric(0, 0x05).chordName(), "A");
    EXPECT_EQ(makeNumeric(0, 0x06).chordName(), "B");
}

TEST(Tst_EncChordSym, sharp_roots)
{
    // Upper nibble 0x10 = sharp (#).
    EXPECT_EQ(makeNumeric(0, 0x10).chordName(), "C#");
    EXPECT_EQ(makeNumeric(0, 0x14).chordName(), "G#");
    EXPECT_EQ(makeNumeric(0, 0x15).chordName(), "A#");
}

TEST(Tst_EncChordSym, flat_roots)
{
    // Upper nibble 0x20 = flat (b).
    EXPECT_EQ(makeNumeric(0, 0x21).chordName(), "Db");
    EXPECT_EQ(makeNumeric(0, 0x22).chordName(), "Eb");
    EXPECT_EQ(makeNumeric(0, 0x26).chordName(), "Bb");
}

TEST(Tst_EncChordSym, invalid_root_returns_empty)
{
    // Lower nibble > 6 has no note name; chordName() must return empty.
    EXPECT_TRUE(makeNumeric(0, 0x07).chordName().isEmpty()) << "radiko=7 has no note name";
    EXPECT_TRUE(makeNumeric(0, 0x0F).chordName().isEmpty()) << "radiko=0x0F has no note name";
    EXPECT_TRUE(makeNumeric(0, 0x17).chordName().isEmpty()) << "sharp + invalid nibble";
}

// ---------------------------------------------------------------------------
// Chord quality (toniko index into kChordQuality[])
// ---------------------------------------------------------------------------

TEST(Tst_EncChordSym, major_no_suffix)
{
    EXPECT_EQ(makeNumeric(0, 0x00).chordName(), "C");
    EXPECT_EQ(makeNumeric(0, 0x05).chordName(), "A");
}

TEST(Tst_EncChordSym, minor)
{
    EXPECT_EQ(makeNumeric(1, 0x05).chordName(), "Am");
    EXPECT_EQ(makeNumeric(1, 0x00).chordName(), "Cm");
}

TEST(Tst_EncChordSym, augmented)
{
    EXPECT_EQ(makeNumeric(2, 0x00).chordName(), "C+");
}

TEST(Tst_EncChordSym, diminished)
{
    EXPECT_EQ(makeNumeric(3, 0x00).chordName(), "Cdim");
}

TEST(Tst_EncChordSym, dominant7_index4)
{
    EXPECT_EQ(makeNumeric(4, 0x03).chordName(), "F7");
}

TEST(Tst_EncChordSym, dominant7_index24_alternate_encoding)
{
    // toniko=24 and toniko=4 both map to "7" quality.
    EXPECT_EQ(makeNumeric(24, 0x03).chordName(), "F7");
}

TEST(Tst_EncChordSym, maj7)
{
    EXPECT_EQ(makeNumeric(12, 0x00).chordName(), "Cmaj7");
}

TEST(Tst_EncChordSym, minor7)
{
    EXPECT_EQ(makeNumeric(54, 0x01).chordName(), "Dm7");
}

TEST(Tst_EncChordSym, sus4)
{
    EXPECT_EQ(makeNumeric(46, 0x04).chordName(), "Gsus4");
}

TEST(Tst_EncChordSym, sus2)
{
    EXPECT_EQ(makeNumeric(44, 0x04).chordName(), "Gsus2");
}

TEST(Tst_EncChordSym, half_diminished)
{
    EXPECT_EQ(makeNumeric(56, 0x05).chordName(), "Am7(b5)");
}

TEST(Tst_EncChordSym, out_of_range_toniko_treated_as_major)
{
    // toniko=63 is beyond the table; degrades to just the root.
    EXPECT_EQ(makeNumeric(63, 0x00).chordName(), "C");
}

// ---------------------------------------------------------------------------
// Bass note (tipo bit 1 = bass present, baso same encoding as radiko)
// ---------------------------------------------------------------------------

TEST(Tst_EncChordSym, slash_chord_with_bass)
{
    // tipo=2 (bit 1): bass note present. C major over G.
    EncChordSym cs = makeNumeric(0, 0x00, 0x04, 0x02);
    EXPECT_EQ(cs.chordName(), "C/G");
}

TEST(Tst_EncChordSym, slash_chord_minor_with_flat_bass)
{
    // Am over E.
    EncChordSym cs = makeNumeric(1, 0x05, 0x02, 0x02);
    EXPECT_EQ(cs.chordName(), "Am/E");
}

TEST(Tst_EncChordSym, bass_ignored_when_tipo_bit1_clear)
{
    // baso is set but tipo bit 1 is 0: bass must not appear.
    EncChordSym cs = makeNumeric(0, 0x00, 0x04, 0x00);
    EXPECT_EQ(cs.chordName(), "C");
}

// ---------------------------------------------------------------------------
// Text mode: tipo bit 0 set -> teksto is returned verbatim
// ---------------------------------------------------------------------------

TEST(Tst_EncChordSym, text_mode_returns_teksto)
{
    EXPECT_EQ(makeText("Am").chordName(), "Am");
    EXPECT_EQ(makeText("G7").chordName(), "G7");
    EXPECT_EQ(makeText("Cmaj7(b5)").chordName(), "Cmaj7(b5)");
}

TEST(Tst_EncChordSym, text_mode_ignores_numeric_fields)
{
    // Even with radiko pointing to a different note, teksto wins.
    EncChordSym cs = makeText("F#m7");
    cs.toniko = 0;
    cs.radiko = 0x00;   // would decode to "C" without text
    EXPECT_EQ(cs.chordName(), "F#m7");
}

TEST(Tst_EncChordSym, empty_teksto_falls_through_to_numeric)
{
    EncChordSym cs;
    cs.tipo   = 0;       // no text flag
    cs.teksto = {};
    cs.toniko = 1;
    cs.radiko = 0x05;    // A minor
    EXPECT_EQ(cs.chordName(), "Am");
}
