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

// Tuplet import: explicit and implied groups, face-value-sum grouping and boundary fills,
// dotted-note and duration resolution, overfull-measure truncation and tuplet-integrity guards.

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

class Tst_NotesTuplets : public ::testing::Test, public MTest
{
protected:
    void SetUp() override { setRootDir(ENC_DIR); }
};

TEST_F(Tst_NotesTuplets, explicit_triplets_in_score)
{
    // Fixture: measure 1 has 9 explicit triplet eighths forming three 3:2 groups.
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

TEST_F(Tst_NotesTuplets, tuplet_notes_have_correct_actual_ticks)
{
    // A 3:2 triplet eighth has actualTicks = (1/8) / (3/2) = 1/12.
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

TEST_F(Tst_NotesTuplets, tuplet_measure_fills_correctly)
{
    MasterScore* score = readEncoreScore("notes_triplets.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Triplet score should pass sanityCheck: " << ret.text();
    delete score;
}

// Guards against Tuplet::ticks() returning 0 (setTicks omitted), which made checkMeasure add rests.
TEST_F(Tst_NotesTuplets, tuplet_ticks_not_zero)
{
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

// Tuplet state must be cleared between measures: a triplet opened in measure N must not stay
// active in N+1 and pull the next measure's plain quarters into a stale triplet group.
TEST_F(Tst_NotesTuplets, tuplet_state_cleared_between_measures)
{
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
                    << den << ", tuplet state may have bled from the previous measure";
            }
        }
    }
    delete score;
}

// When a non-tuplet quarter is written before a tuplet eighth at the same tick, the tuplet note
// must sort first so the group is started; otherwise the quarter wins and the voice sum is wrong.
TEST_F(Tst_NotesTuplets, tuplet_note_sorts_before_non_tuplet_at_same_tick)
{
    MasterScore* score = readEncoreScore("notes_tuplet_sort.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Tuplet-sort file should pass sanityCheck: " << ret.text();
    delete score;
}

// A corrupt tuplet byte (0xFF) yields a degenerate 15:15 ratio that reduces to 1:1; such tuplets
// must be skipped. Fixture has the 0xFF corruption. See ENCORE_FORMAT.md §Rhythm encoding.
TEST_F(Tst_NotesTuplets, no_degenerate_tuplet_ratios)
{
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

// A swing-quantized note can trigger a spurious implied triplet whose offset is off the canonical
// grid; it must be removed so the bar stays three plain quarters instead of overflowing.
TEST_F(Tst_NotesTuplets, swing_offgrid_spurious_triplet_removed)
{
    MasterScore* score = readEncoreScore("notes_swing_offgrid.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Off-grid swing file should pass sanityCheck after tuplet removal: "
                     << ret.text();
    delete score;
}

// Explicitly encoded 3:2 triplets are always detected regardless of the implied-detection logic.
// Fixture: 3/4 bar, three explicit triplet quarters + one plain quarter.
TEST_F(Tst_NotesTuplets, canonical_implied_triplet_preserved)
{
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

// When face-value placement fills the voice, a further note that would overflow is skipped rather
// than corrupting the measure. Fixture: 2/4 bar with two half notes; the second is dropped.
TEST_F(Tst_NotesTuplets, overflow_measure_extended)
{
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

// Encore encodes a whole-measure rest as faceValue=1 regardless of the time signature, so in 2/4
// it must resolve to a half rest (from rdur), not a whole rest that would overflow the bar.
// See ENCORE_FORMAT.md §REST element.
TEST_F(Tst_NotesTuplets, whole_rest_in_partial_measure)
{
    MasterScore* score = readEncoreScore("notes_whole_rest_2_4.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "2/4 whole-measure rest should pass sanityCheck: " << ret.text();
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(2, 4)) << "Time signature should be 2/4";
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

// An explicit tuplet note must take its duration from the face value, not a truncated rdur (which
// would read as a 32nd and corrupt the bar). Fixture: 6/8, three explicit triplet eighths, the
// last with a short rdur. See ENCORE_FORMAT.md §Rhythm encoding.
TEST_F(Tst_NotesTuplets, explicit_tuplet_facevalue_not_rdur)
{
    MasterScore* score = readEncoreScore("notes_explicit_tup_rdur_truncated.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Explicit triplet with truncated rdur should pass sanityCheck: " << ret.text();

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

// An isolated tuplet-flagged note after a complete 3:2 group is treated as a plain quarter, not
// the start of a partial tuplet that would overshoot the bar. Fixture: 4/4, three triplet
// quarters + one stray tuplet-flagged quarter + one plain quarter.
TEST_F(Tst_NotesTuplets, partial_explicit_group_treated_as_plain)
{
    MasterScore* score = readEncoreScore("notes_partial_explicit_group.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Partial explicit group should pass sanityCheck: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    std::vector<Chord*> chords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            chords.push_back(toChord(e));
        }
    }
    ASSERT_EQ(chords.size(), 5u) << "Should have 5 chords";
    EXPECT_NE(chords[0]->tuplet(), nullptr) << "Note 1 should be in tuplet";
    EXPECT_NE(chords[1]->tuplet(), nullptr) << "Note 2 should be in tuplet";
    EXPECT_NE(chords[2]->tuplet(), nullptr) << "Note 3 should be in tuplet";
    EXPECT_EQ(chords[3]->tuplet(), nullptr) << "Note 4 (isolated tup byte) should NOT be in tuplet";
    EXPECT_EQ(chords[4]->tuplet(), nullptr) << "Note 5 (plain) should NOT be in tuplet";
    delete score;
}

// A dotted note whose dotted length overruns the remaining space must be capped, so the
// overflow check must include the dots (6/16 > 5/16), not just the base value (4/16 <= 5/16).
// Fixture: 2/4 bar, three sixteenths then a would-be dotted quarter that must drop to plain.
TEST_F(Tst_NotesTuplets, dotted_note_capped_to_remaining_space)
{
    MasterScore* score = readEncoreScore("notes_dotted_note_capping.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Dotted note past measure end should be capped: " << ret.text();

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
    ASSERT_GE(chords.size(), 4u) << "Should have at least 4 chords";
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(chords[i]->durationType().type(), DurationType::V_16TH)
            << "First 3 chords should be sixteenth notes";
    }
    EXPECT_EQ(chords[3]->durationType().type(), DurationType::V_QUARTER)
        << "Dotted quarter must be capped to plain quarter when it overflows";
    EXPECT_EQ(chords[3]->dots(), 0) << "Capped chord must have 0 dots";
    delete score;
}

// When rdur drift is too large for the tick-based dot heuristic to snap, the dotted flag in
// dotControl bit 0 must still force one dot, else a phantom rest appears. Fixture: 2/4, note 0 is
// a dotted eighth with drifted rdur. See ENCORE_IMPORTER.md §Rhythm: face value, dots, tuplets.
TEST_F(Tst_NotesTuplets, dotted_note_dotctrl_bit0_with_rdur_drift)
{
    MasterScore* score = readEncoreScore("notes_dotted_ctrl_bit0_drift.enc");
    ASSERT_NE(score, nullptr) << "Failed to load notes_dotted_ctrl_bit0_drift.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Dotted note with rdur drift must not corrupt measure: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(2, 4));

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

    EXPECT_EQ(chords[0]->durationType().type(), DurationType::V_EIGHTH)
        << "Note 0 base type must be eighth";
    EXPECT_EQ(chords[0]->dots(), 1)
        << "Note 0 must have 1 dot (dotControl bit 0 = dotted flag)";
    EXPECT_EQ(chords[1]->durationType().type(), DurationType::V_16TH);
    EXPECT_EQ(chords[1]->dots(), 0);
    EXPECT_EQ(chords[2]->durationType().type(), DurationType::V_EIGHTH);
    EXPECT_EQ(chords[2]->dots(), 0);
    EXPECT_EQ(chords[3]->durationType().type(), DurationType::V_EIGHTH);
    EXPECT_EQ(chords[3]->dots(), 0);
    delete score;
}

// In v0xC2 the sixteenth of a dotted-eighth+sixteenth group is stored at tick+faceValue(eighth),
// so the eighth's rdur looks plain and its dotControl bit 0 is clear. The tick pattern must set
// the dot, or a trailing rest appears. Fixture: v0xC2 3/4, dotted-E + S + H.
// See ENCORE_IMPORTER.md §Rhythm: face value, dots, tuplets.
TEST_F(Tst_NotesTuplets, v0c2_dotted_eighth_detected_from_tick_pattern)
{
    MasterScore* score = readEncoreScore("notes_v0c2_dotted_eighth.enc");
    ASSERT_NE(score, nullptr) << "Failed to load notes_v0c2_dotted_eighth.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "v0xC2 dotted-eighth measure must pass sanityCheck: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(3, 4));

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

    ASSERT_EQ(chords.size(), 3u) << "Must have exactly 3 chords (dotted-E, S, H); phantom rest signals unfixed bug";
    EXPECT_EQ(rests.size(), 0u) << "No phantom rests: measure must fill 3/4 exactly";

    EXPECT_EQ(chords[0]->durationType().type(), DurationType::V_EIGHTH)
        << "Note 0 base type must be eighth";
    EXPECT_EQ(chords[0]->dots(), 1)
        << "Note 0 must have 1 dot (v0xC2 tick-pattern fix sets dotControl bit 0)";

    EXPECT_EQ(chords[1]->durationType().type(), DurationType::V_16TH);
    EXPECT_EQ(chords[1]->dots(), 0);

    EXPECT_EQ(chords[2]->durationType().type(), DurationType::V_HALF);
    EXPECT_EQ(chords[2]->dots(), 0);

    delete score;
}

// The dotted-eighth-pattern fix is ambiguous when the measure is already exactly full, so a
// faceSum guard blocks it there and the first eighth stays plain. Fixture: a v0xC2 4/4 bar
// (8th+16th+16th+8th+8th) that fills exactly. See ENCORE_IMPORTER.md §Rhythm: face value, dots, tuplets.
TEST_F(Tst_NotesTuplets, v0c2_full_measure_eighth_plus_sixteenth_no_false_dot)
{
    MasterScore* score = readEncoreScore("notes_v0c2_full_measure_no_false_dot.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Full measure must pass sanityCheck: " << ret.text();

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
        << "5 chords expected (8th+16th+16th+8th+8th); spurious dot on first 8th "
        "would overflow the measure and truncate/reshape later notes";
    // First chord must be a plain 8th (not dotted), proving the faceSum guard fired.
    EXPECT_EQ(chords[0]->durationType().type(), DurationType::V_EIGHTH);
    EXPECT_EQ(chords[0]->dots(), 0) << "First 8th must NOT be dotted (full measure: faceSum guard)";
    EXPECT_EQ(chords[1]->durationType().type(), DurationType::V_16TH);
    EXPECT_EQ(chords[1]->dots(), 0);
    EXPECT_EQ(chords[2]->durationType().type(), DurationType::V_16TH);
    EXPECT_EQ(chords[2]->dots(), 0);
    EXPECT_EQ(chords[3]->durationType().type(), DurationType::V_EIGHTH);
    EXPECT_EQ(chords[4]->durationType().type(), DurationType::V_EIGHTH);
    delete score;
}

// Face-value-sum grouping closes a mixed-value 3:2 group when the face sum reaches the threshold
// (here {Q,Q,8th,8th}), and exact-ticks correction sets its ticks so following plain notes fit.
// See ENCORE_IMPORTER.md §Rhythm: face value, dots, tuplets.
TEST_F(Tst_NotesTuplets, mixed_value_tuplet_exact_ticks_and_isolated_partial)
{
    MasterScore* score = readEncoreScore("notes_mixed_value_tuplet.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Mixed-value tuplet group should produce clean 2/4: " << ret.text();
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

// After a complete implied 3:2 group closes, an isolated following note must not start a new
// unvalidated group (the detection guard requires !groupFull); it is treated as a plain 16th.
// Fixture: v0xC2 2/4, a complete triplet then an isolated note.
TEST_F(Tst_NotesTuplets, implied_group_boundary_no_spurious_new_group)
{
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

// Default Truncate must dissolve an overflowing trailing tuplet to plain notes (a tuplet is
// atomic) and drop the excess, never rip one member out and leave an invalid partial tuplet.
// Fixture: 4/4, three plain quarters then a 3:2 quarter triplet that overflows.
// See ENCORE_IMPORTER.md §Overfull measures.
TEST_F(Tst_NotesTuplets, truncate_overfull_tuplet_no_partial_tuplet)
{
    MasterScore* score = readEncoreScore("notes_capped_tuplet_note.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Truncated measure must pass sanity check: " << ret.text();
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(4, 4));
    EXPECT_EQ(m->ticks(), Fraction(4, 4)) << "Truncate keeps a standard 4/4 measure";
    Fraction sum(0, 1);
    int tupletChords = 0;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChordRest()) {
            ChordRest* cr = toChordRest(e);
            sum += cr->actualTicks();
            if (cr->tuplet()) {
                ++tupletChords;
            }
        }
    }
    EXPECT_EQ(tupletChords, 0) << "Truncate must leave NO tuplet (no partial tuplet)";
    EXPECT_EQ(sum, Fraction(4, 4)) << "Voice 0 must sum to exactly 4/4";
    delete score;
}

// Truncate on a messy overfull bar (irregular pre-content plus a triplet, total 33/32) must
// dissolve the triplet, drop trailing notes and dot the survivor to leave an exact 4/4.
// See ENCORE_IMPORTER.md §Overfull measures.
TEST_F(Tst_NotesTuplets, truncate_overfull_messy_precontent_fills_to_4_4)
{
    MasterScore* score = readEncoreScore("notes_overfull_messy_precontent_tuplet.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "Truncated messy measure must pass sanity check";
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->ticks(), Fraction(4, 4)) << "Truncate keeps a standard 4/4 measure";
    Fraction sum(0, 1);
    int tupletChords = 0;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChordRest()) {
            ChordRest* cr = toChordRest(e);
            sum += cr->actualTicks();
            if (cr->tuplet()) {
                ++tupletChords;
            }
        }
    }
    EXPECT_EQ(tupletChords, 0) << "No partial tuplet left";
    EXPECT_EQ(sum, Fraction(4, 4)) << "Voice 0 must fill exactly 4/4 (no underfull gap)";
    delete score;
}

// When Truncate dissolves and removes tuplet members, a slur whose endpoint resolves into the
// modified region must not leave a dangling reference that crashes at layout or teardown.
TEST_F(Tst_NotesTuplets, truncate_overfull_tuplet_with_slur_no_crash)
{
    MasterScore* score = readEncoreScore("notes_overfull_tuplet_with_slur.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck());
    delete score;
}

// Encore omits a tuplet group's final note when it lands on the measure boundary, so an incomplete
// group short on face value must be completed with an invisible fill rest to reach a full 4/4.
// Fixture: 4/4, a half then a 3:2 group with its last member omitted.
TEST_F(Tst_NotesTuplets, mixed_duration_tuplet_boundary_fill)
{
    MasterScore* score = readEncoreScore("notes_mixed_duration_tuplet_boundary_fill.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Mixed-duration tuplet with boundary-omitted note should pass sanityCheck: "
                     << ret.text();
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(4, 4));

    std::vector<ChordRest*> crs;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChordRest()) {
            crs.push_back(toChordRest(e));
        }
    }
    // half + 3 tuplet notes + 1 invisible fill rest = 5 elements
    ASSERT_GE(crs.size(), 5u) << "Expected half + 3 tuplet notes + fill rest";

    EXPECT_EQ(crs[0]->tuplet(), nullptr) << "Half not in tuplet";
    EXPECT_EQ(crs[0]->durationType().type(), DurationType::V_HALF);

    ASSERT_NE(crs[1]->tuplet(), nullptr) << "qtr 1 in tuplet";
    ASSERT_NE(crs[2]->tuplet(), nullptr) << "qtr 2 in tuplet";
    ASSERT_NE(crs[3]->tuplet(), nullptr) << "8th note in tuplet";
    EXPECT_EQ(crs[1]->tuplet(), crs[2]->tuplet()) << "qtrs share bracket";
    EXPECT_EQ(crs[2]->tuplet(), crs[3]->tuplet()) << "8th shares bracket with qtrs";

    ASSERT_NE(crs[4]->tuplet(), nullptr) << "Fill rest in tuplet";
    EXPECT_EQ(crs[3]->tuplet(), crs[4]->tuplet()) << "Fill rest shares bracket";
    EXPECT_TRUE(crs[4]->isRest()) << "Fill is a rest";
    EXPECT_FALSE(crs[4]->visible()) << "Fill rest is invisible";
    EXPECT_EQ(crs[4]->durationType().type(), DurationType::V_EIGHTH) << "Fill rest is 8th";

    delete score;
}

// A 2-note partial 3:2 triplet at the measure end must be fit into the remaining space (reducing
// the second note's duration) rather than advancing by face values and overflowing into voice 1.
// Fixture: 2/4, three plain eighths then a partial triplet whose rdur sum fills the bar.
TEST_F(Tst_NotesTuplets, partial_triplet_at_measure_end_no_voice_overflow)
{
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

    EXPECT_EQ(chords[0]->tuplet(), nullptr) << "Plain 8th 1 no tuplet";
    EXPECT_EQ(chords[1]->tuplet(), nullptr) << "Plain 8th 2 no tuplet";
    EXPECT_EQ(chords[2]->tuplet(), nullptr) << "Plain 8th 3 no tuplet";

    EXPECT_NE(chords[3]->tuplet(), nullptr) << "Triplet note 1 (eighth) should be in tuplet";
    EXPECT_NE(chords[4]->tuplet(), nullptr) << "Triplet note 2 (sixteenth) should be in tuplet";
    EXPECT_EQ(chords[3]->tuplet(), chords[4]->tuplet()) << "Both triplet notes in same bracket";

    // Note 1 fills 2 triplet slots (eighth), note 2 fills 1 slot (sixteenth).
    EXPECT_EQ(chords[3]->durationType().type(), DurationType::V_EIGHTH)
        << "First triplet note: V_EIGHTH (2-slot face value)";
    EXPECT_EQ(chords[4]->durationType().type(), DurationType::V_16TH)
        << "Second triplet note: V_16TH (1-slot, shortened by dt-reduction fix)";

    delete score;
}

// A non-standard gap can leave a residual that no standard duration represents; a V_MEASURE gap
// rest with ticks == residual bridges it. Both fixtures exercise preconditions (exact-ticks
// correction and isolated-partial fill) and must pass with no large denominators from residuals.
TEST_F(Tst_NotesTuplets, cascade_fill_residual_filled_by_vmeasure_rest)
{
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

// Face-value-sum grouping (close when the face sum reaches the threshold), not count-based
// grouping, must span a mixed-value bracket across all four of its elements. Fixture: 2/4, two
// 3:2 brackets, the first with mixed values. See ENCORE_IMPORTER.md §Rhythm: face value, dots, tuplets.
TEST_F(Tst_NotesTuplets, mixed_duration_triplet_face_value_sum_grouping)
{
    MasterScore* score = readEncoreScore("notes_mixed_duration_triplet.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Mixed-duration triplet should produce clean 2/4: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(2, 4));

    // First 4 elements form one bracket, the next 3 another.
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
    EXPECT_NE(crs[0]->tuplet(), nullptr) << "Element 0 (8th note) in tuplet";
    EXPECT_NE(crs[1]->tuplet(), nullptr) << "Element 1 (8th rest) in tuplet";
    EXPECT_NE(crs[2]->tuplet(), nullptr) << "Element 2 (16th rest) in tuplet";
    EXPECT_NE(crs[3]->tuplet(), nullptr) << "Element 3 (16th note) in tuplet";
    EXPECT_EQ(crs[0]->tuplet(), crs[1]->tuplet()) << "All 4 in same first tuplet";
    EXPECT_EQ(crs[0]->tuplet(), crs[2]->tuplet());
    EXPECT_EQ(crs[0]->tuplet(), crs[3]->tuplet());
    EXPECT_NE(crs[4]->tuplet(), nullptr) << "Element 4 in second tuplet";
    EXPECT_NE(crs[5]->tuplet(), nullptr) << "Element 5 in second tuplet";
    EXPECT_NE(crs[6]->tuplet(), nullptr) << "Element 6 in second tuplet";
    EXPECT_NE(crs[0]->tuplet(), crs[4]->tuplet()) << "Two different tuplets";
    delete score;
}

// A group closes when faceSum/actualN is a valid TDuration, so a {Q,E} bracket closes after two
// notes instead of pulling in the following Q and overrunning the bar. Fixture: 4/4, plain-Q +
// {Q,E} bracket + {Q,Q,Q} bracket. See ENCORE_IMPORTER.md §Rhythm: face value, dots, tuplets.
TEST_F(Tst_NotesTuplets, mixed_baseLen_QE_bracket_closes_after_two_notes)
{
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

    EXPECT_EQ(chords[0]->tuplet(), nullptr) << "Plain Q must not be in a tuplet";

    ASSERT_NE(chords[1]->tuplet(), nullptr) << "First Q in bracket 1";
    ASSERT_NE(chords[2]->tuplet(), nullptr) << "E in bracket 1";
    EXPECT_EQ(chords[1]->tuplet(), chords[2]->tuplet()) << "Q and E in same bracket";

    ASSERT_NE(chords[3]->tuplet(), nullptr) << "Q 1 in bracket 2";
    ASSERT_NE(chords[4]->tuplet(), nullptr) << "Q 2 in bracket 2";
    ASSERT_NE(chords[5]->tuplet(), nullptr) << "Q 3 in bracket 2";
    EXPECT_EQ(chords[3]->tuplet(), chords[4]->tuplet()) << "All three in same bracket";
    EXPECT_EQ(chords[3]->tuplet(), chords[5]->tuplet());

    EXPECT_NE(chords[1]->tuplet(), chords[3]->tuplet()) << "Two separate brackets";

    EXPECT_EQ(chords[0]->actualTicks(), Fraction(1, 4)) << "Plain Q = 1/4";
    EXPECT_EQ(chords[1]->actualTicks(), Fraction(1, 6)) << "Q in 3:2 = Q*(2/3) = 1/6";
    EXPECT_EQ(chords[2]->actualTicks(), Fraction(1, 12)) << "E in 3:2 = E*(2/3) = 1/12";
    EXPECT_EQ(chords[3]->actualTicks(), Fraction(1, 6)) << "Q in 3:2 = 1/6";
    delete score;
}

// When a mixed-value tuplet's placed ticks exceed the default baseLen*normalN, its ticks must be
// corrected to the placed value so checkMeasure does not insert a spurious fill and overflow.
TEST_F(Tst_NotesTuplets, mixed_value_tuplet_ticks_corrected_for_overshoot)
{
    MasterScore* score = readEncoreScore("notes_mixed_duration_triplet.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Mixed-value tuplet ticks correction should produce clean 2/4: "
                     << ret.text();
    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    Tuplet* firstTuplet = nullptr;
    for (EngravingItem* e : m->el()) {
        if (e->isTuplet()) {
            firstTuplet = toTuplet(e);
            break;
        }
    }
    ASSERT_NE(firstTuplet, nullptr) << "Must have at least one tuplet";
    // The first bracket {16,16,Q} places 1/4, so its ticks must be 1/4, not the default 1/8.
    EXPECT_EQ(firstTuplet->ticks(), Fraction(1, 4))
        << "Tuplet ticks must equal placedTicks (1/4) not default 1/8";
    delete score;
}

// An unreduced cumTick fraction must be reduced before constructing a TDuration, or TDuration asserts.
TEST_F(Tst_NotesTuplets, partial_triplet_unreduced_cumtick_no_crash)
{
    MasterScore* score = readEncoreScore("notes_partial_triplet_unreduced_cumtick.enc");
    ASSERT_NE(score, nullptr) << "File must import without TDuration assertion failure";
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

// Gap-snap must be suppressed inside an active tuplet so no visible rest appears in the bracket.
TEST_F(Tst_NotesTuplets, no_spurious_rests_inside_active_tuplet_gapsnap_suppressed)
{
    MasterScore* score = readEncoreScore("notes_tuplet_no_gapsnap_spurious_rest.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "Triplet + half must not corrupt";

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    int chordCount = 0;
    bool anyVisibleRestInsideTriplet = false;
    Tuplet* activeTup = nullptr;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (!el) {
            continue;
        }
        if (el->isChord()) {
            ++chordCount;
            activeTup = toChord(el)->tuplet();
        } else if (el->isRest()) {
            Rest* r = toRest(el);
            if (!r->isGap() && activeTup != nullptr) {
                anyVisibleRestInsideTriplet = true;
            }
        }
    }
    EXPECT_EQ(chordCount, 4) << "3 triplet quarters + 1 half = 4 chords";
    EXPECT_FALSE(anyVisibleRestInsideTriplet)
        << "No visible rests must appear inside the tuplet bracket (gap-snap suppressed)";

    delete score;
}

// A run of 15 notes with a tuplet flag becomes one 15:8 bracket with every note kept.
TEST_F(Tst_NotesTuplets, segment_override_15notes_becomes_15_8)
{
    MasterScore* score = readEncoreScore("notes_segment_override_15notes.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "15-note segment override must not corrupt";

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    std::vector<Chord*> chords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord()) {
            chords.push_back(toChord(el));
        }
    }
    EXPECT_EQ(chords.size(), 15u) << "All 15 notes must be placed (none dropped)";

    Tuplet* tup = chords.empty() ? nullptr : chords[0]->tuplet();
    ASSERT_NE(tup, nullptr) << "Notes must be in a Tuplet bracket";
    EXPECT_EQ(tup->ratio().numerator(), 15) << "Override actualN must be 15";
    EXPECT_EQ(tup->ratio().denominator(), 8) << "Override normalN must be 8";
    EXPECT_EQ(tup->baseLen().type(), DurationType::V_EIGHTH);
    for (size_t i = 1; i < chords.size(); ++i) {
        EXPECT_EQ(chords[i]->tuplet(), tup) << "All 15 notes must be in the same Tuplet";
    }

    int voice1Chords = 0;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        if (s->element(1)) {
            ++voice1Chords;
        }
    }
    EXPECT_EQ(voice1Chords, 0) << "Overflow notes must be dropped, not routed to voice 2";

    delete score;
}

// A 12-note override bracket (12:6) leaves the two trailing plain notes outside any tuplet.
TEST_F(Tst_NotesTuplets, segment_override_12notes_plus_2plain_becomes_12_6)
{
    MasterScore* score = readEncoreScore("notes_segment_override_12plus2.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "12+2 segment override must not corrupt";

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    std::vector<Chord*> allChords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord()) {
            allChords.push_back(toChord(el));
        }
    }
    EXPECT_EQ(allChords.size(), 14u) << "12 tuplet + 2 plain = 14 notes total";

    Tuplet* tup = allChords.empty() ? nullptr : allChords[0]->tuplet();
    ASSERT_NE(tup, nullptr);
    EXPECT_EQ(tup->ratio().numerator(), 12);
    EXPECT_EQ(tup->ratio().denominator(), 6);
    for (int i = 0; i < 12; ++i) {
        EXPECT_EQ(allChords[i]->tuplet(), tup) << "Note " << i + 1 << " must be in the [12:6] bracket";
    }
    EXPECT_EQ(allChords[12]->tuplet(), nullptr) << "Trailing note 13 must be plain";
    EXPECT_EQ(allChords[13]->tuplet(), nullptr) << "Trailing note 14 must be plain";

    delete score;
}

// Six notes that form two clean 3:2 groups must stay two separate brackets, not one 6:m override.
TEST_F(Tst_NotesTuplets, segment_override_does_not_fire_for_clean_multiple)
{
    MasterScore* score = readEncoreScore("notes_segment_no_override_clean_multiple.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "Two standard 3:2 groups must not corrupt";

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    std::vector<Chord*> chords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord()) {
            chords.push_back(toChord(el));
        }
    }
    ASSERT_EQ(chords.size(), 6u) << "All 6 notes must be placed";

    Tuplet* t1 = chords[0]->tuplet();
    Tuplet* t2 = chords[3]->tuplet();
    ASSERT_NE(t1, nullptr);
    ASSERT_NE(t2, nullptr);
    EXPECT_NE(t1, t2) << "6 notes with 3:2 must form TWO separate groups, not one [6:m]";
    EXPECT_EQ(t1->ratio().numerator(), 3);
    EXPECT_EQ(t1->ratio().denominator(), 2);
    EXPECT_EQ(t2->ratio().numerator(), 3);
    EXPECT_EQ(t2->ratio().denominator(), 2);
    // Notes 1-3 in group 1, notes 4-6 in group 2
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(chords[i]->tuplet(), t1);
    }
    for (int i = 3; i < 6; ++i) {
        EXPECT_EQ(chords[i]->tuplet(), t2);
    }

    delete score;
}

// A 2:1 duplet (dosillo) imports as a 2-note 2:1 bracket. See ENCORE_FORMAT.md §Rhythm encoding.
TEST_F(Tst_NotesTuplets, non_standard_tuplet_dosillo_2_1)
{
    MasterScore* score = readEncoreScore("notes_tuplet_dosillo_2_1.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "Dosillo 2:1 must not corrupt";

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    std::vector<Chord*> chords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord()) {
            chords.push_back(toChord(el));
        }
    }
    ASSERT_EQ(chords.size(), 2u) << "Dosillo must produce exactly 2 notes";

    Tuplet* tup = chords[0]->tuplet();
    ASSERT_NE(tup, nullptr) << "Both notes must be inside a 2:1 bracket";
    EXPECT_EQ(tup->ratio().numerator(), 2);
    EXPECT_EQ(tup->ratio().denominator(), 1);
    EXPECT_EQ(chords[0]->tuplet(), chords[1]->tuplet());

    delete score;
}

// A 9:4 non-standard tuplet keeps all 9 notes in one bracket.
TEST_F(Tst_NotesTuplets, non_standard_tuplet_9_4_nontuplet)
{
    MasterScore* score = readEncoreScore("notes_tuplet_9_4_nontuplet.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "9:4 nontuplet must not corrupt";

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    std::vector<Chord*> chords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord()) {
            chords.push_back(toChord(el));
        }
    }
    EXPECT_EQ(chords.size(), 9u) << "All 9 notes must be in the 9:4 bracket";

    Tuplet* tup = chords.empty() ? nullptr : chords[0]->tuplet();
    ASSERT_NE(tup, nullptr);
    EXPECT_EQ(tup->ratio().numerator(), 9);
    EXPECT_EQ(tup->ratio().denominator(), 4);
    for (auto* c : chords) {
        EXPECT_EQ(c->tuplet(), tup);
    }

    delete score;
}

// A tuplet's last note with a tiny rdur must survive the MIDI-artifact filter, not be dropped.
TEST_F(Tst_NotesTuplets, last_tuplet_note_short_rdur_not_dropped)
{
    MasterScore* score = readEncoreScore("notes_tuplet_last_note_short_rdur.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "Last note with tiny rdur must not corrupt";

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    std::vector<Chord*> chords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord()) {
            chords.push_back(toChord(el));
        }
    }
    EXPECT_EQ(chords.size(), 10u)
        << "Note 10 with rdur=6 (< 15) must NOT be dropped by the MIDI artifact filter";

    Tuplet* tup = chords.empty() ? nullptr : chords[0]->tuplet();
    ASSERT_NE(tup, nullptr);
    EXPECT_EQ(tup->ratio().numerator(), 10);
    EXPECT_EQ(tup->ratio().denominator(), 4);
    EXPECT_EQ(chords[9]->tuplet(), tup) << "Note 10 must be inside the tuplet bracket";

    delete score;
}

// A triplet whose middle note is missing the tuplet byte must still keep all three members.
TEST_F(Tst_NotesTuplets, triplet_orphan_middle_note_missing_tup_byte)
{
    MasterScore* score = readEncoreScore("notes_triplet_orphan_missing_tup.enc");
    ASSERT_NE(score, nullptr);

    Measure* m0 = measureAt(score, 0);
    ASSERT_NE(m0, nullptr);

    Segment* firstSeg = m0->first(SegmentType::ChordRest);
    ASSERT_NE(firstSeg, nullptr);
    EngravingItem* el = firstSeg->element(0);
    ASSERT_NE(el, nullptr);
    ASSERT_TRUE(el->isChord()) << "First element should be a chord (triplet note 1)";

    Chord* firstChord = toChord(el);
    ASSERT_NE(firstChord->tuplet(), nullptr) << "Triplet note 1 must be inside a tuplet";

    Tuplet* tup = firstChord->tuplet();
    EXPECT_EQ(tup->ratio().reduced(), Fraction(3, 2)) << "Tuplet must be 3:2";
    EXPECT_EQ(static_cast<int>(tup->elements().size()), 3)
        << "Triplet must have 3 elements (orphan middle note must NOT be dropped)";

    // The measure must be properly full (no overflow from treating orphan as plain 8th).
    Fraction measLen = m0->ticks();
    Fraction usedTicks(0, 1);
    for (Segment* s = m0->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        ChordRest* cr = e && e->isChordRest() ? toChordRest(e) : nullptr;
        if (cr && !(cr->isRest() && toRest(cr)->isGap())) {
            usedTicks += cr->actualTicks();
        }
    }
    EXPECT_EQ(usedTicks, measLen) << "Measure must be exactly full (no overflow or gap)";

    delete score;
}

// An orphan triplet note after a complete group must join a second complete 3:2, not be dropped.
TEST_F(Tst_NotesTuplets, triplet_orphan_with_prior_complete_group)
{
    MasterScore* score = readEncoreScore("notes_triplet_orphan_prior_complete_group.enc");
    ASSERT_NE(score, nullptr);

    Measure* m0 = measureAt(score, 0);
    ASSERT_NE(m0, nullptr);

    std::vector<Tuplet*> tuplets;
    for (EngravingItem* e : m0->el()) {
        if (e->isTuplet()) {
            tuplets.push_back(toTuplet(e));
        }
    }
    ASSERT_EQ(tuplets.size(), 2u) << "Measure must contain exactly two 3:2 tuplets";
    for (int t = 0; t < 2; ++t) {
        EXPECT_EQ(static_cast<int>(tuplets[t]->elements().size()), 3)
            << "Tuplet " << t << " must have 3 elements (orphan must not be dropped)";
        EXPECT_EQ(tuplets[t]->ratio().reduced(), Fraction(3, 2))
            << "Tuplet " << t << " must be 3:2";
    }

    delete score;
}

// Tuplet state must not leak across staves: the drum staff must keep its 4 independent 3:2
// brackets (12 notes) instead of collapsing them via cross-staff contamination.
TEST_F(Tst_NotesTuplets, cross_staff_false_nesting_and_drum_corruption)
{
    MasterScore* score = readEncoreScore("notes_cross_staff_false_nesting.enc");
    ASSERT_NE(score, nullptr) << "Failed to load notes_cross_staff_false_nesting.enc";

    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Drum staff corruption or false nesting caused measure overflow: " << ret.text();

    ASSERT_GE(score->nstaves(), 2) << "Fixture must have at least 2 staves";

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    std::vector<Chord*> drumChords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        for (int v = 0; v < static_cast<int>(VOICES); ++v) {
            EngravingItem* e = s->element(static_cast<track_idx_t>(1 * VOICES + v));
            if (e && e->isChord()) {
                Chord* c = toChord(e);
                if (!c->isRest() || !toRest(c)->isGap()) {
                    drumChords.push_back(c);
                }
            }
        }
    }

    EXPECT_EQ(drumChords.size(), 12u)
        << "Staff 1 must have 12 eighth notes (4 triplets x 3); "
        "drum corruption shifts notes outside measure, reducing the count";

    for (size_t i = 0; i < drumChords.size(); ++i) {
        EXPECT_NE(drumChords[i]->tuplet(), nullptr)
            << "Drum chord " << i << " must be in a 3:2 tuplet";
    }

    std::set<Tuplet*> drumTuplets;
    for (Chord* c : drumChords) {
        if (c->tuplet()) {
            drumTuplets.insert(c->tuplet());
        }
    }
    EXPECT_EQ(drumTuplets.size(), 4u)
        << "Staff 1 must form 4 independent 3:2 triplet brackets; "
        "cross-staff contamination collapses them into fewer groups";

    delete score;
}
