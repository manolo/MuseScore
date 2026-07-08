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

// Slurs and ottavas: endpoint resolution from alMezuro/xoffset, grace-to-main and grace-to-later
// anchoring, multi-instrument staff routing, and dropping degenerate or open slurs safely.

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
#include "engraving/dom/ottava.h"
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

// An open slur (SLURSTART with no resolvable end) must be removed, or a Bezier layout with no
// endpoints produces NaN control points. See ENCORE_FORMAT.md §Slur.
TEST_F(Tst_OrnamentsSlurs, no_nan_crash_from_open_slurs)
{
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

// Overfull tick drift can make a later cross-measure slur resolve both grips to the same chord;
// such a zero-length slur must be dropped, or its Bezier layout asserts on a NaN control point.
// The fixture is a trimmed real v0xC4 file (accumulated drift does not reproduce synthetically),
// and the drift only appears under the IrregularMeasure strategy, so it is set explicitly here.
TEST_F(Tst_OrnamentsSlurs, overfull_measure_slur_no_zero_length_arc)
{
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

// .enc binaries have no SLURSTOP, so a multi-measure slur's end is resolved from alMezuro after the
// measure pass. See ENCORE_FORMAT.md §Slur.
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

// A v0xC2 cross-measure slur is resolved from the reliable +16 measure-count (altMezuro), anchoring
// the end to the downbeat of the target measure since its xoffset2 is stale. See ENCORE_FORMAT.md §Slur.
TEST_F(Tst_OrnamentsSlurs, v0xc2_cross_measure_slur_ends_in_next_measure)
{
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
    EXPECT_GT(crossMeasureCount, 0) << "expected at least one cross-measure slur";
    EXPECT_EQ(sameMeasureCount, 0) << "no same-measure slurs expected in this file";
    delete score;
}

// In compact-encoded multi-instrument files the raw staff byte must be translated to the routed
// LINE slot before matching, or staves 1-3 find no notes and the slur falls back to the last chord.
// Fixture: 2 instruments x 2 staves, each with a slur that must end on note2, not note3.
// See ENCORE_FORMAT.md §Multi-staff instruments.
TEST_F(Tst_OrnamentsSlurs, multiinstr_slur_endpoint_on_second_note_not_last_chord)
{
    MasterScore* score = readEncoreScore("ornaments_multiinstr_slur_routing.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_EQ(score->nstaves(), 4);

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

// A v0xC2 within-measure slur with a tiny pixel span is a note-to-next-note arc, so it must end at
// the next note; the stale xoffset2 must not be matched directly. See ENCORE_FORMAT.md §Slur.
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
        ASSERT_TRUE(sp->endElement()->isChord()) << "slur end must be a chord";
        const int endPitch = toChord(sp->endElement())->notes().back()->pitch();
        EXPECT_EQ(endPitch, 64) << "slur must end at E4 (note2), not C4 (decoy note3)";
    }
    EXPECT_TRUE(foundSlur) << "score must contain a slur";
    delete score;
}

// A v0xC2 within-measure slur (count==0) with a tiny pixel span must resolve to the next note in
// the same measure and not extend to a decoy in the next measure. See ENCORE_FORMAT.md §Slur.
TEST_F(Tst_OrnamentsSlurs, v0xc2_same_measure_slur_not_extended_to_next_measure)
{
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

// Combines the two v0xC2 rules on all 4 staves: the compact staff byte maps to the LINE slot so
// the slur finds notes on every staff, and the tiny-span next-note rule ends it at note2, not the
// decoy note3. See ENCORE_FORMAT.md §Multi-staff instruments and §Slur.
TEST_F(Tst_OrnamentsSlurs, v0xc2_multiinstr_slur_endpoint_on_note2_not_decoy)
{
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

// A slur from an appoggiatura grace to its main note at the same Encore tick must not be dropped:
// the zero-span is detected as grace-to-main and the slur starts at the grace chord.
// See ENCORE_FORMAT.md §Grace and cue notes and §Slur.
TEST_F(Tst_OrnamentsSlurs, grace_slur_to_main_not_dropped)
{
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

// A slur from graces to a later note must start at the grace chord: the start tick resolves to the
// note that carries the graces, not fall back to an earlier chord. See ENCORE_FORMAT.md §Slur.
TEST_F(Tst_OrnamentsSlurs, grace_slur_to_later_note_starts_from_grace)
{
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

// When an acciaccatura and its main note share one Encore tick, the zero-span path must set
// tick2 == startTick so the explicit grace-to-main endElement is preserved (the co-located main
// chord), not overwritten by whatever sits at end-of-measure. See ENCORE_FORMAT.md §Slur.
TEST_F(Tst_OrnamentsSlurs, v0c4_grace_slur_to_main_coloc_correct_endpoint)
{
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

// v0xC2 counterpart: the pixel-span heuristic must detect grace+regular co-location and force a
// zero-span so the slur ends at the co-located main chord, not a later decoy note the xoffset
// match would otherwise prefer. See ENCORE_FORMAT.md §Slur.
TEST_F(Tst_OrnamentsSlurs, v0c2_grace_slur_to_main_coloc_correct_endpoint)
{
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

// v0xC4 serializes the main note before its co-tick grace, so the grace arrives as a chord
// extension and must be attached retroactively to the already-placed main chord (not carried to
// the next chord); the slur then anchors to that grace. See ENCORE_FORMAT.md §Grace and cue notes.
TEST_F(Tst_OrnamentsSlurs, v0c4_grace_after_main_in_binary_slur_anchors_to_grace)
{
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

// Same v0xC4 grace-after-main ordering, but the slur points at a LATER note: retroactive
// attachment must put the grace on the main chord, and the refined span shortcut must not force a
// zero-span, so the slur runs grace-to-later. See ENCORE_FORMAT.md §Grace and cue notes and §Slur.
TEST_F(Tst_OrnamentsSlurs, v0c4_grace_after_main_grace_to_later_slur_anchors_to_grace)
{
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

// Same case with preceding notes advancing cumTick first: retroactive attachment must still put
// the grace on the main chord at the advanced tick, not carry it forward to the later note.
// See ENCORE_FORMAT.md §Grace and cue notes and §Slur.
TEST_F(Tst_OrnamentsSlurs, v0c4_grace_after_main_preceding_notes_slur_anchors_to_grace)
{
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

// The slur arc starts at the grace position, so firstNoteXoff must use the grace xoffset even when
// the regular note comes first in binary; the regular's larger xoffset would inflate the target and
// pick a far-away note instead of the co-located main chord. See ENCORE_FORMAT.md §Slur.
TEST_F(Tst_OrnamentsSlurs, v0c4_grace_after_main_slur_arc_starts_at_grace_not_regular)
{
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
        "later note, firstNoteXoff must use the grace xoffset, not the regular";

    delete score;
}

// A cross-measure slur end is resolved by comparing xoffset2 against the target measure's note
// xoffsets, so it lands on the matching note (D4) rather than the last chord (F4).
// See ENCORE_FORMAT.md §Slur.
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

// Two ottava spanners (8va, 8vb): each endpoint is pinned to the next ottava's start, or scoreEnd.
// See ENCORE_FORMAT.md §Spanner endpoints.
TEST_F(Tst_OrnamentsSlurs, v0c4_ottava_two_spanners)
{
    MasterScore* score = readEncoreScore("ornaments_ottava_two_spanners.enc");
    ASSERT_NE(score, nullptr) << "Failed to load ornaments_ottava_two_spanners.enc";

    std::vector<Ottava*> ottavas;
    for (const auto& kv : score->spanner()) {
        Spanner* sp = kv.second;
        if (sp && sp->isOttava()) {
            ottavas.push_back(toOttava(sp));
        }
    }
    ASSERT_EQ(ottavas.size(), 2u) << "expected exactly 2 ottava spanners";

    std::sort(ottavas.begin(), ottavas.end(), [](Ottava* a, Ottava* b) {
        return a->tick() < b->tick();
    });

    EXPECT_EQ(ottavas[0]->ottavaType(), OttavaType::OTTAVA_8VA);
    EXPECT_EQ(ottavas[0]->tick(),  Fraction(0, 1));
    EXPECT_EQ(ottavas[0]->tick2(), Fraction(1, 1));

    EXPECT_EQ(ottavas[1]->ottavaType(), OttavaType::OTTAVA_8VB);
    EXPECT_EQ(ottavas[1]->tick(),  Fraction(1, 1));
    EXPECT_EQ(ottavas[1]->tick2(), Fraction(6, 1));

    delete score;
}

// When any v0xC2 slur's +16 measure-count points past the last measure, the whole file's +16 field
// is treated as unreliable and every slur resolves inside its own bar. See ENCORE_FORMAT.md §Slur.
TEST_F(Tst_OrnamentsSlurs, v0c2_unreliable_slur_count_stays_in_measure)
{
    MasterScore* score = readEncoreScore("ornaments_v0c2_unreliable_slur_count.enc");
    ASSERT_NE(score, nullptr) << "Failed to load ornaments_v0c2_unreliable_slur_count.enc";

    int total = 0;
    int crossMeasure = 0;
    for (auto it : score->spanner()) {
        Spanner* sp = it.second;
        if (!sp || !sp->isSlur()) {
            continue;
        }
        ++total;
        Measure* m1 = score->tick2measure(sp->tick());
        Measure* m2 = score->tick2measure(sp->tick2());
        if (m1 && m2 && m1 != m2) {
            ++crossMeasure;
        }
    }
    EXPECT_EQ(total, 2) << "both slurs must import";
    EXPECT_EQ(crossMeasure, 0)
        << "a plausible-looking count must not extend a slur past its bar when the file's "
        "+16 field is unreliable";
    delete score;
}

// Some v0xC2 files store a constant in-range value at the slur +16 field for every slur, which the
// past-the-end guard misses. A repeated span >= 3 across different start measures also marks +16
// unreliable, so each slur resolves inside its own bar. See ENCORE_FORMAT.md §Slur.
TEST_F(Tst_OrnamentsSlurs, v0c2_constant_slur_count_stays_in_measure)
{
    MasterScore* score = readEncoreScore("ornaments_v0c2_constant_slur_count.enc");
    ASSERT_NE(score, nullptr) << "Failed to load ornaments_v0c2_constant_slur_count.enc";

    int total = 0;
    int crossMeasure = 0;
    for (auto it : score->spanner()) {
        Spanner* sp = it.second;
        if (!sp || !sp->isSlur()) {
            continue;
        }
        ++total;
        Measure* m1 = score->tick2measure(sp->tick());
        Measure* m2 = score->tick2measure(sp->tick2());
        if (m1 && m2 && m1 != m2) {
            ++crossMeasure;
        }
    }
    EXPECT_EQ(total, 2) << "both slurs must import";
    EXPECT_EQ(crossMeasure, 0)
        << "a constant +16 value repeated across start measures must not extend the slurs "
        "into an 11-measure phantom span";
    delete score;
}
