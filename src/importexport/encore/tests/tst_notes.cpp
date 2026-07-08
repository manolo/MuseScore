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

// Note import: pitch/tick scaling, duration and dot resolution, MIDI-artifact filtering,
// grand-staff voice routing, percussion noteheads, transposing spelling and chord symbols.

#include <gtest/gtest.h>

#include "engraving/dom/arpeggio.h"
#include "engraving/dom/drumset.h"
#include "engraving/dom/articulation.h"
#include "engraving/dom/barline.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/fermata.h"
#include "engraving/dom/fingering.h"
#include "engraving/dom/fret.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/harmony.h"
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

TEST_F(Tst_Notes, tick_scaling_quarter_positions)
{
    // Encore 240 ticks/quarter must scale to MuseScore 480, so quarter notes stay on 480-tick grid.
    MasterScore* score = readEncoreScore("chord_parsing.enc");
    ASSERT_NE(score, nullptr);
    Measure* m = measureAt(score, 1);  // measure 2 (0-indexed = 1)
    ASSERT_NE(m, nullptr);
    Fraction mTick = m->tick();

    std::vector<int> relTicks;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChordRest()) {
            relTicks.push_back((s->tick() - mTick).ticks());
        }
    }
    for (int t : relTicks) {
        EXPECT_EQ(t % 480, 0) << "Note at rel tick " << t << " should be on a quarter-note boundary";
    }
    delete score;
}

TEST_F(Tst_Notes, note_pitches_whole_note)
{
    // Imported pitches must land in the valid piano MIDI range.
    MasterScore* score = readEncoreScore("akordo.enc");
    ASSERT_NE(score, nullptr);
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

TEST_F(Tst_Notes, dotted_quarter_note)
{
    // A 180-Encore-tick rest resolves to a dotted eighth (V_EIGHTH + 1 dot).
    MasterScore* score = readEncoreScore("notes_swing.enc");
    ASSERT_NE(score, nullptr);
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
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

TEST_F(Tst_Notes, boundary_notes_not_in_current_measure)
{
    // A note at tick == durTicks belongs to the next measure; adding it here would overflow the bar.
    MasterScore* score = readEncoreScore("chord_parsing.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Chord Parsing should pass sanityCheck: " << ret.text();
    delete score;
}

TEST_F(Tst_Notes, measures_do_not_overflow_4_4)
{
    // No 4/4 measure may hold more than 4/4 worth of notes.
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
        m->setCorrupted(0, false);
    }
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "No 4/4 measure should overflow: " << ret.text();
    delete score;
}

TEST_F(Tst_Notes, last_note_real_duration_not_zero)
{
    // A note at tick == durTicks would get realDuration 0; it must be skipped, so no chord
    // falls through to a V_MEASURE duration.
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
                EXPECT_NE(chord->durationType().type(), DurationType::V_MEASURE)
                    << "No chord should have V_MEASURE type (indicates zero real duration)";
            }
        }
    }
    delete score;
}

TEST_F(Tst_Notes, tick_scaling_no_note_outside_measure)
{
    // With correct 240->480 tick scaling every segment stays within its measure's tick range.
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

TEST_F(Tst_Notes, invalid_facevalue_no_crash)
{
    // Notes with an out-of-range faceValue (0 or > 8) must be skipped, not crash the importer.
    MasterScore* score = readEncoreScore("notes_corrupted.enc");
    ASSERT_NE(score, nullptr) << "Opus 27 should load despite faceValue=0/28 corruption";
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Notes, invalid_facevalue_notes_have_valid_duration_type)
{
    // Every surviving note/rest must have a valid duration type (never V_ZERO or V_INVALID).
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

TEST_F(Tst_Notes, tiny_duration_notes_do_not_create_overlaps)
{
    // Notes with realDuration < 15 ticks are MIDI timing artifacts and must be skipped so two
    // chords never share a tick in voice 0.
    MasterScore* score = readEncoreScore("notes_swing.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
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

TEST_F(Tst_Notes, no_voice_conflict_from_clamping)
{
    // A voice >= VOICES must be dropped, not clamped to voice 3 (clamping collides with real
    // voice-3 elements); no element track index may exceed maxTrack.
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

// Encore encodes leading silences via absolute tick offsets, not REST elements; snapping cumTick
// to that offset preserves beat positions (no reordering).
TEST_F(Tst_Notes, implicit_leading_rest_keeps_note_positions)
{
    MasterScore* score = readEncoreScore("notes_implicit_leading_rest.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->ticks(), Fraction(3, 4)) << "measure must be 3/4";

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

// An isolated note's rdur inflates to the gap-to-measure-end (e.g. 720 in 3/4); the face value
// must win unless rdur is a real dotted multiple, so the chord stays a plain quarter.
TEST_F(Tst_Notes, inflated_rdur_keeps_face_value_quarter_chord)
{
    MasterScore* score = readEncoreScore("notes_inflated_rdur_quarter_chord.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

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

// A triplet-spaced rdur (e.g. 80) must not promote past the face value: a 16th with MIDI gap=80
// stays a 16th instead of becoming an eighth and overflowing.
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

TEST_F(Tst_Notes, grace_notes_only_on_short_facevalues)
{
    // Grace notes only for faceValue >= 4 (eighth or shorter); fv=3 (quarter) must stay NORMAL.
    // Grace chords live in main->graceNotes(); segment-attached chords must be NORMAL.
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

TEST_F(Tst_Notes, offbeat_notes_canonical_placement)
{
    // faceValue-cumulative placement derives position from cumTick, not the drifting MIDI tick,
    // so a note at MIDI tick=241 still lands on the canonical 1/4 beat with no gap fill.
    MasterScore* score = readEncoreScore("notes_offbeat_canonical.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Off-beat MIDI ticks should produce clean measure via cumTick: "
                     << ret.text();
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
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

// On a 5-line PERC staff each note's staff line comes from the Encore position byte
// (line = max(-4, 10 - position)), and the faceValue high nibble selects the notehead.
TEST_F(Tst_Notes, perc_clef_note_positions_from_encore_position_byte)
{
    // Position bytes 1, 3, 12 for pitches 62, 65, 81; fv high nibble 0=normal, 5=cross.
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

    EXPECT_EQ(notes[0]->line(),  9) << "pitch 62 position=1 must be at line 9";
    EXPECT_EQ(notes[1]->line(),  7) << "pitch 65 position=3 must be at line 7";
    EXPECT_EQ(notes[2]->line(), -2) << "pitch 81 position=12 must be at line -2";

    // The rendered head comes from the drumset entry (note->headGroup() is a user override).
    const Drumset* ds = notes[0]->part()->instrument()->drumset();
    ASSERT_NE(ds, nullptr) << "Staff must have a drumset assigned (PERC clef)";

    EXPECT_EQ(ds->noteHead(81), NoteHeadGroup::HEAD_XCIRCLE)
        << "fv high nibble=5 must register HEAD_XCIRCLE in drumset";

    EXPECT_EQ(ds->noteHead(62), NoteHeadGroup::HEAD_NORMAL)
        << "fv high nibble=0 must register HEAD_NORMAL in drumset";
    EXPECT_EQ(ds->noteHead(65), NoteHeadGroup::HEAD_NORMAL)
        << "fv high nibble=0 must register HEAD_NORMAL in drumset";

    delete score;
}

// faceValue must override a standard drumset notehead: pitch 40 is HEAD_SLASH in the default
// MIDI drumset, but a normal faceValue nibble forces HEAD_NORMAL.
TEST_F(Tst_Notes, perc_clef_facevalue_overrides_standard_drumset_notehead)
{
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

// All 10 faceValue high-nibble types map to the correct NoteHeadGroup; non-zero nibbles use
// setFixed(true) so layout does not override the head from the shared drumset entry.
TEST_F(Tst_Notes, perc_notehead_all_nibble_types)
{
    // 10 PERC notes, pitches 50-59, faceValue (nibble<<4)|3, one per nibble 0..9.
    MasterScore* score = readEncoreScore("notes_perc_notehead_all_nibbles.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    // Notes are spread across 3 measures (4 per measure), iterate all.
    std::vector<Note*> notes;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        Measure* m = toMeasure(mb);
        for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* e = s->element(0);
            if (e && e->isChord()) {
                for (Note* n : toChord(e)->notes()) {
                    notes.push_back(n);
                }
            }
        }
    }
    ASSERT_EQ(notes.size(), 10u) << "Expected 10 notes (one per nibble 0-9) across 3 measures";

    using G = NoteHeadGroup;
    const std::vector<G> expected = {
        G::HEAD_NORMAL,        // nibble 0
        G::HEAD_DIAMOND,       // nibble 1
        G::HEAD_TRIANGLE_UP,   // nibble 2
        G::HEAD_CUSTOM,        // nibble 3 (square)
        G::HEAD_CROSS,         // nibble 4
        G::HEAD_XCIRCLE,       // nibble 5
        G::HEAD_PLUS,          // nibble 6
        G::HEAD_SLASH,         // nibble 7
        G::HEAD_LARGE_DIAMOND, // nibble 8
        G::HEAD_NORMAL,        // nibble 9 (invisible, head=NORMAL)
    };

    for (size_t i = 0; i < notes.size(); ++i) {
        EXPECT_EQ(notes[i]->headGroup(), expected[i])
            << "nibble " << i << " (pitch " << notes[i]->pitch() << ")";
        // nibble=9: note must be invisible (sin_cabeza)
        if (i == 9) {
            EXPECT_FALSE(notes[i]->visible()) << "nibble 9 must be invisible (sin_cabeza)";
        }
    }

    delete score;
}

// Two notes sharing a pitch but with different notehead nibbles must each keep their own head:
// the drumset entry is per-pitch, so both non-normal nibbles need setFixed(true) or one overrides
// the other during layout.
TEST_F(Tst_Notes, perc_shared_pitch_two_nibbles_stay_fixed)
{
    // Two PERC notes at pitch=60: nibble=7 (HEAD_SLASH) then nibble=8 (HEAD_LARGE_DIAMOND).
    MasterScore* score = readEncoreScore("notes_perc_shared_pitch_nibbles.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<Note*> notes;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        Measure* m = toMeasure(mb);
        for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* e = s->element(0);
            if (e && e->isChord()) {
                for (Note* n : toChord(e)->notes()) {
                    notes.push_back(n);
                }
            }
        }
    }
    ASSERT_GE(notes.size(), 2u);
    EXPECT_EQ(notes[0]->headGroup(), NoteHeadGroup::HEAD_SLASH)
        << "nibble=7 at pitch=60 must keep HEAD_SLASH even when pitch is shared";
    EXPECT_EQ(notes[1]->headGroup(), NoteHeadGroup::HEAD_LARGE_DIAMOND)
        << "nibble=8 at pitch=60 must keep HEAD_LARGE_DIAMOND even when pitch is shared";
    delete score;
}

TEST_F(Tst_Notes, near_simultaneous_notes_form_chord)
{
    // Notes within CHORD_CLUSTER_THRESHOLD (ticks 0 and 3) form one chord: the cluster is ignored
    // in the rdur calc so the first note keeps rdur=240 instead of being filtered as an artifact.
    MasterScore* score = readEncoreScore("notes_v0c2_near_simultaneous_chord.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Near-simultaneous chord should produce clean 2/4: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(2, 4));

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

TEST_F(Tst_Notes, triple_dotted_advance_matches_chord_ticks)
{
    // A triple-dotted note advances by 15/8 of the base value, so the next chord starts at 15/64,
    // not 14/64; a wrong multiplier leaves a 1/64 overrun.
    MasterScore* score = readEncoreScore("notes_triple_dotted_advance.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Triple-dotted advance must equal chord ticks: " << ret.text();
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
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->ticks(), Fraction(15, 64)) << "Must be triple-dotted 8th (15/64)";
    EXPECT_EQ(first->dots(), 3) << "Must have 3 augmentation dots";
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

// The dotControl byte (not the drifting MIDI realDuration) determines a note's dot count:
// dotControl=180 yields a dotted eighth even when rdur=86 would compute zero dots.
TEST_F(Tst_Notes, dotted_note_uses_dotcontrol_byte)
{
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

// A rest's dot count also comes from dotControl (non-zero) rather than the drifting rdur.
TEST_F(Tst_Notes, dotted_rest_uses_dotcontrol_byte)
{
    MasterScore* score = readEncoreScore("notes_dotted_rest.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Dotted rest test must produce clean score: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(3, 4));

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

// calcDotsSnap uses a 1-tick tolerance to recover dots when dotControl is 0: rdur=211 is one tick
// off the double-dotted-eighth value (210) and must snap to 2 dots.
TEST_F(Tst_Notes, rdur_snap_corrects_dot_count)
{
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

// A short rest (face value >= 32nd) must be kept in order even when MIDI slop shrinks its rdur
// below the artifact threshold; the face value is trusted when faceTicks >= 30.
TEST_F(Tst_Notes, rest_before_note_midi_slop_keeps_rest)
{
    // 5/8 measure: E8 | R32 | N16. | E8 | E8 | E8; the R32 has rdur=5 from MIDI slop.
    MasterScore* score = readEncoreScore("notes_rest_before_note_midi_slop.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "score must be clean: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    std::vector<ChordRest*> crs;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChordRest()) {
            crs.push_back(toChordRest(e));
        }
    }
    ASSERT_GE(crs.size(), 6u) << "Must have 6 ChordRest elements in M1";
    EXPECT_TRUE(crs[0]->isChord())
        << "First element must be a chord (eighth note)";
    EXPECT_EQ(crs[0]->durationType().type(), DurationType::V_EIGHTH);
    EXPECT_TRUE(crs[1]->isRest())
        << "Second element must be a rest (32nd); without fix it appears last";
    EXPECT_EQ(crs[1]->durationType().type(), DurationType::V_32ND)
        << "Rest must be a 32nd (face value preserved despite rdur=5)";
    EXPECT_TRUE(crs[2]->isChord())
        << "Third element must be a chord (dotted 16th)";
    EXPECT_EQ(crs[2]->durationType().type(), DurationType::V_16TH);
    delete score;
}

// isChordExt must compare against the OLD prevMidiTick (set by the preceding rest), so a 64th C4
// at tick=240 with gap 240 is not treated as a chord extension and is filtered by rdur<15.
TEST_F(Tst_Notes, rdur_non_chord_ext_filtered)
{
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

// When a g1low=1 artifact note is filtered its pitch is recorded, so the following g1low=2 note of
// the same pitch (its tie-receiver) is cascade-filtered too.
TEST_F(Tst_Notes, grace1_cascade_filter)
{
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

// A live-recorded chord cluster spread over a few ticks must stay a single chord, not split:
// 4 notes at ticks 100,103,104,105 form one chord tied to 4 receiver notes at tick=240.
TEST_F(Tst_Notes, chord_cluster_5tick_v0c2)
{
    MasterScore* score = readEncoreScore("notes_v0c2_chord_cluster_5tick.enc");
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

    ASSERT_EQ(chords.size(), 2u) << "Must have exactly 2 chords (sender + receiver)";

    EXPECT_EQ(chords[0]->notes().size(), 4u)
        << "All 4 live-recorded chord notes must be in one chord, not split";

    int tiedCount = 0;
    for (Note* n : chords[0]->notes()) {
        if (n->tieFor() && n->tieFor()->endNote()) {
            ++tiedCount;
        }
    }
    EXPECT_EQ(tiedCount, 4)
        << "All 4 sender notes must have outgoing ties to the receiver chord";

    EXPECT_EQ(chords[1]->notes().size(), 4u)
        << "Receiver chord must have all 4 notes";

    delete score;
}

// A pitch encoded twice in the same chord cluster (identical tick/staff/voice/pitch) must yield a
// single notehead, regardless of the grace1 0x40 bit on either copy.
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

// Duplicate suppression must also fire when neither copy carries the grace1 0x40
// chord-extension bit (some v0xC2 files emit two identical NOTE elements without it).
TEST_F(Tst_Notes, duplicate_pitch_no_ext_bit_suppressed)
{
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

// Grand-staff files carry the target staff in the high 2 bits of the raw staff byte
// (staffWithin = rawStaff >> 6): staffWithin=1 voices land on staff 2, staffWithin=0 on staff 1.
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

// A REST element with staffWithin=1 must also route to staff 2.
TEST_F(Tst_Notes, grandstaff_staffwithin_rest_on_second_staff)
{
    MasterScore* score = readEncoreScore("notes_grandstaff_staffwithin_rest_on_second_staff.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "sanity check failed";
    ASSERT_EQ(score->nstaves(), 2);

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

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

// A TIE with staffWithin=1 must be keyed by the ROUTED (staff 2, voice 0), not the raw values,
// so the two bass E3 chords on staff 2 are linked and no pending tie dangles on staff 1.
TEST_F(Tst_Notes, grandstaff_staffwithin_tie_on_second_staff)
{
    MasterScore* score = readEncoreScore("notes_grandstaff_staffwithin_tie_on_second_staff.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "sanity check failed";
    ASSERT_EQ(score->nstaves(), 2);

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

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

// All four Encore voices distribute across the grand staff: voices 0,1 to staff 1 (MS voices 0,1),
// voices 2,3 to staff 2 (MS voices 0,1).
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

// A voice above the staff-2 marker (voice 5..7, staffWithin 0) is a genuine extra voice on its OWN
// staff, not a move to the next staff: a voice-7 top-staff note must stay on staff 0.
TEST_F(Tst_Notes, grandstaff_high_voice_stays_on_own_staff)
{
    MasterScore* score = readEncoreScore("notes_grandstaff_high_voice_own_staff.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "sanity check failed";

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    bool onTopStaff = false, onBassStaff = false;
    for (Segment* seg = m->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
        for (int tr = 0; tr < 2 * static_cast<int>(VOICES); ++tr) {
            EngravingItem* e = seg->element(static_cast<track_idx_t>(tr));
            if (e && e->isChord() && !toChord(e)->notes().empty()
                && toChord(e)->notes().front()->pitch() == 67) {
                (tr < static_cast<int>(VOICES) ? onTopStaff : onBassStaff) = true;
            }
        }
    }
    EXPECT_TRUE(onTopStaff) << "the voice-7 note must stay on its own (top) staff";
    EXPECT_FALSE(onBassStaff) << "the voice-7 note must not be pushed onto the bass staff";
    delete score;
}

// On a single-staff instrument, Encore voice nibble 4 is a genuine second melodic voice, not the
// grand-staff marker; it overlaps voice 0 and must import as a separate MuseScore voice 1 rather
// than being concatenated onto voice 0.
TEST_F(Tst_Notes, singlestaff_voice4_second_voice)
{
    MasterScore* score = readEncoreScore("notes_singlestaff_voice4_second_voice.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "sanity check failed";

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    auto countChordsInVoice = [&](int voice) {
        int count = 0;
        for (Segment* seg = m->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
            EngravingItem* e = seg->element(static_cast<track_idx_t>(voice));
            if (e && e->isChord()) {
                ++count;
            }
        }
        return count;
    };

    EXPECT_EQ(countChordsInVoice(0), 4) << "voice 0 must hold only its own four notes";
    EXPECT_EQ(countChordsInVoice(1), 4) << "the voice-4 second voice must import as a separate voice 1";
    delete score;
}

// Each grand-staff staff advances its own cumTick accumulator, so sequential notes do not bleed
// across staves.
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

// On a transposing staff computeWindow must use the CONCERT key, otherwise the written pitch spells
// as a double-flat (Gbb) instead of F natural. Fixture: oboe (MIDI=69), Key=+6, written F4.
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

// score->spell() is a window heuristic that can drift a whole melody on a transposing staff to
// double-flats (the per-note computeWindow fix does not catch a melody). respellTransposingStaves
// re-derives each TPC from the sounding pitch plus concert key after spell().
// Fixture: oboe (MIDI 69), Key=+6, written key Eb; written pitches 70/65/62/58.
TEST_F(Tst_Notes, transposing_melody_no_double_flat_after_spell)
{
    MasterScore* score = readEncoreScore("notes_transposing_respell_melody.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck());

    const std::vector<int> expectedTpc1 = { 18, 19, 22, 18 };   // E B G# E (concert)
    const std::vector<int> expectedTpc2 = { 12, 13, 16, 12 };   // Bb F D Bb (written)

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    size_t i = 0;
    for (Segment* seg = m->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
        EngravingItem* e = seg->element(0);
        if (!e || !e->isChord()) {
            continue;
        }
        Note* note = toChord(e)->upNote();
        ASSERT_NE(note, nullptr);
        ASSERT_LT(i, expectedTpc1.size());
        EXPECT_EQ(note->tpc1(), expectedTpc1[i])
            << "Concert TPC at note " << i << " must follow the A-major concert key, not a flat drift";
        EXPECT_EQ(note->tpc2(), expectedTpc2[i])
            << "Written TPC at note " << i << " must not be a double-flat (spell() drift)";
        ++i;
    }
    EXPECT_EQ(i, expectedTpc1.size()) << "Expected 4 melody notes on the transposing staff";

    delete score;
}

// An ORN fermata with staffWithin=1 must route to staff 2, and the fermata symbol (above/below)
// follows the ORN tipo.
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

// A string-number anchor byte (au=0x39..0x40) on the first note unlocks options-bit-0 string
// circles for the whole measure, so every note shows its string number (pos+1).
TEST_F(Tst_Notes, scale_string_numbers_from_anchor_bytes)
{
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

// Without an au=0x39..0x40 anchor, options-bit-0 notes must not show string circles.
TEST_F(Tst_Notes, scale_no_anchor_produces_no_circles)
{
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
}

// A standalone string-number ORN must not duplicate a string number the options-bit-0 anchor path
// already placed on the same note; the resolver dedups so each note keeps exactly one number.
TEST_F(Tst_Notes, string_num_orn_does_not_duplicate_anchor_path_number)
{
    MasterScore* score = readEncoreScore("notes_string_num_orn_no_dup.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck());

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    std::map<int, std::vector<int> > numsByBeat;
    int beat = 0;
    for (Segment* seg = m->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
        EngravingItem* el = seg->element(0);
        if (!el || !el->isChord()) {
            continue;
        }
        for (Note* n : toChord(el)->notes()) {
            for (EngravingItem* sub : n->el()) {
                if (sub && sub->isFingering()
                    && toFingering(sub)->textStyleType() == TextStyleType::STRING_NUMBER) {
                    bool ok;
                    int v = toFingering(sub)->plainText().toInt(&ok);
                    if (ok) {
                        numsByBeat[beat].push_back(v);
                    }
                }
            }
        }
        ++beat;
    }
    EXPECT_EQ(numsByBeat[0].size(), 1u) << "n1 must have exactly one string number (1)";
    EXPECT_EQ(numsByBeat[1].size(), 1u) << "n2 must have exactly one string number (2), not two";
    if (!numsByBeat[0].empty()) {
        EXPECT_EQ(numsByBeat[0][0], 1);
    }
    if (!numsByBeat[1].empty()) {
        EXPECT_EQ(numsByBeat[1][0], 2);
    }

    delete score;
}

// Notes that overflow a voice must be dropped, never spilled into another voice.
TEST_F(Tst_Notes, voice_overflow_notes_dropped_not_routed_to_voice2)
{
    MasterScore* score = readEncoreScore("notes_voice_overflow_dropped.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "Overflow must not corrupt";

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    int v0Chords = 0;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord()) {
            ++v0Chords;
        }
    }
    EXPECT_EQ(v0Chords, 2) << "Only notes 1-2 fit; notes 3-5 must be dropped";

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

// A chord symbol with a small MIDI offset (tick=6) must snap to the beat-1 segment, not beat 2.
TEST_F(Tst_Notes, chord_symbol_snaps_to_beat1_despite_midi_offset)
{
    MasterScore* score = readEncoreScore("notes_chord_symbol_snap_to_beat1.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "CHD snap must not corrupt";

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    Segment* firstSeg = m->first(SegmentType::ChordRest);
    ASSERT_NE(firstSeg, nullptr);

    EXPECT_NE(segmentHarmony(firstSeg), nullptr)
        << "Chord symbol with tick=6 (MIDI offset from note at tick=0) must snap to beat-1 segment";

    Segment* secondSeg = firstSeg->next(SegmentType::ChordRest);
    if (secondSeg) {
        EXPECT_EQ(segmentHarmony(secondSeg), nullptr)
            << "Chord symbol must NOT land on beat-2 segment due to MIDI drift";
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

    EXPECT_NE(segmentHarmony(first), nullptr)
        << "CHD@87 (large drift from note@0) must still snap to beat-1 segment";

    delete score;
}

// A chord symbol snaps to the beat floor, not a nearby subdivision: CHD@62 with a note at tick=60
// only 2 ticks away still lands on tick=0 (beat 1).
TEST_F(Tst_Notes, chord_symbol_snaps_to_beat_not_nearby_subdivision)
{
    MasterScore* score = readEncoreScore("notes_chord_symbol_nearbeat_subdivision.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck());

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    Segment* beat1seg = m->first(SegmentType::ChordRest);
    ASSERT_NE(beat1seg, nullptr);
    EXPECT_EQ(beat1seg->tick() - m->tick(), Fraction(0, 1))
        << "First segment must be at tick=0 (beat 1)";

    EXPECT_NE(segmentHarmony(beat1seg), nullptr)
        << "CHD@62 with note at tick=60 only 2t away must NOT snap to tick=60; "
        "beat-floor forces it to tick=0 (beat 1)";

    Segment* seg60 = beat1seg->next(SegmentType::ChordRest);
    if (seg60) {
        EXPECT_EQ(segmentHarmony(seg60), nullptr)
            << "CHD must not land on the tick=60 subdivision segment";
    }

    delete score;
}

// A FretDiagram is emitted only when the tipo fret-frame bit (0x04) is set AND the chord name is
// in MuseScore's database, not on database recognition alone.
//   Measure 0: "Am" WITH the frame bit -> populated FretDiagram wrapping the Harmony.
//   Measure 1: "Am" WITHOUT the frame bit -> plain Harmony.
//   Measure 2: "Zzz" WITH the frame bit but unknown to the database -> plain Harmony.
TEST_F(Tst_Notes, chord_symbol_gets_fretboard_diagram)
{
    MasterScore* score = readEncoreScore("notes_chord_symbol_fretboard.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck());

    auto scanSeg = [](Segment* s, FretDiagram** fdOut, Harmony** bareOut) {
        *fdOut = nullptr;
        *bareOut = nullptr;
        for (EngravingItem* ann : s->annotations()) {
            if (ann && ann->isFretDiagram()) {
                *fdOut = toFretDiagram(ann);
            } else if (ann && ann->isHarmony()) {
                *bareOut = toHarmony(ann);
            }
        }
    };

    Measure* m0 = measureAt(score, 0);
    ASSERT_NE(m0, nullptr);
    Segment* s0 = m0->first(SegmentType::ChordRest);
    ASSERT_NE(s0, nullptr);
    FretDiagram* fd0 = nullptr;
    Harmony* bare0 = nullptr;
    scanSeg(s0, &fd0, &bare0);
    ASSERT_NE(fd0, nullptr) << "\"Am\" with the frame bit must be wrapped in a FretDiagram";
    EXPECT_FALSE(fd0->isClear()) << "FretDiagram for \"Am\" must be populated from the database";
    ASSERT_NE(fd0->harmony(), nullptr) << "FretDiagram must carry the Harmony as its child";
    EXPECT_EQ(fd0->harmony()->harmonyName(), String(u"Am"));
    EXPECT_EQ(bare0, nullptr)
        << "Harmony must live under the FretDiagram, not directly on the segment";

    Measure* m1 = measureAt(score, 1);
    ASSERT_NE(m1, nullptr);
    Segment* s1 = m1->first(SegmentType::ChordRest);
    ASSERT_NE(s1, nullptr);
    FretDiagram* fd1 = nullptr;
    Harmony* bare1 = nullptr;
    scanSeg(s1, &fd1, &bare1);
    EXPECT_EQ(fd1, nullptr)
        << "\"Am\" WITHOUT the frame bit must NOT get a FretDiagram, even though the database knows it";
    EXPECT_NE(bare1, nullptr) << "\"Am\" without the frame bit must remain a plain Harmony";

    Measure* m2 = measureAt(score, 2);
    ASSERT_NE(m2, nullptr);
    Segment* s2 = m2->first(SegmentType::ChordRest);
    ASSERT_NE(s2, nullptr);
    FretDiagram* fd2 = nullptr;
    Harmony* bare2 = nullptr;
    scanSeg(s2, &fd2, &bare2);
    EXPECT_EQ(fd2, nullptr) << "Unknown chord \"Zzz\" must NOT get a FretDiagram";
    EXPECT_NE(bare2, nullptr) << "Unknown chord \"Zzz\" must remain a plain Harmony";

    delete score;
}

// Compact rawStaff encoding is rawStaff = (staffWithin<<6)|instrIdx, so the low bits select the
// instrument, not a LINE slot; each instrument's two staves receive their own notes.
TEST_F(Tst_Notes, notes_multiinstr_compact_routing)
{
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

// v0xC2 counterpart of the compact rawStaff routing (size=22 notes).
TEST_F(Tst_Notes, notes_v0c2_multiinstr_compact_routing)
{
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

// v0xC2 size=24 notes take pitch from offset +13 (tuplet slot, like size=22) and articulation
// from +22; reading pitch at +15 (0 in these files) yields a wrong C-1.
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

// In some v0xC2 size=24 notes with tuplet==0 the pitch is already in semiTonePitch, so the
// tuplet-slot pitch swap must be skipped.
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

// Partially filled non-first measures pad the trailing space with invisible gap rests, not visible
// rests. Fixture: measure 1 holds two eighths (1/4); the trailing 3/4 must be gap rests.
TEST_F(Tst_Notes, trailing_space_uses_invisible_gap_rests)
{
    MasterScore* score = readEncoreScore("notes_implicit_trailing_gap.enc");
    ASSERT_NE(score, nullptr);

    Measure* m1 = measureAt(score, 1);
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

// A 16th with rdur=112 must not read as triple-dotted: the triple-dotted threshold (60*15)/8
// truncates to exactly 112 in integer math (true 112.5), so the snap must not fire here.
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

// Two explicit REST elements at the same Encore tick (voices 5 and 6, both routed to voice 0) must
// not advance cumTick twice: the duplicate at the already-filled position is absorbed, so later
// notes stay put (a chord at enc tick=480 lands at MuseScore tick=960, not 720).
TEST_F(Tst_Notes, dual_explicit_rests_same_tick_no_cumtick_drift)
{
    MasterScore* score = readEncoreScore("notes_dual_rests_same_tick_routing.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    const Fraction measTick = m->tick();

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

    ASSERT_EQ(static_cast<int>(chordTicks.size()), 2)
        << "Expected exactly two chords: D3+F#3 at start, B2+D3 at half-measure";

    EXPECT_EQ(chordTicks[0].ticks(), 0)
        << "First chord (D3+F#3) must be at the start of the measure";

    EXPECT_EQ(chordTicks[1].ticks(), 960)
        << "Second chord (B2+D3) must be at half-measure (MuseScore tick 960); "
        "the two duplicate rests at enc tick=120 must not shift it to tick 720";

    // A gap-fill rest may join the explicit tick=120 rest, so only the minimum is asserted;
    // the load-bearing invariant is the chord tick above.
    EXPECT_GE(restCount, 1)
        << "At least one rest must appear for the enc tick=120 explicit rest";

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

// A v0xC2 note whose dotControl coincidentally has bit 0 set (0x39) but whose realDuration exactly
// matches the plain face value must NOT get a dot; the bit-0 fallback would turn plain 16ths into
// dotted 16ths and overflow the measure.
TEST_F(Tst_Notes, v0c2_plain_sixteenth_with_spurious_dotctrl_bit0_no_dot)
{
    // 4/4: 2 x 16th (dotControl=0x39) + 3 x 8th = 480t.
    MasterScore* score = readEncoreScore("notes_v0c2_plain_sixteenth_no_spurious_dot.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Measure must pass sanityCheck: " << ret.text();

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
    ASSERT_EQ(chords.size(), 5u)
        << "5 chords expected (2x16th + 3x8th); fewer means overflow truncated a note";
    EXPECT_EQ(chords[0]->durationType().type(), DurationType::V_16TH);
    EXPECT_EQ(chords[0]->dots(), 0) << "16th with dotControl=0x39 must NOT be dotted";
    EXPECT_EQ(chords[1]->durationType().type(), DurationType::V_16TH);
    EXPECT_EQ(chords[1]->dots(), 0) << "16th with dotControl=0x39 must NOT be dotted";
    EXPECT_EQ(chords[2]->durationType().type(), DurationType::V_EIGHTH);
    EXPECT_EQ(chords[3]->durationType().type(), DurationType::V_EIGHTH);
    EXPECT_EQ(chords[4]->durationType().type(), DurationType::V_EIGHTH);
    delete score;
}

// An eighth-note chord whose rdur is inflated by the trailing gap (next note stayed at its original
// quarter position) must keep its eighth face value, not become a quarter.
TEST_F(Tst_Notes, inflated_rdur_eighth_chord_keeps_face_value)
{
    MasterScore* score = readEncoreScore("notes_chord_inflated_rdur_keeps_eighth.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    Chord* chordAtQ1 = nullptr;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord()) {
            Chord* c = toChord(el);
            if (c->tick() == m->tick() + Fraction(1, 4)) {
                chordAtQ1 = c;
                break;
            }
        }
    }
    ASSERT_NE(chordAtQ1, nullptr) << "Chord at beat 2 (tick=1/4) not found";
    EXPECT_EQ(chordAtQ1->durationType().type(), DurationType::V_EIGHTH)
        << "fv=4 (eighth) must win over rdur=240 (inflated by trailing gap); chord must be eighth";
    EXPECT_EQ(chordAtQ1->notes().size(), 2u)
        << "G4 and A4 at same tick must merge into one chord";
    delete score;
}
