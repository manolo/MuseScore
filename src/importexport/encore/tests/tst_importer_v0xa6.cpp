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

#include "engraving/dom/barline.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/marker.h"
#include "engraving/dom/part.h"
#include "engraving/dom/stafftext.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/volta.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftype.h"

#include "testbase.h"

static const QString ENC_DIR(QString(iex_encore_tests_DATA_ROOT) + "/data/");

using namespace mu::engraving;

class Tst_ImporterV0xa6 : public ::testing::Test, public MTest
{
protected:
    void SetUp() override
    {
        setRootDir(ENC_DIR);
    }
};

// End-to-end regression for the full v0xA6 fix chain: 4 instruments, Key bytes, pitch at byte +11,
// tuplet at byte +7, duplicate REST collapse, triplet groups, no spurious glyphs.
TEST_F(Tst_ImporterV0xa6, v0xa6_boda_like_full_pipeline)
{
    MasterScore* score = readEncoreScore("importer_v0xa6_boda_like.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_v0xa6_boda_like.enc";

    ASSERT_EQ(score->nstaves(), 4u) << "fixture has 4 instruments";

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    auto staffPitches = [m](int staffIdx) {
        std::vector<int> out;
        const track_idx_t base = staffIdx * VOICES;
        for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(base);
            if (el && el->isChord()) {
                for (Note* n : toChord(el)->notes()) {
                    out.push_back(n->pitch());
                }
            }
        }
        return out;
    };
    auto staffElementCount = [m](int staffIdx) {
        int count = 0;
        const track_idx_t base = staffIdx * VOICES;
        for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            if (s->element(base)) {
                ++count;
            }
        }
        return count;
    };
    auto staffTupletGroups = [m](int staffIdx) {
        std::set<const Tuplet*> seen;
        const track_idx_t base = staffIdx * VOICES;
        for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(base);
            if (el && el->isChord()) {
                if (const Tuplet* t = toChord(el)->tuplet()) {
                    seen.insert(t);
                }
            }
        }
        return seen.size();
    };

    // Staff 0 (B1, Key=0): eighth + 2 triplet groups, 7 notes total.
    EXPECT_EQ(staffTupletGroups(0), 2u) << "B1 must hold 2 triplet groups";
    EXPECT_EQ(staffPitches(0), (std::vector<int> { 88, 88, 89, 88, 86, 88, 86 }))
        << "B1 pitches survive without Key shift";

    // Staff 1 (B2, Key=0): rest + 2 eighths, no shift.
    EXPECT_EQ(staffElementCount(1), 3) << "B2 must hold rest + 2 chords";
    EXPECT_EQ(staffPitches(1), (std::vector<int> { 76, 77 }))
        << "B2 pitches survive without Key shift";

    // Staff 2 (Laud, Key=-12): duplicate REST collapsed, m_pitch = binary -12.
    EXPECT_EQ(staffElementCount(2), 3)
        << "Laud must hold exactly rest + 2 chords after duplicate-REST dedupe";
    EXPECT_EQ(staffPitches(2), (std::vector<int> { 76 - 12, 77 - 12 }))
        << "Laud pitches must drop by Key = -12";

    // Staff 3 (Bajo, Key=-12): 3 eighth notes shifted by -12.
    EXPECT_EQ(staffElementCount(3), 3) << "Bajo holds 3 notes";
    EXPECT_EQ(staffPitches(3), (std::vector<int> { 57 - 12, 60 - 12, 64 - 12 }))
        << "Bajo pitches must drop by Key = -12";

    // No staff should carry spillover into voice 1.
    int v1Count = 0;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        for (size_t st = 0; st < 4; ++st) {
            if (s->element(st * VOICES + 1)) {
                ++v1Count;
            }
        }
    }
    EXPECT_EQ(v1Count, 0) << "no v1 spillover after dedup + correct tuplet handling";

    // Spurious-glyph audit (artic byte zero-out + tuplet override).
    int tremCount = 0;
    int fingerCount = 0;
    int fermataCount = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* ann : s->annotations()) {
                if (ann && ann->isFermata()) {
                    ++fermataCount;
                }
            }
            for (size_t v = 0; v < score->nstaves() * VOICES; ++v) {
                EngravingItem* el = s->element(v);
                if (el && el->isChord()) {
                    Chord* c = toChord(el);
                    if (c->tremoloSingleChord() || c->tremoloTwoChord()) {
                        ++tremCount;
                    }
                    for (Note* nt : c->notes()) {
                        for (EngravingItem* sub : nt->el()) {
                            if (sub && sub->isFingering()) {
                                ++fingerCount;
                            }
                        }
                    }
                }
            }
        }
    }
    EXPECT_EQ(tremCount, 0) << "no spurious tremolo glyphs";
    EXPECT_EQ(fingerCount, 0) << "no spurious fingering glyphs";
    EXPECT_EQ(fermataCount, 0) << "no spurious fermata glyphs";

    delete score;
}

// Regression: v0xA6 NOTE stores the tuplet byte at offset +7 (not +13 as in v0xC4).
// Without the override the 0x32 (3:2) marker is invisible; 6 triplet sixteenths collapse to 4 plain notes.
TEST_F(Tst_ImporterV0xa6, v0xa6_triplet_byte_at_offset_7)
{
    MasterScore* score = readEncoreScore("importer_v0xa6_triplet_byte_at_offset_7.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_v0xa6_triplet_byte_at_offset_7.enc";

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    int tupletCount = 0;
    int noteCount = 0;
    std::vector<int> pitches;
    std::set<const Tuplet*> seenTuplets;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord()) {
            Chord* c = toChord(el);
            ++noteCount;
            for (Note* nt : c->notes()) {
                pitches.push_back(nt->pitch());
            }
            const Tuplet* t = c->tuplet();
            if (t && seenTuplets.insert(t).second) {
                ++tupletCount;
                EXPECT_EQ(t->ratio().numerator(), 3) << "triplet actualNotes";
                EXPECT_EQ(t->ratio().denominator(), 2) << "triplet normalNotes";
            }
        }
    }
    EXPECT_EQ(noteCount, 6) << "6 triplet sixteenths must survive import";
    EXPECT_EQ(tupletCount, 2) << "two 3:2 triplet groups expected";
    const std::vector<int> expected{ 64, 65, 64, 62, 64, 62 };
    EXPECT_EQ(pitches, expected) << "pitches must match the binary in order";

    // Voice 1 must be empty (no spillover).
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EXPECT_EQ(s->element(1), nullptr) << "voice 1 should stay empty";
    }
    delete score;
}

// Regression: v0xA6 can store two byte-identical REST elements at the same tick; Encore renders only one.
// Without dedupe the second REST advanced cumTick past the bar and spilled the next note into voice 1.
TEST_F(Tst_ImporterV0xa6, v0xa6_duplicate_rest_collapse)
{
    MasterScore* score = readEncoreScore("importer_v0xa6_duplicate_rest_collapse.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_v0xa6_duplicate_rest_collapse.enc";

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    std::vector<std::pair<Fraction, bool> > positions;   // (tick, isRest)
    std::vector<int> pitches;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (!el) {
            continue;
        }
        positions.emplace_back(s->tick() - m->tick(), el->isRest());
        if (el->isChord()) {
            pitches.push_back(toChord(el)->notes()[0]->pitch());
        }
    }
    ASSERT_EQ(positions.size(), 3u) << "measure must hold 3 elements after dedupe";
    EXPECT_TRUE(positions[0].second) << "beat 1: rest";
    EXPECT_EQ(positions[0].first, Fraction(0, 1));
    EXPECT_FALSE(positions[1].second) << "beat 2: chord";
    EXPECT_EQ(positions[1].first, Fraction(1, 8));
    EXPECT_FALSE(positions[2].second) << "beat 3: chord";
    EXPECT_EQ(positions[2].first, Fraction(2, 8));
    ASSERT_EQ(pitches.size(), 2u);
    EXPECT_EQ(pitches[0], 64);
    EXPECT_EQ(pitches[1], 64);

    // Voice 1 must be empty (no spillover).
    int v1Count = 0;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        if (s->element(1)) {
            ++v1Count;
        }
    }
    EXPECT_EQ(v1Count, 0) << "voice 1 must be empty after dedupe";
    delete score;
}

// Regression: v0xA6 header ends at 0xA6 (174 bytes), not 0xC2 (194). Old code skipped to 0xC2,
// swallowing TK00 at 0xA6 so Key=-12 was never applied; pitch stayed at binary 60 instead of 48.
TEST_F(Tst_ImporterV0xa6, v0xa6_header_ends_at_0xa6)
{
    MasterScore* score = readEncoreScore("importer_v0xa6_header_ends_at_0xa6.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_v0xa6_header_ends_at_0xa6.enc";

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    Chord* firstChord = nullptr;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord()) {
            firstChord = toChord(el);
            break;
        }
    }
    ASSERT_NE(firstChord, nullptr);
    ASSERT_EQ(firstChord->notes().size(), 1u);
    EXPECT_EQ(firstChord->notes()[0]->pitch(), 48)
        << "TK00 at the v0xA6 file offset 0xA6 must be parsed so its "
        "Key = -12 actually lowers C4 (60) to C3 (48)";
    delete score;
}

// Regression: two bugs in v0xA6 Key pipeline: (1) header skipped to 0xC2 shifting all TK metadata;
// (2) Key byte at TK+42 (not PRG_BASE+n*PRG_STEP). Both fixed: Key=-12 lowers C4(60) to C3(48).
TEST_F(Tst_ImporterV0xa6, v0xa6_key_transposition_octave_lower)
{
    MasterScore* score = readEncoreScore("importer_v0xa6_key_transposition.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_v0xa6_key_transposition.enc";

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    Chord* firstChord = nullptr;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord()) {
            firstChord = toChord(el);
            break;
        }
    }
    ASSERT_NE(firstChord, nullptr);
    ASSERT_EQ(firstChord->notes().size(), 1u);
    EXPECT_EQ(firstChord->notes()[0]->pitch(), 48)
        << "v0xA6 Key = -12 must lower C4 (60) to C3 (48)";
    delete score;
}

// Regression: v0xA6 NOTE is 10 bytes but EncNote::read consumed 27, reading into the next slot's preamble
// as articulation/fingering data, producing thousands of spurious glyphs. Fix: zero those fields when slot size < 27.
TEST_F(Tst_ImporterV0xa6, v0xa6_no_spurious_articulation_glyphs)
{
    MasterScore* score = readEncoreScore("importer_v0xa6_no_spurious_tremolo.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_v0xa6_no_spurious_tremolo.enc";

    int tremCount = 0;
    int fingerCount = 0;
    int articCount = 0;
    int fermataCount = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* ann : s->annotations()) {
                if (ann && ann->isFermata()) {
                    ++fermataCount;
                }
            }
            for (size_t v = 0; v < score->nstaves() * VOICES; ++v) {
                EngravingItem* el = s->element(v);
                if (el && el->isChord()) {
                    Chord* c = toChord(el);
                    if (c->tremoloSingleChord() || c->tremoloTwoChord()) {
                        ++tremCount;
                    }
                    articCount += static_cast<int>(c->articulations().size());
                    for (Note* n : c->notes()) {
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
    EXPECT_EQ(tremCount, 0)
        << "v0xA6 NOTEs do not carry tremolo data";
    EXPECT_EQ(fingerCount, 0)
        << "v0xA6 NOTEs do not carry fingering or open-string data";
    EXPECT_EQ(articCount, 0)
        << "v0xA6 NOTEs do not carry articulation glyphs";
    EXPECT_EQ(fermataCount, 0)
        << "v0xA6 NOTEs do not carry fermata data";
    delete score;
}

// Regression: after a grace note, regular notes at exact face-grid ticks triggered the gap snap (gap=stolenTicks),
// inserting spurious rests. Fix: suppress snap when gap <= stolenTicks.
TEST_F(Tst_ImporterV0xa6, v0xa6_grace_ongrid_snap_suppressed)
{
    MasterScore* score = readEncoreScore("importer_v0xa6_grace_ongrid_snap_suppressed.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_v0xa6_grace_ongrid_snap_suppressed.enc";

    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "sanityCheck failed: " << ret.text();

    Measure* m1 = score->firstMeasure();
    ASSERT_NE(m1, nullptr);

    bool hasSpuriousInterNoteRest = false;
    bool prevWasChord = false;
    int graceCount = 0;
    for (Segment* s = m1->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (!el) {
            continue;
        }
        if (el->isRest()) {
            // A rest immediately after a chord (and not the last element)
            // is suspicious -- it means the snap fired creating an inter-note gap.
            if (prevWasChord && s->next(SegmentType::ChordRest)) {
                hasSpuriousInterNoteRest = true;
            }
            prevWasChord = false;
        } else if (el->isChord()) {
            Chord* c = toChord(el);
            graceCount += static_cast<int>(c->graceNotes().size());
            prevWasChord = true;
        }
    }
    EXPECT_FALSE(hasSpuriousInterNoteRest)
        << "spurious rest between regular notes detected; "
        "stolenTicks snap suppression likely missing for post-grace notes on face grid";
    EXPECT_EQ(graceCount, 1) << "expected exactly 1 grace (leading 32nd)";
    delete score;
}

// Regression: inner grace (g1=0x10) shorter than the leader (g1=0x20) was treated as a regular note,
// producing a spurious rest before the grace group and causing a SIGSEGV in GUI layout.
TEST_F(Tst_ImporterV0xa6, v0xa6_inner_grace_group)
{
    MasterScore* score = readEncoreScore("importer_v0xa6_inner_grace_group.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_v0xa6_inner_grace_group.enc";

    // sanityCheck catches the corrupted structure that previously caused SIGSEGV.
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "sanityCheck failed (would crash in GUI): " << ret.text();

    Measure* m1 = score->firstMeasure();
    ASSERT_NE(m1, nullptr);

    int graceCount = 0;
    bool hasSpuriousPreGraceRest = false;
    bool prevWasChord = false;
    std::vector<DurationType> regularTypes;
    for (Segment* s = m1->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (!el) {
            continue;
        }
        if (el->isRest()) {
            // A rest immediately before a grace-note chord is spurious (snap fired while graces were pending).
            Segment* nx = s->next(SegmentType::ChordRest);
            if (nx) {
                EngravingItem* nxEl = nx->element(0);
                if (nxEl && nxEl->isChord() && !toChord(nxEl)->graceNotes().empty()) {
                    hasSpuriousPreGraceRest = true;
                }
            }
        } else if (el->isChord()) {
            Chord* c = toChord(el);
            for (Chord* gc : c->graceNotes()) {
                ++graceCount;
                (void)gc;
            }
            regularTypes.push_back(c->durationType().type());
            prevWasChord = true;
        }
        (void)prevWasChord;
    }
    // No rest immediately before the grace group (= the crash-inducing structure).
    EXPECT_FALSE(hasSpuriousPreGraceRest)
        << "Rest found immediately before grace-note chord; "
        "this corrupted structure crashes the MuseScore GUI layout";
    // Both leading (32nd) and inner (64th) graces must be recognised.
    EXPECT_EQ(graceCount, 2)
        << "expected 2 graces (32nd leader + 64th inner)";
    // Regular notes: 8th at start and 8th at end.
    ASSERT_GE(regularTypes.size(), 2u);
    EXPECT_EQ(regularTypes.front(), DurationType::V_EIGHTH);
    EXPECT_EQ(regularTypes.back(), DurationType::V_EIGHTH);
    delete score;
}

// Regression: v0xA6 grace notes shift subsequent real notes forward, leaving the last note with realDuration < face.
// calculateRealDurations detects the deficit and restores the face duration. Without fix: last 8th becomes 16th+rest.
TEST_F(Tst_ImporterV0xa6, v0xa6_grace_restores_face_value)
{
    MasterScore* score = readEncoreScore("importer_v0xa6_grace_restores_face_value.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_v0xa6_grace_restores_face_value.enc";

    Measure* m1 = score->firstMeasure();
    ASSERT_NE(m1, nullptr);

    std::vector<std::pair<DurationType, bool> > elements;
    for (Segment* s = m1->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (!el) {
            continue;
        }
        if (el->isRest()) {
            elements.push_back({ toRest(el)->durationType().type(), false });
        } else if (el->isChord()) {
            Chord* c = toChord(el);
            if (!c->graceNotes().empty()) {
                for (Chord* gc : c->graceNotes()) {
                    elements.push_back({ gc->durationType().type(), true });
                }
            }
            elements.push_back({ c->durationType().type(), false });
        }
    }
    // Expected: 8th (regular), 32nd (grace), 16th, 16th, 8th — no rests
    ASSERT_EQ(elements.size(), 5u)
        << "grace time-borrowing correction must restore the last 8th and "
        "eliminate spurious rests; got " << elements.size() << " elements";
    EXPECT_EQ(elements[0].second, false);
    EXPECT_EQ(elements[0].first, DurationType::V_EIGHTH);
    EXPECT_EQ(elements[1].second, true);
    EXPECT_EQ(elements[1].first, DurationType::V_32ND);
    EXPECT_EQ(elements[2].second, false);
    EXPECT_EQ(elements[2].first, DurationType::V_16TH);
    EXPECT_EQ(elements[3].second, false);
    EXPECT_EQ(elements[3].first, DurationType::V_16TH);
    EXPECT_EQ(elements[4].second, false);
    EXPECT_EQ(elements[4].first, DurationType::V_EIGHTH)
        << "last note must be an eighth (face value), not a 16th from rawGap=90";
    delete score;
}
