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

// ===========================================================================
// FEATURE: Basic score structure
// ===========================================================================

TEST_F(Tst_Structure, basic_measure_count)
{
    // bazo.enc has 5 measures (from ref.txt: 2 systems × ~2-3 measures each)
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
    // bando.enc: band score with multiple instruments
    MasterScore* score = readEncoreScore("bando.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->parts().size(), 1u) << "bando.enc should have multiple parts";
    EXPECT_GT(score->nstaves(), 1u);
    delete score;
}

// ===========================================================================
// FEATURE: Time signatures
// ===========================================================================

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
    // notes_triplets.enc is 3/4
    MasterScore* score = readEncoreScore("notes_triplets.enc");
    ASSERT_NE(score, nullptr);
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(3, 4)) << "Synthetic triplet file should be 3/4";
    delete score;
}

TEST_F(Tst_Structure, time_sig_2_4)
{
    // Well, Licky Hear measure 1 is 2/4
    MasterScore* score = readEncoreScore("notes_swing.enc");
    ASSERT_NE(score, nullptr);
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(2, 4)) << "First measure should be 2/4";
    delete score;
}

// ===========================================================================
// FEATURE: Key signatures (encKeyToFifths table)
// ===========================================================================

TEST_F(Tst_Structure, key_sig_no_accidentals)
{
    // bazo.enc is in C major (key index 0 = 0 sharps/flats)
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
    // Before fix, key index 8 (G major = 1 sharp) was treated as key-256 = -248.
    // After fix, encKeyToFifths(8) = 1.
    // Verify no staff has a key > 7 or < -7.
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


// FIX: KEYCHANGE elements with tipo=0 (modulation to C major) are now emitted
// (P4.10). The previous guard skipped them, dropping ~24 of 40 key signatures
// on Beethoven's Plectro arrangement.
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
    // Expect: initial key sig (G major from measure 0) + tipo=0 modulation
    // signature at measure 1. Without the fix, measure 1 would have no key sig.
    EXPECT_GE(keySigCount, 2);
    delete score;
}

// ===========================================================================
// FEATURE: Every Encore navigation option survives the import.
// Encore exposes ten jump / section options in its UI:
//   Segno, Coda, To Coda, Fine, D.C., D.C. al Coda, D.C. al Fine,
//   D.S., D.S. al Coda, D.S. al Fine.
// Three (Segno, Coda, To Coda) travel via ORN tipos 0xA2 / 0xA6 / 0xA5;
// the rest are encoded in the MEAS header coda byte (offset 0x1A low
// byte). The fixture exercises every variant; the importer must create
// the matching Marker (Segno/Coda/To Coda/Fine) or Jump (D.C./D.S. ...)
// element on the right measure.
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
    // Coda glyph comes from ORN 0xA6 AND coda byte 0x89 (CODA2). Coda byte
    // 0x85 (CODA1) is the source measure for "To Coda" and produces a
    // TOCODA marker instead, so codaMarkers counts the two coda-glyph
    // sources only.
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
// FEATURE: Jump marks (To Coda + D.S. / D.C. al Coda / Fine).
// Encore stores "To Coda" as ORN tipo=0xA5 (a measure-attached marker)
// and the per-measure repeat-mark byte at the LOW byte of the coda u32
// at MEAS header offset 0x1A. The previous accessor extracted byte+1 of
// the u32 and consequently missed every DCALCODA / DSALCODA / FINE / DC
// directive. After the fix, m2 (codaByte=0x81) becomes a Jump element
// with D.S. al Coda text, m3 (codaByte=0x87) becomes a D.C. jump, and
// the ORN 0xA5 in m1 becomes a TOCODA marker.
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
// BUG FIX: v0xC2 (old Encore format) — pitch stored at tuplet field offset
// ===========================================================================
// Synthetic files generated by tools/gen_enc_test_files.py (in the test data
// directory).  Each file is hand-crafted to exercise exactly one parsing path.

TEST_F(Tst_Structure, old_format_v0c2_correct_pitches)
{
    // structure_v0c2_pitches.enc: chuMagio=0xC2, 4/4, 4 quarter notes C-E-G-C.
    // In v0xC2 notes have size=22 and MIDI pitch stored at the tuplet-field byte
    // (+13 from elemStart).  needsPitchFix swaps it to semiTonePitch and clears tuplet.
    // Without the fix all notes would have pitch=0.
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
    // structure_v0c2_triplets.enc: 2/4, 6 eighth notes at 80-tick spacing.
    // 80 Encore ticks = 2/3 of an eighth → detectImpliedTuplet returns 3:2.
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
    // structure_v0xa6_basic.enc: chuMagio=0xA6, 2 measures of 2/4, 4 eighth notes each.
    // elemOffset must be 0x1A (not 0x3E).  Wrong offset causes tick=1280 > durTicks=480
    // for all elements, silently dropping all notes and leaving empty measures.
    MasterScore* score = readEncoreScore("structure_v0xa6_basic.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_EQ(score->nmeasures(), 2);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "v0xA6 synthetic file should pass sanityCheck: " << ret.text();
    delete score;
}

TEST_F(Tst_Structure, very_old_format_v0xa6_pitch_encoding)
{
    // v0xA6 pitch is the absolute MIDI value (0..127) at elemStart+11
    // -- the first byte of the 20-byte slot's padding region. The
    // synthetic helper writes MIDI 60+pitch_offset there; for offsets
    // 0,+2,+4,+7 the imported pitches must be 60,62,64,67 (C D E G).
    // The earlier reader looked at byte +9 with a signed-offset +60
    // formula; on real Encore 2.x files +9 holds a staff-position
    // field, so that produced wrong sounding pitches.
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
// FEATURE: System breaks from Encore LINE blocks.
// The skeleton used by all generated fixtures (bazo.enc) declares two LINE
// blocks: system 0 covers measures 0..2 (measureCount=3), system 1 covers
// measures 3..5.  The importer must place a LINE break on the last measure
// of system 0 (index 2) and no break on the last measure of system 1
// (index 5, last system never gets a break).
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
