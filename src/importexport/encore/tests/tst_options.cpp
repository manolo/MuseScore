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

#include "engraving/compat/scoreaccess.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/layoutbreak.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/volta.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/part.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftext.h"
#include "engraving/dom/stafftype.h"
#include "engraving/dom/tempotext.h"
#include "engraving/style/style.h"
#include "engraving/types/fraction.h"

#include "../internal/importer/import-options.h"

#include "testbase.h"

static const QString ENC_DIR(QString(iex_encore_tests_DATA_ROOT) + "/data/");

using namespace mu::engraving;
using namespace mu::iex::enc;

class Tst_Options : public ::testing::Test, public MTest
{
protected:
    void SetUp() override { setRootDir(ENC_DIR); }
};

// ===========================================================================
// importStaffSize
// All test files in data/ have scoreSize=3, which maps to MAG 1.00 (100%).
// ===========================================================================

TEST_F(Tst_Options, importStaffSize_true_applies_encore_scale)
{
    MasterScore* score = readEncoreScore("bazo.enc");
    ASSERT_NE(score, nullptr);
    // scoreSize=3 → kScaleBySize[2] = 1.00 (100%)
    const double mag = score->staff(0)->staffType(Fraction(0, 1))->userMag();
    EXPECT_DOUBLE_EQ(mag, 1.00)
        << "importStaffSize=true (default) must apply Encore scoreSize=3 → MAG 1.00";
    delete score;
}

TEST_F(Tst_Options, importStaffSize_false_keeps_unit_scale)
{
    EncImportOptions opts;
    opts.importStaffSize = false;
    MasterScore* score = readEncoreScoreWithOpts("bazo.enc", opts);
    ASSERT_NE(score, nullptr);
    const double mag = score->staff(0)->staffType(Fraction(0, 1))->userMag();
    EXPECT_DOUBLE_EQ(mag, 1.0)
        << "importStaffSize=false must leave staff MAG at the MuseScore default (1.0)";
    delete score;
}

// ===========================================================================
// underfillMeasureStrategy
// ===========================================================================

TEST_F(Tst_Options, underfill_default_creates_gap_rests)
{
    // structure_pickup_casea_sparse has sparse voices, producing gap rests by default.
    MasterScore* score = readEncoreScore("structure_pickup_casea_sparse.enc");
    ASSERT_NE(score, nullptr);

    int gapCount = 0;
    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        for (Segment* s = m->first(SegmentType::ChordRest); s;
             s = s->next(SegmentType::ChordRest)) {
            for (track_idx_t track = 0; track < score->ntracks(); ++track) {
                EngravingItem* e = s->element(track);
                if (e && e->isRest() && toRest(e)->isGap()) {
                    ++gapCount;
                }
            }
        }
    }
    EXPECT_GT(gapCount, 0)
        << "Default InvisibleRests must produce at least one gap rest in this file";
    delete score;
}

TEST_F(Tst_Options, underfill_visible_rests_produces_no_gap_rests)
{
    EncImportOptions opts;
    opts.underfillMeasureStrategy = UnderfillStrategy::VisibleRests;
    MasterScore* score = readEncoreScoreWithOpts("structure_pickup_casea_sparse.enc", opts);
    ASSERT_NE(score, nullptr);

    int gapCount = 0;
    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        for (Segment* s = m->first(SegmentType::ChordRest); s;
             s = s->next(SegmentType::ChordRest)) {
            for (track_idx_t track = 0; track < score->ntracks(); ++track) {
                EngravingItem* e = s->element(track);
                if (e && e->isRest() && toRest(e)->isGap()) {
                    ++gapCount;
                }
            }
        }
    }
    EXPECT_EQ(gapCount, 0)
        << "VisibleRests strategy must not produce any gap (invisible) rests";
    delete score;
}

// ===========================================================================
// firstMeasureIsPickup
// ===========================================================================

// Default: first measure is shortened to the pickup duration (1/4 for this file).
// firstMeasureIsPickup=false: first measure keeps its full nominal duration.
TEST_F(Tst_Options, firstMeasure_default_is_shortened_to_pickup)
{
    MasterScore* score = readEncoreScore("structure_pickup_measure.enc");
    ASSERT_NE(score, nullptr);
    Measure* m0 = score->firstMeasure();
    ASSERT_NE(m0, nullptr);
    EXPECT_NE(m0->ticks(), m0->timesig())
        << "Default: first measure must be shortened as pickup";
    delete score;
}

TEST_F(Tst_Options, firstMeasure_not_pickup_keeps_full_nominal_duration)
{
    EncImportOptions opts;
    opts.firstMeasureIsPickup = false;
    MasterScore* score = readEncoreScoreWithOpts("structure_pickup_measure.enc", opts);
    ASSERT_NE(score, nullptr);
    Measure* m0 = score->firstMeasure();
    ASSERT_NE(m0, nullptr);
    EXPECT_EQ(m0->ticks(), m0->timesig())
        << "firstMeasureIsPickup=false: first measure must retain full nominal duration";
    delete score;
}

// ===========================================================================
// underfillMeasureStrategy = IrregularMeasure
// ===========================================================================

TEST_F(Tst_Options, underfill_irregular_measure_produces_no_gap_rests)
{
    EncImportOptions opts;
    opts.underfillMeasureStrategy = UnderfillStrategy::IrregularMeasure;
    MasterScore* score = readEncoreScoreWithOpts("structure_pickup_casea_sparse.enc", opts);
    ASSERT_NE(score, nullptr);
    int gapCount = 0;
    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        for (Segment* s = m->first(SegmentType::ChordRest); s;
             s = s->next(SegmentType::ChordRest)) {
            for (track_idx_t tr = 0; tr < score->ntracks(); ++tr) {
                EngravingItem* e = s->element(tr);
                if (e && e->isRest() && toRest(e)->isGap()) {
                    ++gapCount;
                }
            }
        }
    }
    EXPECT_EQ(gapCount, 0)
        << "IrregularMeasure must not produce any gap rests";
    delete score;
}

TEST_F(Tst_Options, underfill_irregular_measure_passes_sanity_check)
{
    EncImportOptions opts;
    opts.underfillMeasureStrategy = UnderfillStrategy::IrregularMeasure;
    MasterScore* score = readEncoreScoreWithOpts("structure_pickup_casea_sparse.enc", opts);
    ASSERT_NE(score, nullptr);
    const muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "IrregularMeasure: score failed sanity check: " << ret.text();
    delete score;
}

// A bar where only one staff has (sparse) notes and another staff is silent must NOT be
// shrunk by IrregularMeasure: the silent staff is a whole-bar rest, so the longest staff is
// the full bar. The bug measured only the note-bearing staff, shrank the whole bar, shifted
// every following measure and corrupted them. Guards both the no-shrink decision and that the
// following full bars survive intact.
TEST_F(Tst_Options, underfill_irregular_does_not_shrink_bar_with_silent_staff)
{
    EncImportOptions opts;
    opts.underfillMeasureStrategy = UnderfillStrategy::IrregularMeasure;
    MasterScore* score = readEncoreScoreWithOpts("options_underfill_irregular_empty_staff.enc", opts);
    ASSERT_NE(score, nullptr);

    const muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "silent-staff bar corrupted the score: " << ret.text();

    auto staffSum = [](Measure* m, size_t st) {
        Fraction sum(0, 1);
        for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* e = s->element(static_cast<track_idx_t>(st * VOICES));
            if (e && e->isChordRest()) {
                sum += toChordRest(e)->actualTicks();
            }
        }
        return sum;
    };

    // Bar 1 (sparse staff0 + silent staff1) keeps its nominal 4/4; both staves fill it.
    Measure* m0 = score->firstMeasure();
    ASSERT_NE(m0, nullptr);
    Measure* m1 = m0->nextMeasure();
    ASSERT_NE(m1, nullptr);
    EXPECT_EQ(m1->ticks(), Fraction(4, 4)) << "bar with a silent staff must not shrink";
    for (size_t st = 0; st < score->nstaves(); ++st) {
        EXPECT_EQ(staffSum(m1, st), Fraction(4, 4)) << "sparse bar staff " << st << " must fill the bar";
    }

    // The surrounding full bars (0, 2, 3) keep their 4/4 content intact.
    for (Measure* m = m0; m; m = m->nextMeasure()) {
        if (m == m1) {
            continue;
        }
        EXPECT_EQ(m->ticks(), Fraction(4, 4)) << "full bar must stay 4/4";
        for (size_t st = 0; st < score->nstaves(); ++st) {
            EXPECT_EQ(staffSum(m, st), Fraction(4, 4)) << "full bar staff " << st << " content lost";
        }
    }
    delete score;
}

// ===========================================================================
// overfillMeasureStrategy -- reserved variants: sanity-only tests
// ===========================================================================

TEST_F(Tst_Options, overfill_stretch_last_note_does_not_crash)
{
    EncImportOptions opts;
    opts.overfillMeasureStrategy = OverfillStrategy::StretchLastNote;
    MasterScore* score = readEncoreScoreWithOpts("bazo.enc", opts);
    ASSERT_NE(score, nullptr) << "StretchLastNote strategy must not crash during import";
    delete score;
}

TEST_F(Tst_Options, overfill_irregular_measure_does_not_crash)
{
    EncImportOptions opts;
    opts.overfillMeasureStrategy = OverfillStrategy::IrregularMeasure;
    MasterScore* score = readEncoreScoreWithOpts("bazo.enc", opts);
    ASSERT_NE(score, nullptr) << "IrregularMeasure overfill strategy must not crash during import";
    delete score;
}

// ===========================================================================
// instrumentSearchMode
// ===========================================================================

// Piano mode: all instruments fall back to Grand Piano.
TEST_F(Tst_Options, instrumentSearchMode_piano_assigns_grand_piano_to_all)
{
    EncImportOptions opts;
    opts.instrumentSearchMode = InstrumentSearchMode::Piano;
    MasterScore* score = readEncoreScoreWithOpts("bazo.enc", opts);
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    for (const Part* part : score->parts()) {
        const Instrument* inst = part->instrument();
        ASSERT_NE(inst, nullptr);
        EXPECT_EQ(inst->id(), String(u"grand-piano"))
            << "Piano mode: every instrument must be Grand Piano";
    }
    delete score;
}

// MidiOnly mode: name matching is skipped, only MIDI program drives selection.
TEST_F(Tst_Options, instrumentSearchMode_midi_only_does_not_crash)
{
    EncImportOptions opts;
    opts.instrumentSearchMode = InstrumentSearchMode::MidiOnly;
    MasterScore* score = readEncoreScoreWithOpts("bazo.enc", opts);
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "MidiOnly mode must not produce a corrupt score: " << ret.text();
    delete score;
}

// Default mode: name+MIDI gives a better result than MidiOnly when the name matches.
TEST_F(Tst_Options, instrumentSearchMode_name_and_midi_resolves_bandurria)
{
    // instruments_abbreviated_name_bandurr.enc has name "Bandurr. I" which matches
    // "Bandurria" via substring (after punctuation stripping).
    MasterScore* score = readEncoreScore("instruments_abbreviated_name_bandurr.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    EXPECT_EQ(score->parts().front()->instrument()->id(), String(u"bandurria"))
        << "Name+MIDI default: 'Bandurr. I' must resolve to bandurria template";
    delete score;
}

// ===========================================================================
// Instrument template bracket clearing
// ===========================================================================

// Accordion template has a brace with span=2 that would overflow into the next
// part when the accordion has only 1 staff.  After clearing template brackets,
// no spurious cross-part bracket should appear.
TEST_F(Tst_Options, template_brackets_cleared_no_spurious_brace)
{
    // akordo.enc has multiple instruments; if template bracket clearing fails,
    // layout may crash or produce wrong bracket spans.
    MasterScore* score = readEncoreScore("akordo.enc");
    ASSERT_NE(score, nullptr);
    // Verify no staff has a bracket that overflows past the score's staves.
    for (staff_idx_t si = 0; si < score->nstaves(); ++si) {
        Staff* st = score->staff(si);
        ASSERT_NE(st, nullptr);
        const size_t span = st->bracketSpan(0);
        if (span > 1) {
            EXPECT_LE(si + span, score->nstaves())
                << "Bracket on staff " << si << " spans " << span
                << " but score only has " << score->nstaves() << " staves";
        }
    }
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();
    delete score;
}
