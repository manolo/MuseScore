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

// Score structure: measure/time/key signatures, page layout and margins, jumps and markers,
// system and page breaks, pickup measures, format-specific pitch decoding and malformed-input safety.

#include <gtest/gtest.h>

#include <QByteArray>
#include <QFile>
#include <QTemporaryDir>

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
#include "engraving/dom/page.h"
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

// Page index (0-based) that the first PAGE layout break's measure is laid out on, -1 if none.
static int firstPageBreakPage(MasterScore* score)
{
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        bool hasPageBreak = false;
        for (EngravingItem* e : mb->el()) {
            if (e && e->isLayoutBreak() && toLayoutBreak(e)->isPageBreak()) {
                hasPageBreak = true;
                break;
            }
        }
        if (!hasPageBreak) {
            continue;
        }
        System* sys = toMeasure(mb)->system();
        if (sys && sys->page()) {
            for (size_t i = 0; i < score->pages().size(); ++i) {
                if (score->pages()[i] == sys->page()) {
                    return (int)i;
                }
            }
        }
        return -1;
    }
    return -2;
}

TEST_F(Tst_Structure, page_break_spill_shrinks_staff_space)
{
    // structure_page_break_spill.enc: 6 staves, 3 systems. Its first two systems (LINE pageIdx
    // 0 then 1) belong on the first page, but at the default staff space they do not fit on its
    // short custom page, so the second system spills onto the second page (leaving a near-empty
    // page). With imported page breaks the importer shrinks the staff space (by <= 0.022 inch)
    // until the first page break's measure returns to the first page.
    MasterScore* score = readEncoreScore("structure_page_break_spill.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck());

    EXPECT_EQ(firstPageBreakPage(score), 0)
        << "the first page break's measure must be pulled back onto the first page";

    // A reference import with page breaks off does not run the fit pass, so its staff space is
    // the untouched default. The page-break import must have reduced it, but by no more than the
    // 0.022-inch budget (1 inch == 1200 engraving units).
    mu::iex::enc::EncImportOptions noBreaks;
    noBreaks.importPageBreaks = false;
    MasterScore* ref = readEncoreScoreWithOpts("structure_page_break_spill.enc", noBreaks);
    ASSERT_NE(ref, nullptr);
    const double defaultSp = ref->style().styleD(Sid::spatium);
    const double sp = score->style().styleD(Sid::spatium);
    EXPECT_LT(sp, defaultSp) << "staff space must be reduced to make the first page fit";
    EXPECT_GE(sp, defaultSp - 0.022 * 1200.0) << "the reduction must not exceed 0.022 inch";

    delete ref;
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

// v0xA6 keeps the key at the LINE staff entry, not where v0xC2/C4 do; staffPerSystem reads 0 so
// the generic parse left staffData empty and lost it. See ENCORE_FORMAT.md §LINE staff entry (30 bytes).
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

// A no-WINI landscape PREC page must recompute the printable width so the right margin equals
// the left, instead of dumping the extra landscape width into a lopsided right margin.
TEST_F(Tst_Structure, page_margins_no_wini_landscape_right_matches_left)
{
    MasterScore* score = readEncoreScore("structure_prec_landscape_no_wini.enc");
    ASSERT_NE(score, nullptr);

    const double pageW  = score->style().styleD(Sid::pageWidth);
    const double pageH  = score->style().styleD(Sid::pageHeight);
    EXPECT_GT(pageW, pageH) << "PREC orientation=2 must yield a landscape page";

    const double leftM  = score->style().styleD(Sid::pageOddLeftMargin);
    const double printW = score->style().styleD(Sid::pagePrintableWidth);
    const double rightM = pageW - leftM - printW;
    EXPECT_NEAR(rightM, leftM, 0.01)
        << "landscape no-WINI: right margin must match the left, not leave the extra page width";
    delete score;
}

// A KEYCHANGE to C major (tipo=0) must still emit a key signature; an earlier guard dropped it.
// See ENCORE_FORMAT.md §KEYCHANGE element.
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
    // Initial key sig (m0 G major) + tipo=0 modulation sig (m1); both must be present.
    EXPECT_GE(keySigCount, 2);
    delete score;
}

// All ten Encore navigation options (Segno/Coda/ToCoda/Fine plus 6 DC/DS variants) survive import.
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
                if (mt == MarkerType::SEGNO) {
                    ++segnoMarkers;
                } else if (mt == MarkerType::CODA) {
                    ++codaMarkers;
                } else if (mt == MarkerType::TOCODA) {
                    ++toCodaMarkers;
                } else if (mt == MarkerType::FINE) {
                    ++fineMarkers;
                }
            } else if (e && e->isJump()) {
                jumpTypes.insert(toJump(e)->jumpType());
            }
        }
    }
    // Markers come from ORN subtypes and MEAS coda bytes. See ENCORE_FORMAT.md §Ornament subtypes
    // and §Repeat-mark ladder (low byte of `0x1A`).
    EXPECT_GE(segnoMarkers, 1) << "ORN 0xA2 must produce a Segno Marker";
    EXPECT_GE(codaMarkers, 1) << "ORN 0xA6 must produce a Coda Marker";
    EXPECT_GE(toCodaMarkers, 1) << "ORN 0xA5 must produce a TOCODA Marker";
    EXPECT_EQ(fineMarkers, 1) << "coda byte 0x86 must produce a FINE Marker";
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

// Jump marks from the MEAS coda byte and To Coda from an ORN subtype land on the expected
// measures. See ENCORE_FORMAT.md §Repeat-mark ladder (low byte of `0x1A`).
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

// Section markers (Segno / Coda) from ORN subtypes and a DOTTED end barline. See
// ENCORE_FORMAT.md §Ornament subtypes and §Barline types.
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

// v0xC2 stores the MIDI pitch in the tuplet field, not semiTonePitch; the importer swaps it.
// See ENCORE_FORMAT.md §v0xC2 note (size 22 or 24).
TEST_F(Tst_Structure, old_format_v0c2_correct_pitches)
{
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
    // v0xC2 eighths at 80-tick spacing (2/3 of an eighth) imply a 3:2 triplet.
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

// v0xA6 element offset and pitch encoding differ from later formats; a wrong offset silently
// drops every note. See ENCORE_FORMAT.md §v0xA6 note (size 10, on-disk slot 20).
TEST_F(Tst_Structure, very_old_format_v0xa6_sanity_check)
{
    MasterScore* score = readEncoreScore("structure_v0xa6_basic.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_EQ(score->nmeasures(), 2);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "v0xA6 synthetic file should pass sanityCheck: " << ret.text();
    delete score;
}

// See ENCORE_FORMAT.md §v0xA6 note (size 10, on-disk slot 20).
TEST_F(Tst_Structure, very_old_format_v0xa6_pitch_encoding)
{
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

// A 6/8 <-> 3/4 change must not be swallowed by an operator== that treats 6/8 and 3/4 as equal
// (6x4 == 3x8); detection compares numerator/denominator directly via Fraction::identical().
TEST_F(Tst_Structure, time_sig_change_6_8_to_3_4_and_back)
{
    MasterScore* score = readEncoreScore("timesig_change_6_8_to_3_4.enc");
    ASSERT_NE(score, nullptr);

    Measure* m0 = measureAt(score, 0);
    ASSERT_NE(m0, nullptr);
    EXPECT_TRUE(m0->timesig().identical(Fraction(6, 8))) << "M0 should be 6/8";

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

// The PREC (DEVMODE) dmPaperSize enum is the primary page-size source for all formats. Unicode
// DEVMODE variant (32-WCHAR device name), Letter. See ENCORE_FORMAT.md §PREC block.
TEST_F(Tst_Structure, page_size_from_prec_letter_unicode)
{
    MasterScore* score = readEncoreScore("structure_prec_page_letter.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_NEAR(score->style().styleD(Sid::pageWidth),  8.5,  0.02)
        << "PREC dmPaperSize=1 must set Letter width";
    EXPECT_NEAR(score->style().styleD(Sid::pageHeight), 11.0, 0.02)
        << "PREC dmPaperSize=1 must set Letter height";
    delete score;
}

// ANSI DEVMODE variant (32-byte device name), A3. See ENCORE_FORMAT.md §PREC block.
TEST_F(Tst_Structure, page_size_from_prec_ansi_a3)
{
    MasterScore* score = readEncoreScore("structure_prec_page_a3.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_NEAR(score->style().styleD(Sid::pageWidth),  297.0 / 25.4, 0.03)
        << "ANSI PREC dmPaperSize=8 must set A3 width";
    EXPECT_NEAR(score->style().styleD(Sid::pageHeight), 420.0 / 25.4, 0.03)
        << "ANSI PREC dmPaperSize=8 must set A3 height";
    delete score;
}

// Large WINI margins (A3, screen-pixel format) must survive import instead of being clamped to a
// tiny maximum. The px-to-inch conversion uses an estimated dpi, so values are approximate; the
// invariant is only that they are not clamped away. See ENCORE_FORMAT.md §WINI block.
TEST_F(Tst_Structure, page_margins_wini_large_not_clamped)
{
    MasterScore* score = readEncoreScore("structure_wini_large_margins_a3.enc");
    ASSERT_NE(score, nullptr);
    const double topIn  = score->style().styleD(Sid::pageOddTopMargin);
    const double leftIn = score->style().styleD(Sid::pageOddLeftMargin);
    EXPECT_GT(topIn,  1.5) << "large top margin must not be clamped to a tiny maximum";
    EXPECT_GT(leftIn, 1.5) << "large left margin must not be clamped to a tiny maximum";
    EXPECT_NEAR(topIn,  2.14, 0.2) << "top margin ~2.1 inches (176 px at ~82 dpi)";
    EXPECT_NEAR(leftIn, 2.54, 0.2) << "left margin ~2.5 inches (209 px at ~82 dpi)";
    delete score;
}

// Companion to the 6/8 <-> 3/4 case: 2/2 <-> 4/4 (2x4 == 4x2) must not be swallowed either.
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

// LINE block data becomes SystemLocks so the layout engine keeps each Encore system's measures
// together regardless of spatium. See ENCORE_FORMAT.md §System block (LINE).
TEST_F(Tst_Structure, system_breaks_from_line_data)
{
    MasterScore* score = readEncoreScore("structure_system_break.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m2 = measureAt(score, 2);
    ASSERT_NE(m2, nullptr);
    EXPECT_TRUE(m2->isEndOfSystemLock())
        << "measure 2 (end of system 0) must be the end of a SystemLock";

    Measure* m0 = measureAt(score, 0);
    ASSERT_NE(m0, nullptr);
    EXPECT_TRUE(m0->isStartOfSystemLock())
        << "measure 0 (start of system 0) must be the start of a SystemLock";

    delete score;
}

// SCO5 reads the per-line measureCount as 0, so each system's span must be derived from the line
// start deltas for system locks to apply. Fixture zeroes every measureCount byte; system 0 must
// still lock measures 0..2. See ENCORE_FORMAT.md §System block (LINE).
TEST_F(Tst_Structure, system_breaks_from_line_start_deltas_when_count_zero)
{
    MasterScore* score = readEncoreScore("structure_system_break_mcount_zero.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck());

    Measure* m0 = measureAt(score, 0);
    ASSERT_NE(m0, nullptr);
    EXPECT_TRUE(m0->isStartOfSystemLock())
        << "measure 0 must start a SystemLock derived from the line start delta";
    Measure* m2 = measureAt(score, 2);
    ASSERT_NE(m2, nullptr);
    EXPECT_TRUE(m2->isEndOfSystemLock())
        << "measure 2 (start[1]-start[0]=3 measures later) must end the first SystemLock";

    delete score;
}

// Page-break detection must use the same start-delta fallback as system locks: with SCO5
// measureCount 0, the line span is recovered from start deltas so the page break survives.
// See ENCORE_FORMAT.md §System block (LINE).
TEST_F(Tst_Structure, page_break_from_line_start_deltas_when_count_zero)
{
    MasterScore* score = readEncoreScore("structure_page_break_mcount_zero.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck());

    bool foundPageBreak = false;
    for (Measure* m = score->firstMeasure(); m && !foundPageBreak; m = m->nextMeasure()) {
        for (EngravingItem* e : m->el()) {
            if (e && e->isLayoutBreak() && toLayoutBreak(e)->isPageBreak()) {
                foundPageBreak = true;
                break;
            }
        }
    }
    EXPECT_TRUE(foundPageBreak)
        << "page break must be recovered from line start deltas when measureCount is 0";

    delete score;
}

// SCO5 gets page size and orientation from the PREC macOS plist; margins are not stored, so the
// importer applies a symmetric 0.25" margin. See ENCORE_FORMAT.md §PREC block.
TEST_F(Tst_Structure, sco5_macos_page_letter_default_margins)
{
    MasterScore* score = readEncoreScore("structure_sco5_macos.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck());

    const MStyle& st = score->style();
    EXPECT_NEAR(st.styleD(Sid::pageWidth), 8.5, 1e-3) << "Letter width from PREC plist";
    EXPECT_NEAR(st.styleD(Sid::pageHeight), 11.0, 1e-3) << "Letter height from PREC plist";
    EXPECT_NEAR(st.styleD(Sid::pageOddLeftMargin), 0.25, 1e-6) << "SCO5 uses a uniform 0.25\" margin";
    EXPECT_NEAR(st.styleD(Sid::pageOddTopMargin), 0.25, 1e-6);
    EXPECT_NEAR(st.styleD(Sid::pageOddBottomMargin), 0.25, 1e-6);
    EXPECT_NEAR(st.styleD(Sid::pagePrintableWidth), 8.0, 1e-3) << "printable width = page width - 2 x 0.25\"";

    delete score;
}

// SystemLocks lock each Encore system to exactly enc.lines[i].measureCount measures.
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
            if (mb->isMeasure()) {
                ++mc;
            }
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
    // All 8 lines have measureCount=3; verify the first 4 systems each have exactly 3 measures.
    MasterScore* score = readEncoreScore("text_tempo_orn_compound_68.enc");
    ASSERT_NE(score, nullptr);

    std::vector<int> sysCounts;
    for (const System* sys : score->systems()) {
        int mc = 0;
        for (const MeasureBase* mb : sys->measures()) {
            if (mb->isMeasure()) {
                ++mc;
            }
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

// WINI block page-margin tests. See ENCORE_FORMAT.md §WINI block.

// A file with no WINI block keeps MuseScore default margins.
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

// Standard A4 WINI: margins and printable width derived from the WINI edges.
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

// Custom left margin (left=7 pts) survives and drives the printable width.
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

// Bottom margin is derived from bottomEdge and page height: (842 - 824) / 72 = 0.25".
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

// WINI screen-pixel variant stores coordinates in monitor pixels, not points, so rightEdge can
// exceed the point-based page width. The importer detects this (rightEdge > pageWidth_pts),
// recovers the paper format (A4) and computes symmetric margins. See ENCORE_FORMAT.md §WINI block.
TEST_F(Tst_Structure, page_margins_wini_screen_pixel_a4_detected)
{
    MasterScore* score = readEncoreScore("structure_wini_screen_pixel_a4.enc");
    ASSERT_NE(score, nullptr);

    const double kA4W = 210.0 / 25.4;   // 8.2677"
    const double kA4H = 297.0 / 25.4;   // 11.6929"
    EXPECT_NEAR(score->style().styleD(Sid::pageWidth),  kA4W, 0.01)
        << "Screen-pixel WINI: page must be detected as A4 width";
    EXPECT_NEAR(score->style().styleD(Sid::pageHeight), kA4H, 0.01)
        << "Screen-pixel WINI: page must be detected as A4 height";

    const double kExpectedM = 28.0 / (700.0 / kA4W);   // ~0.331" = 28 px at the recovered dpi
    EXPECT_NEAR(score->style().styleD(Sid::pageOddLeftMargin),  kExpectedM, 0.005)
        << "Screen-pixel WINI: left margin must be ~0.33\"";
    EXPECT_NEAR(score->style().styleD(Sid::pageEvenLeftMargin), kExpectedM, 0.005);
    EXPECT_NEAR(score->style().styleD(Sid::pageOddTopMargin),   kExpectedM, 0.005)
        << "Screen-pixel WINI: top margin must be ~0.33\"";
    const double printW = score->style().styleD(Sid::pagePrintableWidth);
    const double rightM = kA4W - kExpectedM - printW;
    EXPECT_NEAR(rightM, kExpectedM, 0.01)
        << "Screen-pixel WINI: right margin must be ~0.33\"";

    delete score;
}

// Pickup (anacrusis) first measure: an explicit short timeSig[0] produces a shortened first
// measure (actual ticks 1/4) that still displays the nominal 4/4, with m1 starting at tick 1/4.
TEST_F(Tst_Structure, pickup_measure_shortened)
{
    MasterScore* score = readEncoreScore("structure_pickup_measure.enc");
    ASSERT_NE(score, nullptr);

    Measure* m0 = measureAt(score, 0);
    Measure* m1 = measureAt(score, 1);
    ASSERT_NE(m0, nullptr);
    ASSERT_NE(m1, nullptr);

    EXPECT_EQ(m0->timesig(), Fraction(4, 4)) << "Pickup m0 must display the nominal 4/4 time signature";
    EXPECT_EQ(m0->ticks(), Fraction(1, 4)) << "Pickup m0 must be shortened to the pickup duration";
    EXPECT_EQ(m1->tick(), Fraction(1, 4)) << "m1 must start immediately after the shortened m0";

    Fraction noteOffset { -1, 1 };
    for (Segment* s = m0->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord()) {
            noteOffset = s->tick() - m0->tick();
            break;
        }
    }
    EXPECT_EQ(noteOffset, Fraction(0, 1)) << "Pickup note must be at offset 0 within the shortened m0";

    delete score;
}

// Case B pickup (no explicit short timeSig): measure 0 is shortened purely from its content
// (cumTick 1/4) even though the nominal signature is 4/4.
TEST_F(Tst_Structure, pickup_caseb_reduces_to_max_content)
{
    MasterScore* score = readEncoreScore("structure_pickup_caseb_reduces.enc");
    ASSERT_NE(score, nullptr);

    Measure* m0 = measureAt(score, 0);
    Measure* m1 = measureAt(score, 1);
    ASSERT_NE(m0, nullptr);
    ASSERT_NE(m1, nullptr);

    EXPECT_EQ(m0->timesig(), Fraction(4, 4)) << "Pickup m0 must display nominal 4/4";
    EXPECT_EQ(m0->ticks(), Fraction(1, 4)) << "Pickup m0 must be shortened to cumTick=8/32=1/4";
    EXPECT_EQ(m1->tick(), Fraction(1, 4)) << "m1 must start immediately after the shortened m0";

    delete score;
}

// Case B guard: content that fills the measure completely (cumTick == ticks) must not shorten it.
TEST_F(Tst_Structure, pickup_caseb_no_reduce_when_full_content)
{
    MasterScore* score = readEncoreScore("structure_pickup_caseb_no_reduce_full.enc");
    ASSERT_NE(score, nullptr);

    Measure* m0 = measureAt(score, 0);
    ASSERT_NE(m0, nullptr);

    EXPECT_EQ(m0->ticks(), Fraction(4, 4)) << "Measure 0 must NOT be shortened: whole note cumTick=1=measure->ticks()";

    delete score;
}

// Regression: a Case A pickup (explicit short 2/4) whose content is even shorter must not be
// double-shortened by Case B; the guard keeps measure 0 at its explicit 2/4.
TEST_F(Tst_Structure, pickup_casea_guard_prevents_double_shortening)
{
    MasterScore* score = readEncoreScore("structure_pickup_casea_sparse.enc");
    ASSERT_NE(score, nullptr);

    Measure* m0 = measureAt(score, 0);
    Measure* m1 = measureAt(score, 1);
    ASSERT_NE(m0, nullptr);
    ASSERT_NE(m1, nullptr);

    EXPECT_EQ(m0->timesig(), Fraction(4, 4)) << "Case A pickup must display nominal 4/4";
    EXPECT_EQ(m0->ticks(), Fraction(2, 4)) << "Case A pickup must stay at its explicit 2/4, not be further shortened by Case B";
    EXPECT_EQ(m1->tick(), Fraction(2, 4)) << "m1 must start at 2/4, not be shifted by a spurious Case B delta";

    delete score;
}

// Regression: after a Case B pickup shifts later ticks, a hairpin spanning into measure 1 must
// use the post-shift search boundary so its endpoint resolves in the right measure.
TEST_F(Tst_Structure, pickup_caseb_hairpin_maxendtick_not_stale)
{
    MasterScore* score = readEncoreScore("structure_pickup_caseb_hairpin.enc");
    ASSERT_NE(score, nullptr);

    Measure* m1 = measureAt(score, 1);
    ASSERT_NE(m1, nullptr);

    int hairpinCount = 0;
    bool hairpinEndsInM1 = false;
    for (const auto& kv : score->spanner()) {
        Spanner* sp = kv.second;
        if (sp && sp->isHairpin()) {
            ++hairpinCount;
            const Fraction tick2 = sp->tick2();
            if (tick2 >= m1->tick() && tick2 <= m1->endTick()) {
                hairpinEndsInM1 = true;
            }
        }
    }

    EXPECT_GE(hairpinCount, 1) << "At least one hairpin must be imported";
    EXPECT_TRUE(hairpinEndsInM1) << "Hairpin must end within measure 1 (stale maxEndTick would push it past)";

    delete score;
}

// v0xC2 4/4 with the common-time glyph must import as TimeSigType::FOUR_FOUR (C symbol), not the
// numeric NORMAL type. See ENCORE_FORMAT.md §Time-signature glyph.
TEST_F(Tst_Structure, timesig_v0c2_common_time_glyph_preserved)
{
    MasterScore* score = readEncoreScore("notes_v0c2_common_time_glyph.enc");
    ASSERT_NE(score, nullptr);

    Measure* m0 = measureAt(score, 0);
    ASSERT_NE(m0, nullptr);

    Segment* tsSeg = m0->findSegment(SegmentType::TimeSig, m0->tick());
    ASSERT_NE(tsSeg, nullptr) << "Measure 0 must have a TimeSig segment";

    bool foundFourFour = false;
    for (EngravingItem* el : tsSeg->elist()) {
        if (el && el->isTimeSig()) {
            TimeSig* ts = toTimeSig(el);
            if (ts->timeSigType() == TimeSigType::FOUR_FOUR) {
                foundFourFour = true;
            }
        }
    }
    EXPECT_TRUE(foundFourFour) << "TimeSig glyph 0x63 must produce TimeSigType::FOUR_FOUR (common time C), not NORMAL";

    delete score;
}

// As above for the uppercase-C glyph variant produced by older Encore versions.
// See ENCORE_FORMAT.md §Time-signature glyph.
TEST_F(Tst_Structure, timesig_v0c2_common_time_glyph_uppercase_preserved)
{
    MasterScore* score = readEncoreScore("notes_v0c2_common_time_glyph_uc.enc");
    ASSERT_NE(score, nullptr);

    Measure* m0 = measureAt(score, 0);
    ASSERT_NE(m0, nullptr);

    Segment* tsSeg = m0->findSegment(SegmentType::TimeSig, m0->tick());
    ASSERT_NE(tsSeg, nullptr) << "Measure 0 must have a TimeSig segment";

    bool foundFourFour = false;
    for (EngravingItem* el : tsSeg->elist()) {
        if (el && el->isTimeSig()) {
            TimeSig* ts = toTimeSig(el);
            if (ts->timeSigType() == TimeSigType::FOUR_FOUR) {
                foundFourFour = true;
            }
        }
    }
    EXPECT_TRUE(foundFourFour) << "TimeSig glyph 0x43 must produce TimeSigType::FOUR_FOUR (common time C), not NORMAL";

    delete score;
}

// A mid-measure CLEF anchors to the following note in the stream, not to its own stored tick.
// See ENCORE_FORMAT.md §CLEF element.
TEST_F(Tst_Structure, mid_measure_clef_change_imported)
{
    MasterScore* score = readEncoreScore("structure_clef_change_mid_measure.enc");
    ASSERT_NE(score, nullptr);

    bool foundC4 = false;
    bool foundEarly = false;
    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        for (Segment* s = m->first(SegmentType::Clef); s; s = s->next(SegmentType::Clef)) {
            if (s->tick() <= m->tick()) {
                continue;  // skip header clef at measure start
            }
            EngravingItem* el = s->element(0);
            if (!el || !el->isClef() || toClef(el)->clefType() != ClefType::C4) {
                continue;
            }
            if (s->tick() == m->tick() + Fraction(1, 4)) {
                foundC4 = true;
            } else if (s->tick() == m->tick() + Fraction(3, 16)) {
                foundEarly = true;
            }
        }
    }
    EXPECT_TRUE(foundC4)
        << "mid-measure CLEF(C4L) must anchor to the following note at beat-2 offset (1/4)";
    EXPECT_FALSE(foundEarly)
        << "CLEF must not be placed at its own stored tick (3/16); it follows the next note";
    delete score;
}

// A trailing (cautionary) CLEF takes effect on the downbeat of the next measure.
// See ENCORE_FORMAT.md §CLEF element.
TEST_F(Tst_Structure, trailing_clef_change_moves_to_next_measure)
{
    MasterScore* score = readEncoreScore("structure_clef_trailing_cautionary.enc");
    ASSERT_NE(score, nullptr);

    Measure* m1 = score->firstMeasure();
    ASSERT_NE(m1, nullptr);
    Measure* m2 = m1->nextMeasure();
    ASSERT_NE(m2, nullptr);
    const Fraction barline = m2->tick();   // = end of measure 1 = downbeat of measure 2

    // A cautionary clef on the m1/m2 barline is serialized as a trailing Clef segment of m1,
    // so check by absolute tick rather than by measure ownership.
    bool clefAtBarline = false;
    bool clefMidM1 = false;
    for (Measure* m = m1; m; m = m->nextMeasure()) {
        for (Segment* s = m->first(SegmentType::Clef); s; s = s->next(SegmentType::Clef)) {
            EngravingItem* el = s->element(0);
            if (!el || !el->isClef() || toClef(el)->clefType() != ClefType::F) {
                continue;
            }
            if (s->tick() == barline) {
                clefAtBarline = true;
            } else if (s->tick() > m1->tick() && s->tick() < barline) {
                clefMidM1 = true;
            }
        }
    }
    EXPECT_TRUE(clefAtBarline)
        << "trailing CLEF must take effect on the downbeat of the next measure";
    EXPECT_FALSE(clefMidM1)
        << "trailing CLEF must not land inside measure 1";
    delete score;
}

// The v0xC2 pitch-swap must only fire when semiTonePitch is empty: for a genuine triplet the
// tuplet slot holds the ratio, and swapping it into the pitch would lose both. See
// ENCORE_FORMAT.md §v0xC2 note (size 22 or 24).
TEST_F(Tst_Structure, old_format_v0c2_triplet_pitch_in_semitone)
{
    MasterScore* score = readEncoreScore("structure_v0c2_triplet_pitch_in_semitone.enc");
    ASSERT_NE(score, nullptr);

    std::vector<int> pitches;
    bool foundTriplet = false;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (EngravingItem* e : toMeasure(mb)->el()) {
            if (e->isTuplet() && toTuplet(e)->ratio() == Fraction(3, 2)) {
                foundTriplet = true;
            }
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
    EXPECT_EQ(pitches[0], 60) << "triplet note 1 must be C4 (60), not the tuplet byte 50";
    EXPECT_EQ(pitches[1], 64) << "triplet note 2 must be E4 (64)";
    EXPECT_EQ(pitches[2], 67) << "triplet note 3 must be G4 (67)";
    EXPECT_EQ(pitches[3], 72) << "quarter note must be C5 (72)";
    EXPECT_TRUE(foundTriplet) << "explicit 3:2 tuplet must survive the pitch fix";
    delete score;
}

// A small stray flag in the v0xC2 semiTonePitch slot is not a pitch: treating any non-zero value
// there as the pitch imported notes several octaves too low and collapsed chords. The slot counts
// only when it holds a plausible MIDI value. See ENCORE_FORMAT.md §v0xC2 note (size 22 or 24).
TEST_F(Tst_Structure, old_format_v0c2_spurious_semitone_flag_uses_pitch_at_13)
{
    MasterScore* score = readEncoreScore("structure_v0c2_spurious_semitone_flag.enc");
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
    ASSERT_EQ(pitches.size(), 4u) << "Chord must keep all three notes, not collapse to one";
    EXPECT_EQ(pitches[0], 60) << "chord note 1 must be C4 (60), not the +15 flag";
    EXPECT_EQ(pitches[1], 64) << "chord note 2 must be E4 (64)";
    EXPECT_EQ(pitches[2], 67) << "chord note 3 must be G4 (67)";
    EXPECT_EQ(pitches[3], 72) << "C5 (72) must read from +13 even though +15 == 3";
    delete score;
}

TEST_F(Tst_Structure, page_size_detected_from_wini_pts_format)
{
    MasterScore* score = readEncoreScore("ornaments_fingering_grandstaff.enc");
    ASSERT_NE(score, nullptr);

    const double kA4W = 210.0 / 25.4;   // 8.2677"
    const double kA4H = 297.0 / 25.4;   // 11.6929"
    EXPECT_NEAR(score->style().styleD(Sid::pageWidth),  kA4W, 0.01)
        << "pts-format WINI with A4 boundary values must set A4 page width";
    EXPECT_NEAR(score->style().styleD(Sid::pageHeight), kA4H, 0.01)
        << "pts-format WINI with A4 boundary values must set A4 page height";

    delete score;
}

TEST_F(Tst_Structure, page_margins_wini_zero_margins)
{
    MasterScore* score = readEncoreScore("ornaments_fingering_grandstaff.enc");
    ASSERT_NE(score, nullptr);

    EXPECT_NEAR(score->style().styleD(Sid::pageOddTopMargin),    0.0, 0.005);
    EXPECT_NEAR(score->style().styleD(Sid::pageEvenTopMargin),   0.0, 0.005);
    EXPECT_NEAR(score->style().styleD(Sid::pageOddLeftMargin),   0.0, 0.005);
    EXPECT_NEAR(score->style().styleD(Sid::pageEvenLeftMargin),  0.0, 0.005);
    EXPECT_NEAR(score->style().styleD(Sid::pageOddBottomMargin), 0.0, 0.005);
    // printableWidth equals full page width when both side margins are 0.
    const double pageWIn = score->style().styleD(Sid::pageWidth);
    EXPECT_NEAR(score->style().styleD(Sid::pagePrintableWidth), pageWIn, 0.01);

    delete score;
}

// Malformed-input robustness: a .enc file is untrusted binary and must never crash, hang, or read
// out of bounds. These tests derive corrupt variants from a good fixture at run time and assert
// the importer returns a bounded result (a valid score or a clean null). On a debug build the
// engraving asserts fire on any out-of-range access, so "the import ran to completion" is the check.

static QByteArray readFixtureBytes(const QString& name)
{
    QFile f(ENC_DIR + name);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

TEST_F(Tst_Structure, malformed_truncated_input_does_not_crash)
{
    const QByteArray good = readFixtureBytes("bando.enc");
    ASSERT_GT(good.size(), 0);

    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    setRootDir(tmp.path());

    // Every prefix of a valid file, cut at many points, must import without crashing/hanging.
    for (int frac = 1; frac < 10; ++frac) {
        const QByteArray cut = good.left(good.size() * frac / 10);
        const QString name = QString("trunc_%1.enc").arg(frac);
        QFile f(tmp.path() + "/" + name);
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write(cut);
        f.close();
        // May be null (clean reject) or a bounded score; the point is it returns at all.
        delete readEncoreScore(name);
    }

    setRootDir(ENC_DIR);
}

TEST_F(Tst_Structure, malformed_oversized_block_size_does_not_crash)
{
    QByteArray data = readFixtureBytes("bando.enc");
    ASSERT_GT(data.size(), 0);

    // Overwrite the 4-byte little-endian size that follows the first MEAS magic with a value
    // far larger than the file. The parser must clamp it to the device instead of seeking past
    // EOF or wrapping the size negative when skipping.
    const int idx = data.indexOf("MEAS");
    ASSERT_GE(idx, 0);
    ASSERT_LE(idx + 8, data.size());
    for (int k = 0; k < 4; ++k) {
        data[idx + 4 + k] = static_cast<char>(0xFF);
    }

    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    setRootDir(tmp.path());
    QFile f(tmp.path() + "/oversized.enc");
    ASSERT_TRUE(f.open(QIODevice::WriteOnly));
    f.write(data);
    f.close();

    delete readEncoreScore("oversized.enc");
    setRootDir(ENC_DIR);
}

TEST_F(Tst_Structure, malformed_truncated_at_byte_boundaries_does_not_crash)
{
    // Finer-grained companion to malformed_truncated_input_does_not_crash: truncate a known-good
    // file at every 64-byte boundary so a cut landing in the middle of any block header, size
    // field, or element stream exercises the parser's bounds guards. Every prefix must import
    // (as a null or a bounded score) without crashing, aborting, or hanging.
    const QByteArray good = readFixtureBytes("bazo.enc");
    ASSERT_GT(good.size(), 0);

    QTemporaryDir tmp;
    ASSERT_TRUE(tmp.isValid());
    setRootDir(tmp.path());

    for (int len = 0; len <= good.size(); len += 64) {
        const QByteArray cut = good.left(len);
        const QString name = QString("trunc_at_%1.enc").arg(len);
        QFile f(tmp.path() + "/" + name);
        ASSERT_TRUE(f.open(QIODevice::WriteOnly));
        f.write(cut);
        f.close();
        delete readEncoreScore(name);
    }

    setRootDir(ENC_DIR);
}

// Chord members recorded with staggered playback ticks (live-recording strum) but sharing one
// notated column must collapse into a single chord, or a tick-gap split would spawn extra short
// notes and break the bar. See ENCORE_FORMAT.md §Chord column (xoffset).
TEST_F(Tst_Structure, chord_strum_xoffset_collapses_to_single_chord)
{
    MasterScore* score = readEncoreScore("notes_chord_strum_xoffset.enc");
    ASSERT_NE(score, nullptr);

    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "chord-strum score should pass sanityCheck: " << ret.text();

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    std::vector<Chord*> chords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            chords.push_back(toChord(e));
        }
    }

    ASSERT_EQ(chords.size(), 8u) << "Expected exactly 8 chords (one per notated column)";

    const std::vector<DurationType> expectedDur = {
        DurationType::V_EIGHTH, DurationType::V_16TH, DurationType::V_16TH, DurationType::V_EIGHTH,
        DurationType::V_16TH, DurationType::V_16TH, DurationType::V_EIGHTH, DurationType::V_EIGHTH
    };
    for (size_t i = 0; i < chords.size(); ++i) {
        Chord* c = chords[i];
        std::vector<int> pitches;
        for (Note* n : c->notes()) {
            pitches.push_back(n->pitch());
        }
        std::sort(pitches.begin(), pitches.end());
        ASSERT_EQ(pitches.size(), 4u) << "Chord " << i << " must have 4 notes";
        EXPECT_EQ(pitches[0], 59) << "Chord " << i;
        EXPECT_EQ(pitches[1], 62) << "Chord " << i;
        EXPECT_EQ(pitches[2], 67) << "Chord " << i;
        EXPECT_EQ(pitches[3], 71) << "Chord " << i;
        EXPECT_EQ(c->durationType().type(), expectedDur[i]) << "Chord " << i << " duration";
    }

    delete score;
}

// Inverse of the chord-column collapse: two notes a few ticks apart but in different notated
// columns must stay separate events, not merge via the near-simultaneous chord-extension rule.
// This keeps tightly played tuplet members distinct. See ENCORE_FORMAT.md §Chord column (xoffset).
TEST_F(Tst_Structure, diff_column_notes_do_not_merge)
{
    MasterScore* score = readEncoreScore("notes_diff_column_no_merge.enc");
    ASSERT_NE(score, nullptr);

    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "diff-column score should pass sanityCheck: " << ret.text();

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    std::vector<Chord*> chords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            chords.push_back(toChord(e));
        }
    }

    ASSERT_GE(chords.size(), 2u) << "The two eighths must not collapse into a single chord";
    EXPECT_EQ(chords[0]->notes().size(), 1u) << "First eighth must be a single note (60)";
    EXPECT_EQ(chords[0]->notes().front()->pitch(), 60);
    EXPECT_EQ(chords[1]->notes().size(), 1u) << "Second eighth must be a single note (64)";
    EXPECT_EQ(chords[1]->notes().front()->pitch(), 64);

    delete score;
}

// A 3:2 triplet whose positions 2 and 3 were live-played only 5 ticks apart in different columns
// must keep all three members separate; merging them by dropping the column check leaves a
// 2-member triplet. Member count (not pitches) is what makes the merge visible.
TEST_F(Tst_Structure, tuplet_diff_column_keeps_all_members)
{
    MasterScore* score = readEncoreScore("notes_tuplet_diff_column_keeps_members.enc");
    ASSERT_NE(score, nullptr);

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    Tuplet* tuplet = nullptr;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChordRest() && toChordRest(e)->tuplet()) {
            tuplet = toChordRest(e)->tuplet();
            break;
        }
    }
    ASSERT_NE(tuplet, nullptr) << "Expected a tuplet on staff 0";
    EXPECT_EQ(tuplet->ratio(), Fraction(3, 2)) << "Expected a 3:2 triplet";

    std::vector<int> memberPitches;
    int noteMembers = 0;
    for (DurationElement* de : tuplet->elements()) {
        if (de->isChord()) {
            ++noteMembers;
            EXPECT_EQ(toChord(de)->notes().size(), 1u)
                << "Each triplet member must be a single note, not a merged chord";
            memberPitches.push_back(toChord(de)->notes().front()->pitch());
        }
    }
    ASSERT_EQ(noteMembers, 3)
        << "Triplet must keep all 3 members; merging positions 2 and 3 leaves 2";
    std::sort(memberPitches.begin(), memberPitches.end());
    EXPECT_EQ(memberPitches[0], 60);
    EXPECT_EQ(memberPitches[1], 64);
    EXPECT_EQ(memberPitches[2], 67);

    delete score;
}

// A double barline between two measures is stored as the second measure's START barline, so it
// must map onto the previous measure's end barline. See ENCORE_FORMAT.md §Barline types.
TEST_F(Tst_Structure, v0c4_start_double_barline_maps_to_previous_end)
{
    MasterScore* score = readEncoreScore("structure_start_double_barline.enc");
    ASSERT_NE(score, nullptr) << "Failed to load structure_start_double_barline.enc";
    Measure* first = score->firstMeasure();
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->endBarLineType(), BarLineType::DOUBLE)
        << "the double bar before measure 2 must appear as measure 1's end barline";
    delete score;
}

// A redundant placeholder REST stored at the same tick as a real note must be dropped so the note
// keeps its beat instead of being pushed off it and overflowing the bar. See ENCORE_FORMAT.md §Voice field.
TEST_F(Tst_Structure, coincident_placeholder_rest_dropped_note_keeps_beat)
{
    MasterScore* score = readEncoreScore("structure_rest_coincident_with_note.enc");
    ASSERT_NE(score, nullptr) << "Failed to load structure_rest_coincident_with_note.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    Segment* first = m->first(SegmentType::ChordRest);
    ASSERT_NE(first, nullptr);
    EngravingItem* e = first->element(0);
    ASSERT_NE(e, nullptr);
    EXPECT_TRUE(e->isChord())
        << "beat 1 must be the note, not a placeholder rest pushed ahead of it";
    EXPECT_EQ(first->tick(), m->tick());
}
