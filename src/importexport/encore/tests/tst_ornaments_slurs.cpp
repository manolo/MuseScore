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
#include "../internal/importer/import-options.h"

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

TEST_F(Tst_OrnamentsSlurs, overfull_measure_slur_no_zero_length_arc)
{
    // An overfull measure shifts the ticks of the measures that follow, and a later
    // cross-measure slur can then have both grips resolve to the same chord: findCR for the
    // start grip finds no chord at/before the start tick and falls back to the measure's first
    // chord, which is also what the (exact) end-tick lookup returns. startElement == endElement
    // makes the Bezier layout take atan of a zero-length span and assert on a NaN control point.
    // The importer must drop such a slur, so loading (which lays out) must not crash and no
    // surviving slur may be zero-length.
    //
    // Fixture: a minimized, anonymized real-world v0xC4 (Encore 4.5) score - one staff, an
    // overfull bar near the start, and the slur near the end. The degenerate layout only arises
    // from this accumulated overfull tick drift, which a hand-built synthetic bar does not
    // reproduce, so the fixture is a trimmed real file rather than generator output.
    //
    // The drift only appears under the IrregularMeasure overfill strategy (the shipped GUI/CLI
    // default), which extends the bar instead of truncating it; the struct default used by
    // readEncoreScore() is Truncate, which drops the overflow and hides the bug.
    mu::iex::enc::EncImportOptions opts;
    opts.overfillMeasureStrategy = mu::iex::enc::OverfillStrategy::IrregularMeasure;
    MasterScore* score = readEncoreScoreWithOpts("structure_v0c4_slur_zero_length_overfull.enc", opts);
    ASSERT_NE(score, nullptr) << "must load and lay out without a NaN crash";
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (!sp->isSlur()) {
            continue;
        }
        EXPECT_NE(sp->startElement(), sp->endElement())
            << "no slur may have a coinciding start and end element (zero-length arc)";
    }
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
    // Reproduces the XEQUEABU.ENC pattern: a v0xC2 slur whose arc (xoffset=1,
    // xoffset2=5) starts before the first note of the measure (xoff=3) and extends
    // beyond all same-measure notes (maxXoffInMeas=4, targetEndXoff=7).
    //
    // Measure 0: note@0(xoff=3) + SLURSTART(xoff=1,xoff2=5) + note@240(xoff=2)
    //            + note@480(xoff=3) + note@720(xoff=4)
    // Measure 1: note@0(xoff=7)   ← correct endpoint (dist=0)
    //
    // Bug (before fix): bestEncTick=720 >= 0 and resolved=true after finding the
    // last same-measure note (dist=3). The cross-measure extension condition had
    // !resolved && bestEncTick<0, so it was skipped and the slur landed on tick=720
    // (last note of measure 0) instead of the first note of measure 1.
    //
    // Fix: remove !resolved and bestEncTick<0; replace with !usedTinyPixelSpan &&
    // (targetEndXoff>maxXoffInMeas || bestEncTick<0), excluding grace-to-main slurs.
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
    std::map<int, int> expectedEndPitch = { { 0, 64 }, { 1, 52 }, { 2, 71 }, { 3, 59 } };
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
// REGRESSION: v0xC2 multi-instrument slur routing, combined emLineSlot +
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

    const std::map<int, int> expectedPitch = { { 0, 60 }, { 1, 52 }, { 2, 71 }, { 3, 59 } };
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

// grace_slur_to_main_not_dropped: deferred to B12 (requires grace note emitter).
// grace_slur_to_later_note_starts_from_grace: deferred to B12.
// v0c4_grace_slur_to_main_coloc_correct_endpoint: deferred to B12.
// v0c2_grace_slur_to_main_coloc_correct_endpoint: deferred to B12.
// v0c4_grace_after_main_in_binary_slur_anchors_to_grace: deferred to B12.
// v0c4_grace_after_main_grace_to_later_slur_anchors_to_grace: deferred to B12.
// v0c4_grace_after_main_preceding_notes_slur_anchors_to_grace: deferred to B12.
// v0c4_grace_after_main_slur_arc_starts_at_grace_not_regular: deferred to B12.

// ===========================================================================
// REGRESSION: Cross-measure slur (alMezuro=2) endpoint resolved via Fallback 1
// (xoffset2 direct comparison against note xoffsets in target measure).
// The slur must land on D4 (pitch=62, xoff=15), NOT on F4 (last note, xoff=35).
// Without Fallback 1, the "last ChordRest" fallback would select F4 instead.
// Fixture: M0 SLURSTART(alMezuro=2, xoffset2=15); M2 has 4 notes with
// xoffsets 5/15/25/35 (C4/D4/E4/F4). slurXoffset2=15 → D4 (pitch=62).
// ===========================================================================
TEST_F(Tst_OrnamentsSlurs, cross_measure_slur_endpoint_precision)
{
    MasterScore* score = readEncoreScore("ornaments_cross_measure_slur_precision.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    const Spanner* crossSlur = nullptr;
    const Fraction firstMeasTick = score->firstMeasure()->tick();
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (!sp->isSlur()) {
            continue;
        }
        if (sp->tick() == firstMeasTick && sp->tick2() > sp->tick()) {
            crossSlur = sp;
            break;
        }
    }
    ASSERT_NE(crossSlur, nullptr) << "Cross-measure slur must be created";
    ASSERT_NE(crossSlur->endElement(), nullptr) << "Slur must have a resolved end element";
    ASSERT_TRUE(crossSlur->endElement()->isChord()) << "Slur end element must be a Chord";

    const int endPitch = toChord(crossSlur->endElement())->notes().back()->pitch();
    EXPECT_EQ(endPitch, 62)
        << "slurXoffset2=15 must select D4 (pitch=62, xoff=15), not F4 (last note, xoff=35)";

    delete score;
}
