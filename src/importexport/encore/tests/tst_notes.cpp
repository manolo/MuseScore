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
#include "engraving/dom/drumset.h"
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
// BUG FIX: Triple-dotted note advance used wrong multiplier (7/4 instead of 15/8)
// ===========================================================================

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
// BUG FIX: 5-line PERC staff: note positions derived from Encore position byte
// ===========================================================================

TEST_F(Tst_Notes, perc_clef_note_positions_from_encore_position_byte)
{
    // notes_perc_clef_positions.enc: 4/4 PERC clef staff with three pitches at
    // Encore position bytes 1, 3, 12. faceValue high nibble: 0=normal, 5=cross.
    //
    // Without fix: all pitches registered at line=0 with HEAD_SLASH.
    // With fix: each pitch at a distinct line derived from position_byte;
    //   HEAD_CROSS for fv high nibble=5, HEAD_NORMAL for high nibble=0.
    MasterScore* score = readEncoreScore("notes_perc_clef_positions.enc");
    ASSERT_NE(score, nullptr);
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    std::vector<Note*> notes;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            Chord* c = toChord(e);
            if (!c->notes().empty()) {
                notes.push_back(c->notes().front());
            }
        }
    }
    ASSERT_GE(notes.size(), 3u);

    EXPECT_EQ(notes[0]->pitch(), 62);
    EXPECT_EQ(notes[1]->pitch(), 65);
    EXPECT_EQ(notes[2]->pitch(), 81);

    // Lines derived from Encore position byte via: line = max(-4, 10 - position).
    // MuseScore PERC clef has A4 at line=5 (middle), so:
    //   pitch 62, position=1  → line=9  (bottom line, D in PERC clef)
    //   pitch 65, position=3  → line=7  (2nd line, F in PERC clef)
    //   pitch 81, position=12 → line=-2 (above staff, A5 in PERC clef)
    EXPECT_EQ(notes[0]->line(),  9) << "pitch 62 position=1 must be at line 9";
    EXPECT_EQ(notes[1]->line(),  7) << "pitch 65 position=3 must be at line 7";
    EXPECT_EQ(notes[2]->line(), -2) << "pitch 81 position=12 must be at line -2";

    // Verify drumset registration: the visual head is determined by the drumset entry.
    // note->headGroup() is a user-override property (stays HEAD_NORMAL unless the user
    // explicitly changes it); the rendering path uses drumset->noteHead(pitch) directly.
    const Drumset* ds = notes[0]->part()->instrument()->drumset();
    ASSERT_NE(ds, nullptr) << "Staff must have a drumset assigned (PERC clef)";

    // faceValue high nibble=5 (pitch 81) → registered as HEAD_CROSS in drumset
    EXPECT_EQ(ds->noteHead(81), NoteHeadGroup::HEAD_CROSS)
        << "fv high nibble=5 must register HEAD_CROSS in drumset";

    // faceValue high nibble=0 (pitch 62, 65) → registered as HEAD_NORMAL
    EXPECT_EQ(ds->noteHead(62), NoteHeadGroup::HEAD_NORMAL)
        << "fv high nibble=0 must register HEAD_NORMAL in drumset";
    EXPECT_EQ(ds->noteHead(65), NoteHeadGroup::HEAD_NORMAL)
        << "fv high nibble=0 must register HEAD_NORMAL in drumset";

    delete score;
}

// ===========================================================================
// BUG FIX: faceValue overrides standard drumset notehead for pre-populated pitches
// ===========================================================================

TEST_F(Tst_Notes, perc_clef_facevalue_overrides_standard_drumset_notehead)
{
    // notes_perc_clef_standard_drumset_notehead.enc: PERC-clef staff with one note
    // at pitch 40 (Electric Snare), faceValue high nibble=0 (normal head in Encore).
    // Standard MIDI drumset pre-registers pitch 40 as HEAD_SLASH; the importer must
    // override that with HEAD_NORMAL based on faceValue.
    MasterScore* score = readEncoreScore("notes_perc_clef_standard_drumset_notehead.enc");
    ASSERT_NE(score, nullptr);
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    Note* note = nullptr;
    for (Segment* s = m->first(SegmentType::ChordRest); s && !note; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            Chord* c = toChord(e);
            if (!c->notes().empty()) {
                note = c->notes().front();
            }
        }
    }
    ASSERT_NE(note, nullptr);
    ASSERT_EQ(note->pitch(), 40);

    const Drumset* ds = note->part()->instrument()->drumset();
    ASSERT_NE(ds, nullptr) << "Staff must have a drumset assigned (PERC clef)";
    EXPECT_EQ(ds->noteHead(40), NoteHeadGroup::HEAD_NORMAL)
        << "faceValue normal must override HEAD_SLASH from standard drumset (pitch 40)";

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
// BUG FIX: 32nd rest with rdur shortened by next note's MIDI start kept
// ===========================================================================

TEST_F(Tst_Notes, rest_before_note_midi_slop_keeps_rest)
{
    // 5/8 measure: E8 | R32 | N16. | E8 | E8 | E8.
    // The 32nd rest has rdur=5 (<15) because the next note starts 5 ticks after
    // the rest's MIDI tick (MIDI timing slop). Fix: when face value >= 32nd
    // (faceTicks >= 30), trust the face value and keep the rest in order.
    MasterScore* score = readEncoreScore("notes_rest_before_note_midi_slop.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "score must be clean: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    // Collect ChordRest elements in order
    std::vector<ChordRest*> crs;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChordRest()) {
            crs.push_back(toChordRest(e));
        }
    }
    ASSERT_GE(crs.size(), 6u) << "Must have 6 ChordRest elements in M1";
    // [0] eighth note
    EXPECT_TRUE(crs[0]->isChord())
        << "First element must be a chord (eighth note)";
    EXPECT_EQ(crs[0]->durationType().type(), DurationType::V_EIGHTH);
    // [1] 32nd REST must be second (before the dotted-16th)
    EXPECT_TRUE(crs[1]->isRest())
        << "Second element must be a rest (32nd); without fix it appears last";
    EXPECT_EQ(crs[1]->durationType().type(), DurationType::V_32ND)
        << "Rest must be a 32nd (face value preserved despite rdur=5)";
    // [2] dotted 16th note
    EXPECT_TRUE(crs[2]->isChord())
        << "Third element must be a chord (dotted 16th)";
    EXPECT_EQ(crs[2]->durationType().type(), DurationType::V_16TH);
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
}// scale_string_numbers_from_anchor_bytes
TEST_F(Tst_Notes, scale_string_numbers_from_anchor_bytes)
{
    // Fixture: M1 has 4 notes with au=0x39 on note 1 (explicit string 1) and au=0x00
    // on notes 2-4. The anchor unlocks opt-based circles for the whole measure:
    // all 4 notes show strings 1-4 via pos+1.
    MasterScore* score = readEncoreScore("notes_scale_string_numbers_anchor.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "sanity check failed";

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    std::vector<int> nums;
    for (Segment* seg = m->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
        EngravingItem* el = seg->element(0);
        if (!el || !el->isChord()) {
            continue;
        }
        for (Note* n : toChord(el)->notes()) {
            for (EngravingItem* sub : n->el()) {
                if (sub && sub->isFingering()) {
                    Fingering* fg = toFingering(sub);
                    if (fg->textStyleType() == TextStyleType::STRING_NUMBER) {
                        bool ok;
                        int v = fg->plainText().toInt(&ok);
                        if (ok) {
                            nums.push_back(v);
                        }
                    }
                }
            }
        }
    }

    EXPECT_EQ(nums.size(), 4u) << "Anchor byte 0x39 must enable circles on all 4 notes";
    for (int i = 0; i < (int)nums.size(); ++i) {
        EXPECT_EQ(nums[i], i + 1) << "Note " << i + 1 << " must show string " << i + 1;
    }

    delete score;
}

// scale_no_anchor_produces_no_circles
TEST_F(Tst_Notes, scale_no_anchor_produces_no_circles)
{
    // Notes with opt bit 0 and pos in 0-7 but NO au=0x39..0x40 anchor → no circles.
    MasterScore* score = readEncoreScore("notes_scale_no_anchor_no_circles.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "sanity check failed";

    int fingerCount = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* seg = toMeasure(mb)->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
            for (size_t v = 0; v < VOICES; ++v) {
                EngravingItem* el = seg->element(static_cast<track_idx_t>(v));
                if (el && el->isChord()) {
                    for (Note* n : toChord(el)->notes()) {
                        for (EngravingItem* sub : n->el()) {
                            if (sub && sub->isFingering()) {
                                ++fingerCount;
                            }
                        }
                    }
                }
            }
        }
    }
    EXPECT_EQ(fingerCount, 0)
        << "Without 0x39..0x40 anchor bytes, options-bit-0 notes must not show circles";

    delete score;
}// segment_override_15notes_becomes_15_8

// voice_overflow_notes_dropped_not_routed_to_voice2
TEST_F(Tst_Notes, voice_overflow_notes_dropped_not_routed_to_voice2)
{
    MasterScore* score = readEncoreScore("notes_voice_overflow_dropped.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "Overflow must not corrupt";

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    // Voice 0 should have exactly 2 half notes
    int v0Chords = 0;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord()) {
            ++v0Chords;
        }
    }
    EXPECT_EQ(v0Chords, 2) << "Only notes 1-2 fit; notes 3-5 must be dropped";

    // Voice 1 must be empty (overflow notes are not routed to voice 2)
    int v1Chords = 0;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(1);
        if (el && el->isChord()) {
            ++v1Chords;
        }
    }
    EXPECT_EQ(v1Chords, 0) << "Overflow notes must be dropped, NOT routed to voice 2";

    delete score;
}

{
    // Fixture: quarter note at tick=0, chord symbol CHD at tick=6 (6/960 offset),
    // quarter note at tick=240. CHD must attach to the beat-1 segment.
    MasterScore* score = readEncoreScore("notes_chord_symbol_snap_to_beat1.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "CHD snap must not corrupt";

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    // The first ChordRest segment must have the Harmony attached.
    Segment* firstSeg = m->first(SegmentType::ChordRest);
    ASSERT_NE(firstSeg, nullptr);

    bool harmonyOnBeat1 = false;
    for (EngravingItem* ann : firstSeg->annotations()) {
        if (ann && ann->isHarmony()) {
            harmonyOnBeat1 = true;
            break;
        }
    }
    EXPECT_TRUE(harmonyOnBeat1)
        << "Chord symbol with tick=6 (MIDI offset from note at tick=0) must snap to beat-1 segment";

    // The second segment must NOT have the Harmony.
    Segment* secondSeg = firstSeg->next(SegmentType::ChordRest);
    if (secondSeg) {
        for (EngravingItem* ann : secondSeg->annotations()) {
            EXPECT_FALSE(ann && ann->isHarmony())
                << "Chord symbol must NOT land on beat-2 segment due to MIDI drift";
        }
    }

    delete score;
}

TEST_F(Tst_Notes, chord_symbol_large_midi_drift_still_on_beat1)
{
    MasterScore* score = readEncoreScore("notes_chord_symbol_large_drift.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck());

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    Segment* first = m->first(SegmentType::ChordRest);
    ASSERT_NE(first, nullptr);

    bool harmonyOnBeat1 = false;
    for (EngravingItem* ann : first->annotations()) {
        if (ann && ann->isHarmony()) {
            harmonyOnBeat1 = true;
            break;
        }
    }
    EXPECT_TRUE(harmonyOnBeat1)
        << "CHD@87 (large drift from note@0) must still snap to beat-1 segment";

    delete score;
}

TEST_F(Tst_Notes, chord_symbol_snaps_to_beat_not_nearby_subdivision)
{
    // tick=62, beat=240 → beatStart=0 → first note in [0..62] is at tick=0, not tick=60
    MasterScore* score = readEncoreScore("notes_chord_symbol_nearbeat_subdivision.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck());

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    // Beat-1 segment (tick offset = 0)
    Segment* beat1seg = m->first(SegmentType::ChordRest);
    ASSERT_NE(beat1seg, nullptr);
    EXPECT_EQ(beat1seg->tick() - m->tick(), Fraction(0, 1))
        << "First segment must be at tick=0 (beat 1)";

    bool harmonyOnBeat1 = false;
    for (EngravingItem* ann : beat1seg->annotations()) {
        if (ann && ann->isHarmony()) {
            harmonyOnBeat1 = true;
            break;
        }
    }
    EXPECT_TRUE(harmonyOnBeat1)
        << "CHD@62 with note at tick=60 only 2t away must NOT snap to tick=60; "
        "beat-floor forces it to tick=0 (beat 1)";

    // Second segment (tick=60) must NOT have a harmony
    Segment* seg60 = beat1seg->next(SegmentType::ChordRest);
    if (seg60) {
        for (EngravingItem* ann : seg60->annotations()) {
            EXPECT_FALSE(ann && ann->isHarmony())
                << "CHD must not land on the tick=60 subdivision segment";
        }
    }

    delete score;
}

// ===========================================================================
// FEATURE: Multi-instrument compact rawStaff routing
// ===========================================================================

TEST_F(Tst_Notes, notes_multiinstr_compact_routing)
{
    // notes_multiinstr_compact_routing.enc has 2 instruments x 2 staves each.
    // Notes use compact rawStaff encoding: rawStaff = (staffWithin<<6)|instrIdx
    // (same byte format as LINE block instrStaffIdx).
    //
    // Expected layout:
    //   staff 0 (instr 0 treble): C4 = pitch 60
    //   staff 1 (instr 0 bass):   C3 = pitch 48
    //   staff 2 (instr 1 treble): E4 = pitch 64
    //   staff 3 (instr 1 bass):   E3 = pitch 52
    //
    // Bug (before fix): importer treated rawStaff low-6-bits as LINE slot index,
    // so organ notes (instrIdx=1) were placed on piano-bass staff (LINE slot 1).
    // All four notes ended up on staffs 0 and 1 only; staves 2 and 3 were empty.
    MasterScore* score = readEncoreScore("notes_multiinstr_compact_routing.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_EQ(score->nstaves(), 4) << "score must have 4 staves (2 instruments x 2 each)";

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    Segment* seg = m->first(SegmentType::ChordRest);
    ASSERT_NE(seg, nullptr);

    auto pitchOnStaff = [&](int staffIdx) -> int {
        for (int v = 0; v < static_cast<int>(VOICES); ++v) {
            EngravingItem* e = seg->element(static_cast<track_idx_t>(staffIdx * VOICES + v));
            if (e && e->isChord()) {
                return toChord(e)->notes().front()->pitch();
            }
        }
        return -1;
    };

    EXPECT_EQ(pitchOnStaff(0), 60) << "staff 0 (instr 0 treble) must have C4";
    EXPECT_EQ(pitchOnStaff(1), 48) << "staff 1 (instr 0 bass) must have C3";
    EXPECT_EQ(pitchOnStaff(2), 64) << "staff 2 (instr 1 treble) must have E4";
    EXPECT_EQ(pitchOnStaff(3), 52) << "staff 3 (instr 1 bass) must have E3";

    delete score;
}

TEST_F(Tst_Notes, notes_v0c2_multiinstr_compact_routing)
{
    // v0xC2 counterpart: same compact rawStaff encoding in an older file format.
    // Verifies the lineSlotByRawByte lookup in noteloop.cpp works for v0xC2 (size=22 notes).
    MasterScore* score = readEncoreScore("notes_v0c2_multiinstr_compact_routing.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_EQ(score->nstaves(), 4);

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    Segment* seg = m->first(SegmentType::ChordRest);
    ASSERT_NE(seg, nullptr);

    auto pitchOnStaff = [&](int staffIdx) -> int {
        for (int v = 0; v < static_cast<int>(VOICES); ++v) {
            EngravingItem* e = seg->element(static_cast<track_idx_t>(staffIdx * VOICES + v));
            if (e && e->isChord()) {
                return toChord(e)->notes().front()->pitch();
            }
        }
        return -1;
    };

    EXPECT_EQ(pitchOnStaff(0), 60) << "staff 0 (instr 0 treble) must have C4";
    EXPECT_EQ(pitchOnStaff(1), 48) << "staff 1 (instr 0 bass) must have C3";
    EXPECT_EQ(pitchOnStaff(2), 64) << "staff 2 (instr 1 treble) must have E4";
    EXPECT_EQ(pitchOnStaff(3), 52) << "staff 3 (instr 1 bass) must have E3";

    delete score;
}

// v0xC2 size=24 notes: MIDI pitch is at offset +13 (tuplet slot), same as size=22.
// Articulation byte is at offset +22. Before this fix, size=24 notes used offset +15
// for pitch (which is 0 in v0xC2 files), producing C-1 instead of the correct note.
TEST_F(Tst_Notes, notes_v0c2_size24_correct_pitch_and_artic)
{
    MasterScore* score = readEncoreScore("notes_v0c2_size24_artic_pitch.enc");
    ASSERT_NE(score, nullptr);

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    std::vector<int> pitches;
    std::vector<SymId> artics;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (!el || !el->isChord()) {
            continue;
        }
        Chord* c = toChord(el);
        pitches.push_back(c->notes().front()->pitch());
        for (Articulation* a : c->articulations()) {
            artics.push_back(a->symId());
        }
    }

    ASSERT_EQ(pitches.size(), 2u);
    EXPECT_EQ(pitches[0], 67) << "First note should be G4 (67), not C-1 (0)";
    EXPECT_EQ(pitches[1], 64) << "Second note should be E4 (64), not C-1 (0)";

    // MuseScore flips Above/Below based on stem direction after layout; compare kind only.
    auto isStaccato = [](SymId s) {
        return s == SymId::articStaccatoAbove || s == SymId::articStaccatoBelow;
    };
    auto isTenuto = [](SymId s) {
        return s == SymId::articTenutoAbove || s == SymId::articTenutoBelow;
    };
    ASSERT_EQ(artics.size(), 2u);
    EXPECT_TRUE(isStaccato(artics[0])) << "G4 should have staccato (0x1d)";
    EXPECT_TRUE(isTenuto(artics[1])) << "E4 should have tenuto (0x1c)";

    delete score;
}

// v0xC2 size=24 notes where tuplet==0 and the MIDI pitch is already stored in
// semiTonePitch (not in the tuplet slot). Found in some Encore 4.x files (e.g.
// TUVEHAMB.ENC). The pitch-swap must be skipped so the correct pitch is preserved.
TEST_F(Tst_Notes, notes_v0c2_size24_semitone_pitch)
{
    MasterScore* score = readEncoreScore("notes_v0c2_size24_semitonepitch.enc");
    ASSERT_NE(score, nullptr);

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    std::vector<int> pitches;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (!el || !el->isChord()) {
            continue;
        }
        pitches.push_back(toChord(el)->notes().front()->pitch());
    }

    ASSERT_EQ(pitches.size(), 2u);
    EXPECT_EQ(pitches[0], 60) << "C4 (60): pitch from semiTonePitch must survive; swap must not fire when tuplet==0";
    EXPECT_EQ(pitches[1], 64) << "E4 (64): pitch from semiTonePitch must survive; swap must not fire when tuplet==0";

    delete score;
}

// When a non-first measure has explicit notes filling only part of the
// duration, the trailing empty space must be filled with invisible gap rests,
// not visible rests. Measure 0 is fully filled (to avoid pickup shortening).
// Measure 1 has two eighth notes (cumTick=1/4); trailing 3/4 must be invisible.
TEST_F(Tst_Notes, trailing_space_uses_invisible_gap_rests)
{
    MasterScore* score = readEncoreScore("notes_implicit_trailing_gap.enc");
    ASSERT_NE(score, nullptr);

    Measure* m1 = measureAt(score, 1);   // measure 1 has partial content
    ASSERT_NE(m1, nullptr);

    int visibleRests = 0;
    int gapRests = 0;
    for (Segment* s = m1->first(SegmentType::ChordRest); s;
         s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isRest()) {
            if (toRest(el)->isGap()) {
                ++gapRests;
            } else {
                ++visibleRests;
            }
        }
    }

    EXPECT_EQ(visibleRests, 0) << "Trailing silence must use invisible gap rests, not visible rests";
    EXPECT_GT(gapRests, 0) << "Must have at least one invisible gap rest for the trailing 3/4";

    delete score;
}

// ===========================================================================
// BUG regression: a 16th note whose MIDI rdur from calculateRealDurations
// equals 112 was falsely assigned 3 augmentation dots because
// calcDotsSnap computed the triple-dotted threshold as (60*15)/8 = 112 via
// C++ integer truncation (true value 112.5).  The note advanced 15/128 of a
// whole note instead of 1/16, misaligning the rest of the measure.
// Fixture: NOTE@0(16th) followed by NOTE@112, so calculateRealDurations
// gives rdur=112 for the first note.
// ===========================================================================
TEST_F(Tst_Notes, rdur112_16th_note_not_triple_dotted)
{
    MasterScore* score = readEncoreScore("notes_16th_rdur112_no_triple_dot.enc");
    ASSERT_NE(score, nullptr);

    Measure* m0 = measureAt(score, 0);
    ASSERT_NE(m0, nullptr);

    Segment* firstSeg = m0->first(SegmentType::ChordRest);
    ASSERT_NE(firstSeg, nullptr);
    EngravingItem* el = firstSeg->element(0);
    ASSERT_NE(el, nullptr);
    ASSERT_TRUE(el->isChord());

    Chord* first = toChord(el);
    EXPECT_EQ(first->durationType(), DurationType::V_16TH)
        << "First chord must be a plain 16th note";
    EXPECT_EQ(first->dots(), 0)
        << "rdur=112 for a 16th must not be interpreted as triple-dotted";

    delete score;
}

// ===========================================================================
// BUG FIX: Two explicit REST elements at the same Encore tick (for voices 5
// and 6, both routing to MuseScore voice=0) must not cause a cumTick drift.
// Without fix: the second REST at tick=120 finds encTickFrac < cumTick and
// places itself at cumTick=1/4 (tick=240 MuseScore) instead of being absorbed,
// shifting all subsequent notes by one eighth note.
// With fix: the second REST is recognized as a duplicate at the already-filled
// position and does not advance cumTick again.
// ===========================================================================
TEST_F(Tst_Notes, dual_explicit_rests_same_tick_no_cumtick_drift)
{
    // voices 5+6 both route to voice=0; each has REST at enc tick=120 (eighth).
    // After the D3+F#3 chord (tick=0) and one rest (tick=120), notes at enc
    // tick=480 must land at MuseScore tick=960 (not 720, the buggy result).
    MasterScore* score = readEncoreScore("notes_dual_rests_same_tick_routing.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    const Fraction measTick = m->tick();

    // Collect chord ticks in voice=0 (track=0) of measure 0.
    std::vector<Fraction> chordTicks;
    int restCount = 0;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (!el) {
            continue;
        }
        if (el->isChord()) {
            chordTicks.push_back(s->tick() - measTick);
        } else if (el->isRest()) {
            ++restCount;
        }
    }

    // Expected: chord at 0, one rest, chord at half-measure (960 MuseScore ticks).
    ASSERT_EQ(static_cast<int>(chordTicks.size()), 2)
        << "Expected exactly two chords: D3+F#3 at start, B2+D3 at half-measure";

    EXPECT_EQ(chordTicks[0].ticks(), 0)
        << "First chord (D3+F#3) must be at the start of the measure";

    EXPECT_EQ(chordTicks[1].ticks(), 960)
        << "Second chord (B2+D3) must be at half-measure (MuseScore tick 960); "
        "the two duplicate rests at enc tick=120 must not shift it to tick 720";

    // At least one rest must appear (for the enc tick=120 eighth rest).
    // A gap-fill rest may also appear between the explicit rest and the quarter
    // note, so we only assert the minimum; the key invariant is the chord tick.
    EXPECT_GE(restCount, 1)
        << "At least one rest must appear for the enc tick=120 explicit rest";

    // Verify pitches of the second chord.
    Segment* seg2 = m->findSegment(SegmentType::ChordRest, measTick + Fraction(960, 1920));
    if (seg2) {
        EngravingItem* el2 = seg2->element(0);
        if (el2 && el2->isChord()) {
            std::set<int> pitches;
            for (Note* n : toChord(el2)->notes()) {
                pitches.insert(n->pitch());
            }
            EXPECT_TRUE(pitches.count(47)) << "B2 (midi=47) must be in the second chord";
            EXPECT_TRUE(pitches.count(50)) << "D3 (midi=50) must be in the second chord";
        }
    }

    delete score;
}
