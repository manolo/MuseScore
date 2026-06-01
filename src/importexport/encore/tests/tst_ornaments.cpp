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

class Tst_Ornaments : public ::testing::Test, public MTest
{
protected:
    void SetUp() override { setRootDir(ENC_DIR); }
};

// ===========================================================================
// BUG FIX: Open slurs removed (no NaN in Bezier layout)
// ===========================================================================

TEST_F(Tst_Ornaments, no_nan_crash_from_open_slurs)
{
    // Before fix: Beethoven/Opus27 had SLURSTART without SLURSTOP.
    // The resulting slur with no endpoints caused NaN in computeBezier → crash.
    // After fix: all open slurs are removed from the score.
    // notes_corrupted.enc has a SLURSTART ornament with no matching SLURSTOP.
    // Before fix: the resulting slur had no endpoints → NaN in Bezier layout.
    // After fix: open slurs are removed; all remaining spanners have valid tick ranges.
    MasterScore* score = readEncoreScore("notes_corrupted.enc");
    ASSERT_NE(score, nullptr) << "Corrupted file should load without NaN crash";
    for (auto& [tick, sp] : score->spannerMap().map()) {
        EXPECT_LT(sp->tick(), sp->tick2())
            << "All spanners should have tick < tick2 (valid range)";
    }
    delete score;
}

TEST_F(Tst_Ornaments, no_nan_crash_opus27)
{
    // Re-uses the same synthetic file; verifies the fix is robust.
    MasterScore* score = readEncoreScore("notes_corrupted.enc");
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

TEST_F(Tst_Ornaments, beamed_triplet_capped_no_beam_assert)
{
    MasterScore* score = readEncoreScore("ornaments_beamed_triplet_capped.enc");
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

TEST_F(Tst_Ornaments, zero_length_hairpin_dropped_cleanly)
{
    MasterScore* score = readEncoreScore("ornaments_zero_hairpin.enc");
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

TEST_F(Tst_Ornaments, partial_quarter_triplet_layout_does_not_assert)
{
    MasterScore* score = readEncoreScore("ornaments_partial_quarter_triplet.enc");
    ASSERT_NE(score, nullptr) << "File must load and lay out without assert in beam.cpp";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();
    delete score;
}

TEST_F(Tst_Ornaments, articulations_mapped_beyond_fermata)
{
    MasterScore* score = readEncoreScore("ornaments_articulations.enc");
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
TEST_F(Tst_Ornaments, articulation_combos_expand_to_two_glyphs)
{
    MasterScore* score = readEncoreScore("ornaments_articulations_combo.enc");
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
// FEATURE: Per-chord staccato from size-16 ORN tipo=0xC9.
// Encore stores staccato as a separate ORN at the same tick as the chord;
// its MusicXML exporter drops the byte but the dot is visible. The
// importer must add Staccato per ORN and dedup against the per-note
// artic byte 0x1D (which produces the same glyph).
// ===========================================================================
TEST_F(Tst_Ornaments, staccato_from_orn_c9)
{
    MasterScore* score = readEncoreScore("ornaments_staccato_orn.enc");
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
TEST_F(Tst_Ornaments, trill_spanner_start_markers)
{
    MasterScore* score = readEncoreScore("ornaments_trill_spanner.enc");
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
TEST_F(Tst_Ornaments, technical_markings_per_note_artic_byte)
{
    MasterScore* score = readEncoreScore("ornaments_technical.enc");
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
TEST_F(Tst_Ornaments, fermatas_emit_segment_anchored_element)
{
    MasterScore* score = readEncoreScore("ornaments_fermatas.enc");
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
TEST_F(Tst_Ornaments, tremolos_from_per_note_artic_byte)
{
    MasterScore* score = readEncoreScore("ornaments_tremolos.enc");
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
TEST_F(Tst_Ornaments, trill_mordent_from_per_note_artic_byte)
{
    MasterScore* score = readEncoreScore("ornaments_trill_mordent.enc");
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
TEST_F(Tst_Ornaments, double_barline_lands_on_every_staff)
{
    MasterScore* score = readEncoreScore("ornaments_double_barline_multi_staff.enc");
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
TEST_F(Tst_Ornaments, wedgestart_at_measure_end_boundary)
{
    MasterScore* score = readEncoreScore("ornaments_wedgestart_at_measure_end.enc");
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
TEST_F(Tst_Ornaments, dynamics_from_size16_ornaments)
{
    MasterScore* score = readEncoreScore("ornaments_dynamics.enc");
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
TEST_F(Tst_Ornaments, dynamics_full_ladder_voice4_system_mark)
{
    MasterScore* score = readEncoreScore("ornaments_dynamics_full.enc");
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
TEST_F(Tst_Ornaments, arpeggio_attaches_to_chord)
{
    MasterScore* score = readEncoreScore("ornaments_arpeggio.enc");
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

TEST_F(Tst_Ornaments, multi_measure_slur_resolved_from_almezuro)
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
// FIX: Multi-measure hairpin resolves end tick from WEDGESTART's alMezuro.
// Two hairpins:
//   - crescendo from measure 0 tick=0 with alMezuro=2 -> ends at end of measure 2
//   - diminuendo from measure 1 tick=480 with alMezuro=1 -> ends at end of measure 2
// ===========================================================================

TEST_F(Tst_Ornaments, multi_measure_hairpin_resolved_from_almezuro)
{
    MasterScore* score = readEncoreScore("ornaments_multi_measure_hairpin.enc");
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
// FEATURE: Bowing marks from stand-alone size-16 ORN elements.
// tipo 0xC5 = down-bow (П), tipo 0xC4 = up-bow (V). Placed at the same
// tick as the chord; the importer defers them in pendingBowings and attaches
// an Articulation (stringsDownBow / stringsUpBow) during resolveAll().
// ===========================================================================
TEST_F(Tst_Ornaments, bowing_marks_from_orn_c4_c5)
{
    MasterScore* score = readEncoreScore("ornaments_bowing.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<SymId> bowings;
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
                if (a->symId() == SymId::stringsDownBow
                    || a->symId() == SymId::stringsUpBow) {
                    bowings.push_back(a->symId());
                }
            }
        }
    }
    const std::vector<SymId> expected = {
        SymId::stringsDownBow, SymId::stringsUpBow,
        SymId::stringsDownBow, SymId::stringsUpBow,
    };
    EXPECT_EQ(bowings, expected);
    delete score;
}

// ===========================================================================
// FEATURE: Stand-alone fingering from ORN elements tipo 0xB9..0xBD.
// tipo = 0xB8 + finger (1..5). The importer defers them in pendingOrnFingerings
// and attaches a Fingering with xmlText "1".."5" to the top note of the chord.
// ===========================================================================
TEST_F(Tst_Ornaments, fingering_from_orn_b9_bd)
{
    MasterScore* score = readEncoreScore("ornaments_fingering_orn.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<String> fingerings;
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
        }
    }
    const std::vector<String> expected = { u"1", u"2", u"3", u"4", u"5" };
    EXPECT_EQ(fingerings, expected);
    delete score;
}
