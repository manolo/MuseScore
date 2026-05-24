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

#include <gtest/gtest.h>

#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/part.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/timesig.h"
#include "engraving/style/style.h"
#include "engraving/types/fraction.h"

#include "testbase.h"

static const QString ENC_DIR(QString(iex_encore_tests_DATA_ROOT) + "/data/");

using namespace mu::engraving;

static Measure* measureAt(MasterScore* score, int n)
{
    int idx = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        if (idx == n) {
            return toMeasure(mb);
        }
        ++idx;
    }
    return nullptr;
}

class Tst_Structure : public ::testing::Test, public MTest
{
protected:
    void SetUp() override { setRootDir(ENC_DIR); }
};

TEST_F(Tst_Structure, basic_measure_count)
{
    MasterScore* score = readEncoreScore("bazo.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Structure, basic_single_part)
{
    MasterScore* score = readEncoreScore("bazo.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_EQ(score->parts().size(), 1u);
    EXPECT_EQ(score->nstaves(), 1u);
    delete score;
}

TEST_F(Tst_Structure, multipart_score)
{
    MasterScore* score = readEncoreScore("bando.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->parts().size(), 1u) << "bando.enc should have multiple parts";
    EXPECT_GT(score->nstaves(), 1u);
    delete score;
}

TEST_F(Tst_Structure, time_sig_4_4)
{
    MasterScore* score = readEncoreScore("chord_parsing.enc");
    ASSERT_NE(score, nullptr);
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(4, 4)) << "First measure should be 4/4";
    delete score;
}

TEST_F(Tst_Structure, time_sig_3_4)
{
    MasterScore* score = readEncoreScore("notes_triplets.enc");
    ASSERT_NE(score, nullptr);
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(3, 4)) << "Synthetic triplet file should be 3/4";
    delete score;
}

TEST_F(Tst_Structure, time_sig_2_4)
{
    MasterScore* score = readEncoreScore("notes_swing.enc");
    ASSERT_NE(score, nullptr);
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(2, 4)) << "First measure should be 2/4";
    delete score;
}

TEST_F(Tst_Structure, key_sig_no_accidentals)
{
    MasterScore* score = readEncoreScore("bazo.enc");
    ASSERT_NE(score, nullptr);
    Staff* st = score->staff(0);
    ASSERT_NE(st, nullptr);
    Key k = st->key(Fraction(0, 1));
    EXPECT_EQ(int(k), 0) << "bazo.enc should be in C major (0 accidentals)";
    delete score;
}

// FIX: v0xA6 (MusicTime / Encore 2.x-3.x) stores the written key signature at offset 14 of
// each 22-byte LINE staff entry, not where v0xC2/C4 keep it; its header staffPerSystem also
// reads 0, so the generic parse leaves staffData empty and the key was lost (imported as the
// wrong key / no signature). keyIndex 10 = A major; every staff must import as 3 sharps.
TEST_F(Tst_Structure, key_sig_v0xa6_from_line_entry)
{
    MasterScore* score = readEncoreScore("structure_v0xa6_key_signature.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_GT(score->nstaves(), 0u);
    for (size_t i = 0; i < score->nstaves(); ++i) {
        Staff* st = score->staff(i);
        ASSERT_NE(st, nullptr);
        EXPECT_EQ(int(st->key(Fraction(0, 1))), 3)
            << "v0xA6 staff " << i << " must import A major (3 sharps) from LINE entry offset 14";
    }
    delete score;
}

TEST_F(Tst_Structure, key_sig_no_invalid_large_values)
{
    // encKeyToFifths wrapping was broken before (key index 8 mapped to -248); verify -7..7 range.
    MasterScore* score = readEncoreScore("bando.enc");
    ASSERT_NE(score, nullptr);
    Fraction tick(0, 1);
    for (size_t i = 0; i < score->nstaves(); ++i) {
        Staff* st = score->staff(i);
        int keyVal = int(st->key(tick));
        EXPECT_GE(keyVal, -7) << "Staff " << i << " key should be >= -7";
        EXPECT_LE(keyVal, 7) << "Staff " << i << " key should be <= 7";
    }
    delete score;
}

TEST_F(Tst_Structure, intermediate_time_sig_7_8)
{
    MasterScore* score = readEncoreScore("paloteos_7x8.enc");
    ASSERT_NE(score, nullptr);

    Measure* m0 = measureAt(score, 0);
    ASSERT_NE(m0, nullptr);
    EXPECT_EQ(m0->timesig(), Fraction(4, 4)) << "M0 should be 4/4";

    Measure* m16 = measureAt(score, 16);
    ASSERT_NE(m16, nullptr);
    EXPECT_EQ(m16->timesig(), Fraction(7, 8)) << "M16 should be 7/8";
    EXPECT_EQ(m16->ticks(), Fraction(7, 8)) << "M16 duration should be 7/8";

    Segment* tsSeg = m16->findSegment(SegmentType::TimeSig, m16->tick());
    EXPECT_NE(tsSeg, nullptr) << "M16 must have a TimeSig segment";
    if (tsSeg) {
        bool found7_8 = false;
        for (EngravingItem* el : tsSeg->elist()) {
            if (el && el->isTimeSig()) {
                TimeSig* ts = toTimeSig(el);
                if (ts->sig() == Fraction(7, 8)) {
                    found7_8 = true;
                }
            }
        }
        EXPECT_TRUE(found7_8) << "TimeSig segment at M16 must contain a 7/8 element";
    }

    Measure* m15 = measureAt(score, 15);
    ASSERT_NE(m15, nullptr);
    EXPECT_EQ(m15->timesig(), Fraction(4, 4)) << "M15 should still be 4/4";

    delete score;
}

// ===========================================================================
// BUG FIX: 6/8 → 3/4 (and 3/4 → 6/8) time signature changes were silently
// swallowed because buildInitialSignatures used Fraction::operator== to detect
// changes, and 6/8 == 3/4 by cross-multiplication (6×4 == 3×8 = 24).
// Fix: use Fraction::identical() which compares numerator/denominator directly.
// Fixture: 2 measures 6/8, then 3 measures 3/4, then 2 measures 6/8.
// ===========================================================================
TEST_F(Tst_Structure, time_sig_change_6_8_to_3_4_and_back)
{
    MasterScore* score = readEncoreScore("timesig_change_6_8_to_3_4.enc");
    ASSERT_NE(score, nullptr);

    Measure* m0 = measureAt(score, 0);
    ASSERT_NE(m0, nullptr);
    EXPECT_TRUE(m0->timesig().identical(Fraction(6, 8))) << "M0 should be 6/8";

    // Measure 2: 6/8 → 3/4. Must have a visible TimeSig element.
    Measure* m2 = measureAt(score, 2);
    ASSERT_NE(m2, nullptr);
    EXPECT_TRUE(m2->timesig().identical(Fraction(3, 4))) << "M2 should be 3/4";
    {
        Segment* tsSeg = m2->findSegment(SegmentType::TimeSig, m2->tick());
        ASSERT_NE(tsSeg, nullptr) << "M2 must have a TimeSig segment (6/8 → 3/4 change)";
        bool found = false;
        for (EngravingItem* el : tsSeg->elist()) {
            if (el && el->isTimeSig()) {
                TimeSig* ts = toTimeSig(el);
                if (ts->sig().identical(Fraction(3, 4))) {
                    found = true;
                }
            }
        }
        EXPECT_TRUE(found) << "TimeSig at M2 must carry 3/4, not be merged silently with 6/8";
    }

    // Measure 5: 3/4 → 6/8. Must have a visible TimeSig element.
    Measure* m5 = measureAt(score, 5);
    ASSERT_NE(m5, nullptr);
    EXPECT_TRUE(m5->timesig().identical(Fraction(6, 8))) << "M5 should be 6/8";
    {
        Segment* tsSeg = m5->findSegment(SegmentType::TimeSig, m5->tick());
        ASSERT_NE(tsSeg, nullptr) << "M5 must have a TimeSig segment (3/4 → 6/8 change)";
        bool found = false;
        for (EngravingItem* el : tsSeg->elist()) {
            if (el && el->isTimeSig()) {
                TimeSig* ts = toTimeSig(el);
                if (ts->sig().identical(Fraction(6, 8))) {
                    found = true;
                }
            }
        }
        EXPECT_TRUE(found) << "TimeSig at M5 must carry 6/8, not be merged silently with 3/4";
    }

    delete score;
}

// ===========================================================================
// BUG regression: Fraction(2,2)==Fraction(4,4) via cross-multiplication
// (2x4 == 4x2 = 8), so the 2/2 -> 4/4 change was silently swallowed by the
// same buildInitialSignatures bug as 6/8 -> 3/4.
// Fixture: 2 measures 2/2, then 2 measures 4/4, then 2 measures 2/2.
// ===========================================================================
TEST_F(Tst_Structure, time_sig_change_2_2_to_4_4_and_back)
{
    MasterScore* score = readEncoreScore("timesig_change_2_2_to_4_4.enc");
    ASSERT_NE(score, nullptr);

    Measure* m0 = measureAt(score, 0);
    ASSERT_NE(m0, nullptr);
    EXPECT_TRUE(m0->timesig().identical(Fraction(2, 2))) << "M0 should be 2/2";

    Measure* m2 = measureAt(score, 2);
    ASSERT_NE(m2, nullptr);
    EXPECT_TRUE(m2->timesig().identical(Fraction(4, 4))) << "M2 should be 4/4";
    {
        Segment* tsSeg = m2->findSegment(SegmentType::TimeSig, m2->tick());
        ASSERT_NE(tsSeg, nullptr) << "M2 must have a TimeSig segment (2/2 -> 4/4)";
        bool found = false;
        for (EngravingItem* el : tsSeg->elist()) {
            if (el && el->isTimeSig() && toTimeSig(el)->sig().identical(Fraction(4, 4))) {
                found = true;
            }
        }
        EXPECT_TRUE(found) << "TimeSig at M2 must carry 4/4";
    }

    Measure* m4 = measureAt(score, 4);
    ASSERT_NE(m4, nullptr);
    EXPECT_TRUE(m4->timesig().identical(Fraction(2, 2))) << "M4 should be 2/2";
    {
        Segment* tsSeg = m4->findSegment(SegmentType::TimeSig, m4->tick());
        ASSERT_NE(tsSeg, nullptr) << "M4 must have a TimeSig segment (4/4 -> 2/2)";
        bool found = false;
        for (EngravingItem* el : tsSeg->elist()) {
            if (el && el->isTimeSig() && toTimeSig(el)->sig().identical(Fraction(2, 2))) {
                found = true;
            }
        }
        EXPECT_TRUE(found) << "TimeSig at M4 must carry 2/2";
    }

    delete score;
}

// ===========================================================================
// WINI block / page margin tests
// ===========================================================================

// bazo.enc has a WINI block: top=18 left=18 bEdge=824 rEdge=577 on A4.
TEST_F(Tst_Structure, page_margins_wini_standard_a4)
{
    MasterScore* score = readEncoreScore("bazo.enc");
    ASSERT_NE(score, nullptr);

    const double expectedIn = 18.0 / 72.0;   // 0.25"
    EXPECT_NEAR(score->style().styleD(Sid::pageOddTopMargin),  expectedIn, 0.001);
    EXPECT_NEAR(score->style().styleD(Sid::pageEvenTopMargin), expectedIn, 0.001);
    EXPECT_NEAR(score->style().styleD(Sid::pageOddLeftMargin),  expectedIn, 0.001);
    EXPECT_NEAR(score->style().styleD(Sid::pageEvenLeftMargin), expectedIn, 0.001);

    // printableWidth = (rEdge - left) / 72 = (577 - 18) / 72 = 559 / 72
    const double expectedPrintW = 559.0 / 72.0;
    EXPECT_NEAR(score->style().styleD(Sid::pagePrintableWidth), expectedPrintW, 0.001);

    delete score;
}

// File with custom left margin (left=7 pts, ~0.097 in).
// bazo_left_100.enc: top=18 left=7 bEdge=824 rEdge=577.
TEST_F(Tst_Structure, page_margins_wini_custom_left)
{
    MasterScore* score = readEncoreScore("bazo_left_100.enc");
    ASSERT_NE(score, nullptr);

    EXPECT_NEAR(score->style().styleD(Sid::pageOddTopMargin),   18.0 / 72.0, 0.001);
    EXPECT_NEAR(score->style().styleD(Sid::pageOddLeftMargin),   7.0 / 72.0, 0.001);
    EXPECT_NEAR(score->style().styleD(Sid::pageEvenLeftMargin),  7.0 / 72.0, 0.001);
    // printableWidth = (577 - 7) / 72 = 570 / 72
    EXPECT_NEAR(score->style().styleD(Sid::pagePrintableWidth), 570.0 / 72.0, 0.001);

    delete score;
}

// Verify bottom margin is correctly derived from bottomEdge.
// bazo.enc: top=18 left=18 bEdge=824 rEdge=577 on A4 (842 pts high).
// bottomMargin = (842 - 824) / 72 = 18 / 72 = 0.25"
TEST_F(Tst_Structure, page_margins_wini_bottom_margin_derived)
{
    MasterScore* score = readEncoreScore("bazo.enc");
    ASSERT_NE(score, nullptr);

    const double expectedIn = 18.0 / 72.0;
    EXPECT_NEAR(score->style().styleD(Sid::pageOddBottomMargin),  expectedIn, 0.005)
        << "bottom margin must be derived from bottomEdge and page height";
    EXPECT_NEAR(score->style().styleD(Sid::pageEvenBottomMargin), expectedIn, 0.005);

    delete score;
}

// ===========================================================================
// FIX: WINI screen-pixel format, coordinates in monitor pixels (~84-85 PPI)
// rather than typographic points (1/72").  Symptom: rightEdge=672 exceeds
// A4_width_pts=595, causing the old code to clamp the right margin to ~0.03"
// and the bottom margin to 0 (both wrong).  The fix detects the screen-pixel
// format (rightEdge > pageWidth_pts), identifies the paper format (A4) via a
// two-pass QPageSize scan, and computes symmetric margins (~0.33" = 8.4mm).
// ===========================================================================
TEST_F(Tst_Structure, page_margins_wini_screen_pixel_a4_detected)
{
    // structure_wini_screen_pixel_a4.enc: bazo.enc with WINI patched to
    // screen-pixel coordinates: top=28, left=28, bEdge=962, rEdge=672.
    // Expected: A4 page (8.2677" x 11.6929"), all margins ~0.331" (8.4mm).
    MasterScore* score = readEncoreScore("structure_wini_screen_pixel_a4.enc");
    ASSERT_NE(score, nullptr);

    // Page dimensions must be detected as A4.
    const double kA4W = 210.0 / 25.4;   // 8.2677"
    const double kA4H = 297.0 / 25.4;   // 11.6929"
    EXPECT_NEAR(score->style().styleD(Sid::pageWidth),  kA4W, 0.01)
        << "Screen-pixel WINI: page must be detected as A4 width";
    EXPECT_NEAR(score->style().styleD(Sid::pageHeight), kA4H, 0.01)
        << "Screen-pixel WINI: page must be detected as A4 height";

    // Margins must be symmetric at ~0.331" = 28 / 84.67 DPI.
    // (Old code: L=T=0.389", R=0.030", B=0.10", all wrong.)
    const double kExpectedM = 28.0 / (700.0 / kA4W);   // ≈ 0.331"
    EXPECT_NEAR(score->style().styleD(Sid::pageOddLeftMargin),  kExpectedM, 0.005)
        << "Screen-pixel WINI: left margin must be ~0.33\"";
    EXPECT_NEAR(score->style().styleD(Sid::pageEvenLeftMargin), kExpectedM, 0.005);
    EXPECT_NEAR(score->style().styleD(Sid::pageOddTopMargin),   kExpectedM, 0.005)
        << "Screen-pixel WINI: top margin must be ~0.33\"";
    // Right margin: symmetric (pageWidth - left - printableWidth ≈ kExpectedM).
    const double printW = score->style().styleD(Sid::pagePrintableWidth);
    const double rightM = kA4W - kExpectedM - printW;
    EXPECT_NEAR(rightM, kExpectedM, 0.01)
        << "Screen-pixel WINI: right margin must be ~0.33\"";

    delete score;
}
