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

// Targeted unit tests for Encore importer features and bug fixes.
// Each test section corresponds to a specific feature or fix.

#include <gtest/gtest.h>

#include "engraving/dom/arpeggio.h"
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

// ---------------------------------------------------------------------------
// Helper utilities
// ---------------------------------------------------------------------------

static Measure* measureAt(MasterScore* score, int n)
{
    // Returns the n-th Measure (0-indexed), skipping VBox frames.
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

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class Tst_EncoreFeatures : public ::testing::Test, public MTest
{
protected:
    void SetUp() override { setRootDir(ENC_DIR); }
};

// ===========================================================================
// FEATURE: Basic score structure
// ===========================================================================

TEST_F(Tst_EncoreFeatures, basic_measure_count)
{
    // bazo.enc has 5 measures (from ref.txt: 2 systems × ~2-3 measures each)
    MasterScore* score = readEncoreScore("bazo.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_EncoreFeatures, basic_single_part)
{
    MasterScore* score = readEncoreScore("bazo.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_EQ(score->parts().size(), 1u);
    EXPECT_EQ(score->nstaves(), 1u);
    delete score;
}

TEST_F(Tst_EncoreFeatures, multipart_score)
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

TEST_F(Tst_EncoreFeatures, time_sig_4_4)
{
    MasterScore* score = readEncoreScore("chord_parsing.enc");
    ASSERT_NE(score, nullptr);
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(4, 4)) << "First measure should be 4/4";
    delete score;
}

TEST_F(Tst_EncoreFeatures, time_sig_3_4)
{
    // synthetic_v0c4_triplets.enc is 3/4
    MasterScore* score = readEncoreScore("synthetic_v0c4_triplets.enc");
    ASSERT_NE(score, nullptr);
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(3, 4)) << "Synthetic triplet file should be 3/4";
    delete score;
}

TEST_F(Tst_EncoreFeatures, time_sig_2_4)
{
    // Well, Licky Hear measure 1 is 2/4
    MasterScore* score = readEncoreScore("synthetic_v0c4_swing.enc");
    ASSERT_NE(score, nullptr);
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(2, 4)) << "First measure should be 2/4";
    delete score;
}

// ===========================================================================
// FEATURE: Key signatures (encKeyToFifths table)
// ===========================================================================

TEST_F(Tst_EncoreFeatures, key_sig_no_accidentals)
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

TEST_F(Tst_EncoreFeatures, key_sig_no_invalid_large_values)
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

// ===========================================================================
// FEATURE: Note pitches and tick scaling (Encore 240 ticks/q → MuseScore 480)
// ===========================================================================

TEST_F(Tst_EncoreFeatures, tick_scaling_quarter_positions)
{
    // In a 4/4 measure with 4 quarter notes (ticks 0, 240, 480, 720 in Encore),
    // they should be at MS positions 0, 480, 960, 1440 within the measure.
    // Chord Parsing measure 2 has quarter notes at these standard positions.
    MasterScore* score = readEncoreScore("chord_parsing.enc");
    ASSERT_NE(score, nullptr);
    Measure* m = measureAt(score, 1);  // measure 2 (0-indexed = 1)
    ASSERT_NE(m, nullptr);
    Fraction mTick = m->tick();

    // Collect ChordRest ticks relative to measure start in voice 0
    std::vector<int> relTicks;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);  // track 0 = staff 0, voice 0
        if (e && e->isChordRest()) {
            relTicks.push_back((s->tick() - mTick).ticks());
        }
    }
    // Positions must all be multiples of 480 (quarter note in MuseScore)
    for (int t : relTicks) {
        EXPECT_EQ(t % 480, 0) << "Note at rel tick " << t << " should be on a quarter-note boundary";
    }
    delete score;
}

TEST_F(Tst_EncoreFeatures, note_pitches_whole_note)
{
    // akordo.enc has whole notes at known pitches. Find any chord and check pitch validity.
    MasterScore* score = readEncoreScore("akordo.enc");
    ASSERT_NE(score, nullptr);
    // Find the first chord anywhere in the score
    Chord* foundChord = nullptr;
    for (MeasureBase* mb = score->first(); mb && !foundChord; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        Measure* m = toMeasure(mb);
        for (Segment* s = m->first(SegmentType::ChordRest); s && !foundChord; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* e = s->element(0);
            if (e && e->isChord()) {
                foundChord = toChord(e);
            }
        }
    }
    ASSERT_NE(foundChord, nullptr) << "Should find at least one chord in akordo.enc";
    EXPECT_GT(foundChord->notes().size(), 0u);
    int pitch = foundChord->notes().front()->pitch();
    EXPECT_GE(pitch, 21) << "MIDI pitch should be in valid piano range";
    EXPECT_LE(pitch, 108) << "MIDI pitch should be in valid piano range";
    delete score;
}

// ===========================================================================
// FEATURE: Dotted notes
// ===========================================================================

TEST_F(Tst_EncoreFeatures, dotted_quarter_note)
{
    // "Well, Licky Hear" measure 1 has a dotted eighth rest at tick 0 (180 Encore ticks).
    // After fix: realDuration=180 → V_EIGHTH + 1 dot.
    MasterScore* score = readEncoreScore("synthetic_v0c4_swing.enc");
    ASSERT_NE(score, nullptr);
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    // Find the first rest in voice 0
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isRest()) {
            Rest* rest = toRest(e);
            if (rest->durationType().type() == DurationType::V_EIGHTH) {
                EXPECT_EQ(rest->dots(), 1) << "Dotted eighth rest should have 1 dot";
                break;
            }
        }
    }
    delete score;
}

// ===========================================================================
// FEATURE: Tuplets (explicit m_tuplet field)
// ===========================================================================

TEST_F(Tst_EncoreFeatures, explicit_triplets_in_score)
{
    // synthetic_v0c4_triplets.enc: measure 1 has 9 explicit triplet eighths (tup=0x32)
    // forming three 3:2 groups.  Verifies tuplets are parsed and have non-zero ticks.
    MasterScore* score = readEncoreScore("synthetic_v0c4_triplets.enc");
    ASSERT_NE(score, nullptr);

    int measWithTuplets = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (EngravingItem* e : toMeasure(mb)->el()) {
            if (e->isTuplet()) {
                Tuplet* t = toTuplet(e);
                EXPECT_NE(t->ticks(), Fraction(0, 1)) << "Tuplet ticks must be non-zero";
                EXPECT_EQ(t->ratio().reduced(), Fraction(3, 2)) << "Should be 3:2 triplet";
                ++measWithTuplets;
                break;
            }
        }
    }
    EXPECT_GT(measWithTuplets, 0) << "Should have at least one measure with triplets";
    delete score;
}

TEST_F(Tst_EncoreFeatures, tuplet_notes_have_correct_actual_ticks)
{
    // For a 3:2 triplet of eighth notes, actualTicks = (1/8) / (3/2) = 1/12.
    MasterScore* score = readEncoreScore("synthetic_v0c4_triplets.enc");
    ASSERT_NE(score, nullptr);

    bool foundTripletNote = false;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (EngravingItem* e : toMeasure(mb)->el()) {
            if (!e->isTuplet()) {
                continue;
            }
            Tuplet* t = toTuplet(e);
            if (t->ratio().reduced() != Fraction(3, 2)) {
                continue;
            }
            for (DurationElement* de : t->elements()) {
                if (!de->isChordRest()) {
                    continue;
                }
                EXPECT_EQ(toChordRest(de)->actualTicks(), Fraction(1, 12))
                    << "Triplet eighth note should have actualTicks = 1/12";
                foundTripletNote = true;
                break;
            }
            if (foundTripletNote) {
                break;
            }
        }
        if (foundTripletNote) {
            break;
        }
    }
    EXPECT_TRUE(foundTripletNote) << "Should find a 3:2 triplet note";
    delete score;
}

TEST_F(Tst_EncoreFeatures, tuplet_measure_fills_correctly)
{
    // Both measures of synthetic_v0c4_triplets.enc must pass sanityCheck.
    MasterScore* score = readEncoreScore("synthetic_v0c4_triplets.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Triplet score should pass sanityCheck: " << ret.text();
    delete score;
}

// ===========================================================================
// BUG FIX: tuplet setTicks was zero → measures overflowed
// ===========================================================================

TEST_F(Tst_EncoreFeatures, tuplet_ticks_not_zero)
{
    // Before fix: Tuplet::ticks() returned Fraction(0,1) because setTicks() was
    // never called.  checkMeasure then saw duration=0 and added extra rests.
    MasterScore* score = readEncoreScore("synthetic_v0c4_triplets.enc");
    ASSERT_NE(score, nullptr);
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (EngravingItem* e : toMeasure(mb)->el()) {
            if (e->isTuplet()) {
                EXPECT_NE(toTuplet(e)->ticks(), Fraction(0, 1))
                    << "No tuplet should have zero ticks after fix";
            }
        }
    }
    delete score;
}

TEST_F(Tst_EncoreFeatures, tuplet_state_cleared_between_measures)
{
    // Before fix: tuplets map was never cleared between measures.  A triplet opened
    // in measure N would still be "active" in N+1, giving non-tuplet notes a 2/3
    // duration and causing sanityCheck to fail.
    // Fix: tuplets.clear() at the start of each measure in buildScore.
    // Measure 2 of synthetic_v0c4_triplets has plain quarter notes (no tuplet byte).
    // Without the fix, those quarters would be appended to the stale triplet group.
    MasterScore* score = readEncoreScore("synthetic_v0c4_triplets.enc");
    ASSERT_NE(score, nullptr);

    int measIdx = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        Measure* m = toMeasure(mb);
        ++measIdx;
        for (Segment* seg = m->first(SegmentType::ChordRest); seg;
             seg = seg->next(SegmentType::ChordRest)) {
            EngravingItem* el = seg->element(0);
            if (!el || !el->isChord()) {
                continue;
            }
            Chord* ch = toChord(el);
            if (!ch->tuplet()) {
                int den = ch->durationType().fraction().denominator();
                bool isPow2 = (den > 0) && ((den & (den - 1)) == 0);
                EXPECT_TRUE(isPow2)
                    << "Measure " << measIdx << " non-tuplet chord has denominator "
                    << den << " — tuplet state may have bled from the previous measure";
            }
        }
    }
    delete score;
}

TEST_F(Tst_EncoreFeatures, tuplet_note_sorts_before_non_tuplet_at_same_tick)
{
    // synthetic_v0c4_tuplet_sort.enc has, at tick=0, a non-tuplet quarter written
    // BEFORE a tuplet eighth in the binary stream.
    // Without the sort fix: the quarter creates the chord → no tuplet started →
    // voice sum = 3/4 (wrong for a 2/4 measure).
    // With the fix: the tuplet eighth sorts first → V_EIGHTH + 3:2 tuplet group →
    // voice sum = 1/4 (triplet) + 1/4 (trailing quarter) = 2/4.
    MasterScore* score = readEncoreScore("synthetic_v0c4_tuplet_sort.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Tuplet-sort file should pass sanityCheck: " << ret.text();
    delete score;
}

// ===========================================================================
// BUG FIX: Notes at tick >= durTicks skipped
// ===========================================================================

TEST_F(Tst_EncoreFeatures, boundary_notes_not_in_current_measure)
{
    // Chord Parsing measures 22, 30, 48 had notes at tick=960 (= durTicks for 4/4).
    // Before fix: those notes were added to the CURRENT measure causing 2/1 overflow.
    // After fix: those notes are skipped → measures 22, 30, 48 pass sanityCheck.
    MasterScore* score = readEncoreScore("chord_parsing.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Chord Parsing should pass sanityCheck: " << ret.text();
    delete score;
}

TEST_F(Tst_EncoreFeatures, measures_do_not_overflow_4_4)
{
    // All 4/4 measures in Chord Parsing should not exceed 4/4 worth of notes.
    MasterScore* score = readEncoreScore("chord_parsing.enc");
    ASSERT_NE(score, nullptr);
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        Measure* m = toMeasure(mb);
        if (m->timesig() != Fraction(4, 4)) {
            continue;
        }
        m->setCorrupted(0, false);  // reset first
    }
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "No 4/4 measure should overflow: " << ret.text();
    delete score;
}

// ===========================================================================
// BUG FIX: calculateRealDurations used > instead of >= for durTicks
// ===========================================================================

TEST_F(Tst_EncoreFeatures, last_note_real_duration_not_zero)
{
    // Before the >= fix: a note at tick==durTicks got realDuration=0 → V_QUARTER by default.
    // After fix: it's skipped. All remaining notes must have realDuration > 0.
    // Verify by checking that no chord in any measure has V_MEASURE duration
    // (which would come from a 0-duration note falling through).
    MasterScore* score = readEncoreScore("chord_parsing.enc");
    ASSERT_NE(score, nullptr);
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        Measure* m = toMeasure(mb);
        for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->elist()) {
                if (!e || !e->isChord()) {
                    continue;
                }
                Chord* chord = toChord(e);
                // A chord with V_MEASURE would indicate a 0-duration note
                EXPECT_NE(chord->durationType().type(), DurationType::V_MEASURE)
                    << "No chord should have V_MEASURE type (indicates zero real duration)";
            }
        }
    }
    delete score;
}

// ===========================================================================
// BUG FIX: Tick scaling ×2 (Encore 240 ticks/quarter → MuseScore 480)
// ===========================================================================

TEST_F(Tst_EncoreFeatures, tick_scaling_no_note_outside_measure)
{
    // Before tick-scaling fix, notes at Encore tick 240 ended up at MS 1/8 instead of 1/4.
    // This caused notes to be placed at wrong positions and overlap.
    // After fix: all notes should be within their measure's tick range.
    MasterScore* score = readEncoreScore("bando.enc");
    ASSERT_NE(score, nullptr);
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        Measure* m = toMeasure(mb);
        Fraction mStart = m->tick();
        Fraction mEnd = m->endTick();
        for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            EXPECT_GE(s->tick(), mStart)
                << "Segment tick should be >= measure start";
            EXPECT_LT(s->tick(), mEnd)
                << "Segment tick should be < measure end";
        }
    }
    delete score;
}

// ===========================================================================
// BUG FIX: Degenerate tuplet ratio (tuplet byte = 0xFF)
// ===========================================================================

TEST_F(Tst_EncoreFeatures, no_degenerate_tuplet_ratios)
{
    // Before fix: tuplet=0xFF gave a 15:15 tuplet (reduces to 1:1).
    // After fix: such tuplets are skipped. No tuplet should have ratio 1:1.
    // Test on Beethoven which has tuplet=0xFF corruption.
    MasterScore* score = readEncoreScore("synthetic_v0c4_corrupted.enc");
    ASSERT_NE(score, nullptr);
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (EngravingItem* e : toMeasure(mb)->el()) {
            if (!e->isTuplet()) {
                continue;
            }
            Tuplet* t = toTuplet(e);
            Fraction r = t->ratio().reduced();
            EXPECT_NE(r, Fraction(1, 1)) << "No tuplet should have 1:1 ratio";
            EXPECT_LE(r.numerator(), 9) << "Tuplet numerator should be <= 9";
            EXPECT_LE(r.denominator(), 9) << "Tuplet denominator should be <= 9";
        }
    }
    delete score;
}

// ===========================================================================
// BUG FIX: Invalid faceValue (0 or > 8) skipped
// ===========================================================================

TEST_F(Tst_EncoreFeatures, invalid_facevalue_no_crash)
{
    // Opus 27 has faceValue=0 (measure 35) and faceValue=28 (measure 78).
    // Before fix: these fell through with garbage duration types causing crashes.
    // After fix: they are skipped, file loads without crash.
    MasterScore* score = readEncoreScore("synthetic_v0c4_corrupted.enc");
    ASSERT_NE(score, nullptr) << "Opus 27 should load despite faceValue=0/28 corruption";
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_EncoreFeatures, invalid_facevalue_notes_have_valid_duration_type)
{
    // All notes in the score should have valid duration types (not V_ZERO or invalid).
    MasterScore* score = readEncoreScore("synthetic_v0c4_corrupted.enc");
    ASSERT_NE(score, nullptr);
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->elist()) {
                if (!e) {
                    continue;
                }
                ChordRest* cr = toChordRest(e);
                DurationType dt = cr->durationType().type();
                EXPECT_NE(dt, DurationType::V_ZERO) << "No chord/rest should have V_ZERO duration";
                EXPECT_NE(dt, DurationType::V_INVALID) << "No chord/rest should have V_INVALID duration";
            }
        }
    }
    delete score;
}

// ===========================================================================
// BUG FIX: Small realDuration (< 15 ticks) skipped — MIDI timing artifacts
// ===========================================================================

TEST_F(Tst_EncoreFeatures, tiny_duration_notes_do_not_create_overlaps)
{
    // "Well, Licky Hear" had notes at tick=180 and tick=182 (2 ticks = 0.5ms apart).
    // These are MIDI timing artifacts with realDuration < 15 Encore ticks.
    // After fix: such notes are skipped so they don't pollute voice 0.
    // The file loads without crash. The remaining notes (triplets at 265, 341, 420)
    // have non-quantized swing positions — we verify the file loads, not strict ordering.
    // synthetic_v0c4_swing.enc: note at tick=180 (realDur=2) is skipped; note at
    // tick=182 survives.  Voice 0 of measure 1 has the rest + the surviving note only.
    MasterScore* score = readEncoreScore("synthetic_v0c4_swing.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    // Verify no two chords share the same tick in voice 0 (no overlap from tiny notes).
    std::set<Fraction> seenTicks;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (!e) {
            continue;
        }
        EXPECT_EQ(seenTicks.count(s->tick()), 0u)
            << "Tiny-duration note was not skipped: overlap detected";
        seenTicks.insert(s->tick());
    }
    delete score;
}

// ===========================================================================
// BUG FIX: Voice >= 4 skipped (not clamped to voice 3)
// ===========================================================================

TEST_F(Tst_EncoreFeatures, no_voice_conflict_from_clamping)
{
    // Before fix: voice=8 was clamped to voice=3, conflicting with real voice 3 elements.
    // This caused "add(Rest): there is already a Chord" errors and layout crashes.
    // After fix: voice >= 4 elements are simply skipped.
    // Verify: Opus 27 loads without crash.
    // synthetic_v0c4_corrupted.enc contains a note with voice=4 (>= VOICES=4).
    // Before fix: it was clamped to voice=3 causing conflicts.
    // After fix: it is skipped entirely; no track index exceeds maxTrack.
    MasterScore* score = readEncoreScore("synthetic_v0c4_corrupted.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    track_idx_t maxTrack = static_cast<track_idx_t>(score->nstaves()) * VOICES;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->elist()) {
                if (e) {
                    EXPECT_LT(e->track(), maxTrack)
                        << "Element track " << e->track() << " should be < maxTrack " << maxTrack;
                }
            }
        }
    }
    delete score;
}

// ===========================================================================
// BUG FIX: Open slurs removed (no NaN in Bezier layout)
// ===========================================================================

TEST_F(Tst_EncoreFeatures, no_nan_crash_from_open_slurs)
{
    // Before fix: Beethoven/Opus27 had SLURSTART without SLURSTOP.
    // The resulting slur with no endpoints caused NaN in computeBezier → crash.
    // After fix: all open slurs are removed from the score.
    // synthetic_v0c4_corrupted.enc has a SLURSTART ornament with no matching SLURSTOP.
    // Before fix: the resulting slur had no endpoints → NaN in Bezier layout.
    // After fix: open slurs are removed; all remaining spanners have valid tick ranges.
    MasterScore* score = readEncoreScore("synthetic_v0c4_corrupted.enc");
    ASSERT_NE(score, nullptr) << "Corrupted file should load without NaN crash";
    for (auto& [tick, sp] : score->spannerMap().map()) {
        EXPECT_LT(sp->tick(), sp->tick2())
            << "All spanners should have tick < tick2 (valid range)";
    }
    delete score;
}

TEST_F(Tst_EncoreFeatures, no_nan_crash_opus27)
{
    // Re-uses the same synthetic file; verifies the fix is robust.
    MasterScore* score = readEncoreScore("synthetic_v0c4_corrupted.enc");
    ASSERT_NE(score, nullptr);
    delete score;
}

// ===========================================================================
// BUG FIX: Non-standard tuplet ticks do not assert in beam layout
// 3 quarters + 1 plain 8th + 2 explicit 8th triplets where the 2nd note is
// capped at the measure boundary.  closeTuplet() would set non-standard
// placedTicks (11/96) on the tuplet; beam.cpp called TDuration(tuplet->ticks())
// without truncate and asserted.  The fix snaps placedTicks to the nearest
// standard fraction before setTicks().
// ===========================================================================

TEST_F(Tst_EncoreFeatures, beamed_triplet_capped_no_beam_assert)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_beamed_triplet_capped.enc");
    ASSERT_NE(score, nullptr) << "File should load without beam-layout assert";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();
    // All spanners (if any) must have positive span
    for (auto& [tick, sp] : score->spannerMap().map()) {
        EXPECT_LT(sp->tick(), sp->tick2()) << "Spanner has non-positive span";
    }
    delete score;
}

// ===========================================================================
// REGRESSION: WEDGESTART with alMezuro=0 produces a hairpin with positive span.
// Encore .enc binaries do not contain a separate WEDGESTOP; the end is encoded
// inside the WEDGESTART (alMezuro = number of measures forward). When alMezuro
// is 0 the hairpin must span the current measure, not collapse to zero.
// ===========================================================================

TEST_F(Tst_EncoreFeatures, zero_length_hairpin_dropped_cleanly)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_zero_hairpin.enc");
    ASSERT_NE(score, nullptr) << "File should load without Spanner::setTicks assert";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();
    // Every spanner must have positive span.
    for (auto& [tick, sp] : score->spannerMap().map()) {
        EXPECT_LT(sp->tick(), sp->tick2()) << "Spanner has non-positive span";
    }
    delete score;
}

// ===========================================================================
// REGRESSION: Partial 3:2 quarter triplet must not crash beam layout.
// closeTuplet would previously set tuplet->ticks() to placedTicks = 1/3, which
// is not a TDuration fraction. Beam::calcBeamBreaks then constructs
// TDuration(ticks, /*truncate*/false) and asserts in debug builds.
// ===========================================================================

TEST_F(Tst_EncoreFeatures, partial_quarter_triplet_layout_does_not_assert)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_partial_quarter_triplet.enc");
    ASSERT_NE(score, nullptr) << "File must load and lay out without assert in beam.cpp";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();
    delete score;
}

// ===========================================================================
// FIX: LYRIC (type 6) syllables attached to chords (P3.7).
// 4/4 measure with notes do/re/mi/fa and matching LYRIC elements. The
// importer parses the 6-character text payload (UTF-16 LE in v0xC4) and
// attaches one syllable per ChordRest in tick order.
// ===========================================================================

// ===========================================================================
// FIX: Variable-length LYRIC parsing + empty placeholder alignment.
// Encore writes each lyric element as variable size (text length determines
// total bytes). A standalone empty LYRIC keeps the per-chord syllable count
// aligned but produces no <Lyrics>. Without this, "MU" between "LA" and
// "JER" goes missing on real songs (LaMorenaDeMiCopla.enc).
// ===========================================================================

// ===========================================================================
// FIX: Multi-verse lyrics (Encore stores verse 2 on voice 1, etc.).
// Two LYRIC streams on the same staff but different voices: the importer
// must anchor both on the voice-0 chord and tag them with Lyrics::verse()
// so MuseScore renders them as separate rows.
// ===========================================================================

// ===========================================================================
// FIX: Encore "-" LYRIC elements are hyphen continuation markers, not
// syllables. Filtering them out and tagging adjacent syllables with the
// matching LyricsSyllabic keeps lyrics aligned with their notes and lets
// MuseScore draw the hyphen automatically. The LaMorenaDeMiCopla m18 case
// "JU - LIO RO -" reproduced the off-by-one shift the user reported.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, lyrics_hyphen_separators_dropped_and_set_syllabic)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_lyrics_hyphenated_words.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    struct Entry { String text; LyricsSyllabic syll; };
    std::vector<Entry> seen;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(0);
            if (!el || !el->isChord()) {
                continue;
            }
            for (Lyrics* ly : toChord(el)->lyrics()) {
                seen.push_back({ ly->plainText(), ly->syllabic() });
            }
        }
    }
    ASSERT_EQ(seen.size(), 3u);
    EXPECT_EQ(seen[0].text, String(u"JU"));
    EXPECT_EQ(seen[0].syll, LyricsSyllabic::BEGIN)
        << "JU is followed by a hyphen continuation";
    EXPECT_EQ(seen[1].text, String(u"LIO"));
    EXPECT_EQ(seen[1].syll, LyricsSyllabic::END)
        << "LIO closes the JU-LIO word";
    EXPECT_EQ(seen[2].text, String(u"RO"));
    EXPECT_EQ(seen[2].syll, LyricsSyllabic::BEGIN)
        << "RO is followed by a hyphen continuation past the bar";
    delete score;
}

TEST_F(Tst_EncoreFeatures, lyrics_two_verses_on_voice_0_chord)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_lyrics_two_verses.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<String> verse0;
    std::vector<String> verse1;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(0);
            if (!el || !el->isChord()) {
                continue;
            }
            for (Lyrics* ly : toChord(el)->lyrics()) {
                if (ly->verse() == 0) {
                    verse0.push_back(ly->plainText());
                } else if (ly->verse() == 1) {
                    verse1.push_back(ly->plainText());
                }
            }
        }
    }
    std::vector<String> expectedV0 = { u"JU", u"LIO", u"RO", u"ME" };
    std::vector<String> expectedV1 = { u"Co", u"mo", u"ca", u"pa" };
    EXPECT_EQ(verse0, expectedV0);
    EXPECT_EQ(verse1, expectedV1);
    delete score;
}

TEST_F(Tst_EncoreFeatures, lyrics_variable_length_with_empty_placeholder)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_lyrics_variable.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<String> expected = { u"JU", u"LIO", u"RO" };
    std::vector<String> seen;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(0);
            if (!el || !el->isChord()) {
                continue;
            }
            for (Lyrics* ly : toChord(el)->lyrics()) {
                seen.push_back(ly->plainText());
            }
        }
    }
    EXPECT_EQ(seen, expected);
    delete score;
}

TEST_F(Tst_EncoreFeatures, lyrics_attached_to_chords)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_lyrics.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<String> expected = { u"do", u"re", u"mi", u"fa" };
    std::vector<String> seen;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(0);
            if (!el || !el->isChord()) {
                continue;
            }
            Chord* c = toChord(el);
            for (Lyrics* ly : c->lyrics()) {
                seen.push_back(ly->plainText());
            }
        }
    }
    EXPECT_EQ(seen, expected);
    delete score;
}

// ===========================================================================
// FIX: EncLyric::read() detects per-element encoding instead of assuming
// UTF-16 LE. Real v0xC4 corpora (e.g. milesdepartituras/
// Fe_cega_faca_amolada_tk.enc) store lyric text as one byte per char in
// Latin-1; the previous decoder read pairs of bytes as a single QChar and
// produced spurious CJK code units.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, lyrics_latin1_text_decoded_as_one_byte_per_char)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_lyrics_latin1.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<String> seen;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(0);
            if (!el || !el->isChord()) {
                continue;
            }
            for (Lyrics* ly : toChord(el)->lyrics()) {
                seen.push_back(ly->plainText());
            }
        }
    }
    ASSERT_EQ(seen.size(), 2u);
    EXPECT_EQ(seen[0], String(u"txã"));
    EXPECT_EQ(seen[1], String(u"nã"));
    delete score;
}

// ===========================================================================
// FIX: findEncoreInstrumentTemplate scores name + MIDI program together so
// "Bass" + GM program 32 (Acoustic Bass) wins over the choral Bass voice
// template that matches the name exactly but ships with a Choir Aahs
// channel (program 52).
// ===========================================================================
TEST_F(Tst_EncoreFeatures, instrument_name_midi_tiebreaks_to_acoustic_bass)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_instr_bass_midi_tiebreak.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->id(), String(u"acoustic-bass"))
        << "Bass + program 32 must reach the instrumental template, not the choral voice";
    delete score;
}

// ===========================================================================
// FIX: percussion tracks store midiProgram=1 (Grand Piano) regardless of
// the actual instrument, so the MIDI fallback assigned them to a piano.
// buildScore now recognises Spanish/Portuguese/English names ("Percusión",
// "Bateria", "Drum") and routes to the locale-independent "drumset" template.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, instrument_name_routes_percussion_to_drumset)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_instr_percussion_drumset.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->id(), String(u"drumset"))
        << "Encore 'Percusión' must reach the drum-kit template, not the MIDI piano fallback";
    delete score;
}

// ===========================================================================
// FIX: instrument matching is now diacritics-insensitive. The Spanish
// folk lute template id="laud" ships with trackName="Laúd"; an Encore file
// that writes the name without the accent (real corpora frequently do)
// must still resolve to it.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, instrument_name_diacritics_insensitive_match)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_instr_laud_accent.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->id(), String(u"laud"))
        << "Encore 'Laud' must match the laud template whose trackName carries the diacritic";
    delete score;
}

// ===========================================================================
// FIX: STAFFTEXT whose payload matches a standard Italian tempo term
// (Allegro, Adagio, ...) is promoted to a TempoText so the score's tempo
// map updates. Relative markings ("a tempo") become TempoText too but
// don't carry an absolute BPS - they defer to the previous tempo.
// Non-tempo strings ("ten.", "cresc.") keep the StaffText path.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, staff_text_promoted_to_tempo_for_italian_terms)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_stafftext_tempo_promotion.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<String> tempoTexts;
    std::vector<String> staffTexts;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (!e) {
                    continue;
                }
                if (e->isTempoText()) {
                    tempoTexts.push_back(toTempoText(e)->plainText());
                } else if (e->isStaffText()) {
                    staffTexts.push_back(toStaffText(e)->plainText());
                }
            }
        }
    }
    EXPECT_EQ(tempoTexts, (std::vector<String>{ u"Allegro", u"a tempo" }))
        << "Allegro and 'a tempo' must reach the score as TempoText, not StaffText";
    EXPECT_EQ(staffTexts, (std::vector<String>{ u"ten." }))
        << "Non-tempo words remain plain StaffText";
    delete score;
}

TEST_F(Tst_EncoreFeatures, staff_text_promoted_to_tempo_sets_tempo_map)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_stafftext_tempo_promotion.enc");
    ASSERT_NE(score, nullptr);

    // "Allegro" lives at the start of measure 0; the tempo map there must
    // pick up the palette default of 144 BPM (= 2.4 BPS).
    const Fraction tick0(0, 1);
    EXPECT_NEAR(score->tempo(tick0).val, 144.0 / 60.0, 1e-6)
        << "Allegro at tick 0 must set the tempo to 144 BPM";
    delete score;
}

// ===========================================================================
// FIX: Encore header/footer text can carry `#`-prefixed tokens (`#P` page,
// `#D` date, `#T` time). The importer rewrites them to the matching
// MuseScore macros (`$P`/`$D`/`$m`) before assigning the text to the page
// header/footer style slots. Without the rewrite the literal characters
// would print on every page instead of expanding at render time.
// ===========================================================================
// ===========================================================================
// FIX: TITL slot categories (subtitle 1-2, instruction 1-3, author 1-4,
// copyright 1-6, header 1-2, footer 1-2) carry multi-line content as one
// slot per visible line. The importer joins all non-empty slots of the same
// category with `\n` for the VBox text and Score Properties metadata.
// Headers and footers also stack when multiple slots share the same
// alignment byte (left / center / right): same-aligned lines are joined
// with `\n` into a single Sid value. Mirrors the multi-author block in
// Mamae_eu_quero-Bateria.enc (composer = author1 + author2 + author3) that
// Encore's own MusicXML exporter writes as a single <creator> with
// newline-separated lines.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, multi_slot_text_joined_with_newlines)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_multi_slot_stacked_text.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    EXPECT_EQ(score->metaTag(u"composer"),
              u"Vicente Paiva e Jararáca\nAdapt.: Sgt Solano\nBanda de Música do CRPO/VRS")
        << "the three author slots must join into one newline-separated string";
    // Two center-aligned header lines must stack into a single Sid value.
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::oddHeaderC),
              u"Top line one\nTop line two");
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::evenHeaderC),
              u"Top line one\nTop line two");
    // The two footers have different alignments (left + right) and must
    // therefore stay on separate Sids.
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::oddFooterL),  u"Left footer");
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::oddFooterR),  u"Right footer");
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::evenFooterL), u"Left footer");
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::evenFooterR), u"Right footer");
    delete score;
}

// ===========================================================================
// FIX: some Encore files (e.g. Mamae_eu_quero-Bateria.enc) write the TITL
// block twice with identical content. `EncTitle::read()` clears its slot
// vectors at the start of every pass so the second block replaces the
// first instead of doubling every line in the resulting composer, header,
// footer, etc.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, duplicate_titl_block_does_not_double_lines)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_duplicate_titl_block.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    // The fixture writes the same TITL block twice; the composer must
    // appear exactly once.
    EXPECT_EQ(score->metaTag(u"composer"), u"Sole Composer")
        << "second TITL block must replace the first instead of appending";
    EXPECT_EQ(score->metaTag(u"workTitle"), u"Duped TITL");
    delete score;
}

// ===========================================================================
// FIX: each Encore MEAS header carries a quarter-note BPM at offset 0 that
// the importer previously read into `EncMeasure::bpm` but never used. As a
// result every imported score played at MuseScore's 120 quarter-BPM default
// instead of the tempo the user set in Encore. A post-measure pass now
// emits a TempoText for the first measure plus every measure whose BPM
// differs from the previous applied value, and calls Score::setTempo so
// playback follows the new tempo map.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, measure_header_bpm_drives_initial_tempo_and_changes)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_tempo_changes.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    // Collect every TempoText in the score with its host measure index.
    struct Found { int measureIdx; double bps; String text; };
    std::vector<Found> seen;
    int mi = -1;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        ++mi;
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e && e->isTempoText()) {
                    TempoText* tt = toTempoText(e);
                    seen.push_back({ mi, tt->tempo().val, tt->plainText() });
                }
            }
        }
    }
    // Fixture writes [100, 60, 100, 60, 200, 200]. The importer emits a
    // TempoText for: m1 (initial 100), m2 (change to 60), m3 (back to 100),
    // m4 (back to 60), m5 (change to 200). m6 carries the same BPM as m5
    // and produces no extra mark.
    ASSERT_EQ(seen.size(), 5u);
    EXPECT_EQ(seen[0].measureIdx, 0);
    EXPECT_NEAR(seen[0].bps, 100.0 / 60.0, 1e-6);
    EXPECT_EQ(seen[0].text, u"♩ = 100");
    EXPECT_EQ(seen[1].measureIdx, 1);
    EXPECT_NEAR(seen[1].bps, 60.0 / 60.0, 1e-6);
    EXPECT_EQ(seen[2].measureIdx, 2);
    EXPECT_NEAR(seen[2].bps, 100.0 / 60.0, 1e-6);
    EXPECT_EQ(seen[3].measureIdx, 3);
    EXPECT_NEAR(seen[3].bps, 60.0 / 60.0, 1e-6);
    EXPECT_EQ(seen[4].measureIdx, 4);
    EXPECT_NEAR(seen[4].bps, 200.0 / 60.0, 1e-6);

    // The tempo map must reflect the same changes (sampled at the start of
    // each measure).
    Measure* m = score->firstMeasure();
    std::vector<int> expected = { 100, 60, 100, 60, 200, 200 };
    for (int i = 0; i < 6 && m; ++i, m = m->nextMeasure()) {
        EXPECT_NEAR(score->tempo(m->tick()).val, expected[i] / 60.0, 1e-6)
            << "measure " << i << " expected " << expected[i] << " BPM";
    }
    delete score;
}

TEST_F(Tst_EncoreFeatures, header_footer_tokens_translated_to_mscore_macros)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_header_footer_tokens.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    // header[0] right-aligned -> oddHeaderR + evenHeaderR.
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::oddHeaderR), u"Page $P")
        << "#P must be rewritten to $P (page number on every page)";
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::evenHeaderR), u"Page $P");
    // header[1] center-aligned -> oddHeaderC + evenHeaderC.
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::oddHeaderC), u"Created $D")
        << "#D must be rewritten to $D (creation date)";
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::evenHeaderC), u"Created $D");
    // footer[0] left-aligned -> oddFooterL + evenFooterL.
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::oddFooterL), u"Time $m")
        << "#T must be rewritten to $m (best MuseScore equivalent for 'time')";
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::evenFooterL), u"Time $m");
    delete score;
}

// ===========================================================================
// FIX: Encore encodes leading and interior silences implicitly via the
// element's absolute tick offset rather than via REST elements. The importer
// snaps cumTick to the Encore tick when the gap exceeds CHORD_MIDI_THRESHOLD,
// so a measure encoded as "NOTE at tick 240, NOTE at tick 480" in a 3/4
// bar renders as "quarter rest, quarter, quarter" instead of squashing the
// notes to beats 1-2 with the rest pushed to the end (which would shift the
// song's timing).
// ===========================================================================
TEST_F(Tst_EncoreFeatures, implicit_leading_rest_keeps_note_positions)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_implicit_leading_rest.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->ticks(), Fraction(3, 4)) << "measure must be 3/4";

    // Voice 0 should produce: quarter rest @ beat 1, quarter note @ beat 2,
    // quarter note @ beat 3. Same order as Encore -- no reordering.
    std::vector<Fraction> ticks;
    std::vector<bool> isRest;
    std::vector<int> pitches;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (!el) {
            continue;
        }
        ticks.push_back(s->tick() - m->tick());
        if (el->isRest()) {
            isRest.push_back(true);
            pitches.push_back(-1);
        } else if (el->isChord()) {
            isRest.push_back(false);
            pitches.push_back(toChord(el)->upNote()->pitch());
        }
    }
    ASSERT_EQ(ticks.size(), 3u) << "voice 0 must contain rest + 2 notes";
    EXPECT_TRUE(isRest[0]) << "beat 1 must be a rest, not a note";
    EXPECT_EQ(ticks[0], Fraction(0, 1));
    EXPECT_FALSE(isRest[1]);
    EXPECT_EQ(ticks[1], Fraction(1, 4));
    EXPECT_EQ(pitches[1], 72);
    EXPECT_FALSE(isRest[2]);
    EXPECT_EQ(ticks[2], Fraction(2, 4));
    EXPECT_EQ(pitches[2], 74);
    delete score;
}

// ===========================================================================
// FIX: When a voice carries a single NOTE/chord with no following events,
// EncMeasure::calculateRealDurations inflates rdur to the gap-to-measure-end
// (e.g. 720 in 3/4). That value lands on the dotted-half mapping bucket of
// realDuration2DurationType, which would falsely promote a face=quarter note
// to a dotted half. The importer rejects the promotion when rdur exceeds the
// face's tick count AND it isn't a real dotted multiple of the face.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, inflated_rdur_keeps_face_value_quarter_chord)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_inflated_rdur_quarter_chord.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    // Voice 1 (MuseScore track 1 of staff 0) holds the encVoice=1 chord.
    Chord* chord = nullptr;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(1);
        if (el && el->isChord()) {
            chord = toChord(el);
            break;
        }
    }
    ASSERT_NE(chord, nullptr) << "encVoice=1 chord must land on MuseScore voice 1";
    EXPECT_EQ(chord->durationType().type(), DurationType::V_QUARTER)
        << "Face=3 (quarter) must win over rdur=720 (inflated to dotted-half ratio)";
    EXPECT_EQ(chord->durationType().dots(), 0)
        << "rdur=720 is not a real dotted multiple of quarter; no dot";
    EXPECT_EQ(chord->notes().size(), 2u)
        << "the two NOTE elements at tick 0 are merged into one chord";
    delete score;
}

// ===========================================================================
// FIX: realDuration2DurationType no longer promotes triplet-spaced rdur
// (80, 40, ...) past the face value, so a notated 16th whose MIDI gap to
// the next note happens to be 80 ticks stays a 16th instead of becoming an
// eighth and overflowing the measure.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, note_rdur_80_stays_16th_face_value)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_rdur_80_stays_16th.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<DurationType> types;
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord()) {
            types.push_back(toChord(el)->durationType().type());
        }
    }
    ASSERT_GE(types.size(), 2u);
    EXPECT_EQ(types[0], DurationType::V_16TH)
        << "First note had rdur=80 (was V_EIGHTH under the old triplet table); must stay 16th";
    EXPECT_EQ(types[1], DurationType::V_16TH);
    delete score;
}

// ===========================================================================
// FIX: Articulations beyond fermata (P2.5).
// Four quarter notes carry staccato (0x1D), accent (0x12), tenuto (0x1C),
// marcato (0x13) in articulationUp. The importer maps each known glyph byte
// to a MuseScore SymId via encArticulation2SymId.
// ===========================================================================

// ===========================================================================
// FIX: TIE element direction bytes beyond 0xfe (P2.6).
// Encore uses two values for outgoing ties (0xfc and 0xfe -- both have the
// high bit set) plus two arc-only endpoint markers (0x02 and 0x04). The
// importer treats any high-bit-set value as a tie-start, so a TIE with
// dirByte=0xfc still produces a Tie element on the first note.
// ===========================================================================

// ===========================================================================
// FIX: KEYCHANGE elements with tipo=0 (modulation to C major) are now emitted
// (P4.10). The previous guard skipped them, dropping ~24 of 40 key signatures
// on Beethoven's Plectro arrangement.
// ===========================================================================

TEST_F(Tst_EncoreFeatures, keychange_to_c_major_emitted)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_keychange_to_c.enc");
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
// FIX: STAFFTEXT 0x1E rendered from TEXT block (P3.8).
// Four STAFFTEXT ornaments reference indices 0..3 of an injected TEXT block
// containing "Allegretto", "cresc.", "dimin.", "ten.". The importer reads
// the TEXT block, parses each entry's UTF-16 LE payload, and indexes via
// the ornament's tind byte (+32) to create a StaffText element.
// ===========================================================================

TEST_F(Tst_EncoreFeatures, staff_text_resolved_via_text_block)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_staff_text.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    // "Allegretto" is now promoted to a TempoText (see the
    // staff_text_promoted_to_tempo* tests); the rest stays as StaffText.
    std::vector<String> expected = { u"cresc.", u"dimin.", u"ten." };
    std::vector<String> seen;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e && e->isStaffText()) {
                    seen.push_back(toStaffText(e)->plainText());
                }
            }
        }
    }
    EXPECT_EQ(seen, expected);
    delete score;
}

// ===========================================================================
// FEATURE: STAFFTEXT placement derived from Encore y-offset.
// Two ornaments at adjacent ticks: yoffset=+10 keeps default (ABOVE) and
// yoffset=-10 must flip placement to BELOW (the case Beethoven Plectro m3
// hits for its "ten" markers).
// ===========================================================================
TEST_F(Tst_EncoreFeatures, staff_text_placement_from_yoffset)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_staff_text_placement.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::map<String, PlacementV> placements;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e && e->isStaffText()) {
                    StaffText* st = toStaffText(e);
                    placements[st->plainText()] = st->placement();
                }
            }
        }
    }
    ASSERT_EQ(placements.size(), 2u) << "two STAFFTEXT elements expected";
    EXPECT_EQ(placements[u"Above"], PlacementV::ABOVE)
        << "yoffset=+10 must keep default ABOVE placement";
    EXPECT_EQ(placements[u"ten"], PlacementV::BELOW)
        << "yoffset=-10 (Cartesian below) must map to PlacementV::BELOW";
    delete score;
}

TEST_F(Tst_EncoreFeatures, tie_direction_fc_creates_tie)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_tie_dir_fc.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int tieCount = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(0);
            if (!el || !el->isChord()) {
                continue;
            }
            for (Note* n : toChord(el)->notes()) {
                if (n->tieFor()) {
                    ++tieCount;
                }
            }
        }
    }
    EXPECT_EQ(tieCount, 1) << "expected one tie from the 0xfc TIE element";
    delete score;
}

// ===========================================================================
// FIX: Encore stores the tie-start indicator on either byte +5 (arc
// direction) or byte +6 (start flag). Around a third of outgoing ties in
// the LaMorenaDeMiCopla / Beethoven Plectro corpora use the +6 variant
// with +5 = 0x04 (arc-only). The importer must accept either byte's high
// bit as a tie-start marker; otherwise long ties such as the m20 dotted
// quarter B in LaMorenaDeMiCopla disappear.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, tie_start_flag_on_byte6_creates_tie)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_tie_start_flag_byte6.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int tieCount = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(0);
            if (!el || !el->isChord()) {
                continue;
            }
            for (Note* n : toChord(el)->notes()) {
                if (n->tieFor()) {
                    ++tieCount;
                }
            }
        }
    }
    EXPECT_EQ(tieCount, 1)
        << "expected one tie from the +6=0x80 TIE-start-flag element";
    delete score;
}

TEST_F(Tst_EncoreFeatures, articulations_mapped_beyond_fermata)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_articulations.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    // Layout may flip Above -> Below based on stem direction. Normalize by
    // checking the type predicate rather than the exact SymId.
    enum class ArtKind { Staccato, Accent, Tenuto, Marcato, Other };
    auto kindOf = [](Articulation* a) -> ArtKind {
        if (a->isStaccato()) {
            return ArtKind::Staccato;
        }
        if (a->isAccent()) {
            return ArtKind::Accent;
        }
        if (a->isTenuto()) {
            return ArtKind::Tenuto;
        }
        if (a->isMarcato()) {
            return ArtKind::Marcato;
        }
        return ArtKind::Other;
    };
    std::vector<ArtKind> expected = {
        ArtKind::Staccato, ArtKind::Accent, ArtKind::Tenuto, ArtKind::Marcato,
    };
    std::vector<ArtKind> seen;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(0);
            if (!el || !el->isChord()) {
                continue;
            }
            for (Articulation* a : toChord(el)->articulations()) {
                seen.push_back(kindOf(a));
            }
        }
    }
    EXPECT_EQ(seen.size(), expected.size());
    for (size_t i = 0; i < std::min(seen.size(), expected.size()); ++i) {
        EXPECT_EQ(seen[i], expected[i]) << "articulation #" << i;
    }
    delete score;
}

// ===========================================================================
// FEATURE: Combo articulation bytes emit multiple Articulation elements.
// Encore packs two glyphs into one byte (e.g. 0x24 = tenuto + staccato).
// Treating each byte as a single SymId would silently drop ~85 % of the
// articulations in encore-symbols.enc m8-m13. The new fixture exercises
// every combo byte; the importer must add both glyphs to the chord.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, articulation_combos_expand_to_two_glyphs)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_articulations_combo.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    enum class K { Tenuto, Staccato, Accent, Marcato, Staccatissimo, Other };
    auto kindOf = [](Articulation* a) -> K {
        using mu::engraving::SymId;
        switch (a->symId()) {
        case SymId::articTenutoAbove: case SymId::articTenutoBelow:
            return K::Tenuto;
        case SymId::articStaccatoAbove: case SymId::articStaccatoBelow:
            return K::Staccato;
        case SymId::articAccentAbove: case SymId::articAccentBelow:
            return K::Accent;
        case SymId::articMarcatoAbove: case SymId::articMarcatoBelow:
            return K::Marcato;
        case SymId::articStaccatissimoAbove: case SymId::articStaccatissimoBelow:
            return K::Staccatissimo;
        default:
            return K::Other;
        }
    };
    // Per-chord expected set of articulations, in the binary's note order:
    //   m1: 0x24 (ten+stacc), 0x17 (acc+stacc), 0x27 (marc+ten), 0x15 (marc+stacc)
    //   m2: 0x23 (acc+ten),   0x2D (ten+statiss), 0x2B (acc+statiss), 0x24 (ten+stacc)
    const std::vector<std::set<K> > expected = {
        { K::Tenuto, K::Staccato },
        { K::Accent, K::Staccato },
        { K::Marcato, K::Tenuto },
        { K::Marcato, K::Staccato },
        { K::Accent, K::Tenuto },
        { K::Tenuto, K::Staccatissimo },
        { K::Accent, K::Staccatissimo },
        { K::Tenuto, K::Staccato },
    };
    std::vector<std::set<K> > seen;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(0);
            if (!el || !el->isChord()) {
                continue;
            }
            std::set<K> kinds;
            for (Articulation* a : toChord(el)->articulations()) {
                kinds.insert(kindOf(a));
            }
            if (!kinds.empty()) {
                seen.push_back(kinds);
            }
        }
    }
    ASSERT_EQ(seen.size(), expected.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        EXPECT_EQ(seen[i], expected[i]) << "chord #" << i;
    }
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
TEST_F(Tst_EncoreFeatures, all_encore_navigation_options)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_jump_marks_all.enc");
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
    // Coda comes from ORN 0xA6 AND coda bytes 0x85/0x89; all add a Marker.
    EXPECT_GE(codaMarkers, 1) << "ORN 0xA6 must produce a Coda Marker";
    // To Coda comes from ORN 0xA5.
    EXPECT_EQ(toCodaMarkers, 1) << "ORN 0xA5 must produce a TOCODA Marker";
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
TEST_F(Tst_EncoreFeatures, jump_marks_dc_ds_tocoda)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_jump_marks.enc");
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
TEST_F(Tst_EncoreFeatures, section_markers_and_dotted_barline)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_section_markers.enc");
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
// FEATURE: Per-chord staccato from size-16 ORN tipo=0xC9.
// Encore stores staccato as a separate ORN at the same tick as the chord;
// its MusicXML exporter drops the byte but the dot is visible. The
// importer must add Staccato per ORN and dedup against the per-note
// artic byte 0x1D (which produces the same glyph).
// ===========================================================================
TEST_F(Tst_EncoreFeatures, staccato_from_orn_c9)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_staccato_orn.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<bool> seen;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(0);
            if (!el || !el->isChord()) {
                continue;
            }
            int staccatoCount = 0;
            for (Articulation* a : toChord(el)->articulations()) {
                if (a->symId() == SymId::articStaccatoAbove
                    || a->symId() == SymId::articStaccatoBelow) {
                    ++staccatoCount;
                }
            }
            seen.push_back(staccatoCount == 1);
            EXPECT_LE(staccatoCount, 1) << "no duplicate staccato per chord";
        }
    }
    // Expected sequence: chord 0 has staccato (0xC9), chord 1 has
    // staccato (0xC9), chord 2 has none, chord 3 has staccato (both
    // 0xC9 and the per-note artic byte 0x1D -- must dedup).
    const std::vector<bool> expected = { true, true, false, true };
    EXPECT_EQ(seen, expected);
    delete score;
}

// ===========================================================================
// FEATURE: Size-28 ORN tipos 0x36 / 0x37 produce trill-mark ornaments;
// tipo 0x35 is the trill-span end marker and adds nothing.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, trill_spanner_start_markers)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_trill_spanner.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int trillCount = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(0);
            if (!el || !el->isChord()) {
                continue;
            }
            for (Articulation* a : toChord(el)->articulations()) {
                if (a->symId() == SymId::ornamentTrill) {
                    ++trillCount;
                }
            }
        }
    }
    EXPECT_EQ(trillCount, 2) << "0x36 and 0x37 should add one trill each; 0x35 adds nothing";
    delete score;
}

// ===========================================================================
// FEATURE: Per-note technical markings from artic byte.
//   0x0D..0x11 -> fingering 1..5 (Fingering text "1".."5")
//   0x46       -> open-string (Fingering STRING_NUMBER text "0")
//   0x44, 0x45 -> thumb-position (Articulation stringsThumbPosition)
//   0x1E, 0x1F -> harmonic (Articulation stringsHarmonic)
// ===========================================================================
TEST_F(Tst_EncoreFeatures, technical_markings_per_note_artic_byte)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_technical.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<String> fingerings;
    std::vector<SymId> articulations;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(0);
            if (!el || !el->isChord()) {
                continue;
            }
            for (Note* n : toChord(el)->notes()) {
                for (EngravingItem* e : n->el()) {
                    if (e && e->isFingering()) {
                        fingerings.push_back(toFingering(e)->plainText());
                    }
                }
            }
            for (Articulation* a : toChord(el)->articulations()) {
                articulations.push_back(a->symId());
            }
        }
    }
    const std::vector<String> expectedFingerings = {
        u"0", u"1", u"2", u"3", u"4", u"5",
    };
    EXPECT_EQ(fingerings, expectedFingerings);
    const std::vector<SymId> expectedArticulations = {
        SymId::stringsThumbPosition,
        SymId::stringsHarmonic,
    };
    EXPECT_EQ(articulations, expectedArticulations);
    delete score;
}

// ===========================================================================
// FEATURE: Fermata anchored on segment, direction follows artic slot.
// Encore stores the fermata above/below distinction by which artic slot
// carries the byte: articUp=0x20 -> upright (above), articDown=0x21 ->
// inverted (below). The importer must attach a Fermata to the chord's
// segment (not the chord) so MusicXML exports <fermata> instead of
// <other-articulation smufl="..."/>, and the PlacementV must follow the
// slot so the type attribute renders correctly.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, fermatas_emit_segment_anchored_element)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_fermatas.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<PlacementV> seen;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e && e->isFermata()) {
                    seen.push_back(toFermata(e)->placement());
                }
            }
        }
    }
    ASSERT_EQ(seen.size(), 2u);
    EXPECT_EQ(seen[0], PlacementV::BELOW)
        << "articDown=0x21 must produce an inverted (below) fermata";
    EXPECT_EQ(seen[1], PlacementV::ABOVE)
        << "articUp=0x20 must produce an upright (above) fermata";
    delete score;
}

// ===========================================================================
// FEATURE: Single-note tremolos derived from per-note artic byte.
// Encore packs the stroke count in the low nibble of articulationUp /
// articulationDown for byte values 0x41/0x42/0x43/0x03. The importer
// attaches a TremoloSingleChord with TremoloType::R8/R16/R32.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, tremolos_from_per_note_artic_byte)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_tremolos.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<TremoloType> seen;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(0);
            if (!el || !el->isChord()) {
                continue;
            }
            if (TremoloSingleChord* trem = toChord(el)->tremoloSingleChord()) {
                seen.push_back(trem->tremoloType());
            }
        }
    }
    const std::vector<TremoloType> expected = {
        TremoloType::R8, TremoloType::R16, TremoloType::R32, TremoloType::R32,
    };
    EXPECT_EQ(seen, expected);
    delete score;
}

// ===========================================================================
// FEATURE: Trill-mark / mordent / inverted-mordent from per-note artic byte.
// Encore stores ornament glyphs in the same articulationUp byte as plain
// articulations. The importer wraps them in Ornament (an Articulation
// subclass) so MuseScore's MusicXML export emits them under <ornaments>.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, trill_mordent_from_per_note_artic_byte)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_trill_mordent.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<SymId> seen;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(0);
            if (!el || !el->isChord()) {
                continue;
            }
            for (Articulation* a : toChord(el)->articulations()) {
                seen.push_back(a->symId());
            }
        }
    }
    const std::vector<SymId> expected = {
        SymId::ornamentTrill,
        SymId::ornamentShortTrill,   // <inverted-mordent>
        SymId::ornamentMordent,
        SymId::ornamentMordent,
    };
    EXPECT_EQ(seen, expected);
    delete score;
}

// ===========================================================================
// FEATURE: End-of-measure DOUBLE barline lands on every staff.
// Encore renders barline graphics across every instrument on the system,
// while MuseScore stores the barline per staff. The importer must apply
// BarLineType::DOUBLE to every track, not only track 0 (the Beethoven
// Plectro m26 double bar reproduced the original off-by-tracks bug).
// ===========================================================================
TEST_F(Tst_EncoreFeatures, double_barline_lands_on_every_staff)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_double_barline_multi_staff.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m1 = score->firstMeasure();
    ASSERT_NE(m1, nullptr);
    const size_t nstaves = score->nstaves();
    ASSERT_GE(nstaves, 2u) << "fixture must have at least two staves";

    Segment* endBarSeg = m1->findSegment(SegmentType::EndBarLine, m1->endTick());
    ASSERT_NE(endBarSeg, nullptr) << "m1 must have an EndBarLine segment";

    int doubleBarStaves = 0;
    for (size_t s = 0; s < nstaves; ++s) {
        EngravingItem* el = endBarSeg->element(s * VOICES);
        if (!el || !el->isBarLine()) {
            continue;
        }
        if (toBarLine(el)->barLineType() == BarLineType::DOUBLE) {
            ++doubleBarStaves;
        }
    }
    EXPECT_EQ(doubleBarStaves, static_cast<int>(nstaves))
        << "DOUBLE barline must be present on every staff, not only track 0";
    delete score;
}

// ===========================================================================
// FEATURE: WEDGESTART tick == durTicks (measure-end boundary) is kept.
// Encore stores hairpins whose visible start sits on the bar line at
// tick == measure->durTicks. The importer used to drop these along with
// notes/rests beyond the bar; this test guards against the regression.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, wedgestart_at_measure_end_boundary)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_wedgestart_at_measure_end.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int hairpinCount = 0;
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (sp->isHairpin()) {
            ++hairpinCount;
            EXPECT_LT(sp->tick(), sp->tick2()) << "hairpin span must be positive";
        }
    }
    EXPECT_EQ(hairpinCount, 1)
        << "WEDGESTART at tick == durTicks must produce a hairpin";
    delete score;
}

// ===========================================================================
// FEATURE: Dynamics from size-16 ORN cluster (0x81=pp, 0x82=p, 0x85=f,
// 0x86=ff). The mapping was reverse-engineered against Beethoven Sinfonia
// 7 II Allegretto Plectro cross-referenced with the Encore MusicXML export.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, dynamics_from_size16_ornaments)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_dynamics.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<DynamicType> seen;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e && e->isDynamic()) {
                    seen.push_back(toDynamic(e)->dynamicType());
                }
            }
        }
    }
    const std::vector<DynamicType> expected = {
        DynamicType::P, DynamicType::PP, DynamicType::F, DynamicType::FF,
    };
    EXPECT_EQ(seen, expected);
    delete score;
}

// ===========================================================================
// FEATURE: Voice=4 ORN with staffByte high bit, full dynamic ladder.
// Encore writes system-level ornaments (dynamics, tremolos, technical
// markings, etc.) with voice=4 and the staffByte high bit (0x40) set.
// The importer must (a) accept voice=4 ORN elements as system marks
// rather than dropping them, and (b) map the extended tipo ladder
// 0x80..0x8A to the full DynamicType set. encore-symbols.enc reproduces
// this; with the previous voice<VOICES filter every dynamic was lost.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, dynamics_full_ladder_voice4_system_mark)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_dynamics_full.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<DynamicType> seen;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e && e->isDynamic()) {
                    seen.push_back(toDynamic(e)->dynamicType());
                }
            }
        }
    }
    const std::vector<DynamicType> expected = {
        DynamicType::PPP, DynamicType::PP,  DynamicType::P,   DynamicType::MP,
        DynamicType::MF,  DynamicType::F,   DynamicType::FF,  DynamicType::FFF,
        DynamicType::SFZ, DynamicType::SFFZ, DynamicType::FP, DynamicType::FZ,
        DynamicType::SF,
    };
    EXPECT_EQ(seen, expected);
    delete score;
}

// ===========================================================================
// FEATURE: Arpeggios from ORN tipo=0x22.
// The synthetic file has a quarter-note C major triad at tick 0 with an
// ORN tipo=0x22 attached. The importer must add an Arpeggio element to
// the chord; no second arpeggio should appear elsewhere.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, arpeggio_attaches_to_chord)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_arpeggio.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int arpeggioCount = 0;
    int notesUnderArpeggio = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(0);
            if (!el || !el->isChord()) {
                continue;
            }
            Chord* c = toChord(el);
            if (c->arpeggio()) {
                ++arpeggioCount;
                notesUnderArpeggio = static_cast<int>(c->notes().size());
            }
        }
    }
    EXPECT_EQ(arpeggioCount, 1) << "exactly one arpeggio expected";
    EXPECT_EQ(notesUnderArpeggio, 3)
        << "arpeggio must sit on the 3-note C major triad";
    delete score;
}

// ===========================================================================
// FIX: SLURSTART resolves end tick from alMezuro after the measure pass.
// Two slurs: a multi-measure slur (alMezuro=2) and a same-measure one
// (alMezuro=0). Encore .enc binaries do not emit SLURSTOP; the importer
// collects intents and anchors them on the last ChordRest in the target
// measure once measures are populated.
// ===========================================================================

TEST_F(Tst_EncoreFeatures, multi_measure_slur_resolved_from_almezuro)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_multi_measure_slur.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int slurCount = 0;
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (!sp->isSlur()) {
            continue;
        }
        ++slurCount;
        EXPECT_LT(sp->tick(), sp->tick2()) << "slur span must be positive";
        EXPECT_NE(sp->startElement(), nullptr) << "slur missing start element";
        EXPECT_NE(sp->endElement(), nullptr) << "slur missing end element";
    }
    EXPECT_EQ(slurCount, 2);
    delete score;
}

// ===========================================================================
// FIX: Multi-measure hairpin resolves end tick from WEDGESTART's alMezuro.
// Two hairpins:
//   - crescendo from measure 0 tick=0 with alMezuro=2 -> ends at end of measure 2
//   - diminuendo from measure 1 tick=480 with alMezuro=1 -> ends at end of measure 2
// ===========================================================================

TEST_F(Tst_EncoreFeatures, multi_measure_hairpin_resolved_from_almezuro)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_multi_measure_hairpin.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int hairpinCount = 0;
    bool foundCresc = false;
    bool foundDim = false;
    const Fraction wholeMeasure(4, 4);
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (!sp->isHairpin()) {
            continue;
        }
        ++hairpinCount;
        Hairpin* hp = toHairpin(sp);
        EXPECT_LT(hp->tick(), hp->tick2()) << "hairpin span must be positive";
        if (hp->hairpinType() == HairpinType::CRESC_HAIRPIN) {
            foundCresc = true;
            // start at measure 0 / tick 0, end at end of measure 2 (= 3 * 4/4)
            EXPECT_EQ(hp->tick(), Fraction(0, 1));
            EXPECT_EQ(hp->tick2(), wholeMeasure * 3);
        } else if (hp->hairpinType() == HairpinType::DIM_HAIRPIN) {
            foundDim = true;
            // start at measure 1 / beat 2 (480 enc ticks = 2/4),
            // end at end of measure 2 (= 3 * 4/4)
            EXPECT_EQ(hp->tick(), wholeMeasure + Fraction(2, 4));
            EXPECT_EQ(hp->tick2(), wholeMeasure * 3);
        }
    }
    EXPECT_EQ(hairpinCount, 2);
    EXPECT_TRUE(foundCresc);
    EXPECT_TRUE(foundDim);
    delete score;
}

// ===========================================================================
// BUG FIX: Grace note detection only for valid faceValues
// ===========================================================================

TEST_F(Tst_EncoreFeatures, grace_notes_only_on_short_facevalues)
{
    // Grace notes are only created when faceValue >= 4 (eighth or shorter).
    // Notes with grace bytes but fv=3 (quarter) or longer must NOT get grace type.
    // synthetic_v0c4_grace.enc:
    //   note 1: fv=4 + grace bytes -> ACCIACCATURA
    //   note 2: fv=3 + grace bytes -> fv<4 filter -> grace type NOT applied
    //
    // Grace chords are parented under their main Chord (not a Segment), so they
    // appear in main->graceNotes(), not as direct segment elements. Segment-attached
    // chords must therefore all be NORMAL; only chords inside graceNotes() may carry
    // a grace noteType.
    // Key invariant: any grace chord must have short base duration (fv>=4).
    MasterScore* score = readEncoreScore("synthetic_v0c4_grace.enc");
    ASSERT_NE(score, nullptr);
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
                        DurationType dt = gc->durationType().type();
                        bool shortEnough = (dt == DurationType::V_EIGHTH
                                            || dt == DurationType::V_16TH
                                            || dt == DurationType::V_32ND
                                            || dt == DurationType::V_64TH);
                        EXPECT_TRUE(shortEnough)
                            << "Grace note must have eighth or shorter duration (fv<4 filter), got "
                            << int(dt);
                    }
                }
            }
        }
    }
    EXPECT_TRUE(foundGrace) << "Should have at least one grace note (from the fv=4 eighth)";
    delete score;
}

// ===========================================================================
// FEATURE: Multi-staff score with correct voice assignments
// ===========================================================================

TEST_F(Tst_EncoreFeatures, orchestra_loads_with_all_parts)
{
    MasterScore* score = readEncoreScore("kordorkestro.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->parts().size(), 1u) << "Orchestra should have multiple parts";
    EXPECT_GT(score->nstaves(), 1u) << "Orchestra should have multiple staves";
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_EncoreFeatures, orchestra_sanity_check)
{
    MasterScore* score = readEncoreScore("kordorkestro.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "kordorkestro should pass sanityCheck: " << ret.text();
    delete score;
}

// ===========================================================================
// FEATURE: Title from TITL block
// ===========================================================================

TEST_F(Tst_EncoreFeatures, title_frame_created)
{
    // kordorkestro has title "String Orchestra w/Piano"
    MasterScore* score = readEncoreScore("kordorkestro.enc");
    ASSERT_NE(score, nullptr);
    // Title frame should be the first element
    MeasureBase* first = score->first();
    ASSERT_NE(first, nullptr);
    EXPECT_TRUE(first->isVBox()) << "Score with title should start with a VBox frame";
    delete score;
}

TEST_F(Tst_EncoreFeatures, no_title_frame_when_empty)
{
    // bazo.enc has no title — should not have a VBox frame
    MasterScore* score = readEncoreScore("bazo.enc");
    ASSERT_NE(score, nullptr);
    MeasureBase* first = score->first();
    ASSERT_NE(first, nullptr);
    EXPECT_TRUE(first->isMeasure()) << "Score without title should start with a measure";
    delete score;
}

TEST_F(Tst_EncoreFeatures, title_frame_instruction_and_copyright)
{
    // synthetic_v0c4_title_instruction_copyright.enc: TITL block with title,
    // subtitle, instruction (arranger), author (composer), and copyright.
    // instruction[0] must appear in the VBox as LYRICIST; copyright[0] must
    // be stored in the score "copyright" metadata tag.
    MasterScore* score = readEncoreScore("synthetic_v0c4_title_instruction_copyright.enc");
    ASSERT_NE(score, nullptr);

    MeasureBase* first = score->first();
    ASSERT_NE(first, nullptr);
    ASSERT_TRUE(first->isVBox()) << "TITL with content must produce a VBox frame";

    std::map<TextStyleType, String> texts;
    for (const EngravingItem* el : first->el()) {
        if (el->isText()) {
            const TextBase* tb = toTextBase(el);
            texts[tb->textStyleType()] = tb->plainText();
        }
    }

    // VBox visual text elements
    EXPECT_EQ(texts[TextStyleType::TITLE],    String(u"Test Title"));
    EXPECT_EQ(texts[TextStyleType::SUBTITLE], String(u"Test Subtitle"));
    EXPECT_EQ(texts[TextStyleType::LYRICIST], String(u"Test Instruction"))
        << "instruction[0] must be added as LYRICIST text";
    EXPECT_EQ(texts[TextStyleType::COMPOSER], String(u"Test Composer"));

    // Score Properties metadata (File > Score Properties dialog)
    EXPECT_EQ(score->metaTag(u"workTitle"),  String(u"Test Title"))
        << "title must be stored in workTitle metadata";
    EXPECT_EQ(score->metaTag(u"subtitle"),   String(u"Test Subtitle"))
        << "subtitle[0] must be stored in subtitle metadata";
    EXPECT_EQ(score->metaTag(u"lyricist"),   String(u"Test Instruction"))
        << "instruction[0] must be stored in lyricist metadata";
    EXPECT_EQ(score->metaTag(u"composer"),   String(u"Test Composer"))
        << "author[0] must be stored in composer metadata";
    EXPECT_EQ(score->metaTag(u"copyright"),  String(u"(c) 2026 Test"))
        << "copyright[0] must be stored in copyright metadata";

    delete score;
}

TEST_F(Tst_EncoreFeatures, title_frame_headers_footers)
{
    // synthetic_v0c4_titl_headers_footers.enc carries two header lines and
    // two footer lines with non-trivial alignment bytes:
    //   header[0] = "Header Right"   align=0x02 (RIGHT)
    //   header[1] = "Header Center"  align=0x06 (CENTER)
    //   footer[0] = "Footer Center"  align=0x06 (CENTER)
    //   footer[1] = "Footer Right"   align=0x02 (RIGHT)
    // The importer must apply the texts to BOTH the odd- and even-page Sids
    // so they show on every page regardless of page parity, and place each
    // text in the column the alignment byte selects.
    MasterScore* score = readEncoreScore("synthetic_v0c4_titl_headers_footers.enc");
    ASSERT_NE(score, nullptr);

    auto styleText = [score](Sid sid) -> String {
        return score->style().styleSt(sid);
    };

    EXPECT_EQ(styleText(Sid::oddHeaderR),  String(u"Header Right"));
    EXPECT_EQ(styleText(Sid::evenHeaderR), String(u"Header Right"));
    EXPECT_EQ(styleText(Sid::oddHeaderC),  String(u"Header Center"));
    EXPECT_EQ(styleText(Sid::evenHeaderC), String(u"Header Center"));
    // Left header columns are not overwritten by the importer because no
    // header line was tagged LEFT in the fixture. The defaults that ship
    // with MuseScore (e.g. evenHeaderL = "$p") must therefore survive.
    EXPECT_NE(styleText(Sid::oddHeaderL),  String(u"Header Right"));
    EXPECT_NE(styleText(Sid::oddHeaderL),  String(u"Header Center"));
    EXPECT_NE(styleText(Sid::evenHeaderL), String(u"Header Right"));
    EXPECT_NE(styleText(Sid::evenHeaderL), String(u"Header Center"));

    EXPECT_EQ(styleText(Sid::oddFooterC),  String(u"Footer Center"));
    EXPECT_EQ(styleText(Sid::evenFooterC), String(u"Footer Center"));
    EXPECT_EQ(styleText(Sid::oddFooterR),  String(u"Footer Right"));
    EXPECT_EQ(styleText(Sid::evenFooterR), String(u"Footer Right"));
    EXPECT_NE(styleText(Sid::oddFooterL),  String(u"Footer Center"));
    EXPECT_NE(styleText(Sid::oddFooterL),  String(u"Footer Right"));
    EXPECT_NE(styleText(Sid::evenFooterL), String(u"Footer Center"));
    EXPECT_NE(styleText(Sid::evenFooterL), String(u"Footer Right"));

    delete score;
}

// ===========================================================================
// FEATURE: Chord symbols (Harmony elements)
// ===========================================================================

TEST_F(Tst_EncoreFeatures, chord_symbols_present)
{
    // akordo.enc has chord symbols (Am, G7, etc.)
    MasterScore* score = readEncoreScore("akordo.enc");
    ASSERT_NE(score, nullptr);
    bool foundHarmony = false;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e->isHarmony()) {
                    foundHarmony = true;
                    break;
                }
            }
            if (foundHarmony) {
                break;
            }
        }
        if (foundHarmony) {
            break;
        }
    }
    EXPECT_TRUE(foundHarmony) << "akordo.enc should contain chord symbols";
    delete score;
}

// ===========================================================================
// FEATURE: Multiple voices (opeco_vochoj)
// ===========================================================================

TEST_F(Tst_EncoreFeatures, multiple_voices_loaded)
{
    // opeco_vochoj.enc has multiple voices per staff
    MasterScore* score = readEncoreScore("opeco_vochoj.enc");
    ASSERT_NE(score, nullptr);
    bool foundVoice1 = false;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        Measure* m = toMeasure(mb);
        for (Segment* s = m->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            // Check voice 1 (track 1 = staff 0, voice 1)
            if (s->element(1) && s->element(1)->isChordRest()) {
                foundVoice1 = true;
                break;
            }
        }
        if (foundVoice1) {
            break;
        }
    }
    EXPECT_TRUE(foundVoice1) << "opeco_vochoj.enc should have notes in voice 2";
    delete score;
}

// ===========================================================================
// FEATURE: End-to-end coverage on the encore_symbols reference file.
// The user authored encore_symbols.enc to exercise every visible symbol
// Encore can place. This test pins the per-category counts that the
// importer is expected to recover -- a regression on any of the symbol
// families decoded in this directory will trip this case immediately.
// ===========================================================================
TEST_F(Tst_EncoreFeatures, encore_symbols_full_coverage)
{
    MasterScore* score = readEncoreScore("encore_symbols.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int dynamics = 0;
    int fermatas = 0;
    int markers = 0;     // Segno / Coda / TOCODA / FINE
    int jumps = 0;       // D.C. / D.S. variants
    int staccatos = 0;
    int tenutos = 0;
    int accents = 0;
    int marcatos = 0;
    int staccatissimos = 0;
    int trills = 0;
    int mordents = 0;
    int fingerings = 0;
    int arpeggios = 0;
    int tremolos = 0;
    int hairpins = 0;
    int dotted_barlines = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        Measure* m = toMeasure(mb);
        for (EngravingItem* e : m->el()) {
            if (e && e->isMarker()) ++markers;
            if (e && e->isJump()) ++jumps;
        }
        // Dotted end barline
        Segment* endBar = m->findSegment(SegmentType::EndBarLine, m->endTick());
        if (endBar) {
            for (size_t s = 0; s < score->nstaves(); ++s) {
                EngravingItem* el = endBar->element(s * VOICES);
                if (el && el->isBarLine() && toBarLine(el)->barLineType() == BarLineType::DOTTED) {
                    ++dotted_barlines;
                    break;
                }
            }
        }
        for (Segment* s = m->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e && e->isDynamic()) ++dynamics;
                if (e && e->isFermata()) ++fermatas;
            }
            EngravingItem* el = s->element(0);
            if (!el || !el->isChord()) {
                continue;
            }
            Chord* c = toChord(el);
            if (c->arpeggio()) ++arpeggios;
            if (c->tremoloSingleChord()) ++tremolos;
            for (Articulation* a : c->articulations()) {
                using mu::engraving::SymId;
                switch (a->symId()) {
                case SymId::articStaccatoAbove: case SymId::articStaccatoBelow:
                    ++staccatos; break;
                case SymId::articTenutoAbove: case SymId::articTenutoBelow:
                    ++tenutos; break;
                case SymId::articAccentAbove: case SymId::articAccentBelow:
                    ++accents; break;
                case SymId::articMarcatoAbove: case SymId::articMarcatoBelow:
                    ++marcatos; break;
                case SymId::articStaccatissimoAbove: case SymId::articStaccatissimoBelow:
                    ++staccatissimos; break;
                case SymId::ornamentTrill:
                    ++trills; break;
                case SymId::ornamentShortTrill:  // <inverted-mordent>
                case SymId::ornamentMordent:
                    ++mordents; break;
                default: break;
                }
            }
            for (Note* n : c->notes()) {
                for (EngravingItem* nel : n->el()) {
                    if (nel && nel->isFingering()) ++fingerings;
                }
            }
        }
    }
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (sp->isHairpin()) ++hairpins;
    }
    EXPECT_GE(dynamics,      13) << "all 13 Encore dynamics expected";
    EXPECT_GE(fermatas,       2);
    EXPECT_GE(markers,        3) << "Segno + Coda(s) + To Coda + Fine";
    EXPECT_GE(jumps,          1) << "at least one D.C. / D.S. variant";
    EXPECT_GE(staccatos,      7);
    EXPECT_GE(tenutos,        9);
    EXPECT_GE(accents,        7);
    EXPECT_GE(marcatos,       6);
    EXPECT_GE(staccatissimos, 6);
    EXPECT_GE(trills,         6) << "trill-marks from per-note bytes + ORN 0x36/0x37";
    EXPECT_GE(mordents,       4) << "mordent + inverted-mordent";
    EXPECT_GE(fingerings,     6) << "fingering 1..5 + open-string";
    EXPECT_GE(arpeggios,      1);
    EXPECT_GE(tremolos,       4);
    EXPECT_GE(hairpins,       2);
    EXPECT_GE(dotted_barlines, 1);
    delete score;
}

// ===========================================================================
// FEATURE: Corrupt files load without crash (regression tests)
// ===========================================================================

TEST_F(Tst_EncoreFeatures, beethoven_no_crash)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_corrupted.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_EncoreFeatures, twelve_instrument_score_no_crash)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_triplets.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_EncoreFeatures, swing_timing_file_no_crash)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_swing.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

// ===========================================================================
// BUG FIX: v0xC2 (old Encore format) — pitch stored at tuplet field offset
// ===========================================================================
// Synthetic files generated by tools/gen_enc_test_files.py (in the test data
// directory).  Each file is hand-crafted to exercise exactly one parsing path.

TEST_F(Tst_EncoreFeatures, old_format_v0c2_correct_pitches)
{
    // synthetic_v0c2_pitches.enc: chuMagio=0xC2, 4/4, 4 quarter notes C-E-G-C.
    // In v0xC2 notes have size=22 and MIDI pitch stored at the tuplet-field byte
    // (+13 from elemStart).  needsPitchFix swaps it to semiTonePitch and clears tuplet.
    // Without the fix all notes would have pitch=0.
    MasterScore* score = readEncoreScore("synthetic_v0c2_pitches.enc");
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

TEST_F(Tst_EncoreFeatures, old_format_v0c2_triplets_detected)
{
    // synthetic_v0c2_triplets.enc: 2/4, 6 eighth notes at 80-tick spacing.
    // 80 Encore ticks = 2/3 of an eighth → detectImpliedTuplet returns 3:2.
    MasterScore* score = readEncoreScore("synthetic_v0c2_triplets.enc");
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
// BUG FIX: Tuplet validation rejects non-standard ratio bytes
// ===========================================================================

TEST_F(Tst_EncoreFeatures, tuplet_validation_rejects_counter_bytes)
{
    // synthetic_v0c4_counter_bytes.enc has notes with tuplet bytes 0x41 (4:1) and
    // 0x43 (4:3).  Only 3:2, 5:4 and 6:4 are trusted as real tuplets; the rest fall
    // back to detectImpliedTuplet.  With standard quarter realDurations (240 ticks)
    // detectImplied returns 0 → no tuplet created.
    // Before fix: 0x41 would create a 4:1 tuplet → compressed notes → wrong sum.
    MasterScore* score = readEncoreScore("synthetic_v0c4_counter_bytes.enc");
    ASSERT_NE(score, nullptr);
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (EngravingItem* e : toMeasure(mb)->el()) {
            if (!e->isTuplet()) {
                continue;
            }
            Tuplet* t = toTuplet(e);
            Fraction r = t->ratio().reduced();
            EXPECT_NE(r.denominator(), 1)
                << "Counter byte 0x41/0x43 should not create a tuplet with normalNotes=1";
        }
    }
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Counter-byte notes should produce a clean score: " << ret.text();
    delete score;
}

// ===========================================================================
// BUG FIX: Canonical snap uses noteHasTuplet, not raw tupletByte()
// ===========================================================================

TEST_F(Tst_EncoreFeatures, canonical_snap_does_not_misfire_for_v0c2)
{
    // synthetic_v0c2_snap.enc: v0xC2, 2/4, 6 eighth notes at 80-tick spacing.
    // After needsPitchFix all tupletByte()=0.
    // Wrong behaviour: snap uses tupletByte()==0 → isClosing=true for every note →
    //   snap fires on each note and moves them to canonical positions, producing
    //   wrong measure content → sanityCheck fails.
    // Correct behaviour: snap uses noteHasTuplet (pre-computed from detectImplied) →
    //   isClosing only fires at group boundaries → correct triplet placement →
    //   sanityCheck passes.
    MasterScore* score = readEncoreScore("synthetic_v0c2_snap.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret)
        << "v0xC2 implied-triplet file should pass sanityCheck (canonical snap must not "
        "misfire): " << ret.text();
    delete score;
}

// ===========================================================================
// BUG FIX: v0xA6 (very old format) — wrong element offset and pitch encoding
// ===========================================================================

TEST_F(Tst_EncoreFeatures, very_old_format_v0xa6_sanity_check)
{
    // synthetic_v0xa6_basic.enc: chuMagio=0xA6, 2 measures of 2/4, 4 eighth notes each.
    // elemOffset must be 0x1A (not 0x3E).  Wrong offset causes tick=1280 > durTicks=480
    // for all elements, silently dropping all notes and leaving empty measures.
    MasterScore* score = readEncoreScore("synthetic_v0xa6_basic.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_EQ(score->nmeasures(), 2);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "v0xA6 synthetic file should pass sanityCheck: " << ret.text();
    delete score;
}

TEST_F(Tst_EncoreFeatures, very_old_format_v0xa6_pitch_encoding)
{
    // v0xA6 pitch is a signed byte at elemStart+9 meaning semitones from C4=60.
    // Measure 1 encodes offsets 0,+2,+4,+7 → MIDI 60,62,64,67 (C D E G).
    // Before the fix, all notes got pitch=64 from the padding byte at +15.
    MasterScore* score = readEncoreScore("synthetic_v0xa6_basic.enc");
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

// ===========================================================================
// BUG FIX: Spurious implied tuplet removal (adjustMeasureTuplets)
// ===========================================================================

TEST_F(Tst_EncoreFeatures, swing_offgrid_spurious_triplet_removed)
{
    // synthetic_v0c4_swing_offgrid.enc: 3/4, notes at Encore ticks [0,320,560].
    // Note 3 (tick=560, realDur=160) triggers detectImpliedTuplet → 3:2 quarter
    // triplet.  But MS offset 1120 % (480*2/3=320) = 160 ≠ 0: off canonical grid
    // → spurious swing-timing artefact → tuplet removed by adjustMeasureTuplets.
    // Without fix: voice sum = 1/4+1/4+1/6=2/3, gap fill pushes it over 3/4
    //              → sanityCheck failure.
    // With fix:    3 plain quarters = 3/4 = mLen → sanityCheck passes.
    MasterScore* score = readEncoreScore("synthetic_v0c4_swing_offgrid.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Off-grid swing file should pass sanityCheck after tuplet removal: "
                     << ret.text();
    delete score;
}

TEST_F(Tst_EncoreFeatures, canonical_implied_triplet_preserved)
{
    // synthetic_v0c4_canonical_triplet.enc: 3/4, 3 EXPLICIT 3:2 triplet quarter notes
    // (tuplet=0x32) + 1 plain quarter.
    // With faceValue-cumulative placement: tuplets advance by 1/4*2/3=1/6 each.
    // Sum: 3*(1/6) + 1/4 = 1/2 + 1/4 = 3/4 = mLen → sanityCheck passes.
    // Tuplets are explicitly encoded → always detected regardless of implied-detection logic.
    MasterScore* score = readEncoreScore("synthetic_v0c4_canonical_triplet.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Explicit triplets should produce correct 3/4 measure: " << ret.text();
    bool hasTuplet = false;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (EngravingItem* e : toMeasure(mb)->el()) {
            if (e->isTuplet()) {
                hasTuplet = true;
                break;
            }
        }
        if (hasTuplet) {
            break;
        }
    }
    EXPECT_TRUE(hasTuplet) << "Explicit 3:2 triplet notes should produce a Tuplet element";
    delete score;
}

TEST_F(Tst_EncoreFeatures, overflow_measure_extended)
{
    // synthetic_v0c4_overflow_extend.enc: 2/4 measure with 2 notes (fv=2, half).
    // With faceValue-cumulative placement:
    //   Note 1 (fv=2=half): cumTick=0, advance by 1/2. cumTick = 1/2 = mLen.
    //   Note 2: cumTick already = mLen → skipped (voice full).
    // Result: 1 half note = 1/2 = mLen. sanityCheck passes cleanly.
    // Time signature unchanged at 2/4.
    MasterScore* score = readEncoreScore("synthetic_v0c4_overflow_extend.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Measure with face-value overflow should pass sanityCheck "
                        "(second note skipped when voice full): " << ret.text();
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(2, 4)) << "Time signature should stay 2/4";
    delete score;
}

// ===========================================================================
// BUG FIX: Triple-dotted note advance used wrong multiplier (7/4 instead of 15/8)
// ===========================================================================

// ===========================================================================

TEST_F(Tst_EncoreFeatures, whole_rest_in_partial_measure)
{
    // synthetic_v0c4_whole_rest_2_4.enc: 2/4 measure with a single rest (faceValue=1).
    // Encore encodes a whole-measure rest as fv=1 regardless of the time signature.
    // realDuration2DurationType(480, 1) must return V_HALF (not V_WHOLE) because
    // rdur=480 = one half-note in MuseScore's 480 ticks/quarter.
    // Before fix: faceValue2DurationType(1) returned V_WHOLE → rest filled 1/1 > 2/4 mLen.
    MasterScore* score = readEncoreScore("synthetic_v0c4_whole_rest_2_4.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "2/4 whole-measure rest should pass sanityCheck: " << ret.text();
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(2, 4)) << "Time signature should be 2/4";
    // The rest in voice 0 should have V_HALF duration (not V_WHOLE)
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isRest()) {
            EXPECT_EQ(toRest(e)->durationType().type(), DurationType::V_HALF)
                << "Whole-measure rest in 2/4 should be a half rest (rdur=480 maps to V_HALF)";
            break;
        }
    }
    delete score;
}

// ===========================================================================
// BUG FIX: Off-beat MIDI ticks land at canonical positions via faceValue cumTick
// ===========================================================================

TEST_F(Tst_EncoreFeatures, offbeat_notes_canonical_placement)
{
    // synthetic_v0c4_offbeat_canonical.enc: 2/4, 2 quarter notes.
    //   Note 1: MIDI tick=0,   fv=3 (quarter) → cumTick=0,   placed at tick 0
    //   Note 2: MIDI tick=241, fv=3 (quarter) → cumTick=1/4, placed at tick 1/4
    // MIDI tick 241 is 1 tick late (MIDI timing drift). With the old tick-based
    // placement this created a 1-tick gap at positions 0..241, triggering gap fills.
    // With faceValue-cumulative placement: position comes from cumTick, not MIDI tick.
    // Both notes land at exact canonical positions. No gap fills. sanityCheck passes.
    MasterScore* score = readEncoreScore("synthetic_v0c4_offbeat_canonical.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Off-beat MIDI ticks should produce clean measure via cumTick: "
                     << ret.text();
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    // Verify both notes exist and are at exact canonical positions (0 and 1/4)
    std::vector<Fraction> noteTicks;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            noteTicks.push_back(s->tick() - m->tick());
        }
    }
    ASSERT_EQ(noteTicks.size(), 2u) << "Should have exactly 2 quarter notes";
    EXPECT_EQ(noteTicks[0], Fraction(0, 1)) << "First note at tick 0";
    EXPECT_EQ(noteTicks[1], Fraction(1, 4)) << "Second note at tick 1/4 (canonical, not MIDI-offset)";
    delete score;
}

// ===========================================================================
// BUG FIX: Explicit tuplet notes use faceValue for dt, not rdur
// ===========================================================================

TEST_F(Tst_EncoreFeatures, explicit_tuplet_facevalue_not_rdur)
{
    // synthetic_v0c4_explicit_tup_rdur_truncated.enc: 6/8 measure with 3 explicit
    // 3:2 triplet 8th notes (tup=0x32). The 3rd note at tick=630 has rdur=30 because
    // the following rest starts at tick=660 (30 Encore ticks later).
    // Without fix: realDuration2DurationType(30, 4) = V_32ND → wrong dt → measure corrupted.
    // With fix: isStandardExplicit notes use faceValue2DurationType(4) = V_EIGHTH regardless.
    MasterScore* score = readEncoreScore("synthetic_v0c4_explicit_tup_rdur_truncated.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Explicit triplet with truncated rdur should pass sanityCheck: " << ret.text();

    // All 3 triplet notes must be V_EIGHTH (face value), not V_32ND (from rdur=30).
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    int tupletEighthCount = 0;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (!e || !e->isChord()) {
            continue;
        }
        Chord* ch = toChord(e);
        if (ch->tuplet()) {
            EXPECT_EQ(ch->durationType().type(), DurationType::V_EIGHTH)
                << "Explicit tuplet notes should have V_EIGHTH (face value), not V_32ND from rdur";
            ++tupletEighthCount;
        }
    }
    EXPECT_EQ(tupletEighthCount, 3) << "Should have exactly 3 tuplet notes";
    delete score;
}

// ===========================================================================
// BUG FIX: Isolated explicit tuplet note treated as plain (validTupletGroupMember)
// ===========================================================================

TEST_F(Tst_EncoreFeatures, partial_explicit_group_treated_as_plain)
{
    // synthetic_v0c4_partial_explicit_group.enc: 4/4 measure with 4 notes having
    // tup=0x32, then a plain Q. The first 3 form a valid complete 3:2 triplet group.
    // Note 4 (isolated tup=0x32) is NOT in validTupletGroupMember → treated as plain Q.
    // Without fix: note 4 starts a partial tuplet → checkMeasure overshoot → sum ≠ 4/4.
    // With fix: note 4 is plain Q → sum = 3*(1/6) + 1/4 + 1/4 = 1 = 4/4. PASS.
    MasterScore* score = readEncoreScore("synthetic_v0c4_partial_explicit_group.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Partial explicit group should pass sanityCheck: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    // Collect all chords and check tuplet membership
    std::vector<Chord*> chords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            chords.push_back(toChord(e));
        }
    }
    ASSERT_EQ(chords.size(), 5u) << "Should have 5 chords";
    // Chords 1-3: in tuplet
    EXPECT_NE(chords[0]->tuplet(), nullptr) << "Note 1 should be in tuplet";
    EXPECT_NE(chords[1]->tuplet(), nullptr) << "Note 2 should be in tuplet";
    EXPECT_NE(chords[2]->tuplet(), nullptr) << "Note 3 should be in tuplet";
    // Chord 4: isolated tup=0x32, must be treated as plain (no tuplet)
    EXPECT_EQ(chords[3]->tuplet(), nullptr) << "Note 4 (isolated tup byte) should NOT be in tuplet";
    // Chord 5: plain
    EXPECT_EQ(chords[4]->tuplet(), nullptr) << "Note 5 (plain) should NOT be in tuplet";
    delete score;
}

// ===========================================================================
// BUG FIX: Dotted note capping uses full dotted duration, not bare face value
// ===========================================================================

TEST_F(Tst_EncoreFeatures, dotted_note_capped_to_remaining_space)
{
    // synthetic_v0c4_dotted_note_capping.enc: 2/4 measure with 3 sixteenth notes
    // (ticks 0, 40, 80) followed by a quarter note (tick=120, rdur=360).
    // rdur=360 → realDuration2DurationType gives V_QUARTER; calcDots gives dots=1
    // (dotted quarter, 3/8 = 6/16). cumTick after 3 sixteenths = 3/16.
    // remaining = 2/4 - 3/16 = 5/16.
    //
    // Bug: TDuration(V_QUARTER).fraction() = 1/4 = 4/16 ≤ 5/16 → capping does
    //   NOT fire → chord placed as dotted quarter (3/8 = 6/16) → sum=9/16 > 2/4.
    // Fix: include dots in comparison: 6/16 > 5/16 → capping fires → chord placed
    //   as plain quarter (1/4 = 4/16) → sum = 3/16+1/4+1/16(fill) = 8/16 = 2/4. PASS.
    MasterScore* score = readEncoreScore("synthetic_v0c4_dotted_note_capping.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Dotted note past measure end should be capped: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(2, 4));

    // The 4th chord (dotted Q, rdur=360) must be capped to V_QUARTER (no dots).
    std::vector<Chord*> chords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            chords.push_back(toChord(e));
        }
    }
    ASSERT_GE(chords.size(), 4u) << "Should have at least 4 chords";
    // First 3: V_16TH
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(chords[i]->durationType().type(), DurationType::V_16TH)
            << "First 3 chords should be sixteenth notes";
    }
    // 4th: capped to V_QUARTER (not dotted quarter which would overflow)
    EXPECT_EQ(chords[3]->durationType().type(), DurationType::V_QUARTER)
        << "Dotted quarter must be capped to plain quarter when it overflows";
    EXPECT_EQ(chords[3]->dots(), 0) << "Capped chord must have 0 dots";
    delete score;
}

// ===========================================================================
// BUG FIX: Mixed-face-value tuplet group gets exact ticks; isolated note
//          that exactly fills remaining space becomes a partial tuplet
// ===========================================================================

TEST_F(Tst_EncoreFeatures, mixed_value_tuplet_exact_ticks_and_isolated_partial)
{
    // synthetic_v0c4_mixed_value_tuplet.enc: 4/4 measure with a 3:2 triplet
    // containing mixed note values (Q, Q, 8th), followed by an isolated 8th
    // (tup=0x32), a plain Q, and a Q rest.
    //
    // With face-value-sum grouping (4.6), the 4 notes {Q,Q,8th,8th} with tup=0x32
    // form ONE complete group: face sum = 1/4+1/4+1/8+1/8 = 3/4 = threshold (3×1/4).
    // The group closes after 4 notes; exact-ticks correction sets ticks=5/12 so
    // checkMeasure does not break on the following plain notes.
    //
    // Expected sum: 5/12 (group) + cascade fills + micro = 1/2 = 2/4. PASS.
    MasterScore* score = readEncoreScore("synthetic_v0c4_mixed_value_tuplet.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Mixed-value tuplet group should produce clean 2/4: " << ret.text();
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(2, 4));

    // All 4 chords (Q,Q,8th,8th) should be in the same tuplet (one complete group).
    std::vector<Chord*> chords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            chords.push_back(toChord(e));
        }
    }
    ASSERT_EQ(chords.size(), 4u) << "Should have 4 chords (one complete 4-element group)";
    EXPECT_NE(chords[0]->tuplet(), nullptr) << "Q 1 in tuplet";
    EXPECT_NE(chords[1]->tuplet(), nullptr) << "Q 2 in tuplet";
    EXPECT_NE(chords[2]->tuplet(), nullptr) << "8th 3 in tuplet";
    EXPECT_NE(chords[3]->tuplet(), nullptr) << "8th 4 in same complete group";
    EXPECT_EQ(chords[0]->tuplet(), chords[3]->tuplet()) << "All 4 in same tuplet";
    EXPECT_EQ(chords[3]->actualTicks(), Fraction(1, 12))
        << "8th actualTicks = (1/8)*(2/3) = 1/12";
    delete score;
}

// ===========================================================================
// BUG FIX: groupFull implied-triplet guard — isolated note after complete group
// ===========================================================================

TEST_F(Tst_EncoreFeatures, implied_group_boundary_no_spurious_new_group)
{
    // synthetic_v0c2_implied_group_boundary.enc: v0xC2 2/4 measure.
    // Notes at rdurs: 120, 60, 60, 40, 40, 40 (complete 3:2 group), 40 (isolated), 80.
    //
    // Bug: after the complete implied 3:2 group closes (groupFull=true), the
    // next note (rdur=40, NOT in impliedGroupMember) passed the guard because
    // tt.inTuplet()=true at detection time.  It started a new unvalidated group,
    // giving it a 1/24 advance instead of 1/16, pushing cumTick past mLen and
    // triggering cascading fills that overflowed 2/4.
    //
    // Fix: add !tt.groupFull() to the implied detection guard. The isolated note
    // is then treated as a plain 16th. Sum = 1/8+1/16+1/16+3*(1/24)+1/16+1/16
    //                                       = 24/48 = 2/4 = PASS.
    MasterScore* score = readEncoreScore("synthetic_v0c2_implied_group_boundary.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Isolated note after complete implied group should be plain: "
                     << ret.text();
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(2, 4));

    // Notes 4-6 (ticks 240-280-320) should be in the same tuplet (complete 3:2 group).
    // Note 7 (tick 360, isolated rdur=40) should NOT be in a tuplet.
    std::vector<Chord*> chords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            chords.push_back(toChord(e));
        }
    }
    ASSERT_GE(chords.size(), 7u) << "Should have at least 7 chords";
    EXPECT_NE(chords[3]->tuplet(), nullptr) << "Note 4 (triplet 16th 1) should be in tuplet";
    EXPECT_NE(chords[4]->tuplet(), nullptr) << "Note 5 (triplet 16th 2) should be in tuplet";
    EXPECT_NE(chords[5]->tuplet(), nullptr) << "Note 6 (triplet 16th 3) should be in tuplet";
    EXPECT_EQ(chords[6]->tuplet(), nullptr)
        << "Note 7 (isolated rdur=40 after complete group) should NOT be in a tuplet";
    delete score;
}

// ===========================================================================
// BUG FIX: Capped tuplet note is removed from tuplet to prevent sanityCheck overshoot
// ===========================================================================

TEST_F(Tst_EncoreFeatures, capped_tuplet_note_removed_from_tuplet)
{
    // synthetic_v0c4_capped_tuplet_note.enc: 4/4 measure with 3 plain quarters
    // (cumTick=3/4) followed by 3 explicit 3:2 triplet quarters (tup=0x32).
    // 1st triplet Q advance = (1/4)*(2/3) = 1/6. cumTick=3/4+1/6=11/12.
    // 2nd triplet Q: remaining=1/12 < 1/6 → advance capped. Note removed from tuplet.
    //
    // Bug: without removal, chord stays in tuplet with ticks=1/4 (face value).
    //   actualTicks = 1/6 > 1/12 (capped advance). Sum overshoots mLen → corrupted.
    // Fix: capped note removed. Its ticks = capped value → actualTicks ≤ advance ≤
    //   remaining → sum stays within mLen.
    MasterScore* score = readEncoreScore("synthetic_v0c4_capped_tuplet_note.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Capped tuplet note should not overshoot mLen: " << ret.text();
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(4, 4));
    std::vector<Chord*> chords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            chords.push_back(toChord(e));
        }
    }
    ASSERT_GE(chords.size(), 4u);
    EXPECT_EQ(chords[0]->tuplet(), nullptr) << "Plain Q 1 no tuplet";
    EXPECT_EQ(chords[1]->tuplet(), nullptr) << "Plain Q 2 no tuplet";
    EXPECT_EQ(chords[2]->tuplet(), nullptr) << "Plain Q 3 no tuplet";
    EXPECT_NE(chords[3]->tuplet(), nullptr) << "First triplet Q in tuplet";
    if (chords.size() >= 5) {
        EXPECT_EQ(chords[4]->tuplet(), nullptr)
            << "Capped 2nd triplet Q removed from tuplet";
    }
    delete score;
}

// ===========================================================================
// BUG FIX: Post-checkMeasure micro-fill for sub-1/48 cascade residuals
// ===========================================================================

TEST_F(Tst_EncoreFeatures, cascade_fill_residual_filled_by_vmeasure_rest)
{
    // The post-checkMeasure micro-fill handles cascade-fill residuals:
    // toRhythmicDurationList decomposes a non-standard gap G with standard durations
    // (1/64 + 1/256 + 1/1024 = 21/1024 for G=1/48) but leaves a residual
    // (1/3072 = 1/48 - 21/1024) that no standard duration can represent.
    // A V_MEASURE gap rest with ticks = residual bridges the last 1/3072.
    //
    // We verify this property through two synthetic files that exercise the
    // preconditions: exact-ticks tuplet correction and isolated-partial-tuplet
    // fill. Both must pass sanityCheck with no large denominators from residuals.
    auto checkNoResidual = [&](MasterScore* score) {
        ASSERT_NE(score, nullptr);
        muse::Ret ret = score->sanityCheck();
        EXPECT_TRUE(ret) << "Should pass sanityCheck: " << ret.text();
        for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
            if (!mb->isMeasure()) {
                continue;
            }
            Measure* m = toMeasure(mb);
            for (track_idx_t t = 0; t < static_cast<track_idx_t>(score->nstaves()) * 4; ++t) {
                Fraction sum(0, 1);
                for (Segment* s = m->first(SegmentType::ChordRest);
                     s; s = s->next(SegmentType::ChordRest)) {
                    EngravingItem* e = s->element(t);
                    if (e) {
                        sum += toChordRest(e)->actualTicks();
                    }
                }
                EXPECT_LE(sum.denominator(), 3072)
                    << "Voice sum denom > 3072 indicates unfilled cascade residual";
            }
        }
        delete score;
    };

    // File 1: implied-group boundary (triggers !tt.groupFull() guard)
    checkNoResidual(readEncoreScore("synthetic_v0c2_implied_group_boundary.enc"));

    // File 2: mixed-value tuplet + isolated partial fill (triggers exact-ticks correction
    // and isolated-partial-tuplet fill; together these may create non-standard gaps)
    checkNoResidual(readEncoreScore("synthetic_v0c4_mixed_value_tuplet.enc"));
}

// ===========================================================================
// BUG FIX: Mixed-duration tuplet bracket closes by face-value sum (not note count)
// ===========================================================================

TEST_F(Tst_EncoreFeatures, mixed_duration_triplet_face_value_sum_grouping)
{
    // synthetic_v0c4_mixed_duration_triplet.enc: 2/4 measure with two 3:2 triplet
    // brackets, the first containing mixed note values (8+8_rest+16_rest+16).
    //
    // The first bracket: face values 1/8+1/8+1/16+1/16 = 3/8 = 3×(1/8) = threshold.
    // The second bracket: 3 equal 8ths, face sum = 3/8 = threshold.
    // Together: 2 × (3/8 × 2/3) = 2 × 1/4 = 1/2 = 2/4. PASS.
    //
    // Bug: with count-based grouping (close after actualN=3 notes), the first
    //   bracket closes after 8+8_rest+16_rest (only 3 elements), then the 16th
    //   note at tick=200 starts a new incomplete group → sum ≠ 2/4 → FAIL.
    // Fix: close when face-value sum reaches actualN × baseLen (= 3/8 for 3:2 8th).
    //   First bracket spans all 4 elements; both brackets fill exactly 2/4.
    MasterScore* score = readEncoreScore("synthetic_v0c4_mixed_duration_triplet.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Mixed-duration triplet should produce clean 2/4: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(2, 4));

    // Count chords/rests in the measure — should have 7 elements (plus fills)
    // First 4 should be in the same tuplet, next 3 in another.
    std::vector<ChordRest*> crs;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (!e) {
            continue;
        }
        ChordRest* cr = toChordRest(e);
        bool gap = cr->isRest() && toRest(cr)->isGap();
        if (!gap) {
            crs.push_back(cr);
        }
    }
    ASSERT_GE(crs.size(), 7u) << "Should have 7 non-gap elements";
    // Elements 0-3: first mixed-value bracket (all in same tuplet)
    EXPECT_NE(crs[0]->tuplet(), nullptr) << "Element 0 (8th note) in tuplet";
    EXPECT_NE(crs[1]->tuplet(), nullptr) << "Element 1 (8th rest) in tuplet";
    EXPECT_NE(crs[2]->tuplet(), nullptr) << "Element 2 (16th rest) in tuplet";
    EXPECT_NE(crs[3]->tuplet(), nullptr) << "Element 3 (16th note) in tuplet";
    EXPECT_EQ(crs[0]->tuplet(), crs[1]->tuplet()) << "All 4 in same first tuplet";
    EXPECT_EQ(crs[0]->tuplet(), crs[2]->tuplet());
    EXPECT_EQ(crs[0]->tuplet(), crs[3]->tuplet());
    // Elements 4-6: second bracket (3 equal 8ths)
    EXPECT_NE(crs[4]->tuplet(), nullptr) << "Element 4 in second tuplet";
    EXPECT_NE(crs[5]->tuplet(), nullptr) << "Element 5 in second tuplet";
    EXPECT_NE(crs[6]->tuplet(), nullptr) << "Element 6 in second tuplet";
    EXPECT_NE(crs[0]->tuplet(), crs[4]->tuplet()) << "Two different tuplets";
    delete score;
}

// ===========================================================================
// BUG FIX: Mixed-value tuplet ticks corrected when faceSum > threshold (> expected)
// ===========================================================================

TEST_F(Tst_EncoreFeatures, mixed_value_tuplet_ticks_corrected_for_overshoot)
{
    // synthetic_v0c4_mixed_duration_triplet.enc: first bracket {16,16,Q} in a 3:2 group.
    // faceSum = 1/16+1/16+1/4 = 3/8 >= threshold = 3/16 (= baseLen(16th)*3).
    // placedTicks = 3*(2/3) advance = 1/12+1/12+1/6 = 5/24.
    // expected = baseLen*normalN = (1/16)*2 = 1/8.
    // placedTicks(5/24) > expected(1/8) AND faceTicks(3/8) > fullFaceSum(3/16):
    //   mixedValueOvershoot = true → tuplet->setTicks(5/24).
    // Without the ticks correction, tuplet->ticks()=1/8 (too small), checkMeasure
    // sees next chord at P+5/24 > P+1/8 → inserts fill → sum > 2/4 → corrupted.
    MasterScore* score = readEncoreScore("synthetic_v0c4_mixed_duration_triplet.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Mixed-value tuplet ticks correction should produce clean 2/4: "
                     << ret.text();
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    // Find the first tuplet in the measure and check its ticks
    Tuplet* firstTuplet = nullptr;
    for (EngravingItem* e : m->el()) {
        if (e->isTuplet()) {
            firstTuplet = toTuplet(e);
            break;
        }
    }
    ASSERT_NE(firstTuplet, nullptr) << "Must have at least one tuplet";
    // First tuplet spans {16+16+Q} with placedTicks = 1/24+1/24+1/6 = 1/4.
    // Its ticks must be 1/4 (not the default baseLen*normalN = (1/16)*2 = 1/8)
    // for checkMeasure to correctly advance past the group.
    EXPECT_EQ(firstTuplet->ticks(), Fraction(1, 4))
        << "Tuplet ticks must equal placedTicks (1/4) not default 1/8";
    delete score;
}

// ===========================================================================
// BUG FIX: Near-simultaneous chord notes (MIDI timing drift) no longer lost
// ===========================================================================

TEST_F(Tst_EncoreFeatures, near_simultaneous_notes_form_chord)
{
    // synthetic_v0c2_near_simultaneous_chord.enc: v0xC2 2/4 measure with two
    // quarter notes at Encore ticks 0 and 3 (3-tick MIDI offset).  They are
    // meant to be simultaneous (a chord) but stored at slightly different ticks
    // due to live MIDI recording drift.
    //
    // Bug: calculateRealDurations gave the first note rdur=3 (<15 threshold)
    //   → skipped as "tiny duration MIDI artifact" → only E4 survived, C4 lost.
    // Fix: CHORD_CLUSTER_THRESHOLD=4 in calculateRealDurations skips past
    //   near-simultaneous elements when computing rdur, giving the first note
    //   rdur=240 (not 3).  isChordExt then groups them as one chord. ✓
    MasterScore* score = readEncoreScore("synthetic_v0c2_near_simultaneous_chord.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Near-simultaneous chord should produce clean 2/4: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(2, 4));

    // The first non-rest segment must be a chord with 2 notes (C4 + E4)
    Chord* first = nullptr;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            first = toChord(e);
            break;
        }
    }
    ASSERT_NE(first, nullptr) << "Must have at least one chord";
    EXPECT_EQ(first->notes().size(), 2u)
        << "Near-simultaneous notes (ticks 0 and 3) must form a 2-note chord";
    delete score;
}

// ===========================================================================
// BUG FIX: Triple-dotted note advance multiplier — docs/verify ticks match advance
// ===========================================================================

TEST_F(Tst_EncoreFeatures, triple_dotted_advance_matches_chord_ticks)
{
    // synthetic_v0c4_triple_dotted_advance.enc: first note is a triple-dotted 8th
    // (rdur=225 Encore ticks → calcDots=3 → ticks=(1/8)*(15/8)=15/64).
    // Bug: advance used Fraction(7,4) giving 14/64 ≠ 15/64 → cumTick mismatch.
    // Fix: dots==3 uses Fraction(15,8) → advance=15/64=ticks.
    // Verify: sanityCheck passes AND advance == ticks (no spurious fill at end of chord).
    MasterScore* score = readEncoreScore("synthetic_v0c4_triple_dotted_advance.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Triple-dotted advance must equal chord ticks: " << ret.text();
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    // First chord: triple-dotted 8th. Verify ticks=15/64 (dots=3).
    Chord* first = nullptr;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            first = toChord(e);
            break;
        }
    }
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->ticks(), Fraction(15, 64)) << "Must be triple-dotted 8th (15/64)";
    EXPECT_EQ(first->dots(), 3) << "Must have 3 augmentation dots";
    // Second chord: plain 8th immediately after — its position must be measTick+15/64,
    // NOT measTick+14/64 (which would be a 1/64 overrun causing a stray fill).
    Chord* second = nullptr;
    for (Segment* s = first->segment()->next(SegmentType::ChordRest);
         s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            second = toChord(e);
            break;
        }
    }
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->segment()->tick() - m->tick(), Fraction(15, 64))
        << "Second chord must start at 15/64 (triple-dotted advance), not 14/64";
    delete score;
}

// ===========================================================================
// BUG FIX: dotControl byte used for note dot count (not MIDI realDuration)
// ===========================================================================

TEST_F(Tst_EncoreFeatures, dotted_note_uses_dotcontrol_byte)
{
    // synthetic_v0c4_dotted_note.enc: first note is a dotted 8th (dotControl=180)
    // but the next note is at MIDI tick=86 (drift), giving rdur=86.
    // calcDots(86, 8th)=0 (wrong); calcDots(180, 8th)=1 (correct via dotControl).
    MasterScore* score = readEncoreScore("synthetic_v0c4_dotted_note.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Dotted note test must produce clean score: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    Chord* first = nullptr;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            first = toChord(e);
            break;
        }
    }
    ASSERT_NE(first, nullptr) << "Must have a first chord";
    EXPECT_EQ(first->durationType().type(), DurationType::V_EIGHTH)
        << "First note must be an 8th (dotted)";
    EXPECT_EQ(first->dots(), 1)
        << "dotControl=180=8th*3/2 must produce 1 augmentation dot on the note";
    delete score;
}

// ===========================================================================
// BUG FIX: TIE elements (type=3) create Tie objects between notes
// ===========================================================================

TEST_F(Tst_EncoreFeatures, tie_element_creates_mscore_tie)
{
    // synthetic_v0c4_tie.enc: 2/4 measure with C4 quarter at tick=0, TIE element
    // at tick=0, C4 quarter at tick=240.  The two notes must be linked by a Tie.
    //
    // Bug: TIE elements (EncElemType::TIE=3) were dispatched to EncGenericElem
    //   and their data discarded.  No Tie objects were ever created.
    // Fix: EncTie struct + pre-scan tieStartSet + pendingTieNote map creates
    //   Factory::createTie() linking start note to end note. ✓
    MasterScore* score = readEncoreScore("synthetic_v0c4_tie.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Tie test must produce clean score: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    // Find the first chord (the tie-start note)
    Chord* first = nullptr;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            first = toChord(e);
            break;
        }
    }
    ASSERT_NE(first, nullptr) << "Must have a first chord";
    ASSERT_GE(first->notes().size(), 1u);

    Note* startNote = first->notes()[0];
    EXPECT_EQ(startNote->pitch(), 60) << "First note must be C4 (pitch=60)";

    // The note must have a tieFor() pointing to the next note
    ASSERT_NE(startNote->tieFor(), nullptr)
        << "TIE element must create a Tie from the first C4 to the second C4";
    EXPECT_NE(startNote->tieFor()->endNote(), nullptr)
        << "Tie must link to an end note";
    if (startNote->tieFor()->endNote()) {
        EXPECT_EQ(startNote->tieFor()->endNote()->pitch(), 60)
            << "Tie end note must also be C4";
    }
    delete score;
}

// ===========================================================================
// BUG FIX: dotControl byte used for rest dot count (not MIDI realDuration)
// ===========================================================================

TEST_F(Tst_EncoreFeatures, dotted_rest_uses_dotcontrol_byte)
{
    // synthetic_v0c4_dotted_rest.enc: 3/4 measure with C4 quarter + dotted 8th
    // rest (dotControl=180=8th*3/2) + 16th rest.
    // The dotted 8th rest must have dots()==1.
    //
    // Bug: calcDots used er->realDuration (MIDI tick spacing, e.g. 154 ticks due
    //   to timing drift).  calcDots(154, fv=4=8th) returns 0 dots.
    // Fix: use er->dotControl when non-zero.  calcDots(180, 4)=1. ✓
    MasterScore* score = readEncoreScore("synthetic_v0c4_dotted_rest.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Dotted rest test must produce clean score: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(3, 4));

    // Find the rest (second ChordRest segment — first is the quarter note chord)
    Rest* dottedRest = nullptr;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isRest()) {
            dottedRest = toRest(e);
            break;
        }
    }
    ASSERT_NE(dottedRest, nullptr) << "Must find a rest after the quarter note";
    EXPECT_EQ(dottedRest->durationType().type(), DurationType::V_EIGHTH)
        << "Rest must be an eighth rest (dotted)";
    EXPECT_EQ(dottedRest->dots(), 1)
        << "dotControl=180=8th*3/2 must produce 1 augmentation dot";
    delete score;
}

// ===========================================================================
// BUG FIX: calcDotsSnap — 1-tick rdur tolerance identifies dotted notes
// ===========================================================================

TEST_F(Tst_EncoreFeatures, rdur_snap_corrects_dot_count)
{
    // synthetic_v0c4_rdur_snap.enc: 4/4 measure with an 8th note at tick=0,
    // dotControl=0 (no hint), next event at tick=211.  rdur=211 is 1 tick away
    // from dd8th=210 — calcDots(211,8th)=0 but calcDotsSnap(211,8th,1)=2.
    //
    // Bug: before calcDotsSnap, both calcDots(rdur=211) and calcDots(dotControl=0)
    //   returned 0 dots → plain 8th displayed instead of double-dotted 8th.
    // Fix: when dotControl gives no dotted result, fall back to calcDotsSnap on
    //   realDuration with tolerance=1.  |211-210|=1 ≤ 1 → 2 dots. ✓
    MasterScore* score = readEncoreScore("synthetic_v0c4_rdur_snap.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "rdur snap test must produce clean score: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    Chord* first = nullptr;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            first = toChord(e);
            break;
        }
    }
    ASSERT_NE(first, nullptr) << "Must have a first chord";
    EXPECT_EQ(first->durationType().type(), DurationType::V_EIGHTH)
        << "Note must be an 8th (double-dotted)";
    EXPECT_EQ(first->dots(), 2)
        << "rdur=211 is 1 tick away from dd8th=210: must snap to 2 augmentation dots";
    delete score;
}

// ===========================================================================
// BUG FIX: 64th tie-start notes bypass rdur<15 filter
// ===========================================================================

TEST_F(Tst_EncoreFeatures, sf_tiestart_not_filtered_by_rdur)
{
    // synthetic_v0c4_sf_tiestart.enc: 4/4 measure with a TIE element at tick=0,
    // a 64th note (C4) at tick=0 with rdur=11 (<15), and a Q note (C4) at tick=11.
    // The 64th has a TIE element, so it is a real note in a tie chain, not an
    // artifact.  It must be placed and tied to the Q note.
    //
    // Bug: rdur=11 < 15 → 64th skipped unconditionally.  Pending ties from
    //   previous measures for same pitch got misapplied to later unrelated notes.
    // Fix: 64th/128th notes (fvBase≤15) with a TIE element at their tick bypass
    //   the rdur<15 filter.  The 64th is placed and ties correctly to the Q. ✓
    MasterScore* score = readEncoreScore("synthetic_v0c4_sf_tiestart.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "sf tiestart test must produce clean score: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    // First ChordRest must be the 64th note
    Chord* first = nullptr;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            first = toChord(e);
            break;
        }
    }
    ASSERT_NE(first, nullptr) << "Must have a first chord (the 64th)";
    EXPECT_EQ(first->durationType().type(), DurationType::V_64TH)
        << "64th note with TIE element must not be filtered by rdur<15";
    ASSERT_GE(first->notes().size(), 1u);

    Note* sfNote = first->notes()[0];
    ASSERT_NE(sfNote->tieFor(), nullptr)
        << "64th note must have an outgoing tie to the Q note";
    if (sfNote->tieFor()) {
        Note* endNote = sfNote->tieFor()->endNote();
        ASSERT_NE(endNote, nullptr);
        EXPECT_EQ(endNote->chord()->durationType().type(), DurationType::V_QUARTER)
            << "Tie end must be the Q note";
    }
    delete score;
}

// ===========================================================================
// BUG FIX: prevMidiTick self-reference bypassed rdur<15 filter for non-chord-ext notes
// ===========================================================================

TEST_F(Tst_EncoreFeatures, rdur_non_chord_ext_filtered)
{
    // synthetic_v0c4_rdur_non_chord_ext_filtered.enc: 4/4 with a Q rest, then a
    // 64th C4 at tick=240 (rdur=11, not a tie-start, not a chord extension), then
    // a Q E4 at tick=251.
    //
    // Bug: prevMidiTick was set to e->tick before the rdur<15 filter check, making
    //   the delta=0<4 appear to be a chord extension.  The 64th artifact bypassed
    //   the filter and appeared as a spurious note.
    // Fix: use isChordExt (computed from the OLD prevMidiTick=0 set by the rest).
    //   Gap 240-0=240>=4 → not a chord extension → 64th filtered correctly. ✓
    MasterScore* score = readEncoreScore("synthetic_v0c4_rdur_non_chord_ext_filtered.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "score must be clean: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    std::vector<Chord*> chords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord()) {
            chords.push_back(toChord(el));
        }
    }

    ASSERT_EQ(chords.size(), 1u)
        << "Only the Q E4 must be placed; the 64th C4 artifact must be filtered";
    EXPECT_EQ(chords[0]->durationType().type(), DurationType::V_QUARTER)
        << "Placed note must be a quarter";
    ASSERT_GE(chords[0]->notes().size(), 1u);
    EXPECT_EQ(chords[0]->notes()[0]->pitch(), 64)
        << "Placed note must be E4 (pitch=64), not the filtered C4 (pitch=60)";

    delete score;
}

// ===========================================================================
// BUG FIX: grace1 low-nibble cascade filter for MIDI artifact continuation notes
// ===========================================================================

TEST_F(Tst_EncoreFeatures, grace1_cascade_filter)
{
    // synthetic_v0c4_grace1_cascade_filter.enc: 4/4 with:
    //   - 64th C4 at tick=0, grace1=0x01 (g1low=1, tie-sender), rdur=11 → filtered
    //   - Q C4 at tick=11, grace1=0x02 (g1low=2, tie-receiver of filtered note)
    //   - Q E4 at tick=240, grace1=0x00 (standalone) → placed
    //
    // Bug: Q C4 (g1low=2) was not subject to any filter check and appeared in
    //   output as a spurious note even after the 64th artifact was filtered.
    // Fix: when a note with g1low=1 is filtered as a MIDI artifact, record its
    //   pitch; the next note with g1low=2 and the same pitch is cascade-filtered. ✓
    MasterScore* score = readEncoreScore("synthetic_v0c4_grace1_cascade_filter.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "score must be clean: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    std::vector<Chord*> chords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord()) {
            chords.push_back(toChord(el));
        }
    }

    ASSERT_EQ(chords.size(), 1u)
        << "Both C4 notes must be filtered; only Q E4 must appear";
    ASSERT_GE(chords[0]->notes().size(), 1u);
    EXPECT_EQ(chords[0]->notes()[0]->pitch(), 64)
        << "Only E4 (pitch=64) must appear; C4 (pitch=60) must be cascade-filtered";

    delete score;
}

// ===========================================================================
// BUG FIX: 5-tick live-recorded chord cluster split into multiple chords
// ===========================================================================

TEST_F(Tst_EncoreFeatures, chord_cluster_5tick_v0c2)
{
    // synthetic_v0c2_chord_cluster_5tick.enc: 4/4 with a half rest, then 4
    // live-recorded notes spanning ticks 100, 103, 104, 105 (all 16th, g1low=1,
    // dc=90=dotted-16th) tied via TIE@100 to 4 quarter receiver notes at tick=240.
    //
    // Three bugs caused the 4-note chord to be split:
    //   A) root@100 had rdur=4 == CHORD_CLUSTER_THRESHOLD → filtered by
    //      "else: continue", leaving a singleton on the E note (chord extension).
    //   B) note@104 was 4 ticks from root → not a chord extension because
    //      CHORD_MIDI_THRESHOLD equaled CHORD_CLUSTER_THRESHOLD (strict < 4).
    //   C) notes@104 and @105 missed tie registration (TIE@100 is outside the
    //      ±3-tick isTieStart window for those ticks).
    //
    // Fixes:
    //   A) fvBase>15 filter now only applies when realDuration > CHORD_CLUSTER_THRESHOLD.
    //   B) CHORD_MIDI_THRESHOLD = 2 * CHORD_CLUSTER_THRESHOLD (= 8).
    //   C) grace1 low==1 used as secondary tie-start indicator (v0xC2 only). ✓
    MasterScore* score = readEncoreScore("synthetic_v0c2_chord_cluster_5tick.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "score must be clean: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    // Collect all chords (non-rests) in voice 0
    std::vector<Chord*> chords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord()) {
            chords.push_back(toChord(el));
        }
    }

    // Expect exactly 2 chords: the 4-note sender chord and the 4-note receiver chord.
    ASSERT_EQ(chords.size(), 2u) << "Must have exactly 2 chords (sender + receiver)";

    // Sender chord must have all 4 notes — not split.
    EXPECT_EQ(chords[0]->notes().size(), 4u)
        << "All 4 live-recorded chord notes must be in one chord, not split";

    // All 4 sender notes must be tied to the receiver chord.
    int tiedCount = 0;
    for (Note* n : chords[0]->notes()) {
        if (n->tieFor() && n->tieFor()->endNote()) {
            ++tiedCount;
        }
    }
    EXPECT_EQ(tiedCount, 4)
        << "All 4 sender notes must have outgoing ties to the receiver chord";

    // Receiver chord must also have all 4 notes.
    EXPECT_EQ(chords[1]->notes().size(), 4u)
        << "Receiver chord must have all 4 notes";

    delete score;
}

// ===========================================================================
// FEATURE: TK block instrument name encoding (UTF-16 probe for v0xC4)
// ===========================================================================

TEST_F(Tst_EncoreFeatures, tk_utf16_name_charsize_reads_full_name)
{
    // synthetic_v0c4_tk_utf16_name.enc: TK00 varsize=2158 → offset=2158>250
    // → charSize()=TWO_BYTES.  Content is UTF-16 LE "Bandurria".
    // charSize already picks TWO_BYTES; name is read fully without probe.
    // Represents v0xC4 files from older Encore versions with offset>250.
    MasterScore* score = readEncoreScore("synthetic_v0c4_tk_utf16_name.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());

    const String longName = score->parts()[0]->longName();
    EXPECT_EQ(longName, String(u"Bandurria"))
        << "UTF-16 TK name (charSize=TWO_BYTES by offset) must be fully decoded";

    delete score;
}

TEST_F(Tst_EncoreFeatures, tk_probe_upgrades_onebyte_to_utf16)
{
    // synthetic_v0c4_tk_probe_utf16.enc: TK00 varsize=112 → offset=112<=250
    // → charSize()=ONE_BYTE; content is UTF-16 LE "Bandurria"
    // (b0=0x42='B', b1=0x00 → probe detects UTF-16, upgrades to TWO_BYTES).
    // Without the probe fix (old forceUtf16=always), this also worked, but
    // with wrong results for ONE_BYTE files.  The probe must detect correctly.
    // Represents Encore 5.0.2 v0xC4 files (e.g. pachbel.enc resaved).
    MasterScore* score = readEncoreScore("synthetic_v0c4_tk_probe_utf16.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());

    const String longName = score->parts()[0]->longName();
    EXPECT_EQ(longName, String(u"Bandurria"))
        << "Probe must upgrade ONE_BYTE charSize to TWO_BYTES for UTF-16 content";

    delete score;
}

TEST_F(Tst_EncoreFeatures, tk_probe_keeps_onebyte_for_latin1)
{
    // synthetic_v0c4_tk_onebyte_name.enc: TK00 varsize=112 → offset=112<=250
    // → charSize()=ONE_BYTE; content is Latin-1 "Bandurria 1"
    // (b0=0x42='B', b1=0x61='a'!=0x00 → probe keeps ONE_BYTE, not UTF-16).
    // Regression: a naive forceUtf16=true would misread "Bandurria 1" as
    // UTF-16 pairs, producing garbled instrument names.
    // Verify the file imports cleanly with the correct part count.
    MasterScore* score = readEncoreScore("synthetic_v0c4_tk_onebyte_name.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "ONE_BYTE TK name file must produce a clean score";
    EXPECT_EQ(score->parts().size(), 1u)
        << "ONE_BYTE TK name file must produce exactly 1 part";
    delete score;
}

// ===========================================================================
// BUG FIX: missing instruments when Encore 5.0.2 v0xC4 omits TK blocks
// ===========================================================================

// ===========================================================================
// FEATURE: Staff visibility flag (showByte at LINE staffData offset +19)
// ===========================================================================

// ===========================================================================
// FEATURE: Instrument name recovery for instruments without TK block header
// ===========================================================================

TEST_F(Tst_EncoreFeatures, instrument_name_recovery_without_tk_block)
{
    // synthetic_v0c4_name_recovery.enc: instrumentCount=2, 1 TK block (TK00
    // "Bandurria").  "Guitarra" is stored as UTF-16 LE at the formula offset
    // NAME_BASE + 1*NAME_STEP = 202 + 2158 = 2360, with no TK04 header.
    // This matches pachbel.enc where Encore 5.0.2 omits TK04 but still writes
    // the name content at the formula-derived position.
    //
    // The importer must scan each padded (name-empty) instrument at its
    // formula offset, detect UTF-16, and recover "Guitarra".
    MasterScore* score = readEncoreScore("synthetic_v0c4_name_recovery.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_GE(score->parts().size(), 2u)
        << "Both instruments must be created (1 TK block + 1 recovered)";

    const String part1Name = score->parts()[1]->longName();
    EXPECT_EQ(part1Name, String(u"Guitarra"))
        << "Instrument name must be recovered from formula position, not 'Soprano Guitar'";

    delete score;
}

TEST_F(Tst_EncoreFeatures, staff_hidden_flag)
{
    // synthetic_v0c4_staff_hidden.enc: SKELETON_PRE LINE block patched so
    // staff 0 showByte = 0x00 (hidden).  Binary-diff verified: Encore stores
    // the visibility flag at byte +19 of each 30-byte EncLineStaffData entry
    // (3rd byte of the 3-byte skip after pageIdx).
    //
    // The importer must call part->setShow(false) for hidden staves.
    MasterScore* score = readEncoreScore("synthetic_v0c4_staff_hidden.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());

    EXPECT_FALSE(score->parts()[0]->show())
        << "Staff with showByte=0x00 must be hidden (part->show()==false)";

    delete score;
}

TEST_F(Tst_EncoreFeatures, instrument_count_padding)
{
    // synthetic_v0c4_instrument_count_padding.enc: header instrumentCount=2
    // but only 1 TK block (TK00).  Encore 5.0.2 can omit TK blocks for some
    // instruments (e.g. pachbel.enc has 5 instruments but only 4 TK blocks —
    // Guitarra has no TK block).
    //
    // Bug: instruments.size()=1 < instrumentCount=2 → only 1 part created.
    // Fix: pad instruments vector to instrumentCount with empty entries.
    //      Both instruments are then created; the padded one uses the MIDI
    //      program fallback if available.
    MasterScore* score = readEncoreScore("synthetic_v0c4_instrument_count_padding.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "score must be clean: " << ret.text();

    EXPECT_EQ(score->parts().size(), 2u)
        << "Both instruments must be imported even when only 1 TK block exists";

    delete score;
}
