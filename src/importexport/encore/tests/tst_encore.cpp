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

#include "testbase.h"

static const QString ENC_DIR(QString(iex_encore_tests_DATA_ROOT) + "/data/");

using namespace mu::engraving;

// ---------------------------------------------------------------------------
// Test that the importer can open each sample file without crashing
// and produce a non-empty score
// ---------------------------------------------------------------------------

class Tst_Encore : public ::testing::Test, public MTest
{
protected:
    void SetUp() override
    {
        setRootDir(ENC_DIR);
    }
};

TEST_F(Tst_Encore, bazo)
{
    MasterScore* score = readEncoreScore("bazo.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Encore, akordo)
{
    MasterScore* score = readEncoreScore("akordo.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Encore, ripetoj)
{
    MasterScore* score = readEncoreScore("ripetoj.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Encore, opeco_vochoj)
{
    MasterScore* score = readEncoreScore("opeco_vochoj.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Encore, bando)
{
    MasterScore* score = readEncoreScore("bando.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Encore, kordorkestro)
{
    MasterScore* score = readEncoreScore("kordorkestro.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Encore, chord_parsing)
{
    MasterScore* score = readEncoreScore("chord_parsing.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Score has corrupted measures: " << ret.text();
    delete score;
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Synthetic files covering behaviors from the Encore 5.0 example files.
// The original example files (Beethoven.enc, Opus27.enc, etc.) are distributed
// with Encore software and cannot be included in an open-source repository.
// ---------------------------------------------------------------------------

#define ENC_SANITY_TEST(testName, fileName) \
    TEST_F(Tst_Encore, testName) { \
        MasterScore* score = readEncoreScore(fileName); \
        ASSERT_NE(score, nullptr) << "Failed to load " << fileName; \
        EXPECT_GT(score->nmeasures(), 0); \
        muse::Ret ret = score->sanityCheck(); \
        EXPECT_TRUE(ret) << "Corrupted: " << ret.text(); \
        delete score; \
    }

// Covers: tuplet=0xFF (degenerate), faceValue=0 (invalid), voice>=4, open SLURSTART
// (previously tested by Beethoven.enc and Opus 27 First Movement.enc)
ENC_SANITY_TEST(corrupted_elements,         "synthetic_v0c4_corrupted.enc")

// Covers: tiny realDuration (<15 ticks) skipped, dotted rest, 2/4 time sig
// (previously tested by Well, Licky Hear.enc)
// No sanityCheck: swing timing files produce slight measure shortfalls (by design).
TEST_F(Tst_Encore, swing_timing)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_swing.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

// Covers: explicit 3:2 triplets, 3/4 time sig, multi-measure
// (previously tested by Chansonette.enc and other 3/4 files)
ENC_SANITY_TEST(explicit_triplets_3_4,      "synthetic_v0c4_triplets.enc")

// Covers: grace note filtering (fv>=4 only), ACCIACCATURA
// (previously tested by Grace.enc and Beethoven.enc)
ENC_SANITY_TEST(grace_notes,                "synthetic_v0c4_grace.enc")

// Regression for the beam-layout crash on grace + adjacent beamed eighths.
// The old importer attached grace chords to a Segment, so beam layout pulled
// them into a beam group and Chord::pagePos asserted toChord(explicitParent())
// against a Segment instead of the main Chord. readEncoreScore runs doLayout,
// so just loading this fixture proves the path is crash-free. We also assert
// the grace lives under a main chord's graceNotes() and the segment-attached
// chord is NORMAL, which is the structural invariant the fix establishes.
TEST_F(Tst_Encore, grace_with_beamed_eighths_no_layout_crash)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_grace_beam.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0c4_grace_beam.enc";
    EXPECT_GT(score->nmeasures(), 0);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();

    bool foundGrace = false;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->elist()) {
                if (!e || !e->isChord()) {
                    continue;
                }
                Chord* c = toChord(e);
                EXPECT_EQ(c->noteType(), NoteType::NORMAL)
                    << "Segment-attached chord must be NORMAL; grace chords belong in graceNotes()";
                for (Chord* gc : c->graceNotes()) {
                    if (gc->noteType() != NoteType::NORMAL) {
                        foundGrace = true;
                    }
                }
            }
        }
    }
    EXPECT_TRUE(foundGrace) << "Grace eighth should be attached as a graceNotes() child";
    delete score;
}

// Regression for the multi-stream voice switch loop. With a single switch the
// importer would happily place a note in a target voice that was already full
// (e.g. occupied by a half rest), overrunning the measure. The loop fix keeps
// switching until it finds a voice with remaining space.
TEST_F(Tst_Encore, multi_stream_switch_skips_voice_filled_by_rest)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_full_voice_skipped.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0c4_full_voice_skipped.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();
    delete score;
}

// Regression for the non-tuplet branch of the second cap. An explicit-but-
// isolated tuplet note that gets treated as plain (tupAdv != remaining) had
// its chord placed with the face value, but the cumTick advance was capped
// to fit the remaining measure space. Without updating the chord's ticks the
// voice overran by face - capped. The fix always sets the chord duration to
// the capped value.
TEST_F(Tst_Encore, isolated_explicit_tuplet_caps_chord_ticks)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_isolated_explicit_tuplet_capped.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0c4_isolated_explicit_tuplet_capped.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();
    delete score;
}

// Regression for treating a rest as a chord-extension anchor. Previously both
// notes and rests updated prevMidiTick, so a note arriving at the same MIDI
// tick as a recent rest was mis-detected as a chord extension; the importer
// then silently replaced the rest's segment with the note's chord while
// cumTick still carried the rest's contribution, producing voice/cumTick
// mismatch and a measure that fails sanityCheck.
TEST_F(Tst_Encore, rest_does_not_anchor_chord_extension)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_rest_not_chord_anchor.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();
    delete score;
}

// Regression for the rest path's second cap. A rest arriving with a tuplet
// still open had its first cap skipped; the tuplet then closed (rest has no
// tuplet bytes); the second cap shortened the cumTick advance but left the
// rest's ticks at the uncapped face value. cr->actualTicks() then exceeded
// the actual cumTick advance and the voice overran the measure.
TEST_F(Tst_Encore, rest_caps_its_ticks_when_advance_is_capped)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_rest_caps_in_open_tuplet.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();
    delete score;
}

// Regression for the duplicate `if (tt.inTuplet()) tt.placedTicks += advance;`
// in the rest path. The bug double-counted a rest's contribution to the
// tuplet's placedTicks, masking the tuplet's undershoot and skipping the
// closeTuplet shrink. checkMeasure then reported the resulting non-standard
// gap as Incomplete measure. The fixture has a 3:2 quarter triplet whose
// first member is a quarter rest, followed by two eighth-note chords; total
// content sums to 1/3 of the 2/4 measure (= 1/6 + 1/12 + 1/12) and the
// tuplet must shrink to 1/3 so the remaining 1/6 can be filled by checkMeasure.
TEST_F(Tst_Encore, rest_in_tuplet_does_not_double_count_placed_ticks)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_rest_in_tuplet.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0c4_rest_in_tuplet.enc";
    EXPECT_GT(score->nmeasures(), 0);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();
    delete score;
}
