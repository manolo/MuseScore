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

#include "engraving/dom/arpeggio.h"
#include "engraving/dom/system.h"
#include "engraving/dom/layoutbreak.h"
#include "engraving/dom/articulation.h"
#include "engraving/dom/barline.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/fermata.h"
#include "engraving/dom/fingering.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/jump.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/marker.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/stafftext.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/textbase.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tuplet.h"

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


// ===========================================================================
// FIX: KEYCHANGE tipo=0 (C major modulation) must be emitted; previous guard silently dropped it.
// ===========================================================================

TEST_F(Tst_Structure, keychange_to_c_major_emitted)
{
    MasterScore* score = readEncoreScore("structure_keychange_to_c.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int keySigCount = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        Measure* m = toMeasure(mb);
        for (Segment* s = m->first(SegmentType::KeySig); s; s = s->next(SegmentType::KeySig)) {
            if (s->element(0)) {
                ++keySigCount;
            }
        }
    }
    // Initial key sig (m0 G major) + tipo=0 modulation sig (m1); both must be present.
    EXPECT_GE(keySigCount, 2);
    delete score;
}

// ===========================================================================
// FEATURE: All ten Encore navigation options (Segno/Coda/ToCoda/Fine + 6 DC/DS variants) survive import.
// ===========================================================================
TEST_F(Tst_Structure, all_encore_navigation_options)
{
    MasterScore* score = readEncoreScore("structure_jump_marks_all.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int segnoMarkers = 0;
    int codaMarkers = 0;
    int toCodaMarkers = 0;
    int fineMarkers = 0;
    std::set<JumpType> jumpTypes;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (EngravingItem* e : mb->el()) {
            if (e && e->isMarker()) {
                MarkerType mt = toMarker(e)->markerType();
                if (mt == MarkerType::SEGNO) ++segnoMarkers;
                else if (mt == MarkerType::CODA) ++codaMarkers;
                else if (mt == MarkerType::TOCODA) ++toCodaMarkers;
                else if (mt == MarkerType::FINE) ++fineMarkers;
            } else if (e && e->isJump()) {
                jumpTypes.insert(toJump(e)->jumpType());
            }
        }
    }
    // Segno comes from ORN 0xA2 AND coda byte 0x88; both add a Marker.
    EXPECT_GE(segnoMarkers, 1) << "ORN 0xA2 must produce a Segno Marker";
    // Coda from ORN 0xA6 + byte 0x89; byte 0x85 produces TOCODA instead.
    EXPECT_GE(codaMarkers, 1) << "ORN 0xA6 must produce a Coda Marker";
    // "To Coda" comes from ORN 0xA5 AND coda byte 0x85 (CODA1).
    EXPECT_GE(toCodaMarkers, 1) << "ORN 0xA5 must produce a TOCODA Marker";
    // Fine comes from coda byte 0x86.
    EXPECT_EQ(fineMarkers, 1) << "coda byte 0x86 must produce a FINE Marker";
    // Every Jump variant must appear at least once.
    const std::set<JumpType> expectedJumps = {
        JumpType::DC, JumpType::DS,
        JumpType::DC_AL_FINE, JumpType::DS_AL_FINE,
        JumpType::DC_AL_CODA, JumpType::DS_AL_CODA,
    };
    for (JumpType j : expectedJumps) {
        EXPECT_TRUE(jumpTypes.count(j) > 0)
            << "missing Jump variant for the Encore-UI option";
    }
    delete score;
}

// ===========================================================================
// FIX: Jump marks from MEAS coda byte at offset 0x1A (low byte); To Coda from ORN tipo=0xA5.
// ===========================================================================
TEST_F(Tst_Structure, jump_marks_dc_ds_tocoda)
{
    MasterScore* score = readEncoreScore("structure_jump_marks.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<std::pair<int, String> > seen;  // measure number (1-based), text
    int measIdx = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        ++measIdx;
        for (EngravingItem* e : mb->el()) {
            if (e && e->isMarker()) {
                seen.emplace_back(measIdx, toMarker(e)->plainText());
            } else if (e && e->isJump()) {
                seen.emplace_back(measIdx, toJump(e)->plainText());
            }
        }
    }
    // Marker (TOCODA) lands on m1; Jumps land on m2 and m3.
    ASSERT_EQ(seen.size(), 3u);
    EXPECT_EQ(seen[0].first, 1);
    EXPECT_TRUE(seen[0].second.contains(u"Coda"))
        << "expected To Coda Marker on m1";
    EXPECT_EQ(seen[1].first, 2);
    EXPECT_TRUE(seen[1].second.contains(u"D.S."))
        << "expected D.S. al Coda Jump on m2";
    EXPECT_EQ(seen[2].first, 3);
    EXPECT_TRUE(seen[2].second.contains(u"D.C."))
        << "expected D.C. Jump on m3";
    delete score;
}

// ===========================================================================
// FEATURE: Section markers (Segno / Coda) from ORN tipos 0xA2 / 0xA6 and
// DOTTED end barline (barTypeEnd=0x08).
// ===========================================================================
TEST_F(Tst_Structure, section_markers_and_dotted_barline)
{
    MasterScore* score = readEncoreScore("structure_section_markers.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<MarkerType> seenMarkers;
    BarLineType m3Bar = BarLineType::NORMAL;
    int measIdx = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        ++measIdx;
        for (EngravingItem* e : mb->el()) {
            if (e && e->isMarker()) {
                seenMarkers.push_back(toMarker(e)->markerType());
            }
        }
        if (measIdx == 3) {
            Measure* m3 = toMeasure(mb);
            Segment* seg = m3->findSegment(SegmentType::EndBarLine, m3->endTick());
            if (seg) {
                if (EngravingItem* el = seg->element(0)) {
                    if (el->isBarLine()) {
                        m3Bar = toBarLine(el)->barLineType();
                    }
                }
            }
        }
    }
    const std::vector<MarkerType> expectedMarkers = {
        MarkerType::SEGNO, MarkerType::CODA,
    };
    EXPECT_EQ(seenMarkers, expectedMarkers);
    EXPECT_EQ(m3Bar, BarLineType::DOTTED)
        << "m3 end barline must be DOTTED (barTypeEnd=0x08)";
    delete score;
}

// ===========================================================================
// FIX: v0xC2 (old Encore format) — MIDI pitch stored at byte +13 (tuplet field), not semiTonePitch.
// ===========================================================================

TEST_F(Tst_Structure, old_format_v0c2_correct_pitches)
{
    // v0xC2: MIDI pitch at byte +13 (tuplet-field); needsPitchFix swaps it to semiTonePitch.
    MasterScore* score = readEncoreScore("structure_v0c2_pitches.enc");
    ASSERT_NE(score, nullptr);

    std::vector<int> pitches;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest); s;
             s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->elist()) {
                if (e && e->isChord()) {
                    for (Note* n : toChord(e)->notes()) {
                        pitches.push_back(n->pitch());
                    }
                }
            }
        }
    }
    ASSERT_EQ(pitches.size(), 4u) << "Should have 4 notes";
    EXPECT_EQ(pitches[0], 60) << "First note should be C4 (60)";
    EXPECT_EQ(pitches[1], 64) << "Second note should be E4 (64)";
    EXPECT_EQ(pitches[2], 67) << "Third note should be G4 (67)";
    EXPECT_EQ(pitches[3], 72) << "Fourth note should be C5 (72)";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "v0xC2 pitch-fixed score should pass sanityCheck: " << ret.text();
    delete score;
}

TEST_F(Tst_Structure, old_format_v0c2_triplets_detected)
{
    // v0xC2: 6 eighth notes at 80-tick spacing (2/3 of an eighth) → detectImpliedTuplet returns 3:2.
    MasterScore* score = readEncoreScore("structure_v0c2_triplets.enc");
    ASSERT_NE(score, nullptr);

    bool foundTriplet = false;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (EngravingItem* e : toMeasure(mb)->el()) {
            if (e->isTuplet() && toTuplet(e)->ratio() == Fraction(3, 2)) {
                foundTriplet = true;
                break;
            }
        }
        if (foundTriplet) {
            break;
        }
    }
    EXPECT_TRUE(foundTriplet) << "v0xC2 implied triplets should be detected";
    delete score;
}


// ===========================================================================
// BUG FIX: v0xA6 (very old format) — wrong element offset and pitch encoding
// ===========================================================================

TEST_F(Tst_Structure, very_old_format_v0xa6_sanity_check)
{
    // v0xA6: elemOffset must be 0x1A (not 0x3E); wrong offset drops all notes silently.
    MasterScore* score = readEncoreScore("structure_v0xa6_basic.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_EQ(score->nmeasures(), 2);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "v0xA6 synthetic file should pass sanityCheck: " << ret.text();
    delete score;
}

TEST_F(Tst_Structure, very_old_format_v0xa6_pitch_encoding)
{
    // v0xA6: MIDI pitch is absolute value at elemStart+11 (not byte +9 with signed offset).
    MasterScore* score = readEncoreScore("structure_v0xa6_basic.enc");
    ASSERT_NE(score, nullptr);

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    std::vector<int> pitches;
    for (Segment* s = m->first(SegmentType::ChordRest); s;
         s = s->next(SegmentType::ChordRest)) {
        for (EngravingItem* e : s->elist()) {
            if (e && e->isChord()) {
                for (Note* n : toChord(e)->notes()) {
                    pitches.push_back(n->pitch());
                }
            }
        }
    }
    ASSERT_EQ(pitches.size(), 4u) << "Measure 1 should have 4 notes";
    EXPECT_EQ(pitches[0], 60) << "C4";
    EXPECT_EQ(pitches[1], 62) << "D4";
    EXPECT_EQ(pitches[2], 64) << "E4";
    EXPECT_EQ(pitches[3], 67) << "G4";
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
// FEATURE: LINE block system breaks: break after last measure of each non-final system.
// ===========================================================================
TEST_F(Tst_Structure, system_breaks_from_line_data)
{
    MasterScore* score = readEncoreScore("structure_system_break.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    auto hasLineBreak = [](Measure* m) {
        for (EngravingItem* e : m->el()) {
            if (e->isLayoutBreak()
                && toLayoutBreak(e)->layoutBreakType() == LayoutBreakType::LINE) {
                return true;
            }
        }
        return false;
    };

    Measure* m2 = measureAt(score, 2);
    ASSERT_NE(m2, nullptr);
    EXPECT_TRUE(hasLineBreak(m2)) << "LINE break expected after measure 2 (end of system 0)";

    Measure* m5 = measureAt(score, 5);
    ASSERT_NE(m5, nullptr);
    EXPECT_FALSE(hasLineBreak(m5)) << "last system must not get a break";

    delete score;
}

// ===========================================================================
// FEATURE: fitSpatiumToLineBreaks reduces spatium so each music system holds at least enc.lines[i].measureCount measures.
// ===========================================================================
TEST_F(Tst_Structure, fit_spatium_first_system_measure_count)
{
    MasterScore* score = readEncoreScore("text_tempo_orn_compound_68.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int firstSystemMeasureCount = 0;
    for (const System* sys : score->systems()) {
        int mc = 0;
        for (const MeasureBase* mb : sys->measures()) {
            if (mb->isMeasure()) { ++mc; }
        }
        if (mc > 0) {
            firstSystemMeasureCount = mc;
            break;
        }
    }
    EXPECT_GE(firstSystemMeasureCount, 3)
        << "first system must fit at least enc.lines[0].measureCount (3) measures";

    delete score;
}

TEST_F(Tst_Structure, fit_spatium_multiple_systems_measure_count)
{
    // All 8 lines have measureCount=3; verify the first 4 systems each have >= 3 measures.
    MasterScore* score = readEncoreScore("text_tempo_orn_compound_68.enc");
    ASSERT_NE(score, nullptr);

    std::vector<int> sysCounts;
    for (const System* sys : score->systems()) {
        int mc = 0;
        for (const MeasureBase* mb : sys->measures()) {
            if (mb->isMeasure()) { ++mc; }
        }
        if (mc > 0) {
            sysCounts.push_back(mc);
        }
    }

    // The fixture has 8 lines; we require at least the first 4 to be present.
    ASSERT_GE(sysCounts.size(), 4u) << "fixture must produce at least 4 music systems";

    for (int j = 0; j < 4; ++j) {
        EXPECT_GE(sysCounts[j], 3)
            << "system " << j << " must fit at least 3 measures (enc.lines[" << j << "].measureCount)";
    }

    delete score;
}

// ===========================================================================
// WINI block / page margin tests
// ===========================================================================

// File with no WINI block must leave MuseScore default margins intact.
// text_tempo_orn_compound_68.enc has no WINI block.
TEST_F(Tst_Structure, page_margins_no_wini_uses_defaults)
{
    MasterScore* score = readEncoreScore("text_tempo_orn_compound_68.enc");
    ASSERT_NE(score, nullptr);

    const double defaultLeftIn = 15.0 / INCH;
    EXPECT_NEAR(score->style().styleD(Sid::pageOddLeftMargin),  defaultLeftIn, 0.001)
        << "no-WINI file must keep default left margin";
    EXPECT_NEAR(score->style().styleD(Sid::pageEvenLeftMargin), defaultLeftIn, 0.001);
    EXPECT_NEAR(score->style().styleD(Sid::pageOddTopMargin),   defaultLeftIn, 0.001)
        << "no-WINI file must keep default top margin";

    delete score;
}

// File with standard A4 WINI (top=18, left=18, bEdge=824, rEdge=577).
// bazo.enc in the test fixtures has exactly this block.
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

// File with WINI top=0 left=0 (zero margins, full-page printable area).
// ornaments_fingering_grandstaff.enc: top=0 left=0 bEdge=842 rEdge=595.
// Zero margins are clamped to the minimum safe values so staves stay within the page.
TEST_F(Tst_Structure, page_margins_wini_zero_margins_clamped)
{
    MasterScore* score = readEncoreScore("ornaments_fingering_grandstaff.enc");
    ASSERT_NE(score, nullptr);

    // Margins clamped to minimums: LR=0.03", TB=0.10".
    EXPECT_NEAR(score->style().styleD(Sid::pageOddTopMargin),    0.10, 0.001);
    EXPECT_NEAR(score->style().styleD(Sid::pageEvenTopMargin),   0.10, 0.001);
    EXPECT_NEAR(score->style().styleD(Sid::pageOddLeftMargin),   0.03, 0.001);
    EXPECT_NEAR(score->style().styleD(Sid::pageEvenLeftMargin),  0.03, 0.001);
    EXPECT_NEAR(score->style().styleD(Sid::pageOddBottomMargin), 0.10, 0.001);
    // printableWidth capped to pageWidth - leftMargin - minRightMargin.
    const double pageWIn = score->style().styleD(Sid::pageWidth);
    EXPECT_NEAR(score->style().styleD(Sid::pagePrintableWidth), pageWIn - 0.03 - 0.03, 0.01);

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
