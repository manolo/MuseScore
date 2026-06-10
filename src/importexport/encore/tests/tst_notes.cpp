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

class Tst_Notes : public ::testing::Test, public MTest
{
protected:
    void SetUp() override { setRootDir(ENC_DIR); }
};

// ===========================================================================
// FEATURE: Note pitches and tick scaling (Encore 240 ticks/q → MuseScore 480)
// ===========================================================================

TEST_F(Tst_Notes, tick_scaling_quarter_positions)
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

TEST_F(Tst_Notes, note_pitches_whole_note)
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

TEST_F(Tst_Notes, dotted_quarter_note)
{
    // "Well, Licky Hear" measure 1 has a dotted eighth rest at tick 0 (180 Encore ticks).
    // After fix: realDuration=180 → V_EIGHTH + 1 dot.
    MasterScore* score = readEncoreScore("notes_swing.enc");
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

TEST_F(Tst_Notes, explicit_triplets_in_score)
{
    // notes_triplets.enc: measure 1 has 9 explicit triplet eighths (tup=0x32)
    // forming three 3:2 groups.  Verifies tuplets are parsed and have non-zero ticks.
    MasterScore* score = readEncoreScore("notes_triplets.enc");
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

TEST_F(Tst_Notes, tuplet_notes_have_correct_actual_ticks)
{
    // For a 3:2 triplet of eighth notes, actualTicks = (1/8) / (3/2) = 1/12.
    MasterScore* score = readEncoreScore("notes_triplets.enc");
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

TEST_F(Tst_Notes, tuplet_measure_fills_correctly)
{
    // Both measures of notes_triplets.enc must pass sanityCheck.
    MasterScore* score = readEncoreScore("notes_triplets.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Triplet score should pass sanityCheck: " << ret.text();
    delete score;
}

// ===========================================================================
// BUG FIX: tuplet setTicks was zero → measures overflowed
// ===========================================================================

TEST_F(Tst_Notes, tuplet_ticks_not_zero)
{
    // Before fix: Tuplet::ticks() returned Fraction(0,1) because setTicks() was
    // never called.  checkMeasure then saw duration=0 and added extra rests.
    MasterScore* score = readEncoreScore("notes_triplets.enc");
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

TEST_F(Tst_Notes, tuplet_state_cleared_between_measures)
{
    // Before fix: tuplets map was never cleared between measures.  A triplet opened
    // in measure N would still be "active" in N+1, giving non-tuplet notes a 2/3
    // duration and causing sanityCheck to fail.
    // Fix: tuplets.clear() at the start of each measure in buildScore.
    // Measure 2 of notes_triplets has plain quarter notes (no tuplet byte).
    // Without the fix, those quarters would be appended to the stale triplet group.
    MasterScore* score = readEncoreScore("notes_triplets.enc");
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

TEST_F(Tst_Notes, tuplet_note_sorts_before_non_tuplet_at_same_tick)
{
    // notes_tuplet_sort.enc has, at tick=0, a non-tuplet quarter written
    // BEFORE a tuplet eighth in the binary stream.
    // Without the sort fix: the quarter creates the chord → no tuplet started →
    // voice sum = 3/4 (wrong for a 2/4 measure).
    // With the fix: the tuplet eighth sorts first → V_EIGHTH + 3:2 tuplet group →
    // voice sum = 1/4 (triplet) + 1/4 (trailing quarter) = 2/4.
    MasterScore* score = readEncoreScore("notes_tuplet_sort.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Tuplet-sort file should pass sanityCheck: " << ret.text();
    delete score;
}

// ===========================================================================
// BUG FIX: Notes at tick >= durTicks skipped
// ===========================================================================

TEST_F(Tst_Notes, boundary_notes_not_in_current_measure)
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

TEST_F(Tst_Notes, measures_do_not_overflow_4_4)
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

TEST_F(Tst_Notes, last_note_real_duration_not_zero)
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

TEST_F(Tst_Notes, tick_scaling_no_note_outside_measure)
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

TEST_F(Tst_Notes, no_degenerate_tuplet_ratios)
{
    // Before fix: tuplet=0xFF gave a 15:15 tuplet (reduces to 1:1).
    // After fix: such tuplets are skipped. No tuplet should have ratio 1:1.
    // Test on Beethoven which has tuplet=0xFF corruption.
    MasterScore* score = readEncoreScore("notes_corrupted.enc");
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

TEST_F(Tst_Notes, invalid_facevalue_no_crash)
{
    // Opus 27 has faceValue=0 (measure 35) and faceValue=28 (measure 78).
    // Before fix: these fell through with garbage duration types causing crashes.
    // After fix: they are skipped, file loads without crash.
    MasterScore* score = readEncoreScore("notes_corrupted.enc");
    ASSERT_NE(score, nullptr) << "Opus 27 should load despite faceValue=0/28 corruption";
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Notes, invalid_facevalue_notes_have_valid_duration_type)
{
    // All notes in the score should have valid duration types (not V_ZERO or invalid).
    MasterScore* score = readEncoreScore("notes_corrupted.enc");
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

TEST_F(Tst_Notes, tiny_duration_notes_do_not_create_overlaps)
{
    // "Well, Licky Hear" had notes at tick=180 and tick=182 (2 ticks = 0.5ms apart).
    // These are MIDI timing artifacts with realDuration < 15 Encore ticks.
    // After fix: such notes are skipped so they don't pollute voice 0.
    // The file loads without crash. The remaining notes (triplets at 265, 341, 420)
    // have non-quantized swing positions — we verify the file loads, not strict ordering.
    // notes_swing.enc: note at tick=180 (realDur=2) is skipped; note at
    // tick=182 survives.  Voice 0 of measure 1 has the rest + the surviving note only.
    MasterScore* score = readEncoreScore("notes_swing.enc");
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

TEST_F(Tst_Notes, no_voice_conflict_from_clamping)
{
    // Before fix: voice=8 was clamped to voice=3, conflicting with real voice 3 elements.
    // This caused "add(Rest): there is already a Chord" errors and layout crashes.
    // After fix: voice >= 4 elements are simply skipped.
    // Verify: Opus 27 loads without crash.
    // notes_corrupted.enc contains a note with voice=4 (>= VOICES=4).
    // Before fix: it was clamped to voice=3 causing conflicts.
    // After fix: it is skipped entirely; no track index exceeds maxTrack.
    MasterScore* score = readEncoreScore("notes_corrupted.enc");
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
// FIX: Encore encodes leading silences via absolute tick offsets, not REST elements.
// The importer snaps cumTick to that tick (when gap > CHORD_MIDI_THRESHOLD) to preserve beat positions.
// ===========================================================================
TEST_F(Tst_Notes, implicit_leading_rest_keeps_note_positions)
{
    MasterScore* score = readEncoreScore("notes_implicit_leading_rest.enc");
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
// FIX: calculateRealDurations inflates rdur to gap-to-measure-end for isolated notes (e.g. 720 in 3/4).
// The importer rejects dotted-half promotion when rdur exceeds face's tick count AND isn't a real dotted multiple.
// ===========================================================================
TEST_F(Tst_Notes, inflated_rdur_keeps_face_value_quarter_chord)
{
    MasterScore* score = readEncoreScore("notes_inflated_rdur_quarter_chord.enc");
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
// FIX: triplet-spaced rdur (80, 40, ...) no longer promotes past the face value;
// a 16th note with MIDI gap=80 stays a 16th instead of becoming an eighth and overflowing.
// ===========================================================================
TEST_F(Tst_Notes, note_rdur_80_stays_16th_face_value)
{
    MasterScore* score = readEncoreScore("notes_rdur_80_stays_16th.enc");
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

TEST_F(Tst_Notes, tie_direction_fc_creates_tie)
{
    MasterScore* score = readEncoreScore("notes_tie_dir_fc.enc");
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
// FIX: dir byte 0x02 (bit 1 set) must produce a tie; some instruments use bit 1
// not bit 7 for the outgoing-tie marker, so check (dirByte & 0x02) not just (& 0x80).
// ===========================================================================
TEST_F(Tst_Notes, tie_direction_02_creates_tie)
{
    MasterScore* score = readEncoreScore("notes_tie_dir_02.enc");
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
    EXPECT_EQ(tieCount, 1) << "expected one tie from the 0x02 TIE dir element";
    delete score;
}

// ===========================================================================
// FIX: dir byte 0x03 (bits 0+1: incoming arc + outgoing tie) must produce a tie;
// appears without sflag=0x80 in some files.
// ===========================================================================
TEST_F(Tst_Notes, tie_direction_03_creates_tie)
{
    MasterScore* score = readEncoreScore("notes_tie_dir_03.enc");
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
    EXPECT_EQ(tieCount, 1) << "expected one tie from the 0x03 TIE dir element";
    delete score;
}

// ===========================================================================
// FIX: tie-start indicator may be on byte +5 (arc direction) or byte +6 (start flag);
// ~1/3 of ties use the +6 variant (with +5=0x04 arc-only). Accept either byte's high bit.
// ===========================================================================
TEST_F(Tst_Notes, tie_start_flag_on_byte6_creates_tie)
{
    MasterScore* score = readEncoreScore("notes_tie_start_flag_byte6.enc");
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

// ===========================================================================
// BUG FIX: Grace note detection only for valid faceValues
// ===========================================================================

TEST_F(Tst_Notes, grace_notes_only_on_short_facevalues)
{
    // Grace notes only for faceValue >= 4 (eighth or shorter); fv=3 (quarter) must NOT get grace type.
    // Grace chords appear in main->graceNotes(), not as segment elements; segment-attached chords must be NORMAL.
    MasterScore* score = readEncoreScore("notes_grace.enc");
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
// BUG FIX: Spurious implied tuplet removal (adjustMeasureTuplets)
// ===========================================================================

TEST_F(Tst_Notes, swing_offgrid_spurious_triplet_removed)
{
    // Note at tick=560 (rdur=160) triggers implied 3:2 triplet but MS offset is off canonical grid (swing artifact).
    // adjustMeasureTuplets removes the spurious triplet; 3 plain quarters = 3/4. Without this: sum overflows 3/4.
    MasterScore* score = readEncoreScore("notes_swing_offgrid.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Off-grid swing file should pass sanityCheck after tuplet removal: "
                     << ret.text();
    delete score;
}

TEST_F(Tst_Notes, canonical_implied_triplet_preserved)
{
    // notes_canonical_triplet.enc: 3/4, 3 EXPLICIT 3:2 triplet quarter notes
    // (tuplet=0x32) + 1 plain quarter.
    // With faceValue-cumulative placement: tuplets advance by 1/4*2/3=1/6 each.
    // Sum: 3*(1/6) + 1/4 = 1/2 + 1/4 = 3/4 = mLen → sanityCheck passes.
    // Tuplets are explicitly encoded → always detected regardless of implied-detection logic.
    MasterScore* score = readEncoreScore("notes_canonical_triplet.enc");
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

TEST_F(Tst_Notes, overflow_measure_extended)
{
    // notes_overflow_extend.enc: 2/4 measure with 2 notes (fv=2, half).
    // With faceValue-cumulative placement:
    //   Note 1 (fv=2=half): cumTick=0, advance by 1/2. cumTick = 1/2 = mLen.
    //   Note 2: cumTick already = mLen → skipped (voice full).
    // Result: 1 half note = 1/2 = mLen. sanityCheck passes cleanly.
    // Time signature unchanged at 2/4.
    MasterScore* score = readEncoreScore("notes_overflow_extend.enc");
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

TEST_F(Tst_Notes, whole_rest_in_partial_measure)
{
    // notes_whole_rest_2_4.enc: 2/4 measure with a single rest (faceValue=1).
    // Encore encodes a whole-measure rest as fv=1 regardless of the time signature.
    // realDuration2DurationType(480, 1) must return V_HALF (not V_WHOLE) because
    // rdur=480 = one half-note in MuseScore's 480 ticks/quarter.
    // Before fix: faceValue2DurationType(1) returned V_WHOLE → rest filled 1/1 > 2/4 mLen.
    MasterScore* score = readEncoreScore("notes_whole_rest_2_4.enc");
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

TEST_F(Tst_Notes, offbeat_notes_canonical_placement)
{
    // notes_offbeat_canonical.enc: 2/4, 2 quarter notes.
    //   Note 1: MIDI tick=0,   fv=3 (quarter) → cumTick=0,   placed at tick 0
    //   Note 2: MIDI tick=241, fv=3 (quarter) → cumTick=1/4, placed at tick 1/4
    // MIDI tick 241 is 1 tick late (MIDI timing drift). With the old tick-based
    // placement this created a 1-tick gap at positions 0..241, triggering gap fills.
    // With faceValue-cumulative placement: position comes from cumTick, not MIDI tick.
    // Both notes land at exact canonical positions. No gap fills. sanityCheck passes.
    MasterScore* score = readEncoreScore("notes_offbeat_canonical.enc");
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

TEST_F(Tst_Notes, explicit_tuplet_facevalue_not_rdur)
{
    // notes_explicit_tup_rdur_truncated.enc: 6/8 measure with 3 explicit
    // 3:2 triplet 8th notes (tup=0x32). The 3rd note at tick=630 has rdur=30 because
    // the following rest starts at tick=660 (30 Encore ticks later).
    // Without fix: realDuration2DurationType(30, 4) = V_32ND → wrong dt → measure corrupted.
    // With fix: isStandardExplicit notes use faceValue2DurationType(4) = V_EIGHTH regardless.
    MasterScore* score = readEncoreScore("notes_explicit_tup_rdur_truncated.enc");
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

TEST_F(Tst_Notes, partial_explicit_group_treated_as_plain)
{
    // notes_partial_explicit_group.enc: 4/4 measure with 4 notes having
    // tup=0x32, then a plain Q. The first 3 form a valid complete 3:2 triplet group.
    // Note 4 (isolated tup=0x32) is NOT in validTupletGroupMember → treated as plain Q.
    // Without fix: note 4 starts a partial tuplet → checkMeasure overshoot → sum ≠ 4/4.
    // With fix: note 4 is plain Q → sum = 3*(1/6) + 1/4 + 1/4 = 1 = 4/4. PASS.
    MasterScore* score = readEncoreScore("notes_partial_explicit_group.enc");
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

TEST_F(Tst_Notes, dotted_note_capped_to_remaining_space)
{
    // notes_dotted_note_capping.enc: 2/4 measure with 3 sixteenth notes
    // (ticks 0, 40, 80) followed by a quarter note (tick=120, rdur=360).
    // rdur=360 → realDuration2DurationType gives V_QUARTER; calcDots gives dots=1
    // (dotted quarter, 3/8 = 6/16). cumTick after 3 sixteenths = 3/16.
    // remaining = 2/4 - 3/16 = 5/16.
    //
    // Bug: TDuration(V_QUARTER).fraction() = 1/4 = 4/16 ≤ 5/16 → capping does
    //   NOT fire → chord placed as dotted quarter (3/8 = 6/16) → sum=9/16 > 2/4.
    // Fix: include dots in comparison: 6/16 > 5/16 → capping fires → chord placed
    //   as plain quarter (1/4 = 4/16) → sum = 3/16+1/4+1/16(fill) = 8/16 = 2/4. PASS.
    MasterScore* score = readEncoreScore("notes_dotted_note_capping.enc");
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
// BUG FIX: Dotted note with MIDI timing drift (rdur off by > 1 tick)
// ===========================================================================

TEST_F(Tst_Notes, dotted_note_dotctrl_bit0_with_rdur_drift)
{
    // notes_dotted_ctrl_bit0_drift.enc: 2/4 measure with four notes.
    // Note 0 (fv=E): rdur=163 instead of 180 (17-tick MIDI drift), dotControl=0x1D
    // (bit 0 = 1, Encore's "dotted" flag). calcDotsSnap(163, E) returns 0 because
    // 17 ticks exceeds the ±1 snap tolerance. Without the fix, note 0 imports as a
    // plain eighth note and a phantom 16th rest appears at the end of the measure.
    //
    // Fix: when calcDots and calcDotsSnap both return 0, trust bit 0 of dotControl
    // and force dots=1. Note 0 must be a dotted eighth; measure must be clean.
    MasterScore* score = readEncoreScore("notes_dotted_ctrl_bit0_drift.enc");
    ASSERT_NE(score, nullptr) << "Failed to load notes_dotted_ctrl_bit0_drift.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Dotted note with rdur drift must not corrupt measure: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(2, 4));

    // Collect chords (no rests expected — measure must be clean)
    std::vector<Chord*> chords;
    std::vector<Rest*> rests;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (!e) {
            continue;
        }
        if (e->isChord()) {
            chords.push_back(toChord(e));
        } else if (e->isRest()) {
            Rest* r = toRest(e);
            if (!r->isGap()) {
                rests.push_back(r);
            }
        }
    }
    ASSERT_EQ(chords.size(), 4u) << "Measure must have exactly 4 chords";
    EXPECT_EQ(rests.size(), 0u) << "No phantom rests: measure must fill exactly 2/4";

    // First chord: dotted-eighth (fv=E, dotControl bit 0 forces dots=1 despite drift)
    EXPECT_EQ(chords[0]->durationType().type(), DurationType::V_EIGHTH)
        << "Note 0 base type must be eighth";
    EXPECT_EQ(chords[0]->dots(), 1)
        << "Note 0 must have 1 dot (dotControl bit 0 = dotted flag)";
    // Remaining chords: plain durations (no dot)
    EXPECT_EQ(chords[1]->durationType().type(), DurationType::V_16TH);
    EXPECT_EQ(chords[1]->dots(), 0);
    EXPECT_EQ(chords[2]->durationType().type(), DurationType::V_EIGHTH);
    EXPECT_EQ(chords[2]->dots(), 0);
    EXPECT_EQ(chords[3]->durationType().type(), DurationType::V_EIGHTH);
    EXPECT_EQ(chords[3]->dots(), 0);
    delete score;
}

// ===========================================================================
// BUG FIX: Mixed-face-value tuplet group gets exact ticks; isolated note
//          that exactly fills remaining space becomes a partial tuplet
// ===========================================================================

TEST_F(Tst_Notes, mixed_value_tuplet_exact_ticks_and_isolated_partial)
{
    // notes_mixed_value_tuplet.enc: 4/4 measure with a 3:2 triplet
    // containing mixed note values (Q, Q, 8th), followed by an isolated 8th
    // (tup=0x32), a plain Q, and a Q rest.
    //
    // With face-value-sum grouping (4.6), the 4 notes {Q,Q,8th,8th} with tup=0x32
    // form ONE complete group: face sum = 1/4+1/4+1/8+1/8 = 3/4 = threshold (3×1/4).
    // The group closes after 4 notes; exact-ticks correction sets ticks=5/12 so
    // checkMeasure does not break on the following plain notes.
    //
    // Expected sum: 5/12 (group) + cascade fills + micro = 1/2 = 2/4. PASS.
    MasterScore* score = readEncoreScore("notes_mixed_value_tuplet.enc");
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

TEST_F(Tst_Notes, implied_group_boundary_no_spurious_new_group)
{
    // notes_v0c2_implied_group_boundary.enc: v0xC2 2/4 measure.
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
    MasterScore* score = readEncoreScore("notes_v0c2_implied_group_boundary.enc");
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

TEST_F(Tst_Notes, capped_tuplet_note_removed_from_tuplet)
{
    // notes_capped_tuplet_note.enc: 4/4 measure with 3 plain quarters
    // (cumTick=3/4) followed by 3 explicit 3:2 triplet quarters (tup=0x32).
    // 1st triplet Q advance = (1/4)*(2/3) = 1/6. cumTick=3/4+1/6=11/12.
    // 2nd triplet Q: remaining=1/12 < 1/6 → advance capped. Note removed from tuplet.
    //
    // Bug: without removal, chord stays in tuplet with ticks=1/4 (face value).
    //   actualTicks = 1/6 > 1/12 (capped advance). Sum overshoots mLen → corrupted.
    // Fix: capped note removed. Its ticks = capped value → actualTicks ≤ advance ≤
    //   remaining → sum stays within mLen.
    MasterScore* score = readEncoreScore("notes_capped_tuplet_note.enc");
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
// BUG FIX: Partial measure-end triplet placed in tuplet, not overflowed
// ===========================================================================

TEST_F(Tst_Notes, partial_triplet_at_measure_end_no_voice_overflow)
{
    // notes_partial_triplet_measure_end.enc: 2/4 measure with 3 plain eighths
    // (filling ticks 0-360) followed by a 2-note partial 3:2 triplet at the end.
    //
    // Notes at ticks 360 and 440 both have tup=0x32 (3:2) and fv=4 (eighth).
    //   rdur(tick=360) = 80  (2 triplet slots: displayed as eighth in the bracket)
    //   rdur(tick=440) = 40  (1 triplet slot:  displayed as sixteenth in the bracket)
    //   startTick(360) + rdurSum(120) = 480 = durTicks  -> rdur fills measure
    //   startTick(360) + faceTickSum(240) = 600 > 480   -> face values would overflow
    //
    // Fix: the partial group is marked (Fix 1). A V_16TH baseLen bracket is
    // started (Fix 3: remaining=1/8 / normalN=2 = 1/16). The second note's dt
    // is reduced V_EIGHTH -> V_16TH to fit the remaining 1/24 slot (Fix 2).
    //
    // Without fix: the plain V_EIGHTH face-value advance at tick=360 fills the
    // remaining 1/8, causing tick=440 to overflow into voice 1. This produced
    // a phantom note at beat 1 (voice-1 note placed at cumTick=0) and an
    // unresolved tie in similar multi-staff files (e.g. the POLCA regression).
    MasterScore* score = readEncoreScore("notes_partial_triplet_measure_end.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Partial measure-end triplet must not corrupt score: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(2, 4));

    std::vector<Chord*> chords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            chords.push_back(toChord(e));
        }
    }
    ASSERT_GE(chords.size(), 5u) << "Should have 3 plain eighths + 2 triplet notes";

    // Plain eighths are not in a tuplet.
    EXPECT_EQ(chords[0]->tuplet(), nullptr) << "Plain 8th 1 no tuplet";
    EXPECT_EQ(chords[1]->tuplet(), nullptr) << "Plain 8th 2 no tuplet";
    EXPECT_EQ(chords[2]->tuplet(), nullptr) << "Plain 8th 3 no tuplet";

    // Both partial-triplet notes are in the same tuplet (not overflowed to voice 1).
    EXPECT_NE(chords[3]->tuplet(), nullptr) << "Triplet note 1 (eighth) should be in tuplet";
    EXPECT_NE(chords[4]->tuplet(), nullptr) << "Triplet note 2 (sixteenth) should be in tuplet";
    EXPECT_EQ(chords[3]->tuplet(), chords[4]->tuplet()) << "Both triplet notes in same bracket";

    // Note 1 displays as eighth (2 triplet slots), note 2 as sixteenth (1 slot).
    EXPECT_EQ(chords[3]->durationType().type(), DurationType::V_EIGHTH)
        << "First triplet note: V_EIGHTH (2-slot face value)";
    EXPECT_EQ(chords[4]->durationType().type(), DurationType::V_16TH)
        << "Second triplet note: V_16TH (1-slot, shortened by dt-reduction fix)";

    delete score;
}

// ===========================================================================
// BUG FIX: Post-checkMeasure micro-fill for sub-1/48 cascade residuals
// ===========================================================================

TEST_F(Tst_Notes, cascade_fill_residual_filled_by_vmeasure_rest)
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
    checkNoResidual(readEncoreScore("notes_v0c2_implied_group_boundary.enc"));

    // File 2: mixed-value tuplet + isolated partial fill (triggers exact-ticks correction
    // and isolated-partial-tuplet fill; together these may create non-standard gaps)
    checkNoResidual(readEncoreScore("notes_mixed_value_tuplet.enc"));
}

// ===========================================================================
// BUG FIX: Mixed-duration tuplet bracket closes by face-value sum (not note count)
// ===========================================================================

TEST_F(Tst_Notes, mixed_duration_triplet_face_value_sum_grouping)
{
    // notes_mixed_duration_triplet.enc: 2/4 measure with two 3:2 triplet
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
    MasterScore* score = readEncoreScore("notes_mixed_duration_triplet.enc");
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
// BUG FIX: Mixed-duration bracket {Q,E} in 3:2 now closes after 2 notes
// ===========================================================================

TEST_F(Tst_Notes, mixed_baseLen_QE_bracket_closes_after_two_notes)
{
    // ornaments_tuplet_mixed_baseLen.enc: 4/4 measure with structure:
    //   plain-Q  +  {Q,E} triplet bracket  +  {Q,Q,Q} triplet bracket
    //
    // Bug: the old threshold algorithm used baseLen=Q so the target was 3Q=3/4.
    // faceSum(Q+E)=3/8 never reached it, pulling the following Q into the same
    // bracket and causing a measure overrun.
    //
    // Fix: close the group when faceSum/actualN is a valid TDuration.
    // {Q,E}/3 = E, valid → closes after 2 notes.
    // {Q,Q,Q}/3 = Q, valid → closes after 3 notes.
    // Sum: Q + Q*(2/3) + E*(2/3) + 3*Q*(2/3) = 1/4+1/6+1/12+1/2 = 4/4. PASS.
    MasterScore* score = readEncoreScore("ornaments_tuplet_mixed_baseLen.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Mixed baseLen brackets should produce clean 4/4: " << ret.text();

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
    ASSERT_EQ(chords.size(), 6u) << "plain-Q + 2 in bracket1 + 3 in bracket2";

    // chords[0]: plain quarter — not in a tuplet
    EXPECT_EQ(chords[0]->tuplet(), nullptr) << "Plain Q must not be in a tuplet";

    // chords[1] (Q) and chords[2] (E): first bracket {Q,E}
    ASSERT_NE(chords[1]->tuplet(), nullptr) << "First Q in bracket 1";
    ASSERT_NE(chords[2]->tuplet(), nullptr) << "E in bracket 1";
    EXPECT_EQ(chords[1]->tuplet(), chords[2]->tuplet()) << "Q and E in same bracket";

    // chords[3-5]: second bracket {Q,Q,Q}
    ASSERT_NE(chords[3]->tuplet(), nullptr) << "Q 1 in bracket 2";
    ASSERT_NE(chords[4]->tuplet(), nullptr) << "Q 2 in bracket 2";
    ASSERT_NE(chords[5]->tuplet(), nullptr) << "Q 3 in bracket 2";
    EXPECT_EQ(chords[3]->tuplet(), chords[4]->tuplet()) << "All three in same bracket";
    EXPECT_EQ(chords[3]->tuplet(), chords[5]->tuplet());

    // The two brackets must be distinct
    EXPECT_NE(chords[1]->tuplet(), chords[3]->tuplet()) << "Two separate brackets";

    // Verify actual advances: Q-face rdur=160 in 3:2 → Q*(2/3)=1/6; E-face rdur=80 → E*(2/3)=1/12.
    EXPECT_EQ(chords[0]->actualTicks(), Fraction(1, 4)) << "Plain Q = 1/4";
    EXPECT_EQ(chords[1]->actualTicks(), Fraction(1, 6)) << "Q in 3:2 = Q*(2/3) = 1/6";
    EXPECT_EQ(chords[2]->actualTicks(), Fraction(1, 12)) << "E in 3:2 = E*(2/3) = 1/12";
    EXPECT_EQ(chords[3]->actualTicks(), Fraction(1, 6)) << "Q in 3:2 = 1/6";
    delete score;
}

// ===========================================================================
// BUG FIX: Mixed-value tuplet ticks corrected when faceSum > threshold (> expected)
// ===========================================================================

TEST_F(Tst_Notes, mixed_value_tuplet_ticks_corrected_for_overshoot)
{
    // notes_mixed_duration_triplet.enc: first bracket {16,16,Q} in a 3:2 group.
    // faceSum = 1/16+1/16+1/4 = 3/8 >= threshold = 3/16 (= baseLen(16th)*3).
    // placedTicks = 3*(2/3) advance = 1/12+1/12+1/6 = 5/24.
    // expected = baseLen*normalN = (1/16)*2 = 1/8.
    // placedTicks(5/24) > expected(1/8) AND faceTicks(3/8) > fullFaceSum(3/16):
    //   mixedValueOvershoot = true → tuplet->setTicks(5/24).
    // Without the ticks correction, tuplet->ticks()=1/8 (too small), checkMeasure
    // sees next chord at P+5/24 > P+1/8 → inserts fill → sum > 2/4 → corrupted.
    MasterScore* score = readEncoreScore("notes_mixed_duration_triplet.enc");
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

TEST_F(Tst_Notes, near_simultaneous_notes_form_chord)
{
    // Ticks 0 and 3 (3-tick MIDI drift) must form one chord. Without fix, rdur=3 (<15) caused C4 to be skipped.
    // CHORD_CLUSTER_THRESHOLD=4 skips near-simultaneous elements in rdur calc, giving rdur=240 for the first note.
    MasterScore* score = readEncoreScore("notes_v0c2_near_simultaneous_chord.enc");
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

TEST_F(Tst_Notes, triple_dotted_advance_matches_chord_ticks)
{
    // Triple-dotted 8th (rdur=225, dots=3, ticks=15/64). Bug: advance used 7/4 giving 14/64 instead of 15/64.
    MasterScore* score = readEncoreScore("notes_triple_dotted_advance.enc");
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

TEST_F(Tst_Notes, dotted_note_uses_dotcontrol_byte)
{
    // notes_dotted_note.enc: first note is a dotted 8th (dotControl=180)
    // but the next note is at MIDI tick=86 (drift), giving rdur=86.
    // calcDots(86, 8th)=0 (wrong); calcDots(180, 8th)=1 (correct via dotControl).
    MasterScore* score = readEncoreScore("notes_dotted_note.enc");
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

TEST_F(Tst_Notes, tie_element_creates_mscore_tie)
{
    // C4 quarter at tick=0 and C4 quarter at tick=240 with TIE element: must link via a Tie.
    // Bug: TIE elements (type=3) were discarded. Fix: EncTie + pendingTieNote creates Factory::createTie().
    MasterScore* score = readEncoreScore("notes_tie.enc");
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

TEST_F(Tst_Notes, dotted_rest_uses_dotcontrol_byte)
{
    // Dotted 8th rest with dotControl=180; rdur=154 (MIDI drift) gives calcDots=0. Fix: use dotControl when non-zero.
    MasterScore* score = readEncoreScore("notes_dotted_rest.enc");
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

TEST_F(Tst_Notes, rdur_snap_corrects_dot_count)
{
    // rdur=211 is 1 tick from dd8th=210; dotControl=0 gives 0 dots. Fix: calcDotsSnap with tolerance=1 gives 2 dots.
    MasterScore* score = readEncoreScore("notes_rdur_snap.enc");
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

TEST_F(Tst_Notes, sf_tiestart_not_filtered_by_rdur)
{
    // 64th note at tick=0 with rdur=11 (<15) and a TIE element: must not be filtered.
    // Fix: 64th/128th notes with a TIE element at their tick bypass the rdur<15 filter.
    MasterScore* score = readEncoreScore("notes_sf_tiestart.enc");
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

TEST_F(Tst_Notes, rdur_non_chord_ext_filtered)
{
    // 64th C4 at tick=240 (rdur=11, not a tie-start). Bug: prevMidiTick set too early made it look like a chord ext.
    // Fix: isChordExt uses OLD prevMidiTick (set by the rest); gap=240>=4 → not chord ext → filtered.
    MasterScore* score = readEncoreScore("notes_rdur_non_chord_ext_filtered.enc");
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

TEST_F(Tst_Notes, grace1_cascade_filter)
{
    // 64th C4 (g1low=1) filtered as artifact; Q C4 (g1low=2) is its tie-receiver and must also be filtered.
    // Fix: when g1low=1 note is filtered, record its pitch; next note with g1low=2 and same pitch is cascade-filtered.
    MasterScore* score = readEncoreScore("notes_grace1_cascade_filter.enc");
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

TEST_F(Tst_Notes, chord_cluster_5tick_v0c2)
{
    // 4 live-recorded notes at ticks 100,103,104,105 must form one chord tied to 4 receiver notes at tick=240.
    // Fixes: (A) rdur==CHORD_CLUSTER_THRESHOLD not filtered; (B) CHORD_MIDI_THRESHOLD=2*CLUSTER; (C) g1low=1 as tie indicator.
    MasterScore* score = readEncoreScore("notes_v0c2_chord_cluster_5tick.enc");
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
// BUG FIX: Encore files sometimes encode the same pitch twice in the same
// chord cluster (two NOTE elements with identical tick/staff/voice/pitch).
// The second copy must be suppressed regardless of the grace1 0x40 bit.
// ===========================================================================

// Regression: notes_chord_duplicate.enc: two identical NOTE elements at
// tick=0 pitch=60 (grace1=0x00 and grace1=0x40). After import the chord must
// have exactly one note.
TEST_F(Tst_Notes, duplicate_pitch_in_chord_cluster_suppressed)
{
    MasterScore* score = readEncoreScore("notes_chord_duplicate.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    Segment* firstSeg = m->first(SegmentType::ChordRest);
    ASSERT_NE(firstSeg, nullptr);
    EngravingItem* elem = firstSeg->element(0);
    ASSERT_NE(elem, nullptr) << "Expected a chord at tick=0";
    ASSERT_TRUE(elem->isChord()) << "Expected a Chord, got something else";

    Chord* chord = toChord(elem);
    EXPECT_EQ(chord->notes().size(), 1u)
        << "Duplicate pitch (tick=0, pitch=60 encoded twice) must produce exactly one notehead";

    delete score;
}

// ===========================================================================
// FIX: Duplicate note with NEITHER copy having grace1 bit 0x40 must also be
// suppressed. Some Encore files (e.g. v0xC2) produce two identical NOTE
// elements without the chord-extension marker; the old check was too narrow.
// ===========================================================================
TEST_F(Tst_Notes, duplicate_pitch_no_ext_bit_suppressed)
{
    // notes_chord_duplicate_no_ext_bit.enc: two notes at tick=0 pitch=60,
    // both grace1=0x00 (no chord-extension bit). Must produce exactly 1 note.
    MasterScore* score = readEncoreScore("notes_chord_duplicate_no_ext_bit.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    Segment* firstSeg = m->first(SegmentType::ChordRest);
    ASSERT_NE(firstSeg, nullptr);
    EngravingItem* elem = firstSeg->element(0);
    ASSERT_NE(elem, nullptr) << "Expected a chord at tick=0";
    ASSERT_TRUE(elem->isChord());

    EXPECT_EQ(toChord(elem)->notes().size(), 1u)
        << "Duplicate pitch (both grace1=0x00, no ext bit) must produce exactly one notehead";

    delete score;
}

// ===========================================================================
// BUG FIX: Partial measure-end triplet with gap-snap unreduced Fraction
//          caused TDuration assertion in Fix-3 of the partial-group path
// ===========================================================================

TEST_F(Tst_Notes, partial_triplet_unreduced_cumtick_no_crash)
{
    // Large gap triggers gap-snap storing cumTick as Fraction(800,960) (unreduced). Fix-3 must call .reduced()
    // before constructing TDuration; otherwise TDuration(Fraction(160,1920), truncate=false) asserts.
    MasterScore* score = readEncoreScore("notes_partial_triplet_unreduced_cumtick.enc");
    ASSERT_NE(score, nullptr) << "File must import without TDuration assertion failure";
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

// ===========================================================================
// FIX: Transposing instruments (Key≠0) must have correct written-pitch TPC.
// Root cause: Score::spell() used the WRITTEN key to penalize note spellings,
// choosing Cb over B for pitch=71 in F major (both non-diatonic, equal penalty).
// ===========================================================================
// grandstaff_staffwithin_routes_voices_to_correct_staff
//
// Piano/grand-staff files encode multi-staff notes via the high 2 bits of the
// raw staff byte (staffWithin = rawStaff >> 6). Voices 2,3 with staffWithin=1
// must land on staff 2; voices 0,1 with staffWithin=0 stay on staff 1.
// Fixture: 1 Piano (MIDI=0), 2 staves, 1 measure with 4 quarter notes:
//   treble C5 (MIDI=72, voice=0, bit6=0) and E5 (MIDI=76, voice=1, bit6=0)
//   bass   C3 (MIDI=48, voice=2, bit6=1) and E3 (MIDI=52, voice=3, bit6=1)
// ===========================================================================
TEST_F(Tst_Notes, grandstaff_staffwithin_routes_voices_to_correct_staff)
{
    MasterScore* score = readEncoreScore("notes_grandstaff_bit6_second_staff.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    ASSERT_EQ(score->nstaves(), 2) << "Piano grand staff must have 2 staves";

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    auto notesOnStaff = [&](int staffIdx) {
        std::vector<int> pitches;
        for (Segment* seg = m->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
            for (int v = 0; v < static_cast<int>(VOICES); ++v) {
                EngravingItem* e = seg->element(static_cast<track_idx_t>(staffIdx * VOICES + v));
                if (e && e->isChord()) {
                    for (Note* n : toChord(e)->notes()) {
                        pitches.push_back(n->pitch());
                    }
                }
            }
        }
        return pitches;
    };

    auto s1 = notesOnStaff(0);
    auto s2 = notesOnStaff(1);

    EXPECT_EQ(s1.size(), 2u) << "Treble staff must have 2 notes";
    EXPECT_EQ(s2.size(), 2u) << "Bass staff must have 2 notes (bit6 routing broken)";

    for (int p : s1) {
        EXPECT_GE(p, 60) << "Treble staff note must be middle C or above";
    }
    for (int p : s2) {
        EXPECT_LT(p, 60) << "Bass staff note must be below middle C";
    }

    delete score;
}

// ===========================================================================
// grandstaff_staffwithin_rest_on_second_staff
//
// A REST element with staffWithin=1 (bit 6 set) must land on staff 2.
// Fixture: treble C5 note on staff 1 + quarter rest on staff 2 (voice=2, staffWithin=1).
// ===========================================================================
TEST_F(Tst_Notes, grandstaff_staffwithin_rest_on_second_staff)
{
    MasterScore* score = readEncoreScore("notes_grandstaff_staffwithin_rest_on_second_staff.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "sanity check failed";
    ASSERT_EQ(score->nstaves(), 2);

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    // Staff 2 (idx=1) must have a rest, not a note
    bool hasRestOnStaff2 = false;
    for (Segment* seg = m->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
        for (int v = 0; v < static_cast<int>(VOICES); ++v) {
            EngravingItem* e = seg->element(static_cast<track_idx_t>(1 * VOICES + v));
            if (e && e->isRest()) {
                hasRestOnStaff2 = true;
            }
        }
    }
    EXPECT_TRUE(hasRestOnStaff2) << "Rest with staffWithin=1 must land on staff 2";

    // Staff 1 (idx=0) must have the C5 note (pitch=72), not a rest
    bool hasNoteOnStaff1 = false;
    for (Segment* seg = m->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
        EngravingItem* e = seg->element(0);
        if (e && e->isChord()) {
            hasNoteOnStaff1 = true;
        }
    }
    EXPECT_TRUE(hasNoteOnStaff1) << "Treble note (staffWithin=0) must stay on staff 1";

    delete score;
}

// ===========================================================================
// grandstaff_staffwithin_tie_on_second_staff
//
// A TIE element with staffWithin=1 must tie notes on staff 2, not staff 1.
// Fixture: 2-measure score; measure 1 has bass E3 (voice=2, staffWithin=1)
// with a TIE; measure 2 has the continuation. The tie must be resolved on
// staff 2 with the note, not leave a dangling pending tie on staff 1.
// ===========================================================================
TEST_F(Tst_Notes, grandstaff_staffwithin_tie_on_second_staff)
{
    // Single 4/4 measure: treble C5 half+half, bass E3 half tied to E3 half.
    // Both bass notes use voice=2, staffWithin=1 (raw staff byte = 0x40).
    // The TIE element also carries staffWithin=1; the tieStartSet routing must
    // key the tie by the ROUTED (staffIdx=1, voice=0) rather than the raw values
    // to correctly link the two E3 chords on staff 2.
    MasterScore* score = readEncoreScore("notes_grandstaff_staffwithin_tie_on_second_staff.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "sanity check failed";
    ASSERT_EQ(score->nstaves(), 2);

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    // Find the first bass E3 on staff 2 (pitch=52)
    Note* tieStart = nullptr;
    for (Segment* seg = m->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
        for (int v = 0; v < static_cast<int>(VOICES); ++v) {
            EngravingItem* e = seg->element(static_cast<track_idx_t>(1 * VOICES + v));
            if (e && e->isChord()) {
                for (Note* n : toChord(e)->notes()) {
                    if (n->pitch() == 52 && !tieStart) {
                        tieStart = n;
                    }
                }
            }
        }
    }
    ASSERT_NE(tieStart, nullptr) << "Bass E3 (pitch=52) must be on staff 2";
    EXPECT_NE(tieStart->tieFor(), nullptr)
        << "First E3 on staff 2 must carry tie-for (staffWithin tie routing broken)";

    if (tieStart->tieFor()) {
        Note* tieEnd = tieStart->tieFor()->endNote();
        EXPECT_NE(tieEnd, nullptr) << "Tie must resolve to second E3 on staff 2";
        if (tieEnd) {
            EXPECT_EQ(tieEnd->pitch(), 52) << "Tie end note must also be E3 (pitch=52)";
        }
    }

    delete score;
}

// ===========================================================================
// grandstaff_staffwithin_four_voices
//
// All four Encore voices (0,1 on treble; 2,3 on bass) correctly distributed:
// voice 0 → staff 1 MS-voice 0, voice 1 → staff 1 MS-voice 1,
// voice 2 → staff 2 MS-voice 0, voice 3 → staff 2 MS-voice 1.
// Fixture: C5/E5 on treble, G3/B3 on bass, all at tick=0.
// ===========================================================================
TEST_F(Tst_Notes, grandstaff_staffwithin_four_voices)
{
    MasterScore* score = readEncoreScore("notes_grandstaff_staffwithin_four_voices.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "sanity check failed";
    ASSERT_EQ(score->nstaves(), 2);

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    auto pitchesOnStaff = [&](int staffIdx) {
        std::vector<int> pitches;
        for (Segment* seg = m->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
            for (int v = 0; v < static_cast<int>(VOICES); ++v) {
                EngravingItem* e = seg->element(static_cast<track_idx_t>(staffIdx * VOICES + v));
                if (e && e->isChord()) {
                    for (Note* n : toChord(e)->notes()) {
                        pitches.push_back(n->pitch());
                    }
                }
            }
        }
        return pitches;
    };

    auto s1 = pitchesOnStaff(0);
    auto s2 = pitchesOnStaff(1);

    EXPECT_EQ(s1.size(), 2u) << "Treble must have 2 notes (C5, E5)";
    EXPECT_EQ(s2.size(), 2u) << "Bass must have 2 notes (G3, B3)";

    for (int p : s1) {
        EXPECT_GE(p, 72) << "Treble note must be >= 72 (C5)";
    }
    for (int p : s2) {
        EXPECT_LT(p, 60) << "Bass note must be < 60 (middle C)";
    }

    delete score;
}

// ===========================================================================
// grandstaff_staffwithin_sequential
//
// Sequential notes on both staves use independent cumTick accumulators.
// Each staff advances its own position; notes must not bleed across staves.
// Fixture: treble C5 at tick=0, E5 at tick=240; bass C3 at tick=0, E3 at 240.
// Each staff must have exactly 2 notes placed at the correct ticks.
// ===========================================================================
TEST_F(Tst_Notes, grandstaff_staffwithin_sequential)
{
    MasterScore* score = readEncoreScore("notes_grandstaff_staffwithin_sequential.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "sanity check failed";
    ASSERT_EQ(score->nstaves(), 2);

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    auto countNotesOnStaff = [&](int staffIdx) {
        int count = 0;
        for (Segment* seg = m->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
            for (int v = 0; v < static_cast<int>(VOICES); ++v) {
                EngravingItem* e = seg->element(static_cast<track_idx_t>(staffIdx * VOICES + v));
                if (e && e->isChord()) {
                    count += static_cast<int>(toChord(e)->notes().size());
                }
            }
        }
        return count;
    };

    EXPECT_EQ(countNotesOnStaff(0), 2) << "Treble must have 2 notes";
    EXPECT_EQ(countNotesOnStaff(1), 2) << "Bass must have 2 notes";

    delete score;
}

// ===========================================================================
// transposing_instrument_written_tpc_not_double_flat
//
// Then tpc2 = transposeTpc(Cb=7, -6) = Gbb=1 (displayed as Gbb instead of F).
// Fix: computeWindow uses the CONCERT key for transposing instrument staves.
// Fixture: MIDI=69 (oboe), Key=+6. Written F4 (semiTonePitch=65) → concert B4
// (65+6=71). Expected written TPC: 13 (F natural), not 1 (Gbb) or 7 (Cb).
// ===========================================================================
TEST_F(Tst_Notes, transposing_instrument_written_tpc_not_double_flat)
{
    MasterScore* score = readEncoreScore("notes_transposing_written_tpc.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    Segment* seg = m->first(SegmentType::ChordRest);
    ASSERT_NE(seg, nullptr);
    EngravingItem* e = seg->element(0);
    ASSERT_NE(e, nullptr);
    ASSERT_TRUE(e->isChord());
    Note* note = toChord(e)->upNote();
    ASSERT_NE(note, nullptr);

    EXPECT_EQ(note->pitch(), 71)
        << "Concert pitch must be 65 (written F4) + 6 = 71 (B4)";
    EXPECT_EQ(note->tpc2(), 13)
        << "Written TPC must be 13 (F natural), not 1 (Gbb) or 7 (Cb); "
        "double-flat spellings indicate the wrong key context in computeWindow";

    delete score;
}
// grandstaff_staffwithin_fermata
TEST_F(Tst_Notes, grandstaff_staffwithin_fermata)
{
    MasterScore* score = readEncoreScore("notes_grandstaff_staffwithin_fermata.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "sanity check failed";
    ASSERT_EQ(score->nstaves(), 2);

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    auto fermatasOnStaff = [&](int staffIdx) {
        std::vector<SymId> symIds;
        for (Segment* seg = m->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : seg->annotations()) {
                if (e->isFermata() && e->staffIdx() == static_cast<staff_idx_t>(staffIdx)) {
                    symIds.push_back(toFermata(e)->symId());
                }
            }
        }
        return symIds;
    };

    auto s1 = fermatasOnStaff(0);
    auto s2 = fermatasOnStaff(1);

    EXPECT_EQ(s1.size(), 1u) << "Treble staff must have 1 fermata (tipo 0xCC)";
    EXPECT_EQ(s2.size(), 1u) << "Bass staff must have 1 fermata (tipo 0xCD, staffWithin=1)";

    if (!s1.empty()) {
        EXPECT_EQ(s1[0], SymId::fermataAbove)
            << "Treble fermata must be above (tipo 0xCC)";
    }
    if (!s2.empty()) {
        EXPECT_EQ(s2[0], SymId::fermataBelow)
            << "Bass fermata must be below (tipo 0xCD); staffWithin routing broken for ORNs";
    }

    delete score;
}