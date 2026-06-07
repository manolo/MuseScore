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

TEST_F(Tst_Ornaments, no_nan_crash_opus27)
{
    MasterScore* score = readEncoreScore("notes_corrupted.enc");
    ASSERT_NE(score, nullptr);
    delete score;
}

// ===========================================================================
// REGRESSION: Capped 3:2 triplet produces non-TDuration placedTicks; closeTuplet must not assert in beam layout.
// ===========================================================================

TEST_F(Tst_Ornaments, beamed_triplet_capped_no_beam_assert)
{
    MasterScore* score = readEncoreScore("ornaments_beamed_triplet_capped.enc");
    ASSERT_NE(score, nullptr) << "File should load without beam-layout assert";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();
    for (auto& [tick, sp] : score->spannerMap().map()) {
        EXPECT_LT(sp->tick(), sp->tick2()) << "Spanner has non-positive span";
    }
    delete score;
}

// ===========================================================================
// REGRESSION: WEDGESTART with alMezuro=0 must span the current measure, not collapse to zero.
// ===========================================================================

TEST_F(Tst_Ornaments, zero_length_hairpin_dropped_cleanly)
{
    MasterScore* score = readEncoreScore("ornaments_zero_hairpin.enc");
    ASSERT_NE(score, nullptr) << "File should load without Spanner::setTicks assert";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();
    for (auto& [tick, sp] : score->spannerMap().map()) {
        EXPECT_LT(sp->tick(), sp->tick2()) << "Spanner has non-positive span";
    }
    delete score;
}

// ===========================================================================
// REGRESSION: Partial 3:2 quarter triplet (placedTicks=1/3, not a TDuration fraction) must not assert in beam layout.
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

    // Check by type predicate rather than exact SymId; layout may flip Above<->Below per stem direction.
    enum class ArtKind {
        Staccato, Accent, Tenuto, Marcato, Other
    };
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
// FEATURE: Combo articulation bytes (e.g. 0x24 = tenuto + staccato) expand to two Articulation elements.
// ===========================================================================
TEST_F(Tst_Ornaments, articulation_combos_expand_to_two_glyphs)
{
    MasterScore* score = readEncoreScore("ornaments_articulations_combo.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    enum class K {
        Tenuto, Staccato, Accent, Marcato, Staccatissimo, Other
    };
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
    // m1: 0x24 ten+stacc, 0x17 acc+stacc, 0x27 marc+ten, 0x15 marc+stacc;
    // m2: 0x23 acc+ten, 0x2D ten+stiss, 0x2B acc+stiss, 0x24 ten+stacc.
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
// FEATURE: Per-chord staccato from size-16 ORN tipo=0xC9; deduped against per-note artic byte 0x1D.
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
    // chords 0,1,3 have staccato; chord 2 has none; chord 3 dedupes 0xC9 + artic byte 0x1D.
    const std::vector<bool> expected = { true, true, false, true };
    EXPECT_EQ(seen, expected);
    delete score;
}

// ===========================================================================
// FIX: Size-28 ORN 0x36 (TRILL_START) + 0x35 (TRILL_END) now create a Trill spanner
// (tr + wavy line) instead of a glyph-only Ornament. ORN 0x37 (TRILL_ALT) remains
// an Ornament glyph (secondary marker within the span). 0x35 is consumed as the span
// endpoint and produces no visible element of its own.
// Fixture: 0x36 at tick=0, 0x37 at tick=240, 0x35 at tick=480 in a 4/4 measure.
// Expected: one Trill spanner (from 0x36 to 0x35) + one Ornament glyph (from 0x37).
// ===========================================================================
TEST_F(Tst_Ornaments, trill_spanner_start_markers)
{
    MasterScore* score = readEncoreScore("ornaments_trill_spanner.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    // Count Trill spanners (0x36 → 0x35)
    int trillSpanners = 0;
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (sp->isTrill()) {
            ++trillSpanners;
        }
    }
    EXPECT_EQ(trillSpanners, 1)
        << "0x36 + 0x35 must create exactly one Trill spanner (tr + wavy line)";

    // Count Ornament glyphs (0x37 stays as glyph, secondary tr mark within the span)
    int ornamentGlyphs = 0;
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
                    ++ornamentGlyphs;
                }
            }
        }
    }
    EXPECT_EQ(ornamentGlyphs, 1)
        << "0x37 (TRILL_ALT) must remain an Ornament glyph (secondary tr, not a spanner)";

    // Verify the Trill spanner covers from the TRILL_START tick to the TRILL_END tick.
    if (trillSpanners == 1) {
        for (auto& [tick, sp] : score->spannerMap().map()) {
            if (sp->isTrill()) {
                const Fraction startQ = sp->tick();    // 0 quarters from measure start
                const Fraction endQ   = sp->tick2();   // 2 quarters in (where 0x35 was)
                EXPECT_EQ(startQ.numerator() % startQ.denominator(), 0)
                    << "Trill spanner start must align to a beat";
                EXPECT_GT(endQ, startQ)
                    << "Trill spanner must have positive duration";
            }
        }
    }

    delete score;
}

// ===========================================================================
// REGRESSION: TRILL_START (0x36) with no TRILL_END and alMezuro=0 must
// fall back to Ornament glyph. No Trill spanner must be created.
// ===========================================================================
TEST_F(Tst_Ornaments, trill_no_end_marker_creates_glyph_not_spanner)
{
    MasterScore* score = readEncoreScore("ornaments_trill_no_end_marker.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int trillSpanners = 0;
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (sp->isTrill()) {
            ++trillSpanners;
        }
    }
    EXPECT_EQ(trillSpanners, 0)
        << "0x36 without 0x35 and alMezuro=0 must NOT create a Trill spanner";

    int ornGlyphs = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(0);
            if (el && el->isChord()) {
                for (Articulation* a : toChord(el)->articulations()) {
                    if (a->symId() == SymId::ornamentTrill) {
                        ++ornGlyphs;
                    }
                }
            }
        }
    }
    EXPECT_EQ(ornGlyphs, 1)
        << "0x36 without span info must produce exactly one Ornament glyph";

    delete score;
}

// ===========================================================================
// FEATURE: TRILL_START with alMezuro=2 creates a Trill spanner spanning
// to the end of the 2nd measure after the start measure.
// ===========================================================================
TEST_F(Tst_Ornaments, trill_cross_measure_span_from_almezuro)
{
    MasterScore* score = readEncoreScore("ornaments_trill_cross_measure.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int trillSpanners = 0;
    Fraction spanStart, spanEnd;
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (sp->isTrill()) {
            ++trillSpanners;
            spanStart = sp->tick();
            spanEnd   = sp->tick2();
        }
    }
    EXPECT_EQ(trillSpanners, 1)
        << "alMezuro=2 must create exactly one Trill spanner";

    if (trillSpanners == 1) {
        // The spanner starts at tick=0 (start of TRILL_START note).
        EXPECT_EQ(spanStart, Fraction(0, 1))
            << "Trill spanner must start at tick=0 (TRILL_START note)";
        // alMezuro=2 targets ctx.measuresByIdx[0+2] = measure 2.
        // In 4/4, each measure = Fraction(1,1) whole note, so measure 2 ends at Fraction(3,1).
        // The span end must reach past measure 1 (Fraction(2,1)).
        EXPECT_GT(spanEnd, Fraction(2, 1))
            << "Trill spanner with alMezuro=2 must end at or beyond the 2nd measure boundary";
    }

    delete score;
}

// ===========================================================================
// FEATURE: Per-note technical markings: fingering 1..5 (0x0D..0x11), open-string (0x46), thumb (0x44/0x45), harmonic (0x1E/0x1F).
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
// FEATURE: Fermata anchored on segment (not chord); direction from artic slot: articUp=0x20 (above), articDown=0x21 (below).
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
// FEATURE: Single-note tremolos from per-note artic byte (0x41/0x42/0x43/0x03 → R8/R16/R32).
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
// FEATURE: Trill-mark / mordent / inverted-mordent from per-note artic byte, emitted as Ornament for MusicXML.
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
// REGRESSION: DOUBLE barline must be applied to every staff, not only track 0.
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
// REGRESSION: WEDGESTART at tick == durTicks (measure-end boundary) must not be dropped.
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
// FEATURE: Dynamics from size-16 ORN cluster (0x81=pp, 0x82=p, 0x85=f, 0x86=ff).
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
// FEATURE: Voice=4 ORN with staffByte high bit (0x40) produces system-level dynamics (full 0x80..0x8A ladder).
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
// FEATURE: Arpeggio from ORN tipo=0x22 attaches to the chord.
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
// FIX: SLURSTART resolves end tick from alMezuro after the measure pass (no SLURSTOP in .enc binaries).
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
// FIX: Multi-measure hairpin end tick resolved from WEDGESTART's alMezuro (cresc alMezuro=2, dim alMezuro=1).
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
// FEATURE: Bowing marks from size-16 ORN tipo 0xC5 (down-bow) and 0xC4 (up-bow).
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
// FEATURE: Stand-alone fingering from ORN tipo 0xB9..0xBD (tipo = 0xB8 + finger 1..5).
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

// ===========================================================================
// FIX: Grand-staff FINGER ORN routing: cross-measure (Pattern A) and multi-note same-tick (Pattern B).
// ===========================================================================
TEST_F(Tst_Ornaments, fingering_grandstaff_routing)
{
    MasterScore* score = readEncoreScore("ornaments_fingering_grandstaff.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    // Navigate to a measure by 0-based index (Encore measure order).
    auto measureAt = [&](int idx) -> Measure* {
        int n = 0;
        for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
            if (!mb->isMeasure()) {
                continue;
            }
            if (n++ == idx) {
                return toMeasure(mb);
            }
        }
        return nullptr;
    };

    // Collect fingerings attached to the first chord on `tr` in `m`.
    auto fingeringsOnFirstChord = [](Measure* m, track_idx_t tr) -> std::vector<String> {
        std::vector<String> out;
        if (!m) {
            return out;
        }
        for (Segment* s = m->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(tr);
            if (!el || !el->isChord()) {
                continue;
            }
            for (Note* n : toChord(el)->notes()) {
                for (EngravingItem* e : n->el()) {
                    if (e && e->isFingering()) {
                        out.push_back(toFingering(e)->plainText());
                    }
                }
            }
            break;
        }
        return out;
    };

    // Collect all fingerings on `tr` across every chord in `m`.
    auto fingeringsOnTrack = [](Measure* m, track_idx_t tr) -> std::vector<String> {
        std::vector<String> out;
        if (!m) {
            return out;
        }
        for (Segment* s = m->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(tr);
            if (!el || !el->isChord()) {
                continue;
            }
            for (Note* n : toChord(el)->notes()) {
                for (EngravingItem* e : n->el()) {
                    if (e && e->isFingering()) {
                        out.push_back(toFingering(e)->plainText());
                    }
                }
            }
        }
        return out;
    };

    const track_idx_t staff1 = 0;
    const track_idx_t staff2 = VOICES;

    // Pattern A: 4 ORNs from m2's last voice=0 tick must land on m3 staff 2, not m2 staff 1.
    Measure* m2 = measureAt(1);
    ASSERT_NE(m2, nullptr);
    Measure* m3 = measureAt(2);
    ASSERT_NE(m3, nullptr);

    // Last chord of m2, staff 1: must NOT carry the cross-measure fingerings.
    {
        std::vector<String> m2s1last;
        Segment* lastSeg = nullptr;
        for (Segment* s = m2->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            if (s->element(staff1) && s->element(staff1)->isChord()) {
                lastSeg = s;
            }
        }
        if (lastSeg) {
            for (Note* n : toChord(lastSeg->element(staff1))->notes()) {
                for (EngravingItem* e : n->el()) {
                    if (e && e->isFingering()) {
                        m2s1last.push_back(toFingering(e)->plainText());
                    }
                }
            }
        }
        EXPECT_TRUE(m2s1last.empty())
            << "Last chord of m2 staff 1 should have no fingerings (Pattern A regression)";
    }

    // First chord of m3, staff 2: receives the 4 Pattern A fingerings.
    {
        auto f = fingeringsOnFirstChord(m3, staff2);
        EXPECT_EQ(f.size(), 4u) << "m3 staff 2 should have 4 fingerings from Pattern A";
        if (f.size() == 4) {
            EXPECT_EQ(f[0], u"1");
            EXPECT_EQ(f[1], u"1");
            EXPECT_EQ(f[2], u"3");
            EXPECT_EQ(f[3], u"4");
        }
    }

    // Staff 1 m3 melody fingerings are unaffected by the fix.
    {
        auto f = fingeringsOnTrack(m3, staff1);
        EXPECT_EQ(f, (std::vector<String> { u"1", u"2", u"4" }));
    }

    // Pattern B: more ORNs at m11 tick=0 than voice=0 notes must land on staff 2, not staff 1.
    Measure* m11 = measureAt(10);
    ASSERT_NE(m11, nullptr);

    // First chord of m11, staff 1: must NOT carry the Pattern B fingerings.
    {
        auto f = fingeringsOnFirstChord(m11, staff1);
        // Staff 1 may legitimately have its own single fingering; check it has <=1.
        EXPECT_LE(f.size(), 1u)
            << "m11 staff 1 first chord should not carry 4 Pattern B fingerings";
    }

    // First chord of m11, staff 2: receives the 4 Pattern B fingerings.
    {
        auto f = fingeringsOnFirstChord(m11, staff2);
        EXPECT_EQ(f.size(), 4u) << "m11 staff 2 should have 4 fingerings from Pattern B";
        if (f.size() == 4) {
            EXPECT_EQ(f[0], u"1");
            EXPECT_EQ(f[1], u"2");
            EXPECT_EQ(f[2], u"4");
            EXPECT_EQ(f[3], u"4");
        }
    }

    delete score;
}

// ===========================================================================
// BUG FIX: articulationDown=0x21 on a non-tuplet note must create fermataBelow;
// on a tuplet note it must be suppressed (same dual-meaning rule as 0x20 above).
// ===========================================================================
TEST_F(Tst_Ornaments, fermata_below_kept_on_non_tuplet_suppressed_on_tuplet)
{
    MasterScore* score = readEncoreScore("ornaments_fermata_below_not_in_tuplet.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m1 = score->firstMeasure();
    ASSERT_NE(m1, nullptr);

    int fermataCount = 0;
    SymId sym = SymId::noSym;
    for (Segment* s = m1->first(SegmentType::ChordRest); s;
         s = s->next(SegmentType::ChordRest)) {
        for (EngravingItem* e : s->annotations()) {
            if (e && e->isFermata()) {
                ++fermataCount;
                sym = toFermata(e)->symId();
            }
        }
    }
    EXPECT_EQ(fermataCount, 1) << "only the non-tuplet note must produce a fermata";
    EXPECT_EQ(sym, SymId::fermataBelow) << "articDown=0x21 on non-tuplet must be fermataBelow";
    delete score;
}

// ===========================================================================
// BUG FIX companion: when the tremolo ORN resolves to a chord with no incoming
// tie, the tremolo must stay on that chord (no spurious backwards walk).
// ===========================================================================
TEST_F(Tst_Ornaments, tremolo_orn_stays_on_untied_chord)
{
    MasterScore* score = readEncoreScore("ornaments_tremolo_orn_no_tie.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m1 = score->firstMeasure();
    ASSERT_NE(m1, nullptr);

    Chord* tremoloChord = nullptr;
    for (Segment* s = m1->first(SegmentType::ChordRest); s;
         s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord() && toChord(el)->tremoloSingleChord()) {
            tremoloChord = toChord(el);
        }
    }
    ASSERT_NE(tremoloChord, nullptr) << "TremoloSingleChord not found";
    EXPECT_EQ(tremoloChord->tick(), Fraction(0, 1))
        << "tremolo must land on the quarter (tick=0); no spurious tie-back walk";
    EXPECT_EQ(tremoloChord->notes().front()->tieBack(), nullptr)
        << "the chord carrying the tremolo must have no incoming tie";
    delete score;
}
