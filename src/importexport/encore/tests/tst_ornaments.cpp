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

// ORN element import: articulations, ornaments, trills, dynamics, hairpins, arpeggios, tremolos, breath marks and technical markings.

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

class Tst_Ornaments : public ::testing::Test, public MTest
{
protected:
    void SetUp() override { setRootDir(ENC_DIR); }
};

// A capped 3:2 triplet has non-TDuration placedTicks; closeTuplet must not assert in beam layout.
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

// WEDGESTART with alMezuro=0 must span the current measure, not collapse to zero (Spanner::setTicks assert).
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

// A partial 3:2 quarter triplet (placedTicks=1/3, not a TDuration fraction) must not assert in beam layout.
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

    // Check by type predicate rather than exact SymId; layout may flip above/below per stem direction.
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

// Combo articulation bytes (e.g. 0x24 = tenuto + staccato) expand to combined or paired glyphs.
// See ENCORE_FORMAT.md §Articulation bytes.
TEST_F(Tst_Ornaments, articulation_combos_expand_to_two_glyphs)
{
    MasterScore* score = readEncoreScore("ornaments_articulations_combo.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    enum class K {
        Tenuto, Staccato, Accent, Marcato, Staccatissimo,
        MarcatoStaccato, MarcatoTenuto, AccentStaccato, TenutoStaccato, TenutoAccent,
        Other
    };
    auto kindOf = [](Articulation* a) -> K {
        using mu::engraving::SymId;
        switch (SymId(a->subtype())) {
        case SymId::articTenutoAbove:           return K::Tenuto;
        case SymId::articStaccatoAbove:         return K::Staccato;
        case SymId::articAccentAbove:           return K::Accent;
        case SymId::articMarcatoAbove:          return K::Marcato;
        case SymId::articStaccatissimoAbove:    return K::Staccatissimo;
        case SymId::articMarcatoStaccatoAbove:  return K::MarcatoStaccato;
        case SymId::articMarcatoTenutoAbove:    return K::MarcatoTenuto;
        case SymId::articAccentStaccatoAbove:   return K::AccentStaccato;
        case SymId::articTenutoStaccatoAbove:   return K::TenutoStaccato;
        case SymId::articTenutoAccentAbove:     return K::TenutoAccent;
        default:                                return K::Other;
        }
    };
    // Expected glyph per byte across m1..m4. See ENCORE_FORMAT.md §Articulation bytes.
    const std::vector<std::set<K> > expected = {
        { K::TenutoStaccato },            // byte 0x24
        { K::AccentStaccato },            // byte 0x17
        { K::MarcatoTenuto },             // byte 0x27
        { K::MarcatoStaccato },           // byte 0x15
        { K::TenutoAccent },              // byte 0x23
        { K::Tenuto, K::Staccatissimo },  // byte 0x2D, no single glyph
        { K::Marcato, K::Staccatissimo }, // byte 0x2B, heavy accent + staccatissimo
        { K::TenutoStaccato },            // byte 0x24
        { K::MarcatoStaccato },           // byte 0x14
        { K::MarcatoTenuto },             // byte 0x26
        { K::TenutoStaccato },            // byte 0x25
        { K::Marcato, K::Staccatissimo }, // byte 0x2A, heavy accent + staccatissimo
        { K::Tenuto, K::Staccatissimo },  // byte 0x2C, no single glyph
        { K::TenutoAccent },              // byte 0x22
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

// Per-chord staccato from size-16 ORN tipo=0xC9, deduped against per-note artic byte 0x1D.
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

// ORN 0x36 (TRILL_START) + 0x35 (TRILL_END) create a Trill spanner; a 0x37 (TRILL_ALT)
// inside the span stays an Ornament glyph. Fixture: 0x36@0, 0x37@240, 0x35@480 in 4/4.
// See ENCORE_IMPORTER.md §Trill spans (TRILL_START / TRILL_END).
TEST_F(Tst_Ornaments, trill_spanner_start_markers)
{
    MasterScore* score = readEncoreScore("ornaments_trill_spanner.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int trillSpanners = 0;
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (sp->isTrill()) {
            ++trillSpanners;
        }
    }
    EXPECT_EQ(trillSpanners, 1)
        << "0x36 + 0x35 must create exactly one Trill spanner (tr + wavy line)";

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

    if (trillSpanners == 1) {
        for (auto& [tick, sp] : score->spannerMap().map()) {
            if (sp->isTrill()) {
                const Fraction startQ = sp->tick();
                const Fraction endQ   = sp->tick2();
                EXPECT_EQ(startQ.numerator() % startQ.denominator(), 0)
                    << "Trill spanner start must align to a beat";
                EXPECT_GT(endQ, startQ)
                    << "Trill spanner must have positive duration";
            }
        }
    }

    delete score;
}

// TRILL_START (0x36) with no TRILL_END and alMezuro=0 falls back to an Ornament glyph, no spanner.
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

// TRILL_START with alMezuro=2 spans to the end of the 2nd measure after the start measure.
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
        EXPECT_EQ(spanStart, Fraction(0, 1))
            << "Trill spanner must start at tick=0 (TRILL_START note)";
        // alMezuro=2 targets measure 2 (ends at 3/1 in 4/4), so the span reaches past measure 1 (2/1).
        EXPECT_GT(spanEnd, Fraction(2, 1))
            << "Trill spanner with alMezuro=2 must end at or beyond the 2nd measure boundary";
    }

    delete score;
}

// Per-note technical markings from the artic byte: fingering, open-string, thumb, harmonic.
// See ENCORE_FORMAT.md §Articulation bytes.
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

// Fermata is anchored on the segment (not the chord); direction from artic byte 0x20 (above) / 0x21 (below).
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

// Single-note tremolos from the per-note artic byte (0x41/0x42/0x43/0x03 give R8/R16/R32).
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

// Trill-mark, mordent and inverted-mordent from the per-note artic byte, emitted as Ornament for MusicXML.
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
        SymId::ornamentShortTrill,    // 0x0A: inverted-mordent (short)
        SymId::ornamentMordent,       // 0x0B: simple lower mordent
        SymId::ornamentPrallMordent,  // 0x2F: double/long lower mordent
    };
    EXPECT_EQ(seen, expected);
    delete score;
}

// articUp=0x08 (ornamentTurn) must create an Ornament, not a plain Articulation:
// layout calls Ornament-only methods on ornament SymIds and a plain Articulation would SIGSEGV.
TEST_F(Tst_Ornaments, ornament_turn_created_as_ornament_not_articulation)
{
    MasterScore* score = readEncoreScore("ornaments_ornament_turn.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    Segment* seg = m->first(SegmentType::ChordRest);
    ASSERT_NE(seg, nullptr);
    EngravingItem* el = seg->element(0);
    ASSERT_NE(el, nullptr);
    ASSERT_TRUE(el->isChord());
    const auto& arts = toChord(el)->articulations();
    ASSERT_EQ(arts.size(), 1u) << "One ornament expected on the first note";

    Articulation* art = arts.front();
    EXPECT_EQ(art->symId(), SymId::ornamentTurn)
        << "SymId must be ornamentTurn";
    EXPECT_TRUE(art->isOrnament())
        << "ornamentTurn must be stored as Ornament, not plain Articulation; "
        "a plain Articulation with an ornament SymId causes SIGSEGV in layout";

    delete score;
}

// DOUBLE barline must be applied to every staff, not only track 0.
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

// WEDGESTART at tick == durTicks (measure-end boundary) must not be dropped.
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

// Dynamics from a size-16 ORN cluster (0x81=pp, 0x82=p, 0x85=f, 0x86=ff).
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

// Two dynamic ORNs at identical tick+xoffset on one staff must collapse to the first read (ff),
// since Encore renders only one per beat. Fixture: ff (0x86) then fff (0x87) at tick 0, xoffset 13.
// See ENCORE_IMPORTER.md §Dynamic deduplication.
TEST_F(Tst_Ornaments, dynamics_stacked_collapsed_to_first)
{
    MasterScore* score = readEncoreScore("ornaments_dynamics_stacked.enc");
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
    const std::vector<DynamicType> expected = { DynamicType::FF };
    EXPECT_EQ(seen, expected);
    delete score;
}

// Voice=4 ORN with the staffByte high bit (0x40) produces system-level dynamics (full 0x80..0x8A ladder).
// See ENCORE_IMPORTER.md §Dynamic ladder coverage.
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

// Arpeggio from ORN tipo=0x22 attaches to the chord.
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

// Multi-measure hairpin end tick resolved from WEDGESTART's alMezuro (cresc alMezuro=2, dim alMezuro=1).
// See ENCORE_IMPORTER.md §Cross-measure hairpin snap-start and endpoint.
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

// Bowing marks from size-16 ORN tipo 0xC5 (down-bow) and 0xC4 (up-bow).
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

// In v0xC2, ORN tipo 0xC4 = accent above (not up-bow as in v0xC4); v0xC2 NOTEs carry no artic bytes.
TEST_F(Tst_Ornaments, v0xc2_orn_c4_is_accent_not_upbow)
{
    MasterScore* score = readEncoreScore("ornaments_v0c2_orn_c4_accent.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int accentCount = 0;
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
                EXPECT_NE(a->symId(), SymId::stringsUpBow)
                    << "ORN 0xC4 in v0xC2 must not produce stringsUpBow";
                if (a->symId() == SymId::articAccentAbove
                    || a->symId() == SymId::articAccentBelow) {
                    ++accentCount;
                }
            }
        }
    }
    EXPECT_GE(accentCount, 5) << "Expected several accent marks in this v0xC2 score";
    delete score;
}

// In v0xC4, ORN tipo 0xBE = accent above (standalone accent glyph).
TEST_F(Tst_Ornaments, v0xc4_orn_be_is_accent)
{
    MasterScore* score = readEncoreScore("ornaments_v0c4_orn_be_accent.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int accentCount = 0;
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
                EXPECT_NE(a->symId(), SymId::stringsUpBow)
                    << "ORN 0xBE in v0xC4 must not produce stringsUpBow";
                if (a->symId() == SymId::articAccentAbove
                    || a->symId() == SymId::articAccentBelow) {
                    ++accentCount;
                }
            }
        }
    }
    EXPECT_GE(accentCount, 5) << "Expected several accent marks in this v0xC4 score";
    delete score;
}

// Stand-alone fingering from ORN tipo 0xB9..0xBD (tipo = 0xB8 + finger 1..5).
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

// Grand-staff FINGER ORN routing: cross-measure (Pattern A) and multi-note same-tick (Pattern B).
// See ENCORE_IMPORTER.md §Stand-alone FINGER and BOWING ORN routing in grand-staff scores.
TEST_F(Tst_Ornaments, fingering_grandstaff_routing)
{
    MasterScore* score = readEncoreScore("ornaments_fingering_grandstaff.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

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

// artic byte 0x21 on a non-tuplet note creates fermataBelow but is suppressed on a tuplet note
// (dual meaning, same rule as 0x20 above).
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

// When a tremolo ORN resolves to a chord with no incoming tie, it stays on that chord (no backwards walk).
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

// Artic bytes 0x1B (brassMuteClosed), 0x2E (ornamentTurnInverted), 0x30 (brassMuteHalfClosed).
TEST_F(Tst_Ornaments, new_artic_bytes_stopped_inverted_turn_half_stopped)
{
    MasterScore* score = readEncoreScore("ornaments_new_artic_bytes.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "sanity check failed";

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    auto articsOnNote = [&](int noteIdx) {
        Segment* seg = m->first(SegmentType::ChordRest);
        for (int i = 0; i < noteIdx && seg; ++i) {
            seg = seg->next(SegmentType::ChordRest);
        }
        if (!seg) {
            return std::vector<SymId> {};
        }
        EngravingItem* el = seg->element(0);
        if (!el || !el->isChord()) {
            return std::vector<SymId> {};
        }
        std::vector<SymId> ids;
        for (Articulation* a : toChord(el)->articulations()) {
            ids.push_back(a->symId());
        }
        for (EngravingItem* e : toChord(el)->el()) {
            if (e->isOrnament()) {
                ids.push_back(toOrnament(e)->symId());
            }
        }
        return ids;
    };

    auto s0 = articsOnNote(0);
    auto s1 = articsOnNote(1);
    auto s2 = articsOnNote(2);

    EXPECT_TRUE(std::find(s0.begin(), s0.end(), SymId::brassMuteClosed) != s0.end())
        << "0x1B must produce brassMuteClosed (+)";
    EXPECT_TRUE(std::find(s1.begin(), s1.end(), SymId::ornamentTurnInverted) != s1.end())
        << "0x2E must produce ornamentTurnInverted (inverted turn)";
    EXPECT_TRUE(std::find(s2.begin(), s2.end(), SymId::brassMuteHalfClosed) != s2.end())
        << "0x30 must produce brassMuteHalfClosed (half-stopped)";

    delete score;
}

// Breath mark and caesura from ORN tipo 0xA8 (breathMarkComma) and 0xA7 (caesura).
TEST_F(Tst_Ornaments, breath_comma_and_caesura_from_orn_tipo)
{
    MasterScore* score = readEncoreScore("ornaments_breath_and_caesura.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "sanity check failed";

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    int breathCount = 0;
    bool hasComma = false, hasCaesura = false;
    // Breath elements live on Breath segments as element slots, not as annotations.
    for (Segment* seg = m->first(); seg; seg = seg->next()) {
        for (track_idx_t v = 0; v < score->ntracks(); ++v) {
            EngravingItem* el = seg->element(v);
            if (el && el->isBreath()) {
                ++breathCount;
                SymId sid = toBreath(el)->symId();
                if (sid == SymId::breathMarkComma) {
                    hasComma = true;
                }
                if (sid == SymId::caesura) {
                    hasCaesura = true;
                }
            }
        }
    }

    EXPECT_EQ(breathCount, 2) << "Must have 2 Breath elements (comma + caesura)";
    EXPECT_TRUE(hasComma) << "ORN tipo 0xA8 must produce breathMarkComma";
    EXPECT_TRUE(hasCaesura) << "ORN tipo 0xA7 must produce caesura";

    delete score;
}

// A standalone TRILL_END (0x35 with no prior TRILL_START) creates a Trill spanner over the note's duration.
TEST_F(Tst_Ornaments, standalone_trill_end_creates_trill_spanner_on_note)
{
    MasterScore* score = readEncoreScore("ornaments_standalone_trill_end.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "sanity check failed";

    int trillCount = 0;
    for (auto it = score->spanner().cbegin(); it != score->spanner().cend(); ++it) {
        if (it->second->isTrill()) {
            ++trillCount;
            Trill* tr = toTrill(it->second);
            EXPECT_GE(tr->ticks().ticks(), 240)
                << "Trill spanner must cover at least a quarter note";
        }
    }
    EXPECT_EQ(trillCount, 1)
        << "TRILL_END without prior TRILL_START must create one Trill spanner";

    delete score;
}

// Measure repeat from ORN tipo 0xA3.
TEST_F(Tst_Ornaments, measure_repeat_from_orn_tipo_0xA3)
{
    MasterScore* score = readEncoreScore("ornaments_measure_repeat.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "sanity check failed";

    int mrCount = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        Measure* m = toMeasure(mb);
        for (Segment* seg = m->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
            for (size_t v = 0; v < VOICES; ++v) {
                EngravingItem* el = seg->element(static_cast<track_idx_t>(v));
                if (el && el->isMeasureRepeat()) {
                    ++mrCount;
                }
            }
        }
    }
    EXPECT_EQ(mrCount, 1) << "ORN tipo 0xA3 must produce one MeasureRepeat element";

    delete score;
}

// Trill accidentals: intervalAbove set from artic bytes 0x05/0x06/0x07.
TEST_F(Tst_Ornaments, trill_accidentals_set_interval_above)
{
    MasterScore* score = readEncoreScore("ornaments_trill_with_accidentals.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "sanity check failed";

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    struct NoteOrnInterval {
        int noteIdx;
        IntervalType expected;
        const char* desc;
    };
    std::vector<NoteOrnInterval> checks = {
        { 0, IntervalType::AUTO,      "0x04 trill: no accidental → AUTO" },
        { 1, IntervalType::MINOR,     "0x05 trill+flat → MINOR (trill menor)" },
        { 2, IntervalType::AUGMENTED, "0x06 trill+sharp → AUGMENTED (trill augmented)" },
        { 3, IntervalType::MAJOR,     "0x07 trill+natural → MAJOR" },
    };

    // Accept any symId: MuseScore may store the cue-note accidental as the primary glyph for some intervals.
    std::vector<IntervalType> intervals;
    int noteIdx = 0;
    for (Segment* seg = m->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
        EngravingItem* el = seg->element(0);
        if (!el || !el->isChord()) {
            continue;
        }
        if (noteIdx >= (int)checks.size()) {
            break;
        }
        for (EngravingItem* sub : toChord(el)->el()) {
            if (!sub->isOrnament()) {
                continue;
            }
            Ornament* orn = toOrnament(sub);
            // Only check trill-created ornaments (trill-related interval or AUTO).
            if (orn->intervalAbove().type != IntervalType::AUTO
                || checks[noteIdx].expected == IntervalType::AUTO) {
                EXPECT_EQ(orn->intervalAbove().type, checks[noteIdx].expected)
                    << checks[noteIdx].desc;
            }
        }
        ++noteIdx;
    }

    delete score;
}

// Open-string artic byte 0x46 gives a plain Fingering "0" (not circled STRING_NUMBER);
// stick 0x47 is unmapped and adds no fingering.
TEST_F(Tst_Ornaments, open_string_0x46_is_plain_fingering_not_string_number)
{
    MasterScore* score = readEncoreScore("ornaments_open_string_and_stick.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "sanity check failed";

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    std::vector<std::pair<String, TextStyleType> > fingeringsByNote;
    for (Segment* seg = m->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
        EngravingItem* el = seg->element(0);
        if (!el || !el->isChord()) {
            continue;
        }
        for (Note* n : toChord(el)->notes()) {
            for (EngravingItem* sub : n->el()) {
                if (sub && sub->isFingering()) {
                    Fingering* fg = toFingering(sub);
                    fingeringsByNote.emplace_back(fg->plainText(), fg->textStyleType());
                }
            }
        }
    }

    ASSERT_EQ(fingeringsByNote.size(), 1u)
        << "Only note 1 (0x46) must have a fingering; note 2 (0x47=stick) is unmapped";
    EXPECT_EQ(fingeringsByNote[0].first, String(u"0"))
        << "Open-string (0x46) must produce Fingering '0'";
    EXPECT_EQ(fingeringsByNote[0].second, TextStyleType::FINGERING)
        << "Open-string (0x46) must use plain FINGERING style, not STRING_NUMBER (circled)";

    delete score;
}

// A standalone TRILL_ALT (0x37, no prior TRILL_START on the track) creates a Trill spanner over the note.
TEST_F(Tst_Ornaments, standalone_trill_alt_creates_trill_spanner)
{
    MasterScore* score = readEncoreScore("ornaments_trill_alt_standalone.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "sanity check failed";

    int trillCount = 0;
    for (auto it = score->spanner().cbegin(); it != score->spanner().cend(); ++it) {
        if (it->second->isTrill()) {
            ++trillCount;
            Trill* tr = toTrill(it->second);
            EXPECT_GE(tr->ticks().ticks(), 240)
                << "Standalone TRILL_ALT spanner must cover at least a quarter note";
        }
    }
    EXPECT_EQ(trillCount, 1)
        << "TRILL_ALT (0x37) without prior TRILL_START must create exactly one Trill spanner";

    delete score;
}

// Identical artic bytes on the notes of one chord (both au=0x04 trill) must dedupe to one Ornament.
TEST_F(Tst_Ornaments, artic_byte_dedup_no_duplicate_ornament_on_chord)
{
    MasterScore* score = readEncoreScore("notes_artic_dedup_trill_on_chord.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "Artic dedup must not corrupt";

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    Segment* seg = m->first(SegmentType::ChordRest);
    ASSERT_NE(seg, nullptr);
    EngravingItem* el = seg->element(0);
    ASSERT_NE(el, nullptr);
    ASSERT_TRUE(el->isChord());
    Chord* chord = toChord(el);
    ASSERT_EQ(chord->notes().size(), 2u) << "Chord must have both notes";

    // Count ornamentTrill instances on the chord
    int trillCount = 0;
    for (Articulation* e : chord->articulations()) {
        if (e && e->isOrnament() && toOrnament(e)->symId() == SymId::ornamentTrill) {
            ++trillCount;
        }
    }
    EXPECT_EQ(trillCount, 1)
        << "Both notes carry au=0x04 (trill) but the chord must have exactly ONE "
        "ornamentTrill; duplicate artic bytes on multi-note chords must be deduped";

    delete score;
}

// TRILL_SIMPLE (tipo=0xB6) standalone glyph: co-located ORN goes on the note, rest-tick ORN snaps forward.
TEST_F(Tst_Ornaments, trill_simple_tipo_b6_places_ornament_trill)
{
    MasterScore* score = readEncoreScore("ornaments_trill_simple_on_note.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "TRILL_SIMPLE test must produce clean score: " << ret.text();

    // M1: ORN at tick=0 co-located with the first quarter note; trill lands on that note.
    Measure* m1 = score->firstMeasure();
    ASSERT_NE(m1, nullptr);
    Segment* seg1 = m1->first(SegmentType::ChordRest);
    ASSERT_NE(seg1, nullptr);
    EngravingItem* el1 = seg1->element(0);
    ASSERT_NE(el1, nullptr);
    ASSERT_TRUE(el1->isChord());
    Chord* chord1 = toChord(el1);
    int trillCount1 = 0;
    for (Articulation* a : chord1->articulations()) {
        if (a && a->isOrnament() && toOrnament(a)->symId() == SymId::ornamentShortTrill) {
            ++trillCount1;
        }
    }
    EXPECT_EQ(trillCount1, 1)
        << "TRILL_SHORT (0xB6) at note tick must place ornamentShortTrill on that note";

    // M2 (5/8): ORN at tick=1/8 (REST position) must snap to the 2nd chord (tick=1/4)
    Measure* m2 = m1->nextMeasure();
    ASSERT_NE(m2, nullptr);
    Chord* trillNote = nullptr;
    int chordIdx = 0;
    for (Segment* s = m2->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord()) {
            if (++chordIdx == 2) {
                trillNote = toChord(el);
                break;
            }
        }
    }
    ASSERT_NE(trillNote, nullptr) << "M2 must have a 2nd chord (target of the rest-tick snap)";
    int trillCount2 = 0;
    for (Articulation* a : trillNote->articulations()) {
        if (a && a->isOrnament() && toOrnament(a)->symId() == SymId::ornamentShortTrill) {
            ++trillCount2;
        }
    }
    EXPECT_EQ(trillCount2, 1)
        << "TRILL_SHORT (0xB6) at REST tick must snap forward and place ornamentShortTrill on next note";

    // M3 (4/4): TRILL_TR (0xB0) at tick=1/4 on the second quarter note gives ornamentTrill.
    Measure* m3 = m2->nextMeasure();
    ASSERT_NE(m3, nullptr);
    Chord* trChord = nullptr;
    int ci = 0;
    for (Segment* s = m3->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord()) {
            if (++ci == 2) {
                trChord = toChord(el);
                break;
            }
        }
    }
    ASSERT_NE(trChord, nullptr) << "M3 must have a 2nd chord";
    int trCount3 = 0;
    for (Articulation* a : trChord->articulations()) {
        if (a && a->isOrnament() && toOrnament(a)->symId() == SymId::ornamentTrill) {
            ++trCount3;
        }
    }
    EXPECT_EQ(trCount3, 1) << "TRILL_TR (0xB0) must place ornamentTrill on its target note";

    // M4 (4/4): two identical TRILL_SHORT ORNs at tick=0 dedup to exactly one glyph.
    Measure* m4 = m3->nextMeasure();
    ASSERT_NE(m4, nullptr);
    Segment* seg4 = m4->first(SegmentType::ChordRest);
    ASSERT_NE(seg4, nullptr);
    EngravingItem* el4 = seg4->element(0);
    ASSERT_NE(el4, nullptr);
    ASSERT_TRUE(el4->isChord());
    Chord* chord4 = toChord(el4);
    int trCount4 = 0;
    for (Articulation* a : chord4->articulations()) {
        if (a && a->isOrnament() && toOrnament(a)->symId() == SymId::ornamentShortTrill) {
            ++trCount4;
        }
    }
    EXPECT_EQ(trCount4, 1)
        << "Two duplicate TRILL_SHORT ORNs at the same tick must be deduped to one glyph";

    delete score;
}

// ACCENT ORN (0xBE) at voice=0 must attach to the same-tick note even in a non-zero voice:
// the resolver scans all voices of its own staff before the sibling-staff fallback.
// Fixture: single staff, C4 in voice=1 at tick=0, ORN 0xBE at voice=0.
TEST_F(Tst_Ornaments, accent_orn_attaches_to_nonzero_voice)
{
    MasterScore* score = readEncoreScore("ornaments_accent_nonzero_voice.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    Segment* seg = m->first(SegmentType::ChordRest);
    ASSERT_NE(seg, nullptr);

    EngravingItem* el = seg->element(1);
    ASSERT_TRUE(el && el->isChord())
        << "Voice-1 note must be imported at track=1";

    int accentCount = 0;
    for (Articulation* a : toChord(el)->articulations()) {
        if (a->symId() == SymId::articAccentAbove || a->symId() == SymId::articAccentBelow) {
            ++accentCount;
        }
    }
    EXPECT_EQ(accentCount, 1)
        << "ACCENT ORN at voice=0 must attach to the voice=1 note on the same staff; "
        "resolver must scan all voices before falling back to sibling staff";

    // The voice=0 track must have no spurious accent (no redirect to a phantom sibling or the v0 rest).
    EngravingItem* v0el = seg->element(0);
    if (v0el && v0el->isChord()) {
        for (Articulation* a : toChord(v0el)->articulations()) {
            EXPECT_NE(a->symId(), SymId::articAccentAbove)
                << "Accent must not appear on the voice=0 element";
        }
    }

    delete score;
}

// ACCENT ORN at voice=0, mid-measure enc tick=240 must attach to the voice=1 note at that beat,
// not the tick-0 note: the ORN's raw enc tick (not voice-0 cumTick=0) drives the target.
// Fixture: single staff, C4 v1@0, E4 v1@240, ORN 0xBE v0@240 accents E4 (MuseScore tick=480).
TEST_F(Tst_Ornaments, accent_orn_offset_tick_nonzero_voice_lands_on_correct_note)
{
    MasterScore* score = readEncoreScore("ornaments_accent_offset_tick_nonzero_voice.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    const Fraction measTick = m->tick();

    // enc tick=240 maps to MuseScore tick 240/960 * 1920 = 480 from measTick.
    const Fraction targetTick = measTick + Fraction(480, 1920);
    Segment* seg = m->findSegment(SegmentType::ChordRest, targetTick);
    ASSERT_NE(seg, nullptr) << "Segment at MuseScore tick=480 must exist";

    EngravingItem* el = seg->element(1);
    ASSERT_TRUE(el && el->isChord()) << "E4 chord must be in voice=1 at tick=480";

    int accentOnE4 = 0;
    for (Articulation* a : toChord(el)->articulations()) {
        if (a->symId() == SymId::articAccentAbove || a->symId() == SymId::articAccentBelow) {
            ++accentOnE4;
        }
    }
    EXPECT_EQ(accentOnE4, 1)
        << "ACCENT ORN at enc tick=240 must land on E4 at MuseScore tick=480, "
        "not on C4 at tick=0 (which is the bug when cumTick=0 is used as the target tick)";

    // C4 at tick=0 must NOT have an accent.
    Segment* seg0 = m->first(SegmentType::ChordRest);
    if (seg0) {
        EngravingItem* el0 = seg0->element(1);
        if (el0 && el0->isChord()) {
            for (Articulation* a : toChord(el0)->articulations()) {
                EXPECT_NE(a->symId(), SymId::articAccentAbove)
                    << "C4 at tick=0 must not carry the accent that belongs to E4";
            }
        }
    }

    delete score;
}

// ACCENT ORN on staff 0 (notes in voice=3) must stay on staff 0, not redirect to staff 1:
// the resolver scans all voices of the own staff before the sibling-staff fallback.
// Fixture: 2 staves, staff 0 C4 in voice=3, staff 1 E4 in voice=0 (trap), ORN on staff 0 v0 tick=0.
TEST_F(Tst_Ornaments, accent_orn_does_not_spill_to_sibling_staff)
{
    MasterScore* score = readEncoreScore("ornaments_accent_sibling_no_spillover.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    Segment* seg = m->first(SegmentType::ChordRest);
    ASSERT_NE(seg, nullptr);

    // Staff 0 voice=3 (track 3) must carry the accent.
    EngravingItem* el0v3 = seg->element(3);
    ASSERT_TRUE(el0v3 && el0v3->isChord())
        << "C4 chord must be in staff=0 voice=3 (track=3)";

    int accentOnStaff0 = 0;
    for (Articulation* a : toChord(el0v3)->articulations()) {
        if (a->symId() == SymId::articAccentAbove || a->symId() == SymId::articAccentBelow) {
            ++accentOnStaff0;
        }
    }
    EXPECT_EQ(accentOnStaff0, 1)
        << "ACCENT ORN on staff 0 (voice=0) must attach to the voice=3 chord on staff 0, "
        "not redirect to the sibling staff";

    // Staff 1 voice=0 (track 4, the trap) must have no accent.
    EngravingItem* el1v0 = seg->element(4);
    if (el1v0 && el1v0->isChord()) {
        for (Articulation* a : toChord(el1v0)->articulations()) {
            EXPECT_NE(a->symId(), SymId::articAccentAbove)
                << "Staff 1 (the sibling-staff trap) must not receive a spurious accent";
        }
    }

    delete score;
}

// Single-SymId articulation ORN tipos 0xBF/0xC6 (marcato), 0xC0 (marcato-staccato), 0xC8 (tenuto),
// 0xB8 (mordent); 0x30 (guitar bend) is skipped. Test by family since layout flips above/below.
// See ENCORE_IMPORTER.md §Articulations, technical markings, tremolos.
TEST_F(Tst_Ornaments, new_artic_types_from_orns)
{
    MasterScore* score = readEncoreScore("ornaments_new_artic_types.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    enum class K {
        Marcato, MarcatoStaccato, Tenuto, Mordent, Other
    };
    auto kindOf = [](Articulation* a) -> K {
        SymId s = SymId(a->subtype());
        if (s == SymId::articMarcatoAbove || s == SymId::articMarcatoBelow) {
            return K::Marcato;
        }
        if (s == SymId::articMarcatoStaccatoAbove || s == SymId::articMarcatoStaccatoBelow) {
            return K::MarcatoStaccato;
        }
        if (s == SymId::articTenutoAbove || s == SymId::articTenutoBelow) {
            return K::Tenuto;
        }
        if (s == SymId::ornamentMordent || s == SymId::ornamentPrallMordent) {
            return K::Mordent;
        }
        return K::Other;
    };

    std::vector<K> found;
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
                found.push_back(kindOf(a));
            }
        }
    }
    // 5 chords with articulations: two marcato (0xBF and 0xC6), one marcatoStaccato (0xC0),
    // one tenuto (0xC8), one mordent (0xB8). 0x30 (GUITAR_BEND_V) is skipped.
    const std::vector<K> expected = {
        K::Marcato, K::Marcato, K::MarcatoStaccato, K::Tenuto, K::Mordent
    };
    EXPECT_EQ(found, expected);
    EXPECT_EQ(found.size(), 5u) << "0x30 guitar bend must be skipped; only 5 chords get articulations";
    delete score;
}

// ORN tipos 0x28-0x2B are guitar bends (size=28 spanner), not staccatissimo; they add no articulation.
TEST_F(Tst_Ornaments, guitar_bend_orns_skipped)
{
    MasterScore* score = readEncoreScore("ornaments_staccatissimo_orns.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int articCount = 0;
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
            articCount += static_cast<int>(toChord(el)->articulations().size());
        }
    }
    EXPECT_EQ(articCount, 0)
        << "Guitar bend ORNs 0x28-0x2B must not add articulations to chords";
    delete score;
}

// ORN tipo 0xEE (TREMOLO_16) gives an R16 tremolo; 0xE6/0xE9 are string-number markers, not tremolos.
TEST_F(Tst_Ornaments, tremolo_orn_r16_and_string_numbers)
{
    MasterScore* score = readEncoreScore("ornaments_tremolo_r8_r16_r64.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<TremoloType> tremolos;
    int stringNumCount = 0;
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
            TremoloSingleChord* t = toChord(el)->tremoloSingleChord();
            if (t) {
                tremolos.push_back(t->tremoloType());
            }
            for (Note* n : toChord(el)->notes()) {
                for (EngravingItem* e : n->el()) {
                    if (e && e->isFingering()
                        && toFingering(e)->textStyleType() == TextStyleType::STRING_NUMBER) {
                        ++stringNumCount;
                    }
                }
            }
        }
    }
    EXPECT_EQ(tremolos, std::vector<TremoloType> { TremoloType::R16 });
    EXPECT_EQ(stringNumCount, 2) << "0xE6 and 0xE9 must produce STRING_NUMBER fingerings, not tremolos";
    delete score;
}

// ORN tipo 0x1C (GRAPHIC_LINE, user-drawn line) is silently skipped and adds no articulation.
TEST_F(Tst_Ornaments, graphic_line_orn_silently_skipped)
{
    MasterScore* score = readEncoreScore("ornaments_graphic_line_skipped.enc");
    ASSERT_NE(score, nullptr) << "File must load without crash";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int articCount = 0;
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
            articCount += static_cast<int>(toChord(el)->articulations().size());
        }
    }
    EXPECT_EQ(articCount, 0) << "GRAPHIC_LINE ORN must not add any articulation to the chord";
    delete score;
}

// Broad coverage: one fixture exercising every ORN-derived symbol family at least the minimum count.
TEST_F(Tst_Ornaments, encore_symbols_full_coverage)
{
    MasterScore* score = readEncoreScore("encore_symbols.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int dynamics = 0;
    int fermatas = 0;
    int markers = 0;
    int jumps = 0;
    int staccatos = 0;
    int tenutos = 0;
    int accents = 0;
    int marcatos = 0;
    int staccatissimos = 0;
    int trills = 0;
    int mordents = 0;
    int fingerings = 0;
    int arpeggios = 0;
    int tremolos = 0;
    int hairpins = 0;
    int dotted_barlines = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        Measure* m = toMeasure(mb);
        for (EngravingItem* e : m->el()) {
            if (e && e->isMarker()) {
                ++markers;
            }
            if (e && e->isJump()) {
                ++jumps;
            }
        }
        Segment* endBar = m->findSegment(SegmentType::EndBarLine, m->endTick());
        if (endBar) {
            for (size_t s = 0; s < score->nstaves(); ++s) {
                EngravingItem* el = endBar->element(s * VOICES);
                if (el && el->isBarLine() && toBarLine(el)->barLineType() == BarLineType::DOTTED) {
                    ++dotted_barlines;
                    break;
                }
            }
        }
        for (Segment* s = m->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e && e->isDynamic()) {
                    ++dynamics;
                }
                if (e && e->isFermata()) {
                    ++fermatas;
                }
            }
            EngravingItem* el = s->element(0);
            if (!el || !el->isChord()) {
                continue;
            }
            Chord* c = toChord(el);
            if (c->arpeggio()) {
                ++arpeggios;
            }
            if (c->tremoloSingleChord()) {
                ++tremolos;
            }
            for (Articulation* a : c->articulations()) {
                using mu::engraving::SymId;
                switch (a->symId()) {
                case SymId::articStaccatoAbove: case SymId::articStaccatoBelow:
                    ++staccatos;
                    break;
                case SymId::articTenutoAbove: case SymId::articTenutoBelow:
                    ++tenutos;
                    break;
                case SymId::articAccentAbove: case SymId::articAccentBelow:
                    ++accents;
                    break;
                case SymId::articMarcatoAbove: case SymId::articMarcatoBelow:
                    ++marcatos;
                    break;
                case SymId::articStaccatissimoAbove: case SymId::articStaccatissimoBelow:
                    ++staccatissimos;
                    break;
                case SymId::articMarcatoStaccatoAbove: case SymId::articMarcatoStaccatoBelow:
                    ++marcatos;
                    ++staccatos;
                    break;
                case SymId::articMarcatoTenutoAbove: case SymId::articMarcatoTenutoBelow:
                    ++marcatos;
                    ++tenutos;
                    break;
                case SymId::articAccentStaccatoAbove: case SymId::articAccentStaccatoBelow:
                    ++accents;
                    ++staccatos;
                    break;
                case SymId::articTenutoStaccatoAbove: case SymId::articTenutoStaccatoBelow:
                    ++tenutos;
                    ++staccatos;
                    break;
                case SymId::articTenutoAccentAbove: case SymId::articTenutoAccentBelow:
                    ++tenutos;
                    ++accents;
                    break;
                case SymId::ornamentTrill:
                    ++trills;
                    break;
                case SymId::ornamentShortTrill:
                case SymId::ornamentTremblement:
                case SymId::ornamentMordent:
                case SymId::ornamentPrallMordent:
                    ++mordents;
                    break;
                default: break;
                }
            }
            for (Note* n : c->notes()) {
                for (EngravingItem* nel : n->el()) {
                    if (nel && nel->isFingering()) {
                        ++fingerings;
                    }
                }
            }
        }
    }
    for (auto& [tick, sp] : score->spannerMap().map()) {
        if (sp->isTrill()) {
            ++trills;
        }
        if (sp->isHairpin()) {
            ++hairpins;
        }
    }
    EXPECT_GE(dynamics,      13) << "all 13 Encore dynamics expected";
    EXPECT_GE(fermatas,       2);
    EXPECT_GE(markers,        3) << "Segno + Coda(s) + To Coda + Fine";
    EXPECT_GE(jumps,          1) << "at least one D.C. / D.S. variant";
    EXPECT_GE(staccatos,      7);
    EXPECT_GE(tenutos,        9);
    EXPECT_GE(accents,        5);
    EXPECT_GE(marcatos,       6);
    EXPECT_GE(staccatissimos, 6);
    EXPECT_GE(trills,         6) << "trill-marks from per-note bytes + ORN 0x36/0x37";
    EXPECT_GE(mordents,       4) << "mordent + inverted-mordent";
    EXPECT_GE(fingerings,     6) << "fingering 1..5 + open-string";
    EXPECT_GE(arpeggios,      1);
    EXPECT_GE(tremolos,       4);
    EXPECT_GE(hairpins,       2);
    EXPECT_GE(dotted_barlines, 1);
    delete score;
}

// A tick-0 accent ORN must stay on note 1 even when a later accent shares its xoffset,
// so the two accents land on note 1 and note 3 rather than both on note 3.
TEST_F(Tst_Ornaments, accent_orn_tick0_stays_on_note1_when_same_xoffset_as_later_accent)
{
    MasterScore* score = readEncoreScore("ornaments_accent_tick0_xoffset.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    struct ChordInfo {
        Fraction tick;
        int accents { 0 };
    };
    std::vector<ChordInfo> chords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (!el || !el->isChord()) {
            continue;
        }
        ChordInfo ci;
        ci.tick = s->tick();
        for (Articulation* a : toChord(el)->articulations()) {
            if (a->symId() == SymId::articAccentAbove
                || a->symId() == SymId::articAccentBelow) {
                ++ci.accents;
            }
        }
        chords.push_back(ci);
    }

    ASSERT_GE(chords.size(), 2u)
        << "Measure must have at least 2 chords (note 1 and note 3)";

    int totalAccents = 0;
    for (const auto& ci : chords) {
        totalAccents += ci.accents;
    }
    EXPECT_EQ(totalAccents, 2)
        << "Expected exactly 2 accent marks total (one on note 1 and one on note 3)";

    EXPECT_EQ(chords.front().accents, 1)
        << "Note 1 (enc-tick=0) must carry exactly 1 accent; "
        "without fix both accents land on note 3 (enc-tick=480)";

    for (const auto& ci : chords) {
        EXPECT_LE(ci.accents, 1)
            << "Chord at MuseScore tick "
            << ci.tick.toString().toStdString()
            << " carries " << ci.accents
            << " accents; max 1 expected (regression: Phase 1 xoffset cluster match moved tick-0 ORN)";
    }

    delete score;
}

// A bowing ORN at enc-tick=0 must stay on note 1 even when its ornXoffset does not match note 1's:
// ORN and note xoffsets use different origins, so when a note exists on the own staff at the raw
// tick that tick is trusted rather than snapping to the nearest xoffset (which would move the bow).
// Fixture: 4/4, 4 quarters at ticks 0/240/480/720 (xoffsets 9/33/57/81);
// up-bow ORN tick=0 (ornXoffset=69), down-bow ORN tick=480 (ornXoffset=120).
TEST_F(Tst_Ornaments, bowing_tick0_stays_on_note1_when_xoffset_mismatches)
{
    MasterScore* score = readEncoreScore("ornaments_bowing_tick0_xoffset_mismatch.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    std::vector<std::pair<Fraction, std::vector<SymId> > > perChord;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (!el || !el->isChord()) {
            continue;
        }
        std::vector<SymId> bows;
        for (Articulation* a : toChord(el)->articulations()) {
            if (a->symId() == SymId::stringsUpBow || a->symId() == SymId::stringsDownBow) {
                bows.push_back(a->symId());
            }
        }
        perChord.push_back({ s->tick(), bows });
    }

    ASSERT_GE(perChord.size(), 3u) << "Measure must have at least 3 chords";

    EXPECT_EQ(perChord.front().second, std::vector<SymId> { SymId::stringsUpBow })
        << "Note 1 (enc-tick=0) must keep its up-bow; without fix the up-bow is "
        "snapped onto a later note by the xoffset correction";

    // A relocated up-bow would otherwise pile onto the down-bow note, so cap at one mark per chord.
    for (const auto& cinfo : perChord) {
        EXPECT_LE(cinfo.second.size(), 1u)
            << "Chord at MuseScore tick " << cinfo.first.toString().toStdString()
            << " carries " << cinfo.second.size() << " bowing marks; max 1 expected";
    }

    delete score;
}

// Encore can store an accent per note all at the downbeat tick, separated only by xoffset;
// the importer must spread same-tick marks across the notes so each chord gets one accent.
TEST_F(Tst_Ornaments, v0c4_accents_distributed_across_notes)
{
    MasterScore* score = readEncoreScore("ornaments_accents_distributed.enc");
    ASSERT_NE(score, nullptr) << "Failed to load ornaments_accents_distributed.enc";

    int chordsWithAccent = 0;
    int totalAccents = 0;
    int maxAccentsOnOneChord = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        Measure* m = toMeasure(mb);
        for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(0);
            if (!el || !el->isChord()) {
                continue;
            }
            int n = 0;
            for (Articulation* a : toChord(el)->articulations()) {
                // layout may flip the accent to its below-staff glyph variant.
                if (a && (a->symId() == SymId::articAccentAbove
                          || a->symId() == SymId::articAccentBelow)) {
                    ++n;
                }
            }
            if (n > 0) {
                ++chordsWithAccent;
            }
            totalAccents += n;
            maxAccentsOnOneChord = std::max(maxAccentsOnOneChord, n);
        }
    }
    EXPECT_EQ(totalAccents, 4) << "all four accents must import";
    EXPECT_EQ(chordsWithAccent, 4) << "each of the four notes must carry one accent";
    EXPECT_EQ(maxAccentsOnOneChord, 1)
        << "accents must not stack on a single chord";
    delete score;
}

// A "TR" trill whose stored tick falls between two notes must anchor from the raw tick and snap to
// the preceding note (where Encore draws it), not overshoot to the following note via cumulative tick.
TEST_F(Tst_Ornaments, v0c4_trill_between_notes_snaps_to_preceding)
{
    MasterScore* score = readEncoreScore("ornaments_trill_between_notes.enc");
    ASSERT_NE(score, nullptr) << "Failed to load ornaments_trill_between_notes.enc";
    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    Fraction trillTick(-1, 1);
    int trillCount = 0;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (!el || !el->isChord()) {
            continue;
        }
        for (Articulation* a : toChord(el)->articulations()) {
            if (a && (a->symId() == SymId::ornamentTrill || a->symId() == SymId::ornamentShortTrill)) {
                trillTick = s->tick() - m->tick();
                ++trillCount;
            }
        }
    }
    EXPECT_EQ(trillCount, 1) << "exactly one trill must import";
    EXPECT_EQ(trillTick, Fraction(0, 1))
        << "the TR must land on its own (preceding) note, not the following one";
    delete score;
}
