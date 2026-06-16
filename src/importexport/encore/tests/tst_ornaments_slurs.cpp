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
#include "engraving/dom/breath.h"
#include "engraving/dom/measurerepeat.h"
#include "engraving/dom/ornament.h"
#include "engraving/dom/trill.h"

#include "testbase.h"

static const QString ENC_DIR(QString(iex_encore_tests_DATA_ROOT) + "/data/");

using namespace mu::engraving;

class Tst_OrnamentsSlurs : public ::testing::Test, public MTest
{
protected:
    void SetUp() override { setRootDir(ENC_DIR); }
};

// ===========================================================================
// BUG FIX: Open slurs removed (no NaN in Bezier layout)
// ===========================================================================

TEST_F(Tst_OrnamentsSlurs, no_nan_crash_from_open_slurs)
{
    // notes_corrupted.enc has SLURSTART without SLURSTOP. No endpoints → NaN in Bezier layout.
    // Fix: remove all open slurs; all remaining spanners must have valid tick ranges.
    MasterScore* score = readEncoreScore("notes_corrupted.enc");
    ASSERT_NE(score, nullptr) << "Corrupted file should load without NaN crash";
    for (auto& [tick, sp] : score->spannerMap().map()) {
        EXPECT_LT(sp->tick(), sp->tick2())
            << "All spanners should have tick < tick2 (valid range)";
    }
    delete score;
}

TEST_F(Tst_OrnamentsSlurs, no_nan_crash_opus27)
{
    MasterScore* score = readEncoreScore("notes_corrupted.enc");
    ASSERT_NE(score, nullptr);
    delete score;
}

// ===========================================================================
// FIX: SLURSTART resolves end tick from alMezuro after the measure pass (no SLURSTOP in .enc binaries).
// ===========================================================================

TEST_F(Tst_OrnamentsSlurs, multi_measure_slur_resolved_from_almezuro)
{
    MasterScore* score = readEncoreScore("ornaments_multi_measure_slur.enc");
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
// FIX: v0xC2 cross-measure slurs resolved via xoffset span heuristic extended
// to the next measure when targetEndXoff exceeds the start measure's range.
// ===========================================================================

TEST_F(Tst_OrnamentsSlurs, v0xc2_cross_measure_slur_ends_in_next_measure)
{
    // XEQUEABU.ENC is a v0xC2 file with slurs on staff 2 that span from the first
    // note of a measure to the first note of the NEXT measure. Before the fix the
    // same-measure xoffset heuristic picked the last note of the start measure
    // because targetEndXoff exceeded all xoffsets in that measure; the correct
    // endpoint is in the following measure.
    MasterScore* score = readEncoreScore("ornaments_v0c2_cross_measure_slur.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int crossMeasureCount = 0;
    int sameMeasureCount = 0;
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (!sp->isSlur()) {
            continue;
        }
        EXPECT_LT(sp->tick(), sp->tick2()) << "slur span must be positive";
        EXPECT_NE(sp->startElement(), nullptr) << "slur missing start element";
        EXPECT_NE(sp->endElement(), nullptr) << "slur missing end element";
        // Determine whether the slur crosses a barline.
        if (sp->startElement() && sp->endElement()) {
            const EngravingItem* startEl = sp->startElement();
            const EngravingItem* endEl   = sp->endElement();
            const Measure* startMeas = startEl->findMeasure();
            const Measure* endMeas   = endEl->findMeasure();
            if (startMeas && endMeas && startMeas != endMeas) {
                ++crossMeasureCount;
            } else {
                ++sameMeasureCount;
            }
        }
    }
    // All slurs in this file should be cross-measure (not same-measure).
    EXPECT_GT(crossMeasureCount, 0) << "expected at least one cross-measure slur";
    EXPECT_EQ(sameMeasureCount, 0) << "no same-measure slurs expected in this file";
    delete score;
}

// ===========================================================================
// FIX: resolvers-slur.cpp staffIdx mismatch in multi-instrument compact-encoded files.
// ps.staffIdx = routed LINE slot; em->staffIdx = raw compact instrument index.
// Before fix: staves 1-3 find no notes (mismatch) → last-chord fallback picks note3.
// After fix: emLineSlot() translates raw byte to LINE slot before comparing.
// ===========================================================================

TEST_F(Tst_OrnamentsSlurs, multiinstr_slur_endpoint_on_second_note_not_last_chord)
{
    // ornaments_multiinstr_slur_routing.enc: 2 instruments × 2 staves,
    // 3 quarter notes per staff in measure 0 with a SLURSTART at note1.
    // Expected: each slur ends at note2 (beat 2), not note3 (beat 3, last chord).
    //   staff 0 (piano treble): note2 pitch = E4 = 64
    //   staff 1 (piano bass):   note2 pitch = E3 = 52
    //   staff 2 (organ treble): note2 pitch = B4 = 71
    //   staff 3 (organ bass):   note2 pitch = B3 = 59
    MasterScore* score = readEncoreScore("ornaments_multiinstr_slur_routing.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_EQ(score->nstaves(), 4);

    // Map staffIdx → expected note2 pitch
    std::map<int, int> expectedEndPitch = { {0, 64}, {1, 52}, {2, 71}, {3, 59} };
    std::map<int, bool> staffSeen;

    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (!sp->isSlur()) {
            continue;
        }
        EXPECT_NE(sp->startElement(), nullptr) << "slur missing start";
        EXPECT_NE(sp->endElement(),   nullptr) << "slur missing end";
        if (!sp->startElement() || !sp->endElement()) {
            continue;
        }
        const int si = static_cast<int>(sp->staffIdx());
        staffSeen[si] = true;
        EXPECT_LT(sp->tick(), sp->tick2()) << "slur span must be positive, staff " << si;

        // Verify the end element is a chord and its pitch matches note2 (not note3).
        const EngravingItem* endEl = sp->endElement();
        ASSERT_TRUE(endEl->isChord()) << "slur end must be a chord, staff " << si;
        const int endPitch = toChord(endEl)->notes().back()->pitch();
        auto it = expectedEndPitch.find(si);
        if (it != expectedEndPitch.end()) {
            EXPECT_EQ(endPitch, it->second)
                << "slur on staff " << si << " must end at note2 (pitch " << it->second
                << "), not note3";
        }
    }

    for (const auto& [si, expected] : expectedEndPitch) {
        EXPECT_TRUE(staffSeen.count(si) > 0) << "missing slur on staff " << si;
    }
    delete score;
}

// ===========================================================================
// FIX: targetEndXoff = slurXoffset2 (not firstNoteXoff + pixelSpan).
// When firstNoteXoff << slurXoffset the old formula underestimates the target
// and a "decoy" note with a low xoffset wins over the correct endpoint.
// Pattern: note1(xoff=2) + SLUR(xoff=10,xoff2=11) + note2(xoff=9) + note3(xoff=3).
// OLD target=3 → note3(dist=0) wins ✗. NEW target=11 → note2(dist=2) wins ✓.
// ===========================================================================

TEST_F(Tst_OrnamentsSlurs, v0xc2_slur_ends_at_note2_not_decoy_note3)
{
    MasterScore* score = readEncoreScore("ornaments_v0c2_slur_firstnote_xoff_mismatch.enc");
    ASSERT_NE(score, nullptr);

    const Measure* m0 = score->firstMeasure();
    ASSERT_NE(m0, nullptr);

    bool foundSlur = false;
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (!sp->isSlur()) {
            continue;
        }
        foundSlur = true;
        ASSERT_NE(sp->endElement(), nullptr) << "slur must have an end element";
        // The slur must end at note2 (E4 = pitch 64), NOT at note3 (C4 = pitch 60).
        ASSERT_TRUE(sp->endElement()->isChord()) << "slur end must be a chord";
        const int endPitch = toChord(sp->endElement())->notes().back()->pitch();
        EXPECT_EQ(endPitch, 64) << "slur must end at E4 (note2), not C4 (decoy note3)";
    }
    EXPECT_TRUE(foundSlur) << "score must contain a slur";
    delete score;
}

// ===========================================================================
// FIX: v0xC2 same-measure slur must not extend cross-measure when a note
// exists after the slur start in the current measure.
// ===========================================================================

TEST_F(Tst_OrnamentsSlurs, v0xc2_same_measure_slur_not_extended_to_next_measure)
{
    // ornaments_v0c2_same_measure_slur_no_cross.enc reproduces the pattern from
    // SALVEDOL.ENC measure 3: a slur from note 5 to note 6 within the same measure.
    // firstNoteXoff=9, slurXoffset=11, slurXoffset2=12: pixelSpan=1,
    // targetEndXoff=10 > maxXoffInMeas=9 -- tiny overshoot triggers the cross-measure
    // extension without the fix. Measure 1 has a decoy G4 (xoff=9, dist=1) that the
    // extension would incorrectly prefer over the correct same-measure E4 (xoff=5, dist=5).
    MasterScore* score = readEncoreScore("ornaments_v0c2_same_measure_slur_no_cross.enc");
    ASSERT_NE(score, nullptr);

    const Measure* m0 = score->firstMeasure();
    ASSERT_NE(m0, nullptr);

    bool foundSlur = false;
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (!sp->isSlur()) {
            continue;
        }
        foundSlur = true;
        EXPECT_NE(sp->startElement(), nullptr) << "slur must have start element";
        EXPECT_NE(sp->endElement(),   nullptr) << "slur must have end element";
        if (!sp->startElement() || !sp->endElement()) {
            continue;
        }
        const Measure* startMeas = sp->startElement()->findMeasure();
        const Measure* endMeas   = sp->endElement()->findMeasure();
        EXPECT_EQ(startMeas, m0) << "slur must start in measure 0";
        EXPECT_EQ(endMeas, m0) << "slur must end in measure 0, not in the decoy measure 1";
    }
    EXPECT_TRUE(foundSlur) << "score must contain a slur";
    delete score;
}

// ===========================================================================
// REGRESSION: v0xC2 multi-instrument slur routing — combined emLineSlot +
// targetEndXoff fix. Reproduces the SALVEDOL organ-bass pattern on all 4 staves.
// ===========================================================================

TEST_F(Tst_OrnamentsSlurs, v0xc2_multiinstr_slur_endpoint_on_note2_not_decoy)
{
    // ornaments_v0c2_multiinstr_slur_routing.enc: v0xC2, 2 instruments × 2 staves,
    // 3 quarter notes per staff with a SLURSTART at note1.
    // SALVEDOL organ-bass pattern: note1(xoff=2), SLUR(xoff=10,xoff2=11),
    // note2(xoff=9, correct endpoint), note3(xoff=3, decoy — close to OLD target=3).
    //
    // Without emLineSlot fix: staves 1-3 find no notes, strategy-3 picks note3.
    // Without targetEndXoff fix: target=3, note3(dist=0) beats note2(dist=6).
    // Both fixes: note2 wins on every staff.
    //
    // Expected note2 pitches: staff0=60, staff1=52, staff2=71, staff3=59.
    MasterScore* score = readEncoreScore("ornaments_v0c2_multiinstr_slur_routing.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_EQ(score->nstaves(), 4);

    const std::map<int, int> expectedPitch = { {0, 60}, {1, 52}, {2, 71}, {3, 59} };
    std::map<int, bool> staffSeen;

    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (!sp->isSlur()) {
            continue;
        }
        EXPECT_NE(sp->startElement(), nullptr) << "slur missing start";
        EXPECT_NE(sp->endElement(),   nullptr) << "slur missing end";
        if (!sp->startElement() || !sp->endElement()) {
            continue;
        }
        const int si = static_cast<int>(sp->staffIdx());
        staffSeen[si] = true;
        ASSERT_TRUE(sp->endElement()->isChord()) << "slur end not a chord, staff " << si;
        const int endPitch = toChord(sp->endElement())->notes().back()->pitch();
        auto it = expectedPitch.find(si);
        if (it != expectedPitch.end()) {
            EXPECT_EQ(endPitch, it->second)
                << "staff " << si << ": slur must end at note2 (pitch " << it->second
                << "), not note3 (decoy)";
        }
    }
    for (const auto& [si, _] : expectedPitch) {
        EXPECT_TRUE(staffSeen.count(si) > 0) << "missing slur on staff " << si;
    }
    delete score;
}

// ===========================================================================
// BUG FIX: Grace-to-main slur (SLURSTART at same Encore tick as appoggiatura)
// ===========================================================================

TEST_F(Tst_OrnamentsSlurs, grace_slur_to_main_not_dropped)
{
    // ornaments_grace_slur_to_main.enc: 4/4 measure with appoggiatura grace at
    // Encore tick=0, SLURSTART at tick=0 (alMezuro=0), and regular note at tick=15.
    //
    // Both grace and regular note map to MuseScore cumTick=0 (grace steals 15
    // ticks; regular note starts at measure beat 0). The heuristic converted
    // tick=15 to measTick+Fraction(15,960) where no chord exists → slur dropped.
    //
    // Fix: snap end tick to nearest real segment, detect zero-span as
    // grace-to-main, and create slur with startElement = grace chord.
    MasterScore* score = readEncoreScore("ornaments_grace_slur_to_main.enc");
    ASSERT_NE(score, nullptr);

    int slurCount = 0;
    bool graceStart = false;
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (sp->isSlur()) {
            ++slurCount;
            if (sp->startElement() && sp->startElement()->isChord()) {
                graceStart = toChord(sp->startElement())->isGrace();
            }
        }
    }
    bool endInSameMeasure = false;
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (sp->isSlur() && sp->startElement() && sp->endElement()
            && sp->startElement()->isChord() && sp->endElement()->isChord()) {
            const Chord* startCh = toChord(sp->startElement());
            const Chord* endCh   = toChord(sp->endElement());
            if (startCh->isGrace() && !endCh->isGrace()) {
                endInSameMeasure = (startCh->measure() == endCh->measure());
            }
        }
    }
    EXPECT_GE(slurCount, 1) << "At least one slur must be imported";
    EXPECT_TRUE(graceStart)
        << "Slur from appoggiatura grace must have a grace chord as startElement";
    EXPECT_TRUE(endInSameMeasure)
        << "Slur endElement must be the main chord in the same measure, not a note in the next measure";

    delete score;
}

TEST_F(Tst_OrnamentsSlurs, grace_slur_to_later_note_starts_from_grace)
{
    // ornaments_grace_slur_to_later.enc: 3/4 measure with half note at tick=0,
    // then SLURSTART + appoggiatura graces at Encore tick=450, and quarter at tick=480.
    //
    // In MuseScore: half=cumTick=0, graces+quarter=cumTick=1/2.
    // ps.startTick = measTick + Fraction(450,960) = measTick + 15/32.
    // endTick snaps to measTick + 1/2 (the quarter) > ps.startTick → grace-to-LATER slur.
    //
    // Without fix: computeStartElement() creates a TimeTick anchor at 15/32 and
    //   falls back to firstElement(staff) → half note → wrong start.
    // With fix: tick2rightSegment(ps.startTick) finds the quarter note which has
    //   graces → startElement set to first grace chord.
    MasterScore* score = readEncoreScore("ornaments_grace_slur_to_later.enc");
    ASSERT_NE(score, nullptr);

    int slurCount = 0;
    bool graceStart = false;
    bool endIsQuarter = false;
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (sp->isSlur()) {
            ++slurCount;
            if (sp->startElement() && sp->startElement()->isChord()) {
                graceStart = toChord(sp->startElement())->isGrace();
            }
            if (sp->endElement() && sp->endElement()->isChord()) {
                endIsQuarter = (toChord(sp->endElement())->durationType().type()
                                == DurationType::V_QUARTER);
            }
        }
    }
    EXPECT_GE(slurCount, 1) << "Slur from grace to later note must be imported";
    EXPECT_TRUE(graceStart)
        << "Slur startElement must be the grace chord, not the half note";
    EXPECT_TRUE(endIsQuarter)
        << "Slur endElement must be the quarter note that follows the graces";

    delete score;
}

// ===========================================================================
// BUG FIX: Grace-to-main slur with co-located grace+regular (same Encore tick)
//
// When an ACCIACCATURA grace and its main note share the same Encore tick, two
// bugs conspire to produce the wrong slur endpoint:
//
// Bug A (v0xC2): the pixel-span heuristic searches only notes AFTER startEncTick,
// so the co-located main note is excluded and the NEXT note (one beat later) is
// chosen → slur misses the main note.
//
// Bug B (both formats): the zero-span path sets tick2 = end-of-measure instead of
// startTick, making graceToMain=false → computeEndElement() runs and overwrites
// the explicit endElement with whatever is at end-of-measure (often a note in the
// next measure, or a rest).
//
// Fix A: detect grace+regular co-location in the heuristic and force zero-span.
// Fix B: set tick2 = startTick so graceToMain=true → computeEndElement skipped.
// ===========================================================================

TEST_F(Tst_OrnamentsSlurs, v0c4_grace_slur_to_main_coloc_correct_endpoint)
{
    // ornaments_v0c4_grace_slur_to_main_coloc.enc: 3/4 measure with
    // ACCIACCATURA at Encore tick=480 followed by a regular note ALSO at
    // tick=480, and SLURSTART at tick=480 (alMezuro=0, arc pointing within
    // the same beat).
    //
    // Without Fix B: zero-span path sets tick2=end-of-measure → computeEndElement
    // finds a rest or note in measure 2 → endElement is in the wrong measure.
    // With Fix B: tick2=startTick → graceToMain=true → endElement preserved as
    // the co-located main chord in measure 1.
    MasterScore* score = readEncoreScore("ornaments_v0c4_grace_slur_to_main_coloc.enc");
    ASSERT_NE(score, nullptr);

    int slurCount = 0;
    bool graceStart = false;
    bool endIsNonGrace = false;
    bool endInSameMeasure = false;
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (!sp->isSlur()) {
            continue;
        }
        ++slurCount;
        if (sp->startElement() && sp->startElement()->isChord()) {
            graceStart = toChord(sp->startElement())->isGrace();
        }
        if (sp->endElement() && sp->endElement()->isChord()) {
            const Chord* endCh = toChord(sp->endElement());
            endIsNonGrace = !endCh->isGrace();
            if (sp->startElement() && sp->startElement()->isChord()) {
                const Chord* startCh = toChord(sp->startElement());
                endInSameMeasure = (startCh->measure() == endCh->measure());
            }
        }
    }
    EXPECT_GE(slurCount, 1) << "Grace-to-main slur must be imported";
    EXPECT_TRUE(graceStart) << "startElement must be the grace chord";
    EXPECT_TRUE(endIsNonGrace) << "endElement must be the non-grace main chord";
    EXPECT_TRUE(endInSameMeasure)
        << "endElement must be in the same measure as the grace, not in a later measure";

    delete score;
}

TEST_F(Tst_OrnamentsSlurs, v0c2_grace_slur_to_main_coloc_correct_endpoint)
{
    // ornaments_v0c2_grace_slur_to_main_coloc.enc: 3/4 measure (v0xC2 format)
    // with ACCIACCATURA at tick=480 (xoff=6), regular note at tick=480 (xoff=5,
    // main note), regular note at tick=600 (xoff=4, the "bait" for the heuristic),
    // and SLURSTART at tick=480 (xoffset=4, xoffset2=3).
    //
    // Without Fix A: heuristic finds note@600 (dist=1 from targetEnd=5) as best
    // endpoint → endTick=measTick+5/8 ≠ startTick → slur ends at the WRONG note.
    // With Fix A: grace+regular co-location detected → force zero-span → grace-
    // to-main path creates slur with endElement = main chord at tick=480.
    MasterScore* score = readEncoreScore("ornaments_v0c2_grace_slur_to_main_coloc.enc");
    ASSERT_NE(score, nullptr);

    int slurCount = 0;
    bool graceStart = false;
    bool endIsNonGrace = false;
    bool endAtSameTick = false;
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (!sp->isSlur()) {
            continue;
        }
        ++slurCount;
        if (sp->startElement() && sp->startElement()->isChord()) {
            graceStart = toChord(sp->startElement())->isGrace();
        }
        if (sp->endElement() && sp->endElement()->isChord()) {
            const Chord* endCh = toChord(sp->endElement());
            endIsNonGrace = !endCh->isGrace();
            // The main note and grace are co-located: the slur's tick and the
            // endElement's segment tick must match (grace-to-main = zero span).
            endAtSameTick = (endCh->tick() == sp->tick());
        }
    }
    EXPECT_GE(slurCount, 1) << "Grace-to-main slur must be imported";
    EXPECT_TRUE(graceStart) << "startElement must be the grace chord";
    EXPECT_TRUE(endIsNonGrace) << "endElement must be the non-grace main chord";
    EXPECT_TRUE(endAtSameTick)
        << "endElement must be the co-located main chord (same beat as grace), "
        "not the note at the following beat";

    delete score;
}

// ===========================================================================
// BUG FIX: v0xC4 grace follows main in binary (regular BEFORE acciaccatura)
//
// In v0xC4 files, Encore 5 serializes the regular (main) note BEFORE the
// ACCIACCATURA grace note when they share the same Encore tick. This makes
// the grace appear as a chord extension (isChordExt=TRUE) of the already-
// placed regular note. Without the fix, the grace goes to pendingGraces and
// is attached to the NEXT chord, creating wrong note→[grace|note] order.
//
// Fix: when isChordExt=TRUE for a grace and a chord already exists at
// elemTick, attach the grace retroactively to that chord directly.
// ===========================================================================

TEST_F(Tst_OrnamentsSlurs, v0c4_grace_after_main_in_binary_slur_anchors_to_grace)
{
    // ornaments_v0c4_grace_after_main_in_binary.enc: 4/4 measure with
    // SLURSTART + Regular at tick=0 (FIRST in binary, main chord), then
    // ACCIACCATURA at tick=0 (SECOND in binary, grace), then Regular at
    // tick=240 (slur target for the grace-to-later slur).
    //
    // Without fix: ACCIACCATURA (isChordExt=TRUE) goes to pendingGraces and
    //   is attached to the Regular@240 chord. tick2rightSegment(0) finds the
    //   main chord at 0 with no graces → startElement is NOT set to grace.
    //   Result: slur starts at the main chord, not the grace.
    // With fix: ACCIACCATURA is attached retroactively to the already-placed
    //   main chord at tick=0. tick2rightSegment(0) finds the grace → slur
    //   startElement = ACCIACCATURA grace chord.
    MasterScore* score = readEncoreScore("ornaments_v0c4_grace_after_main_in_binary.enc");
    ASSERT_NE(score, nullptr);

    int slurCount = 0;
    bool graceStart = false;
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (!sp->isSlur()) {
            continue;
        }
        ++slurCount;
        if (sp->startElement() && sp->startElement()->isChord()) {
            graceStart = toChord(sp->startElement())->isGrace();
        }
    }
    EXPECT_GE(slurCount, 1) << "Grace-to-later slur must be imported";
    EXPECT_TRUE(graceStart)
        << "startElement must be the grace chord, not the regular (main) chord at same tick";

    delete score;
}

TEST_F(Tst_OrnamentsSlurs, v0c4_grace_after_main_grace_to_later_slur_anchors_to_grace)
{
    // ornaments_v0c4_grace_after_main_grace_to_later.enc: 4/4 measure with
    // Regular at tick=240 (FIRST in binary, xoff=5), ACCIACCATURA at tick=240
    // (SECOND, xoff=3), Note at tick=480 (xoff=9), SLURSTART at tick=240
    // (xoffset=5, xoffset2=9).
    //
    // noteloop — without Fix 1 (retroactive attachment):
    //   Regular@240: gap-snap to 1/4, mainChord@1/4, cumTick→1/2. prevMidiTick=240.
    //   ACCIACCATURA@240: isChordExt=TRUE → pendingGraces.
    //   Note@480: cumTick=1/2, chord@1/2, pendingGraces flushed → ACCIACCATURA on
    //     chord@1/2 (WRONG). With Fix 1: grace retroactively on mainChord@1/4.
    //
    // resolveSlurs — heuristic/shortcut interaction:
    //   firstNoteXoff=5 (Regular@240), targetEnd=9. Note@480 bestDist=0, regularDist=4.
    //   Refined shortcut: 4 > 0 → does NOT force zero-span (grace-to-LATER, not main).
    //   Old shortcut: targetEnd(9) <= maxXoff(9) → DID force zero-span (wrong).
    //
    // Without Fix 1: mainChord@1/4 has no graces → slur startElement = mainChord.
    //   graceStart=FALSE and endAtLaterTick=FALSE. Test FAILS.
    // Without refined shortcut (Fix 2): old shortcut forces zero-span → endTick=tick.
    //   graceStart=TRUE but endAtLaterTick=FALSE. Test FAILS.
    // With both fixes: grace-to-LATER slur; grace@1/4 → Note@1/2.
    MasterScore* score = readEncoreScore("ornaments_v0c4_grace_after_main_grace_to_later.enc");
    ASSERT_NE(score, nullptr);

    int slurCount = 0;
    bool graceStart = false;
    bool endAtLaterTick = false;
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (!sp->isSlur()) {
            continue;
        }
        ++slurCount;
        if (sp->startElement() && sp->startElement()->isChord()) {
            graceStart = toChord(sp->startElement())->isGrace();
        }
        if (sp->endElement() && sp->endElement()->isChord()) {
            // Grace-to-LATER: the slur must end at a chord AFTER the grace (not a
            // zero-span where endElement is the co-located main chord at the same tick).
            endAtLaterTick = (toChord(sp->endElement())->tick() > sp->tick());
        }
    }
    EXPECT_GE(slurCount, 1) << "Grace-to-later slur must be imported";
    EXPECT_TRUE(graceStart)
        << "startElement must be the grace chord retroactively attached to mainChord";
    EXPECT_TRUE(endAtLaterTick)
        << "Slur must end at the LATER note (grace-to-later), not zero-span at main";

    delete score;
}

TEST_F(Tst_OrnamentsSlurs, v0c4_grace_after_main_preceding_notes_slur_anchors_to_grace)
{
    // ornaments_v0c4_grace_after_main_preceding_notes.enc: 4/4 measure with
    // Quarter@0 (preceding note, pushes ctx.cumTick to 1/4 before the pair),
    // Regular@240 (FIRST in binary, xoff=5), ACCIACCATURA@240 (SECOND, xoff=3),
    // Note@480 (xoff=9), SLURSTART@240 (xoffset=5, xoffset2=9).
    //
    // This reproduces the BN-COLET5 scenario: ctx.cumTick was advanced to T by
    // preceding notes, then Regular@240 places mainChord@T. The grace (isChordExt)
    // must be retroactively attached to mainChord@T, not carried forward to Note@480.
    //
    // Without Fix 1 (retroactive attachment): ACCIACCATURA attaches to chord@1/2
    //   (Note@480). mainChord@1/4 has no graces. Slur startElement = mainChord.
    //   graceStart=FALSE and endAtLaterTick=FALSE → test FAILS.
    // Without refined shortcut (Fix 2): old shortcut would force zero-span
    //   (targetEnd=9 <= maxXoff=9). graceStart=TRUE but endAtLaterTick=FALSE.
    // With both fixes: grace-to-later slur; grace@1/4 → Note@1/2. Both TRUE.
    MasterScore* score = readEncoreScore("ornaments_v0c4_grace_after_main_preceding_notes.enc");
    ASSERT_NE(score, nullptr);

    int slurCount = 0;
    bool graceStart = false;
    bool endAtLaterTick = false;
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (!sp->isSlur()) {
            continue;
        }
        ++slurCount;
        if (sp->startElement() && sp->startElement()->isChord()) {
            graceStart = toChord(sp->startElement())->isGrace();
        }
        if (sp->endElement() && sp->endElement()->isChord()) {
            endAtLaterTick = (toChord(sp->endElement())->tick() > sp->tick());
        }
    }
    EXPECT_GE(slurCount, 1) << "Grace-to-later slur must be imported";
    EXPECT_TRUE(graceStart)
        << "startElement must be the grace chord; preceding context must not prevent "
        "retroactive attachment when grace follows main in binary";
    EXPECT_TRUE(endAtLaterTick)
        << "Slur must end at the later note (grace-to-later), not at the main chord";

    delete score;
}

// ===========================================================================
// BUG FIX: firstNoteXoff reference must be the grace note, not the regular
//
// In v0xC4 files the regular (main) note appears first in the binary, so the
// firstNoteXoff search currently picks its xoffset. But the slur arc starts
// at the GRACE position (slurXoffset ≈ grace xoffset), so using the regular's
// larger xoffset as reference inflates targetEndXoff and causes the heuristic
// to pick a far-away later note rather than the co-located main chord.
//
// Fix: prefer grace note xoffset as firstNoteXoff when a grace exists at
// startEncTick (regardless of binary order).
// ===========================================================================

TEST_F(Tst_OrnamentsSlurs, v0c4_grace_after_main_slur_arc_starts_at_grace_not_regular)
{
    // ornaments_v0c4_grace_after_main_slur_to_main.enc: 4/4 measure with
    // Regular@0 (FIRST in binary, xoff=20), ACCIACCATURA@0 (SECOND, xoff=10),
    // Regular@240 (xoff=40), SLURSTART@0 (xoffset=10, xoffset2=22).
    //
    // The slur arc starts near the grace (xoffset=10 ≈ grace xoff=10) and
    // ends near the main note (xoffset2=22 ≈ main xoff=20+2).
    //
    // Without fix (firstNoteXoff = Regular xoff=20):
    //   targetEnd = 20 + 12 = 32. Regular@240 (xoff=40, dist=8) beats
    //   mainChord regularDist=12 → no zero-span → grace-to-later (WRONG).
    //   endAtSameTick=FALSE → test FAILS.
    //
    // With fix (firstNoteXoff = ACCIACCATURA xoff=10):
    //   targetEnd = 10 + 12 = 22. Main chord dist=2 beats Regular@240 dist=18
    //   → zero-span → grace-to-main → endAtSameTick=TRUE → test PASSES.
    MasterScore* score = readEncoreScore("ornaments_v0c4_grace_after_main_slur_to_main.enc");
    ASSERT_NE(score, nullptr);

    int slurCount = 0;
    bool graceStart = false;
    bool endAtSameTick = false;
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (!sp->isSlur()) {
            continue;
        }
        ++slurCount;
        if (sp->startElement() && sp->startElement()->isChord()) {
            graceStart = toChord(sp->startElement())->isGrace();
        }
        if (sp->endElement() && sp->endElement()->isChord()) {
            // Grace-to-main: the slur arc ends at the co-located main chord
            // (same MuseScore tick), not at the later note.
            endAtSameTick = (toChord(sp->endElement())->tick() == sp->tick());
        }
    }
    EXPECT_GE(slurCount, 1) << "Grace-to-main slur must be imported";
    EXPECT_TRUE(graceStart) << "startElement must be the grace chord";
    EXPECT_TRUE(endAtSameTick)
        << "Slur must end at the co-located main chord (grace-to-main), not the "
        "later note — firstNoteXoff must use the grace xoffset, not the regular";

    delete score;
}
