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

#include "testbase.h"

static const QString ENC_DIR(QString(iex_encore_tests_DATA_ROOT) + "/data/");

using namespace mu::engraving;

class Tst_Importer : public ::testing::Test, public MTest
{
protected:
    void SetUp() override
    {
        setRootDir(ENC_DIR);
    }
};

TEST_F(Tst_Importer, bazo)
{
    MasterScore* score = readEncoreScore("bazo.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Importer, akordo)
{
    MasterScore* score = readEncoreScore("akordo.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Importer, ripetoj)
{
    MasterScore* score = readEncoreScore("ripetoj.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Importer, opeco_vochoj)
{
    MasterScore* score = readEncoreScore("opeco_vochoj.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Importer, bando)
{
    MasterScore* score = readEncoreScore("bando.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Importer, kordorkestro)
{
    MasterScore* score = readEncoreScore("kordorkestro.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Importer, chord_parsing)
{
    MasterScore* score = readEncoreScore("chord_parsing.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Score has corrupted measures: " << ret.text();
    delete score;
}

TEST_F(Tst_Importer, encore_symbols)
{
    MasterScore* score = readEncoreScore("encore_symbols.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Score has corrupted measures: " << ret.text();
    delete score;
}

// No sanityCheck: swing timing files produce slight measure shortfalls (by design).
TEST_F(Tst_Importer, swing_timing)
{
    MasterScore* score = readEncoreScore("notes_swing.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

// Regression: grace chords attached to a Segment caused beam layout to assert in Chord::pagePos.
// readEncoreScore runs doLayout; crash-free load + structural invariant (grace in graceNotes(), segment chord NORMAL).
TEST_F(Tst_Importer, grace_with_beamed_eighths_no_layout_crash)
{
    MasterScore* score = readEncoreScore("importer_grace_beam.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_grace_beam.enc";
    EXPECT_GT(score->nmeasures(), 0);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();

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
                    }
                }
            }
        }
    }
    EXPECT_TRUE(foundGrace) << "Grace eighth should be attached as a graceNotes() child";
    delete score;
}

// Regression: single-switch voice overflow placed a note into a full voice (e.g. half rest),
// overrunning the measure. Fix: loop until finding a voice with remaining space.
TEST_F(Tst_Importer, multi_stream_switch_skips_voice_filled_by_rest)
{
    MasterScore* score = readEncoreScore("importer_full_voice_skipped.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_full_voice_skipped.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();
    delete score;
}

// Regression: gap snap fired on drift ticks, producing a zero-length rhythmic gap that aborted
// populateRhythmicList (strongestSubbeatLevelInRange assert). Fix: snap only when e->tick % faceTicks == 0.
TEST_F(Tst_Importer, v0c2_multi_stream_drift_imports_cleanly)
{
    MasterScore* score = readEncoreScore("importer_v0c2_multi_stream_drift.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_v0c2_multi_stream_drift.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();
    delete score;
}

// Regression: gap snap + inflated-rdur guard with Key=-12 (Octave Lower). Voice 0 has implicit leading silence
// (tick offsets, no REST); voice 1 has trailing silence; Key shifts all pitches by -12.
TEST_F(Tst_Importer, v0c4_octave_lower_implicit_silences)
{
    MasterScore* score = readEncoreScore("structure_octave_lower_implicit_silences.enc");
    ASSERT_NE(score, nullptr) << "Failed to load structure_octave_lower_implicit_silences.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    const track_idx_t baseTrack = 0;   // single staff, voice 0

    std::vector<Fraction> v0Ticks;
    std::vector<bool> v0IsRest;
    std::vector<int> v0Pitches;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(baseTrack);
        if (!el) {
            continue;
        }
        v0Ticks.push_back(s->tick() - m->tick());
        if (el->isRest()) {
            v0IsRest.push_back(true);
            v0Pitches.push_back(-1);
        } else if (el->isChord()) {
            v0IsRest.push_back(false);
            v0Pitches.push_back(toChord(el)->upNote()->pitch());
        }
    }
    ASSERT_EQ(v0Ticks.size(), 3u) << "voice 0 must contain rest + 2 notes";
    EXPECT_TRUE(v0IsRest[0]) << "beat 1 must be a rest, not a note";
    EXPECT_EQ(v0Ticks[0], Fraction(0, 1));
    EXPECT_FALSE(v0IsRest[1]);
    EXPECT_EQ(v0Ticks[1], Fraction(1, 4));
    EXPECT_EQ(v0Pitches[1], 73 - 12);   // binary 73 + Key (-12) = 61
    EXPECT_FALSE(v0IsRest[2]);
    EXPECT_EQ(v0Ticks[2], Fraction(2, 4));
    EXPECT_EQ(v0Pitches[2], 74 - 12);   // binary 74 + Key (-12) = 62

    Chord* v1Chord = nullptr;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(baseTrack + 1);
        if (el && el->isChord()) {
            v1Chord = toChord(el);
            break;
        }
    }
    ASSERT_NE(v1Chord, nullptr) << "encVoice=1 chord must land on MuseScore voice 2";
    EXPECT_EQ(v1Chord->durationType().type(), DurationType::V_QUARTER)
        << "face=quarter must win over rdur inflated to dotted-half ratio (720)";
    EXPECT_EQ(v1Chord->durationType().dots(), 0);
    ASSERT_EQ(v1Chord->notes().size(), 2u);
    std::set<int> pitches{ v1Chord->notes()[0]->pitch(), v1Chord->notes()[1]->pitch() };
    EXPECT_EQ(pitches, (std::set<int> { 64 - 12, 73 - 12 }));
    delete score;
}

// End-to-end regression for the full v0xA6 fix chain: 4 instruments, Key bytes, pitch at byte +11,
// tuplet at byte +7, duplicate REST collapse, triplet groups, no spurious glyphs.
TEST_F(Tst_Importer, v0xa6_boda_like_full_pipeline)
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
TEST_F(Tst_Importer, v0xa6_triplet_byte_at_offset_7)
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
TEST_F(Tst_Importer, v0xa6_duplicate_rest_collapse)
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
TEST_F(Tst_Importer, v0xa6_header_ends_at_0xa6)
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
TEST_F(Tst_Importer, v0xa6_key_transposition_octave_lower)
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
TEST_F(Tst_Importer, v0xa6_no_spurious_articulation_glyphs)
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

// Regression: options byte bit 0 and position field must NOT produce string numbers on plain
// piano/vocal notes. As muitas aguas is a grand-staff piece with opt=0x87/0x07 on all notes
// and au=ad=0x00; it must import with zero fingerings.
TEST_F(Tst_Importer, v0c4_no_spurious_string_numbers_from_options_byte)
{
    // Fixture: 4 notes with opt=0x87 or 0x07, au=ad=0x00, pos=3/6/13/5 — matches the
    // "As muitas aguas" piano piece pattern that was incorrectly getting circles.
    MasterScore* score = readEncoreScore("notes_no_spurious_string_numbers.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "sanity check failed";

    int fingerCount = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            for (size_t v = 0; v < score->nstaves() * VOICES; ++v) {
                EngravingItem* el = s->element(static_cast<track_idx_t>(v));
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
        << "Piano/grand-staff notes (instrCount=1, staffPerSystem=2) must not get circled string numbers. "
        "String numbers are only shown when instrCount == staffPerSystem (each instrument has one staff).";

    delete score;
}

// Regression: gap snap used denominator 4*beatTicks (correct only for x/4). x/8 beatTicks=120 gave
// half the correct whole-note value, overflowing by one beat. Fix: wholeTicks = beatTicks * timeSigDen.
TEST_F(Tst_Importer, v0c4_gap_snap_eighth_meter)
{
    MasterScore* score = readEncoreScore("importer_gap_snap_eighth_meter.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_gap_snap_eighth_meter.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    std::vector<Fraction> ticks;
    std::vector<bool> isRest;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (!el) {
            continue;
        }
        ticks.push_back(s->tick() - m->tick());
        isRest.push_back(el->isRest());
    }
    ASSERT_GE(ticks.size(), 3u);
    EXPECT_EQ(ticks[0], Fraction(0, 1));
    EXPECT_TRUE(isRest[0]) << "beat 1 must be a rest";
    EXPECT_EQ(ticks[1], Fraction(1, 8));
    EXPECT_TRUE(isRest[1]) << "beat 2 must be a rest";
    EXPECT_EQ(ticks[2], Fraction(2, 8));
    EXPECT_FALSE(isRest[2]) << "beat 3 must carry the chord";
    delete score;
}

// Regression: compact-TK files (varsize=112) don't follow PRG_BASE+n*PRG_STEP for Key bytes.
// Garbage at the formula offset would mis-shift pitches; fixture asserts pitch=76, not 76+8=84.
TEST_F(Tst_Importer, v0c4_compact_tk_ignores_key_byte)
{
    MasterScore* score = readEncoreScore("instruments_compact_tk_ignores_key_byte.enc");
    ASSERT_NE(score, nullptr) << "Failed to load instruments_compact_tk_ignores_key_byte.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();

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
    EXPECT_EQ(firstChord->notes()[0]->pitch(), 76)
        << "compact-TK file: garbage Key byte at formula offset must be "
        "ignored; imported pitch stays at binary 76 (not 76+8=84)";
    delete score;
}

// Regression: Encore can leave trailing stale MEAS blocks past the rendered count (header.measureCount at 0x34).
// Importing all of them produces ghost measures; fix stops at measureCount.
TEST_F(Tst_Importer, v0c4_header_measure_count_truncates_ghost_measures)
{
    MasterScore* score = readEncoreScore(
        "importer_header_measure_count_truncates_ghost_measures.enc");
    ASSERT_NE(score, nullptr)
        << "Failed to load importer_header_measure_count_truncates_ghost_measures.enc";
    int measureCount = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (mb->isMeasure()) {
            ++measureCount;
        }
    }
    EXPECT_EQ(measureCount, 2)
        << "ghost MEAS blocks past header.measureCount must be dropped";
    delete score;
}

// Regression: importer created one Volta per measure (3 voltas for 1/1/2 bits) and never set begin-text.
// Fix: coalesce equal-bitmask runs into one Volta and set begin-text from the endings list.
TEST_F(Tst_Importer, v0c4_volta_coalesce_and_numbered_text)
{
    MasterScore* score = readEncoreScore("importer_volta_coalesce_and_text.enc");
    ASSERT_NE(score, nullptr)
        << "Failed to load importer_volta_coalesce_and_text.enc";

    std::vector<Volta*> voltas;
    for (const auto& kv : score->spanner()) {
        Spanner* sp = kv.second;
        if (sp && sp->isVolta()) {
            voltas.push_back(toVolta(sp));
        }
    }
    std::sort(voltas.begin(), voltas.end(),
              [](Volta* a, Volta* b) { return a->tick() < b->tick(); });
    ASSERT_EQ(voltas.size(), 2u)
        << "consecutive measures with the same repeatAlternative bitmask "
        "must collapse into one Volta";
    EXPECT_EQ(voltas[0]->beginText(), String(u"1."))
        << "first ending must render the number '1.'";
    EXPECT_EQ(voltas[1]->beginText(), String(u"2."))
        << "second ending must render the number '2.'";
    // The first volta covers two measures (m1 + m2), the second covers one.
    EXPECT_GT(voltas[0]->tick2() - voltas[0]->tick(), Fraction(4, 4))
        << "first Volta must span both alt-1 measures, not just one";
    delete score;
}

// Regression: both coda bytes (0x85 CODA1 and 0x89 CODA2) mapped to CODA, losing the TOCODA distinction.
// Fix: 0x85 → TOCODA, 0x89 → CODA.
TEST_F(Tst_Importer, v0c4_to_coda_distinct_from_coda)
{
    MasterScore* score = readEncoreScore("importer_to_coda_vs_coda_marker.enc");
    ASSERT_NE(score, nullptr)
        << "Failed to load importer_to_coda_vs_coda_marker.enc";

    std::vector<MarkerType> orderedTypes;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (EngravingItem* el : toMeasure(mb)->el()) {
            if (el && el->isMarker()) {
                orderedTypes.push_back(toMarker(el)->markerType());
            }
        }
    }
    ASSERT_EQ(orderedTypes.size(), 2u)
        << "expected one marker per of the two coda-bearing measures";
    EXPECT_EQ(orderedTypes[0], MarkerType::TOCODA)
        << "CODA1 (0x85) must import as TOCODA, not CODA";
    EXPECT_EQ(orderedTypes[1], MarkerType::CODA)
        << "CODA2 (0x89) must import as CODA";
    delete score;
}

// Regression: TEXT block payloads in legacy Latin-1 files were decoded as UTF-16 LE, producing CJK gibberish.
// Probe byte 14/15 of each entry to detect encoding.
TEST_F(Tst_Importer, v0c4_text_block_latin1_decoding)
{
    MasterScore* score = readEncoreScore("text_text_block_latin1_decoding.enc");
    ASSERT_NE(score, nullptr)
        << "Failed to load text_text_block_latin1_decoding.enc";

    StaffText* found = nullptr;
    for (MeasureBase* mb = score->first(); mb && !found; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* ann : s->annotations()) {
                if (ann && ann->isStaffText()) {
                    found = toStaffText(ann);
                    break;
                }
            }
        }
    }
    ASSERT_NE(found, nullptr) << "expected at least one StaffText from a Latin-1 TEXT entry";
    EXPECT_EQ(found->plainText(), String(u"la 1ª vez"))
        << "Latin-1 TEXT payload must decode as readable text, not UTF-16 gibberish";
    delete score;
}

// Regression: ORNs at tick > durTicks (volta-grouped dynamics) were dropped.
// Fix: section-end DYN_* and STAFFTEXT pass the filter and clamp to the last ChordRest of the measure.
TEST_F(Tst_Importer, v0c4_two_dynamics_in_one_measure)
{
    MasterScore* score = readEncoreScore("importer_two_dynamics_in_one_measure.enc");
    ASSERT_NE(score, nullptr)
        << "Failed to load importer_two_dynamics_in_one_measure.enc";

    Measure* first = score->firstMeasure();
    ASSERT_NE(first, nullptr);
    std::vector<DynamicType> dynTypes;
    std::vector<String> textValues;
    for (Segment* s = first->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        for (EngravingItem* ann : s->annotations()) {
            if (!ann) {
                continue;
            }
            if (ann->isDynamic()) {
                dynTypes.push_back(toDynamic(ann)->dynamicType());
            } else if (ann->isStaffText()) {
                textValues.push_back(toStaffText(ann)->plainText());
            }
        }
    }
    ASSERT_EQ(dynTypes.size(), 2u)
        << "both the start-of-measure F and the end-of-measure PP must import";
    EXPECT_EQ(dynTypes[0], DynamicType::F);
    EXPECT_EQ(dynTypes[1], DynamicType::PP);
    ASSERT_EQ(textValues.size(), 2u)
        << "both volta-specific stafftext labels must import";
    EXPECT_EQ(textValues[0], String(u"la 1ª vez"));
    EXPECT_EQ(textValues[1], String(u"la 2ª"));
    delete score;
}

// Regression: CHORD symbol (type=7) text decoded unconditionally as UTF-16 LE.
// Latin-1 chord names ("Am") read as CJK gibberish; same per-element probe applied.
TEST_F(Tst_Importer, v0c4_chord_sym_latin1)
{
    MasterScore* score = readEncoreScore("text_chord_sym_latin1.enc");
    ASSERT_NE(score, nullptr) << "Failed to load text_chord_sym_latin1.enc";

    Harmony* found = nullptr;
    for (MeasureBase* mb = score->first(); mb && !found; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* ann : s->annotations()) {
                if (ann && ann->isHarmony()) {
                    found = toHarmony(ann);
                    break;
                }
            }
        }
    }
    ASSERT_NE(found, nullptr) << "expected one Harmony from the chord-symbol element";
    EXPECT_EQ(found->harmonyName(), String(u"Am"))
        << "Latin-1 chord text must decode as 'Am', not as UTF-16 gibberish";
    delete score;
}

// Regression: TITL encoding inherited TK00 charSize; files with large TK offset but Latin-1 TITL mis-decoded.
// Fix: use TITL's own varsize (< 5000 → ONE_BYTE, >= 10000 → TWO_BYTES).
TEST_F(Tst_Importer, v0c4_titl_latin1_small_varsize)
{
    MasterScore* score = readEncoreScore("text_titl_latin1_small_varsize.enc");
    ASSERT_NE(score, nullptr) << "Failed to load text_titl_latin1_small_varsize.enc";

    EXPECT_EQ(score->metaTag(u"workTitle"), String(u"Romeria"))
        << "small-varsize TITL must decode as Latin-1, not as TWO_BYTES UTF-16";
    delete score;
}

// Regression: formula-offset name recovery probed UTF-16 only; Latin-1 names were discarded silently,
// falling through to a generic GM template. Fix: byte-by-byte read when byte 1 != 0x00 && printable.
TEST_F(Tst_Importer, v0c4_recovered_name_latin1)
{
    MasterScore* score = readEncoreScore("text_recovered_name_latin1.enc");
    ASSERT_NE(score, nullptr) << "Failed to load text_recovered_name_latin1.enc";

    ASSERT_GE(score->parts().size(), 1u);
    const Part* part = score->parts()[0];
    ASSERT_NE(part, nullptr);
    EXPECT_EQ(part->partName(), String(u"Tropa"))
        << "Latin-1 name at NAME_BASE must be recovered when TK block name is empty";
    delete score;
}

// Regression: speguleco direction lives in bit 0; Encore 5 also sets bit 1 (0x02=cresc, 0x03=dim).
// Old `speguleco==0` check treated every Encore 5 hairpin as diminuendo.
TEST_F(Tst_Importer, v0c4_hairpin_speguleco_bit0)
{
    MasterScore* score = readEncoreScore("importer_hairpin_speguleco_bit0.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_hairpin_speguleco_bit0.enc";

    std::vector<HairpinType> seenTypes;
    for (const auto& kv : score->spanner()) {
        Spanner* sp = kv.second;
        if (sp && sp->isHairpin()) {
            seenTypes.push_back(toHairpin(sp)->hairpinType());
        }
    }
    ASSERT_EQ(seenTypes.size(), 2u);
    EXPECT_EQ(seenTypes[0], HairpinType::CRESC_HAIRPIN)
        << "speguleco=0x02 must import as crescendo";
    EXPECT_EQ(seenTypes[1], HairpinType::DIM_HAIRPIN)
        << "speguleco=0x03 must import as diminuendo";
    delete score;
}

// Regression: importer extended hairpins to the end of their alMezuro measure, overlapping adjacent hairpins.
// Fix: scan forward for the first Dynamic within the alMezuro window and stop there.
TEST_F(Tst_Importer, v0c4_hairpin_ends_at_next_dynamic)
{
    MasterScore* score = readEncoreScore("importer_hairpin_ends_at_next_dynamic.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_hairpin_ends_at_next_dynamic.enc";

    Hairpin* found = nullptr;
    for (const auto& kv : score->spanner()) {
        Spanner* sp = kv.second;
        if (sp && sp->isHairpin()) {
            found = toHairpin(sp);
            break;
        }
    }
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->tick(), Fraction(1, 4))
        << "hairpin must start at the WEDGESTART tick";
    EXPECT_EQ(found->tick2(), Fraction(3, 4))
        << "hairpin must end at the next Dynamic (tick=720, beat 4), "
        "not at the bar line of its alMezuro target measure";
    delete score;
}

// Regression: slur end was anchored on the last ChordRest of the alMezuro measure, covering all remaining notes.
// Fix: snap firstNote.xoffset + (xoffset2 - xoffset) to the closest note xoffset in the start measure.
TEST_F(Tst_Importer, v0c4_slur_pixel_span)
{
    MasterScore* score = readEncoreScore("importer_slur_pixel_span.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_slur_pixel_span.enc";

    Slur* found = nullptr;
    for (const auto& kv : score->spanner()) {
        Spanner* sp = kv.second;
        if (sp && sp->isSlur()) {
            found = toSlur(sp);
            break;
        }
    }
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->tick(), Fraction(0, 1))
        << "slur start at the SLURSTART tick (beat 1)";
    EXPECT_EQ(found->tick2(), Fraction(1, 2))
        << "slur end snaps to note 3 at tick=480 (target xoff 70 matches "
        "note xoff 70 exactly); not the last note of the measure";
    delete score;
}

// Regression: pixel-span heuristic in 6/8 (compound meter) used beatTicks×timeSigDen as whole-note
// ticks (giving 240×8=1920) instead of durTicks×timeSigDen/timeSigNum (=720×8/6=960).
// This caused startEncTick to be wrong, so firstNoteXoff was read from the wrong note and
// slurs ended too late.
TEST_F(Tst_Importer, v0c4_slur_pixel_span_6_8)
{
    // 6/8 measure: SLURSTART at enc_tick=120 (note 2, xoff=30), xoffset2=50.
    // pixelSpan=20. firstNoteXoff=30 (note 2). targetEndXoff=50 → note 3 at tick=240. ✓
    MasterScore* score = readEncoreScore("importer_slur_pixel_span_6_8.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_slur_pixel_span_6_8.enc";

    Slur* found = nullptr;
    for (const auto& kv : score->spanner()) {
        Spanner* sp = kv.second;
        if (sp && sp->isSlur()) {
            found = toSlur(sp);
            break;
        }
    }
    ASSERT_NE(found, nullptr) << "A slur must be created";
    // Start: note 2 at enc_tick=120 → measTick + 120/960 = measTick + 1/8
    EXPECT_EQ(found->tick(), Fraction(1, 8))
        << "slur start must be at the 2nd note (enc_tick=120 = 1/8 from measure start)";
    // End: note 3 at enc_tick=240 → measTick + 240/960 = measTick + 1/4
    EXPECT_EQ(found->tick2(), Fraction(1, 4))
        << "slur end must be at note 3 (enc_tick=240 = 1/4); "
        "with wrong formula it lands at note 4 (enc_tick=360 = 3/8)";
    delete score;
}

// Regression: SLURSTART xoffset > 127 must be treated as unsigned for pixel-span computation.
// qint8 sign-extension gives a huge spurious span; quint8 cast gives the correct 1-2 note span.
TEST_F(Tst_Importer, v0c4_slur_xoffset_unsigned)
{
    // xoffset=0x8A (138 unsigned, -118 signed). xoffset2=149. Note2 xoff=88.
    // Unsigned: span=11. target=88+11=99 → note3. Signed: span=267. target=355 → no match.
    MasterScore* score = readEncoreScore("importer_slur_xoffset_unsigned.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_slur_xoffset_unsigned.enc";

    Slur* found = nullptr;
    for (const auto& kv : score->spanner()) {
        Spanner* sp = kv.second;
        if (sp && sp->isSlur()) {
            found = toSlur(sp);
            break;
        }
    }
    ASSERT_NE(found, nullptr) << "A slur must be created";
    EXPECT_EQ(found->tick(), Fraction(1, 4))
        << "slur starts at note 2 (tick=240 = 1/4)";
    EXPECT_EQ(found->tick2(), Fraction(1, 2))
        << "slur ends at note 3 (tick=480 = 1/2); with signed xoffset it lands too late";
    delete score;
}

// Regression: pixel-span heuristic skips cross-measure slurs (alMezuro >= 1) because xoffsets reset at barlines.
// Pins the fallback: alMezuro=1 slur must anchor on the last ChordRest of the target measure.
TEST_F(Tst_Importer, v0c4_slur_cross_measure_fallback)
{
    MasterScore* score = readEncoreScore("importer_slur_cross_measure_fallback.enc");
    ASSERT_NE(score, nullptr)
        << "Failed to load importer_slur_cross_measure_fallback.enc";

    Slur* found = nullptr;
    for (const auto& kv : score->spanner()) {
        Spanner* sp = kv.second;
        if (sp && sp->isSlur()) {
            found = toSlur(sp);
            break;
        }
    }
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->tick(), Fraction(0, 1))
        << "slur start at the SLURSTART tick (m1 beat 1)";
    // m2.tick=1; last ChordRest = 4th quarter = 7/4.
    EXPECT_EQ(found->tick2(), Fraction(7, 4))
        << "cross-measure slur must fall back to the last ChordRest of "
        "the alMezuro target measure (m2 beat 4 = absolute tick 7/4)";
    delete score;
}

// Regression: ORN xoffset < tagged chord-rest xoffset means the glyph belongs to the preceding chord.
// Old importer planted the dynamic at the tagged tick; users saw it one chord later than in Encore.
TEST_F(Tst_Importer, v0c4_dyn_snap_back_by_xoffset)
{
    MasterScore* score = readEncoreScore("importer_dyn_snap_back_by_xoffset.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_dyn_snap_back_by_xoffset.enc";

    Measure* m1 = score->firstMeasure();
    ASSERT_NE(m1, nullptr);
    Dynamic* found = nullptr;
    Fraction foundTick;
    for (Segment* s = m1->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        for (EngravingItem* ann : s->annotations()) {
            if (ann && ann->isDynamic()) {
                found = toDynamic(ann);
                foundTick = s->tick();
                break;
            }
        }
        if (found) {
            break;
        }
    }
    ASSERT_NE(found, nullptr) << "expected one Dynamic in the measure";
    EXPECT_EQ(foundTick, Fraction(1, 8))
        << "dynamic must snap from the tagged eighth (tick 1/4) back to "
        "the previous eighth (tick 1/8) because its xoffset matches "
        "that note's region";
    delete score;
}

// Regression: same snap-back convention for WEDGESTART: xoffset falls inside the half note's region,
// so the hairpin anchors on the half note, not the tagged eighth.
TEST_F(Tst_Importer, v0c4_wedge_snap_back_by_xoffset)
{
    MasterScore* score = readEncoreScore("importer_wedge_snap_back_by_xoffset.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_wedge_snap_back_by_xoffset.enc";

    Hairpin* found = nullptr;
    for (const auto& kv : score->spanner()) {
        Spanner* sp = kv.second;
        if (sp && sp->isHairpin()) {
            found = toHairpin(sp);
            break;
        }
    }
    ASSERT_NE(found, nullptr) << "expected one Hairpin";
    EXPECT_EQ(found->tick(), Fraction(0, 1))
        << "hairpin must snap back from the tagged eighth (tick 1/2) to "
        "the start of the half note (tick 0) because its xoffset is "
        "less than the eighth's xoffset";
    delete score;
}

// Regression: hairpin xoffset2 < first-note xoffset in target measure means Encore ends at the bar line.
// Without the clamp the hairpin bleeds into the next measure and overlaps adjacent hairpins.
TEST_F(Tst_Importer, v0c4_hairpin_barline_clamp)
{
    MasterScore* score = readEncoreScore("importer_hairpin_barline_clamp.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_hairpin_barline_clamp.enc";

    std::vector<Hairpin*> hairpins;
    for (const auto& kv : score->spanner()) {
        Spanner* sp = kv.second;
        if (sp && sp->isHairpin()) {
            hairpins.push_back(toHairpin(sp));
        }
    }
    std::sort(hairpins.begin(), hairpins.end(),
              [](Hairpin* a, Hairpin* b) { return a->tick() < b->tick(); });
    ASSERT_GE(hairpins.size(), 2u);
    // dim from m1: must end at m2 start (= tick 1/1), not extend into m2.
    EXPECT_EQ(hairpins[0]->hairpinType(), HairpinType::DIM_HAIRPIN);
    EXPECT_EQ(hairpins[0]->tick2(), Fraction(1, 1))
        << "dim hairpin with xoffset2 < firstNoteXoff must end at bar line";
    // cresc inside m2: starts at some tick in m2.
    EXPECT_EQ(hairpins[1]->hairpinType(), HairpinType::CRESC_HAIRPIN);
    EXPECT_GE(hairpins[1]->tick(), Fraction(1, 1))
        << "cresc hairpin must start inside m2, not before bar line";
    delete score;
}

// Regression: Encore sometimes writes duplicate dynamics on the same (staff, voice, tick) with different xoffsets.
// Importer must drop the second when it lands on the same segment.
TEST_F(Tst_Importer, v0c4_dyn_dedup)
{
    MasterScore* score = readEncoreScore("importer_dyn_dedup.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_dyn_dedup.enc";

    Measure* m1 = score->firstMeasure();
    ASSERT_NE(m1, nullptr);
    int dynCount = 0;
    for (Segment* s = m1->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        for (EngravingItem* ann : s->annotations()) {
            if (ann && ann->isDynamic()) {
                ++dynCount;
            }
        }
    }
    EXPECT_EQ(dynCount, 1)
        << "two identical MF ORNs at the same tick must collapse to one "
        "Dynamic on the segment";
    delete score;
}

// Regression: dynamic ORN with yoffset > 0 visually belongs to staffIdx-1 (stored on N, rendered on N-1).
// Importer must reroute it to the correct staff.
TEST_F(Tst_Importer, v0c4_dyn_displaced_to_staff_above)
{
    MasterScore* score = readEncoreScore("importer_dyn_displaced_to_staff_above.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_dyn_displaced_to_staff_above.enc";

    // Dynamic must be on staff 0 track 0, not staff 1.
    const track_idx_t trackStaff0 = 0;
    const track_idx_t trackStaff1 = static_cast<track_idx_t>(VOICES);
    bool foundOnStaff0 = false;
    bool foundOnStaff1 = false;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest); s;
             s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* ann : s->annotations()) {
                if (!ann || !ann->isDynamic()) {
                    continue;
                }
                if (ann->track() == trackStaff0) {
                    foundOnStaff0 = true;
                }
                if (ann->track() == trackStaff1) {
                    foundOnStaff1 = true;
                }
            }
        }
    }
    EXPECT_TRUE(foundOnStaff0)
        << "MF with yoffset > 0 must be rerouted to staff 0 (the staff above)";
    EXPECT_FALSE(foundOnStaff1)
        << "the displaced MF must NOT remain on staff 1";
    delete score;
}

// Regression: WEDGE at tick==durTicks (bar line) had no ChordRest; snap-start returned next measure's tick,
// giving zero-span. Fix: backwards scan also fires when no ChordRest at the default tick.
TEST_F(Tst_Importer, v0c4_hairpin_snapstart_at_barline)
{
    MasterScore* score = readEncoreScore("importer_hairpin_snapstart_at_barline.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_hairpin_snapstart_at_barline.enc";

    // Find the DIM hairpin (the one crossing the bar line)
    Hairpin* dim = nullptr;
    for (const auto& kv : score->spanner()) {
        Spanner* sp = kv.second;
        if (sp && sp->isHairpin() && toHairpin(sp)->hairpinType() == HairpinType::DIM_HAIRPIN) {
            dim = toHairpin(sp);
            break;
        }
    }
    ASSERT_NE(dim, nullptr) << "dim hairpin starting at end of m1 must not be dropped";
    // Backwards scan: latest note with xoff<=110 is tick=480 (xoff=90). Start = Fraction(1,2).
    EXPECT_LT(dim->tick(), Fraction(1, 1))
        << "hairpin start must be inside m1 (snap from bar-line tick to last "
        "note with xoff <= ornament.xoffset)";
    EXPECT_EQ(dim->tick(), Fraction(1, 2))
        << "start must snap to tick=480 (xoff=90, latest note with xoff<=110)";
    // MF in m2 at tick=240 xoffset=70 (>= note@240 xoff=60 -> no snap-back)
    // stays at tagged tick. next-dynamic finds MF at m2 + 1/4.
    EXPECT_EQ(dim->tick2(), Fraction(1, 1) + Fraction(1, 4))
        << "dim must end at the MF dynamic in m2 (next-dynamic endpoint)";
    delete score;
}

// Regression: bar-line clamp (xoffset2 < firstNoteXoff) must yield to next-Dynamic endpoint when one exists.
// xoffset2=5 would clamp to bar line, but MF at m2.240 wins: hairpin must end at m2.240.
TEST_F(Tst_Importer, v0c4_hairpin_endpoint_dynamic_wins)
{
    MasterScore* score = readEncoreScore("importer_hairpin_endpoint_dynamic_wins.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_hairpin_endpoint_dynamic_wins.enc";

    Hairpin* dim = nullptr;
    for (const auto& kv : score->spanner()) {
        Spanner* sp = kv.second;
        if (sp && sp->isHairpin() && toHairpin(sp)->hairpinType() == HairpinType::DIM_HAIRPIN) {
            dim = toHairpin(sp);
            break;
        }
    }
    ASSERT_NE(dim, nullptr) << "dim hairpin must be present";
    const Fraction m2tick = Fraction(1, 1);
    EXPECT_GT(dim->tick2(), m2tick)
        << "hairpin must end at MF dynamic in m2 (after bar line), not at m2.tick; "
        "next-dynamic endpoint must win over bar-line clamp";
    // Specifically at m2 + 1/4 (= where MF is placed after snap-back)
    EXPECT_EQ(dim->tick2(), m2tick + Fraction(1, 4))
        << "hairpin must end at the MF dynamic tick (m2 + 1/4)";
    delete score;
}

// Regression: single-chord tremolos encoded as size-16 ORN (tipo=0xAF), not the articulation byte.
// ORN can appear at the chord's tick or at durTicks (measure end); both must attach TremoloSingleChord.
TEST_F(Tst_Importer, v0c4_tremolo_orn_normal_and_barline_tick)
{
    MasterScore* score = readEncoreScore("ornaments_tremolo_orn.enc");
    ASSERT_NE(score, nullptr) << "Failed to load ornaments_tremolo_orn.enc";

    std::vector<std::pair<Fraction, TremoloType> > trems;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest); s;
             s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(0);
            if (!el || !el->isChord()) {
                continue;
            }
            Chord* c = toChord(el);
            if (c->tremoloSingleChord()) {
                trems.push_back({ s->tick(), c->tremoloSingleChord()->tremoloType() });
            }
        }
    }
    ASSERT_EQ(trems.size(), 2u)
        << "expected exactly 2 TremoloSingleChord: one at m1.0 (normal tick) "
        "and one at m2.beat-4 (from ORN at measure end tick)";
    // m1 tremolo: at tick=0 (half note, tipo=0xAF at same tick)
    EXPECT_EQ(trems[0].first, Fraction(0, 1));
    EXPECT_EQ(trems[0].second, TremoloType::R32);
    // m2 tremolo: at tick of last quarter (beat 4 = m2.tick + 3/4)
    EXPECT_EQ(trems[1].first, Fraction(1, 1) + Fraction(3, 4));
    EXPECT_EQ(trems[1].second, TremoloType::R32);
    delete score;
}

// Regression: tremolo ORN is always in voice 0 regardless of the actual note voice.
// Resolver must widen to all staff voices when voice 0 yields no chord.
TEST_F(Tst_Importer, v0c4_tremolo_orn_cross_voice_attaches)
{
    MasterScore* score = readEncoreScore("ornaments_tremolo_orn_crossvoice.enc");
    ASSERT_NE(score, nullptr) << "Failed to load ornaments_tremolo_orn_crossvoice.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    bool foundTremolo = false;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest); s;
             s = s->next(SegmentType::ChordRest)) {
            for (int v = 0; v < static_cast<int>(VOICES); ++v) {
                EngravingItem* el = s->element(static_cast<track_idx_t>(v));
                if (!el || !el->isChord()) {
                    continue;
                }
                if (toChord(el)->tremoloSingleChord()) {
                    EXPECT_EQ(toChord(el)->tremoloSingleChord()->tremoloType(), TremoloType::R32);
                    foundTremolo = true;
                }
            }
        }
    }
    EXPECT_TRUE(foundTremolo)
        << "TremoloSingleChord must attach to the chord even when ORN is in a different voice";
    delete score;
}

// Regression: after a grace note, regular notes at exact face-grid ticks triggered the gap snap (gap=stolenTicks),
// inserting spurious rests. Fix: suppress snap when gap <= stolenTicks.
TEST_F(Tst_Importer, v0xa6_grace_ongrid_snap_suppressed)
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
TEST_F(Tst_Importer, v0xa6_inner_grace_group)
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
TEST_F(Tst_Importer, v0xa6_grace_restores_face_value)
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

// Binary-driven clef rule: G clef + Key=+12 → G8_VA. No template required.
TEST_F(Tst_Importer, v0c4_g_clef_8va_from_key)
{
    MasterScore* score = readEncoreScore("structure_g_clef_8va_from_key.enc");
    ASSERT_NE(score, nullptr);
    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    Segment* seg = m->findSegment(SegmentType::HeaderClef, m->tick());
    ASSERT_NE(seg, nullptr);
    EngravingItem* el = seg->element(0);
    ASSERT_TRUE(el && el->isClef());
    EXPECT_EQ(toClef(el)->clefType(), ClefType::G8_VA)
        << "G clef + Key=+12 must yield G8_VA";
    delete score;
}

// Binary-driven clef rule: F clef + Key=-12 → F8_VB. No template required.
TEST_F(Tst_Importer, v0c4_f_clef_8vb_from_key)
{
    MasterScore* score = readEncoreScore("structure_f_clef_8vb_from_key.enc");
    ASSERT_NE(score, nullptr);
    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    Segment* seg = m->findSegment(SegmentType::HeaderClef, m->tick());
    ASSERT_NE(seg, nullptr);
    EngravingItem* el = seg->element(0);
    ASSERT_TRUE(el && el->isClef());
    EXPECT_EQ(toClef(el)->clefType(), ClefType::F8_VB)
        << "F clef + Key=-12 must yield F8_VB";
    delete score;
}

// Binary-driven clef rule: F clef + Key=+12 → F_8VA. No template required.
TEST_F(Tst_Importer, v0c4_f_clef_8va_from_key)
{
    MasterScore* score = readEncoreScore("structure_f_clef_8va_from_key.enc");
    ASSERT_NE(score, nullptr);
    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    Segment* seg = m->findSegment(SegmentType::HeaderClef, m->tick());
    ASSERT_NE(seg, nullptr);
    EngravingItem* el = seg->element(0);
    ASSERT_TRUE(el && el->isClef());
    EXPECT_EQ(toClef(el)->clefType(), ClefType::F_8VA)
        << "F clef + Key=+12 must yield F_8VA";
    delete score;
}

// Binary-driven clef rule: G clef + Key=-7 (non-octave) → plain G.
// No octave-decorated variant exists for -7 semitones; keep the Encore clef.
TEST_F(Tst_Importer, v0c4_non_octave_key_keeps_clef)
{
    MasterScore* score = readEncoreScore("structure_non_octave_key_keeps_clef.enc");
    ASSERT_NE(score, nullptr);
    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    Segment* seg = m->findSegment(SegmentType::HeaderClef, m->tick());
    ASSERT_NE(seg, nullptr);
    EngravingItem* el = seg->element(0);
    ASSERT_TRUE(el && el->isClef());
    EXPECT_EQ(toClef(el)->clefType(), ClefType::G)
        << "G clef + Key=-7 (non-octave) must keep plain G";
    delete score;
}

// Regression: laud has plain G clef in Encore but Key=-12. Binary-driven rule maps G+Key=-12 → G8_VB.
// Importer must override the staff clef with the template's octave-bassa variant.
TEST_F(Tst_Importer, v0c4_octave_bassa_clef_override)
{
    MasterScore* score = readEncoreScore("structure_octave_bassa_clef_override.enc");
    ASSERT_NE(score, nullptr) << "Failed to load structure_octave_bassa_clef_override.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();

    ASSERT_EQ(score->parts().size(), 1u);
    const Instrument* inst = score->parts()[0]->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->id(), String(u"laud"));

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    Segment* clefSeg = m->findSegment(SegmentType::HeaderClef, m->tick());
    ASSERT_NE(clefSeg, nullptr) << "header clef segment missing";
    EngravingItem* clefEl = clefSeg->element(0);
    ASSERT_NE(clefEl, nullptr);
    ASSERT_TRUE(clefEl->isClef());
    EXPECT_EQ(toClef(clefEl)->clefType(), ClefType::G8_VB)
        << "laud staff must carry G8_VB (template clef), not the plain G "
        "stored by Encore";

    // Key=-12 applies: written pitch 76 → m_pitch 64; with G8_VB the note sits at E5 sounding E4.
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
    EXPECT_EQ(firstChord->notes()[0]->pitch(), 76 - 12);
    delete score;
}

// Regression: bass guitar F clef + Key=-12 must yield F8_VB (binary-driven rule),
// not the plain F transposing clef the template previously preferred.
TEST_F(Tst_Importer, v0c4_bass_guitar_transposing_clef)
{
    MasterScore* score = readEncoreScore("instruments_bass_guitar_transposing_clef.enc");
    ASSERT_NE(score, nullptr) << "Failed to load instruments_bass_guitar_transposing_clef.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();

    ASSERT_EQ(score->parts().size(), 1u);
    const Instrument* inst = score->parts()[0]->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->transpose().chromatic, -12)
        << "matched instrument template must be a bass-guitar variant "
        "(transposeChromatic = -12)";

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    Segment* clefSeg = m->findSegment(SegmentType::HeaderClef, m->tick());
    ASSERT_NE(clefSeg, nullptr);
    EngravingItem* clefEl = clefSeg->element(0);
    ASSERT_NE(clefEl, nullptr);
    ASSERT_TRUE(clefEl->isClef());
    EXPECT_EQ(toClef(clefEl)->clefType(), ClefType::F8_VB)
        << "F clef + Key=-12 must yield F8_VB (binary-driven rule); "
        "template transposing clef (plain F) is no longer preferred";

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
    EXPECT_EQ(firstChord->notes()[0]->pitch(), 45 - 12)
        << "Key = -12 applied: binary 45 -> m_pitch 33 (sounding A1)";
    delete score;
}

// Regression: Key offset (signed int8 at PRG_BASE-23+n*PRG_STEP) must be applied per-staff.
// Fixture: staff 0 Key=0 (pitch 69 unchanged), staff 1 Key=-12 (pitch 69 → 57).
TEST_F(Tst_Importer, v0c4_key_transposition_per_staff)
{
    MasterScore* score = readEncoreScore("structure_key_per_staff.enc");
    ASSERT_NE(score, nullptr) << "Failed to load structure_key_per_staff.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    auto firstChordTopPitch = [m](int staffIdx) -> int {
        const track_idx_t base = staffIdx * VOICES;
        for (Segment* s = m->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (int v = 0; v < static_cast<int>(VOICES); ++v) {
                EngravingItem* el = s->element(base + v);
                if (el && el->isChord()) {
                    return toChord(el)->upNote()->pitch();
                }
            }
        }
        return -1;
    };

    EXPECT_EQ(firstChordTopPitch(0), 69)
        << "staff 0 first chord top pitch (Key=0): expected 69";
    EXPECT_EQ(firstChordTopPitch(1), 69 - 12)
        << "staff 1 first chord top pitch (Key=-12): expected 57";
    delete score;
}

// Regression: voice >= VOICES (e.g. voice=4) was silently dropped; fix maps out-of-range voices to voice 0.
// Also: short names (1-3 chars) skip name+MIDI matcher and fall through to Grand Piano template.
TEST_F(Tst_Importer, v0c4_satb_short_names_with_voice4_bass_lyrics)
{
    MasterScore* score = readEncoreScore("text_satb_short_names_voice4_lyrics.enc");
    ASSERT_NE(score, nullptr) << "Failed to load text_satb_short_names_voice4_lyrics.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();

    const int totalStaves = static_cast<int>(score->nstaves());
    ASSERT_EQ(totalStaves, 4) << "score must carry 4 staves (SATB)";
    const int bassStaff = totalStaves - 1;

    int bassChords = 0;
    int bassLyrics = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (int v = 0; v < static_cast<int>(VOICES); ++v) {
                EngravingItem* el = s->element(bassStaff * VOICES + v);
                if (el && el->isChord()) {
                    Chord* c = toChord(el);
                    ++bassChords;
                    bassLyrics += static_cast<int>(c->lyrics().size());
                }
            }
        }
    }
    EXPECT_GE(bassChords, 4) << "bass staff must carry the four voice-4 chords";
    EXPECT_GE(bassLyrics, 4) << "bass lyrics must attach to the voice-0 chords";

    ASSERT_EQ(score->parts().size(), 4u);
    const String expectedLabels[] = { u"S", u"C", u"T", u"B" };
    for (size_t i = 0; i < 4; ++i) {
        const Part* part = score->parts()[i];
        const Instrument* inst = part->instrument();
        ASSERT_NE(inst, nullptr);
        EXPECT_EQ(inst->id(), String(u"grand-piano"))
            << "part " << i << " (label '" << expectedLabels[i].toStdString()
            << "'): short names must fall back to Grand Piano";
        EXPECT_EQ(part->longName(), expectedLabels[i])
            << "the original Encore label is preserved as longName";
    }
    delete score;
}

// Regression: isolated explicit tuplet note placed with face value but cumTick advance was capped;
// voice overran by face - capped. Fix: always set chord duration to the capped value.
TEST_F(Tst_Importer, isolated_explicit_tuplet_caps_chord_ticks)
{
    MasterScore* score = readEncoreScore("importer_isolated_explicit_tuplet_capped.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_isolated_explicit_tuplet_capped.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();
    delete score;
}

// Regression: note-level path-A cap deleted a chord that belonged to an inner (nested) tuplet.
// Old code called tt.currentTuplet->remove(chord) — the outer tuplet — which does not contain the
// chord, logging "cannot find element" and leaving a dangling pointer in the inner tuplet's
// m_currentElements.  The next innerTuplet->add() call iterated over that dangling pointer → SIGSEGV.
// Fix: use chord->tuplet() (the actual owning tuplet) rather than tt.currentTuplet.
TEST_F(Tst_Importer, inner_tuplet_note_level_cap_no_crash)
{
    MasterScore* score = readEncoreScore("importer_inner_tuplet_note_level_cap.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_inner_tuplet_note_level_cap.enc";
    EXPECT_GT(score->nmeasures(), 0);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();
    delete score;
}

// Regression: both notes and rests updated prevMidiTick; a note at the same tick as a rest was mis-detected
// as chord extension, replacing the rest's segment while cumTick was already advanced past it.
TEST_F(Tst_Importer, rest_does_not_anchor_chord_extension)
{
    MasterScore* score = readEncoreScore("importer_rest_not_chord_anchor.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();
    delete score;
}

// Regression: rest with open tuplet skipped first cap; tuplet closed; second cap shortened cumTick advance
// but left rest ticks at uncapped face value. cr->actualTicks() exceeded cumTick advance, overrunning the measure.
TEST_F(Tst_Importer, rest_caps_its_ticks_when_advance_is_capped)
{
    MasterScore* score = readEncoreScore("importer_rest_caps_in_open_tuplet.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();
    delete score;
}

// Regression: duplicate tt.placedTicks += advance in rest path double-counted the rest's contribution,
// masking the undershoot and skipping closeTuplet shrink. checkMeasure reported "Incomplete measure".
TEST_F(Tst_Importer, rest_in_tuplet_does_not_double_count_placed_ticks)
{
    MasterScore* score = readEncoreScore("importer_rest_in_tuplet.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_rest_in_tuplet.enc";
    EXPECT_GT(score->nmeasures(), 0);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();
    delete score;
}

// Lightweight test macro: load, measure-count > 0, sanityCheck.
#define ENC_SANITY_TEST(testName, fileName) \
    TEST_F(Tst_Importer, testName) { \
        MasterScore* score = readEncoreScore(fileName); \
        ASSERT_NE(score, nullptr) << "Failed to load " << fileName; \
        EXPECT_GT(score->nmeasures(), 0); \
        muse::Ret ret = score->sanityCheck(); \
        EXPECT_TRUE(ret) << "Corrupted: " << ret.text(); \
        delete score; \
    }

// Cross-cutting sanity coverage for the alMezuro-based spanner endpoint
// resolution (hairpins + slurs) and the partial-tuplet TDuration guard.
ENC_SANITY_TEST(multi_measure_hairpin,      "ornaments_multi_measure_hairpin.enc")
ENC_SANITY_TEST(multi_measure_slur,         "ornaments_multi_measure_slur.enc")
ENC_SANITY_TEST(partial_quarter_triplet,    "ornaments_partial_quarter_triplet.enc")
ENC_SANITY_TEST(lyrics_attach,              "text_lyrics.enc")
ENC_SANITY_TEST(lyrics_variable_length,     "text_lyrics_variable.enc")
ENC_SANITY_TEST(lyrics_two_verses,          "text_lyrics_two_verses.enc")
ENC_SANITY_TEST(lyrics_hyphenated_words,    "text_lyrics_hyphenated_words.enc")
ENC_SANITY_TEST(tie_start_flag_byte6,       "notes_tie_start_flag_byte6.enc")
ENC_SANITY_TEST(articulations_extended,     "ornaments_articulations.enc")
ENC_SANITY_TEST(articulations_combo,        "ornaments_articulations_combo.enc")
ENC_SANITY_TEST(trill_mordent,              "ornaments_trill_mordent.enc")
ENC_SANITY_TEST(tremolos,                   "ornaments_tremolos.enc")
ENC_SANITY_TEST(fermatas,                   "ornaments_fermatas.enc")
ENC_SANITY_TEST(technical,                  "ornaments_technical.enc")
ENC_SANITY_TEST(trill_spanner,              "ornaments_trill_spanner.enc")
ENC_SANITY_TEST(staccato_orn,               "ornaments_staccato_orn.enc")
ENC_SANITY_TEST(section_markers,            "structure_section_markers.enc")
ENC_SANITY_TEST(jump_marks,                 "structure_jump_marks.enc")
ENC_SANITY_TEST(jump_marks_all,             "structure_jump_marks_all.enc")
ENC_SANITY_TEST(tie_direction_fc,           "notes_tie_dir_fc.enc")
ENC_SANITY_TEST(keychange_to_c,             "structure_keychange_to_c.enc")
ENC_SANITY_TEST(staff_text,                 "text_staff_text.enc")
ENC_SANITY_TEST(titl_headers_footers,       "text_titl_headers_footers.enc")
ENC_SANITY_TEST(arpeggio,                   "ornaments_arpeggio.enc")
ENC_SANITY_TEST(staff_text_placement,       "text_staff_text_placement.enc")
ENC_SANITY_TEST(dynamics_size16,            "ornaments_dynamics.enc")
ENC_SANITY_TEST(dynamics_full,              "ornaments_dynamics_full.enc")
ENC_SANITY_TEST(wedgestart_at_measure_end,  "ornaments_wedgestart_at_measure_end.enc")
ENC_SANITY_TEST(double_barline_multi_staff, "ornaments_double_barline_multi_staff.enc")

TEST_F(Tst_Importer, orchestra_loads_with_all_parts)
{
    MasterScore* score = readEncoreScore("kordorkestro.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->parts().size(), 1u) << "Orchestra should have multiple parts";
    EXPECT_GT(score->nstaves(), 1u) << "Orchestra should have multiple staves";
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Importer, orchestra_sanity_check)
{
    MasterScore* score = readEncoreScore("kordorkestro.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "kordorkestro should pass sanityCheck: " << ret.text();
    delete score;
}

TEST_F(Tst_Importer, title_frame_created)
{
    // kordorkestro has title "String Orchestra w/Piano"
    MasterScore* score = readEncoreScore("kordorkestro.enc");
    ASSERT_NE(score, nullptr);
    // Title frame should be the first element
    MeasureBase* first = score->first();
    ASSERT_NE(first, nullptr);
    EXPECT_TRUE(first->isVBox()) << "Score with title should start with a VBox frame";
    delete score;
}

TEST_F(Tst_Importer, no_title_frame_when_empty)
{
    // bazo.enc has no title — should not have a VBox frame
    MasterScore* score = readEncoreScore("bazo.enc");
    ASSERT_NE(score, nullptr);
    MeasureBase* first = score->first();
    ASSERT_NE(first, nullptr);
    EXPECT_TRUE(first->isMeasure()) << "Score without title should start with a measure";
    delete score;
}

TEST_F(Tst_Importer, title_frame_instruction_and_copyright)
{
    // TITL block: title, subtitle, instruction → LYRICIST, author → COMPOSER, copyright → metadata tag.
    MasterScore* score = readEncoreScore("text_title_instruction_copyright.enc");
    ASSERT_NE(score, nullptr);

    MeasureBase* first = score->first();
    ASSERT_NE(first, nullptr);
    ASSERT_TRUE(first->isVBox()) << "TITL with content must produce a VBox frame";

    std::map<TextStyleType, String> texts;
    for (const EngravingItem* el : first->el()) {
        if (el->isText()) {
            const TextBase* tb = toTextBase(el);
            texts[tb->textStyleType()] = tb->plainText();
        }
    }

    // VBox visual text elements
    EXPECT_EQ(texts[TextStyleType::TITLE],    String(u"Test Title"));
    EXPECT_EQ(texts[TextStyleType::SUBTITLE], String(u"Test Subtitle"));
    EXPECT_EQ(texts[TextStyleType::LYRICIST], String(u"Test Instruction"))
        << "instruction[0] must be added as LYRICIST text";
    EXPECT_EQ(texts[TextStyleType::COMPOSER], String(u"Test Composer"));

    // Score Properties metadata (File > Score Properties dialog)
    EXPECT_EQ(score->metaTag(u"workTitle"),  String(u"Test Title"))
        << "title must be stored in workTitle metadata";
    EXPECT_EQ(score->metaTag(u"subtitle"),   String(u"Test Subtitle"))
        << "subtitle[0] must be stored in subtitle metadata";
    EXPECT_EQ(score->metaTag(u"lyricist"),   String(u"Test Instruction"))
        << "instruction[0] must be stored in lyricist metadata";
    EXPECT_EQ(score->metaTag(u"composer"),   String(u"Test Composer"))
        << "author[0] must be stored in composer metadata";
    EXPECT_EQ(score->metaTag(u"copyright"),  String(u"(c) 2026 Test"))
        << "copyright[0] must be stored in copyright metadata";

    delete score;
}

TEST_F(Tst_Importer, title_frame_headers_footers)
{
    // Two header lines (RIGHT + CENTER) and two footer lines (CENTER + RIGHT); applied to odd+even Sids.
    MasterScore* score = readEncoreScore("text_titl_headers_footers.enc");
    ASSERT_NE(score, nullptr);

    auto styleText = [score](Sid sid) -> String {
        return score->style().styleSt(sid);
    };

    EXPECT_EQ(styleText(Sid::oddHeaderR),  String(u"Header Right"));
    EXPECT_EQ(styleText(Sid::evenHeaderR), String(u"Header Right"));
    EXPECT_EQ(styleText(Sid::oddHeaderC),  String(u"Header Center"));
    EXPECT_EQ(styleText(Sid::evenHeaderC), String(u"Header Center"));
    // No left-aligned header in fixture; default Sids (e.g. "$p") must survive unchanged.
    EXPECT_NE(styleText(Sid::oddHeaderL),  String(u"Header Right"));
    EXPECT_NE(styleText(Sid::oddHeaderL),  String(u"Header Center"));
    EXPECT_NE(styleText(Sid::evenHeaderL), String(u"Header Right"));
    EXPECT_NE(styleText(Sid::evenHeaderL), String(u"Header Center"));

    EXPECT_EQ(styleText(Sid::oddFooterC),  String(u"Footer Center"));
    EXPECT_EQ(styleText(Sid::evenFooterC), String(u"Footer Center"));
    EXPECT_EQ(styleText(Sid::oddFooterR),  String(u"Footer Right"));
    EXPECT_EQ(styleText(Sid::evenFooterR), String(u"Footer Right"));
    EXPECT_NE(styleText(Sid::oddFooterL),  String(u"Footer Center"));
    EXPECT_NE(styleText(Sid::oddFooterL),  String(u"Footer Right"));
    EXPECT_NE(styleText(Sid::evenFooterL), String(u"Footer Center"));
    EXPECT_NE(styleText(Sid::evenFooterL), String(u"Footer Right"));

    delete score;
}

TEST_F(Tst_Importer, chord_symbols_present)
{
    // akordo.enc has chord symbols (Am, G7, etc.)
    MasterScore* score = readEncoreScore("akordo.enc");
    ASSERT_NE(score, nullptr);
    bool foundHarmony = false;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e->isHarmony()) {
                    foundHarmony = true;
                    break;
                }
            }
            if (foundHarmony) {
                break;
            }
        }
        if (foundHarmony) {
            break;
        }
    }
    EXPECT_TRUE(foundHarmony) << "akordo.enc should contain chord symbols";
    delete score;
}

TEST_F(Tst_Importer, multiple_voices_loaded)
{
    // opeco_vochoj.enc has multiple voices per staff
    MasterScore* score = readEncoreScore("opeco_vochoj.enc");
    ASSERT_NE(score, nullptr);
    bool foundVoice1 = false;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        Measure* m = toMeasure(mb);
        for (Segment* s = m->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            // Check voice 1 (track 1 = staff 0, voice 1)
            if (s->element(1) && s->element(1)->isChordRest()) {
                foundVoice1 = true;
                break;
            }
        }
        if (foundVoice1) {
            break;
        }
    }
    EXPECT_TRUE(foundVoice1) << "opeco_vochoj.enc should have notes in voice 2";
    delete score;
}

TEST_F(Tst_Importer, encore_symbols_full_coverage)
{
    MasterScore* score = readEncoreScore("encore_symbols.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int dynamics = 0;
    int fermatas = 0;
    int markers = 0;     // Segno / Coda / TOCODA / FINE
    int jumps = 0;       // D.C. / D.S. variants
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
        // Dotted end barline
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
                case SymId::ornamentTrill:
                    ++trills;
                    break;
                case SymId::ornamentShortTrill:    // <inverted-mordent>
                case SymId::ornamentTremblement:   // <inverted-mordent long="yes">
                case SymId::ornamentMordent:
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
            ++trills;   // Trill spanners (TRILL_START + TRILL_END → tr + wavy line)
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
    EXPECT_GE(accents,        7);
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

// ===========================================================================
// BUG FIX: articulationUp=0x20 on the last note of a tuplet group is Encore's
// "tuplet bracket placement above" flag, not a fermata. The importer must not
// create a Fermata element when the note has tuplet != 0.
// Real-world case: Paloteos de Moncalvillo (4/4 + 7/8), where every triplet's
// last note had articulationUp=0x20, producing spurious fermatas on all measures.
// ===========================================================================
TEST_F(Tst_Importer, v0c4_fermata_suppressed_on_tuplet_last_note)
{
    MasterScore* score = readEncoreScore("ornaments_fermata_not_in_tuplet.enc");
    ASSERT_NE(score, nullptr) << "Failed to load ornaments_fermata_not_in_tuplet.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m1 = score->firstMeasure();
    ASSERT_NE(m1, nullptr);

    int fermataCount = 0;
    for (Segment* s = m1->first(SegmentType::ChordRest); s;
         s = s->next(SegmentType::ChordRest)) {
        for (EngravingItem* e : s->annotations()) {
            if (e && e->isFermata()) {
                ++fermataCount;
            }
        }
    }
    // Only the non-tuplet note (tick=0) has articUp=0x20 and must get a fermata.
    // The three triplet notes also have articUp=0x20 but must NOT produce fermatas.
    EXPECT_EQ(fermataCount, 1)
        << "Only the non-tuplet note must get a fermata; tuplet notes with "
        "articUp=0x20 encode bracket placement, not a fermata";
    delete score;
}

TEST_F(Tst_Importer, beethoven_no_crash)
{
    MasterScore* score = readEncoreScore("notes_corrupted.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Importer, twelve_instrument_score_no_crash)
{
    MasterScore* score = readEncoreScore("notes_triplets.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Importer, swing_timing_file_no_crash)
{
    MasterScore* score = readEncoreScore("notes_swing.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

// ===========================================================================
// Run LAST: corrupted fixtures (open slurs, invalid voice, degenerate tuplets)
// can leave global layout state dirty and must not precede assertion tests.
// ===========================================================================

// Covers: tuplet=0xFF (degenerate), faceValue=0 (invalid), voice>=4, open SLURSTART
// (previously tested by Beethoven.enc and Opus 27 First Movement.enc)
ENC_SANITY_TEST(corrupted_elements,         "notes_corrupted.enc")

// Covers: explicit 3:2 triplets, 3/4 time sig, multi-measure
// (previously tested by Chansonette.enc and other 3/4 files)
ENC_SANITY_TEST(explicit_triplets_3_4,      "notes_triplets.enc")

// Covers: grace note filtering (fv>=4 only), ACCIACCATURA
// (previously tested by Grace.enc and Beethoven.enc)
ENC_SANITY_TEST(grace_notes,                "notes_grace.enc")

// Oboe with keyTransposeSemitones=5 (augmented 4th / perfect 4th): instrument transposition
// must be set to chromatic=5, diatonic=3 so MuseScore displays the written pitch from Encore.
TEST_F(Tst_Importer, key_transposition_non_octave_oboe)
{
    MasterScore* score = readEncoreScore("importer_transp_oboe_jota.enc");
    ASSERT_NE(score, nullptr);
    Staff* staff = score->staff(0);
    ASSERT_NE(staff, nullptr);
    const Interval iv = staff->part()->instrument()->transpose();
    EXPECT_EQ(iv.chromatic, 5);
    EXPECT_EQ(iv.diatonic, 3);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();
    delete score;
}

// ===========================================================================
// BUG FIX: tremolo ORN on the tied-from note of a quarter->eighth tie.
// The ORN's cumTick position falls on or past the eighth (tie-continuation),
// so the "last chord" fallback resolves to the eighth. The importer must
// check tieBack() on the resolved chord and walk back to the tie-start chord.
// Real-world case: Alborada de Mayo, measure 1, percussion staff.
// ===========================================================================
TEST_F(Tst_Importer, v0c4_tremolo_orn_on_tied_from_note)
{
    MasterScore* score = readEncoreScore("ornaments_tremolo_orn_tied_from.enc");
    ASSERT_NE(score, nullptr) << "Failed to load ornaments_tremolo_orn_tied_from.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    Measure* m1 = score->firstMeasure();
    ASSERT_NE(m1, nullptr);

    Chord* tremoloChord = nullptr;
    for (Segment* s = m1->first(SegmentType::ChordRest); s;
         s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (el && el->isChord()) {
            Chord* c = toChord(el);
            if (c->tremoloSingleChord()) {
                tremoloChord = c;
            }
        }
    }
    ASSERT_NE(tremoloChord, nullptr) << "TremoloSingleChord not found in measure 1";
    EXPECT_EQ(tremoloChord->tremoloSingleChord()->tremoloType(), TremoloType::R32);
    // Must be on the quarter (tick=0), not the eighth continuation (tick=1/4).
    EXPECT_EQ(tremoloChord->tick(), Fraction(0, 1))
        << "Tremolo must land on the tie-start quarter (tick=0), not the eighth continuation";
    // The chord carrying the tremolo must be the tie-start.
    ASSERT_FALSE(tremoloChord->notes().empty());
    EXPECT_NE(tremoloChord->notes().front()->tieFor(), nullptr)
        << "The tremolo chord must have an outgoing tie (it is the quarter note)";
    EXPECT_EQ(tremoloChord->notes().front()->tieBack(), nullptr)
        << "The tremolo chord must NOT have an incoming tie";
    delete score;
}

// ===========================================================================
// Comprehensive synthetic fixture exercising every reader/importer feature:
// 2 instruments, 20 measures, all note values, ties, triplets, grace notes,
// all dynamics, hairpins, articulations, ornaments, tremolos, slurs, arpeggio,
// fingering, tempo, staff text, chord symbols, lyrics, repeat/volta, segno/coda,
// and 6/8 compound meter. Verifies clean import and sanity with expected counts.
// ===========================================================================
TEST_F(Tst_Importer, sintetico_all_features_imports_cleanly)
{
    MasterScore* score = readEncoreScore("sintetico_all_features.enc");
    ASSERT_NE(score, nullptr) << "Failed to load sintetico_all_features.enc";

    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "sanityCheck failed: " << ret.text();

    EXPECT_EQ(score->parts().size(), 2u) << "2 instruments expected";
    EXPECT_EQ(score->nmeasures(), 20) << "20 measures expected";

    // Count selected element types across the whole score.
    int fermatas=0, tuplets=0, lyrics_count=0, hairpins=0, spanners=0;
    int tremolos=0, arpeggios=0, tempos=0, dynamics=0, markers=0;

    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        Measure* m = toMeasure(mb);
        if (m->repeatStart()) {
            ++spanners; // count repeat-start barlines
        }
        // Markers live on the measure directly (added to measure's own element list)
        for (EngravingItem* me : m->el()) {
            if (me && me->isMarker()) {
                ++markers;
            }
        }
        for (Segment* s = m->first(SegmentType::ChordRest); s;
             s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (!e) {
                    continue;
                }
                if (e->isFermata()) {
                    ++fermatas;
                }
                if (e->isDynamic()) {
                    ++dynamics;
                }
                if (e->isTempoText()) {
                    ++tempos;
                }
            }
            for (int ti = 0; ti < static_cast<int>(score->nstaves() * VOICES); ++ti) {
                EngravingItem* el = s->element(static_cast<track_idx_t>(ti));
                if (!el || !el->isChord()) {
                    continue;
                }
                Chord* c = toChord(el);
                if (c->tremoloSingleChord()) {
                    ++tremolos;
                }
                if (c->arpeggio()) {
                    ++arpeggios;
                }
                for (Lyrics* ly : c->lyrics()) {
                    if (ly) {
                        ++lyrics_count;
                    }
                }
            }
            if (s->isChordRestType()) {
                Tuplet* tup = nullptr;
                EngravingItem* el = s->element(0);
                if (el && el->isChordRest()) {
                    tup = toChordRest(el)->tuplet();
                }
                if (tup && tup->elements().front() == s->element(0)) {
                    ++tuplets;
                }
            }
        }
    }
    for (const auto& kv : score->spanner()) {
        Spanner* sp = kv.second;
        if (sp && sp->isHairpin()) {
            ++hairpins;
        }
    }

    EXPECT_GE(fermatas,     1) << "at least 1 fermata (non-tuplet note with articUp=0x20)";
    EXPECT_GE(tuplets,      2) << "at least 2 tuplet groups (triplets in m3)";
    EXPECT_GE(lyrics_count, 4) << "4 lyrics syllables (do re mi fa)";
    EXPECT_GE(dynamics,     8) << "at least 8 of the 13 dynamics (pp-ppp-p-mp-mf-f-ff-fff-sfz...)";
    EXPECT_GE(tempos,       3) << "at least 3 TempoText marks (initial + bpm change + 6/8)";
    EXPECT_GE(hairpins,     1) << "at least 1 hairpin (crescendo or decrescendo)";
    EXPECT_GE(tremolos,     1) << "at least 1 TremoloSingleChord";
    EXPECT_GE(arpeggios,    1) << "at least 1 arpeggio";
    EXPECT_GE(markers,      2) << "at least 2 section markers (segno, coda, to-coda)";
    EXPECT_GE(spanners,     1) << "at least 1 repeat-start barline";

    delete score;
}

// ===========================================================================
// BUG FIX: mixed-duration explicit tuplet bracket {Q,E} in a 3:2 group
// was not closing correctly. faceSum(Q+E)=3/8 never reached the old
// threshold 3Q=3/4, pulling subsequent notes into the same bracket and
// causing a measure overrun (Incomplete measure error).
//
// Fix: close a group when faceSum/actualN is a valid standard TDuration.
// {Q,E}/3 = E, valid → closes after 2 notes. Next {Q,Q,Q}/3 = Q, valid → 3.
//   Q (tied) + (Q+E)*2/3 + (Q+Q+Q)*2/3 = Q + Q + 2Q = 4Q = 4/4 exact
// ===========================================================================
TEST_F(Tst_Importer, v0c4_mixed_duration_tuplet_bracket_closes_correctly)
{
    MasterScore* score = readEncoreScore("ornaments_tuplet_mixed_baseLen.enc");
    ASSERT_NE(score, nullptr) << "Failed to load ornaments_tuplet_mixed_baseLen.enc";

    // No measure corruption — the sanityCheck must pass
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Measure is corrupt (overrun): " << ret.text();

    Measure* m1 = score->firstMeasure();
    ASSERT_NE(m1, nullptr);

    // Count tuplet groups in measure 1
    std::set<Tuplet*> tuplets;
    int noteCount = 0;
    for (Segment* s = m1->first(SegmentType::ChordRest); s;
         s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(0);
        if (!el || !el->isChord()) {
            continue;
        }
        ++noteCount;
        Chord* c = toChord(el);
        if (c->tuplet()) {
            tuplets.insert(c->tuplet());
        }
    }

    // 6 notes: 1 plain Q + 2 in bracket1 + 3 in bracket2
    EXPECT_EQ(noteCount, 6);
    // Exactly 2 separate tuplet brackets
    EXPECT_EQ(tuplets.size(), 2u)
        << "Must form 2 tuplet brackets: {Q,E} and {Q,Q,Q}, not one big group";
    delete score;
}

// ===========================================================================
// BUG FIX: 4:3 quadruplet (tup=0x43) was not recognized; notes appeared
// as plain, with wrong advance (Q instead of E per slot). The fix adds
// 4:3 to getExplicit() and derives note duration from rdur x (actualN/normalN)
// so beat-relative face values (e.g. fv=Q meaning one eighth in 8/8) map to
// the correct MuseScore duration and advance.
// ===========================================================================
TEST_F(Tst_Importer, v0c4_4to3_quadruplet_correct_advance)
{
    MasterScore* score = readEncoreScore("tuplet_4to3_quadruplet.enc");
    ASSERT_NE(score, nullptr) << "Failed to load tuplet_4to3_quadruplet.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "4:3 quadruplet must import without measure corruption: " << ret.text();

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    // Collect chords from measure 1
    std::vector<Chord*> chords;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            chords.push_back(toChord(e));
        }
    }
    ASSERT_EQ(chords.size(), 4u) << "Must have 4 chords in the 4:3 quadruplet";

    // All 4 chords must be in the same 4:3 tuplet
    for (int i = 0; i < 4; ++i) {
        ASSERT_NE(chords[i]->tuplet(), nullptr) << "Chord " << i << " must be in a 4:3 tuplet";
        EXPECT_EQ(chords[i]->tuplet(), chords[0]->tuplet()) << "All 4 in same bracket";
    }
    // Ratio must be 4:3
    EXPECT_EQ(chords[0]->tuplet()->ratio(), Fraction(4, 3)) << "Ratio must be 4:3";
    // Actual advance per note: E * (3/4) = 3/32 of whole note
    EXPECT_EQ(chords[0]->actualTicks(), Fraction(3, 32)) << "E in 4:3 = E*(3/4) = 3/32";
    delete score;
}

// ===========================================================================
// BUG FIX: dotted rests were not recognised because dotControl (a bitmask flag)
// was passed as a tick count to calcDots, always yielding 0 dots. The fix adds
// a calcDotsSnap(realDuration) fallback matching the note handler. Without the
// fix a dotted-quarter rest in 7/8 became a plain quarter rest, leaving a
// gap eighth rest AFTER the notes instead of BEFORE them.
// ===========================================================================
TEST_F(Tst_Importer, v0c4_dotted_rest_correct_duration)
{
    MasterScore* score = readEncoreScore("rest_dotted_before_notes.enc");
    ASSERT_NE(score, nullptr) << "Failed to load rest_dotted_before_notes.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Dotted rest must import without measure corruption: " << ret.text();

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(7, 8));

    // First ChordRest in measure must be a dotted-quarter REST (not a plain quarter).
    // Without the fix the plain quarter rest (240 ticks) leaves a 120-tick gap
    // filled by a phantom eighth rest that is placed AFTER the notes.
    Segment* first = m->first(SegmentType::ChordRest);
    ASSERT_NE(first, nullptr);
    EngravingItem* el = first->element(0);
    ASSERT_NE(el, nullptr);
    ASSERT_TRUE(el->isRest()) << "First element must be a rest";
    Rest* rest = toRest(el);
    // Dotted quarter = Q + E = 3/8 of measure duration
    EXPECT_EQ(rest->durationType().type(), DurationType::V_QUARTER) << "Base type: quarter";
    EXPECT_EQ(rest->dots(), 1) << "Must have 1 dot (dotted-quarter rest)";
    // Actual ticks: dotted-quarter = 3/8 of whole note
    EXPECT_EQ(rest->ticks(), Fraction(3, 8)) << "Dotted-quarter rest spans 3/8";
    delete score;
}

// Percussion file: two WINI/TITL/PREC blocks, large ghost MEAS blocks embedded in binary data.
// ===========================================================================
// BUG FIX: Dotted note not recognised when MIDI timing drift makes rdur
//          > 1 tick off from the theoretical dotted value. dotControl bit 0
//          (Encore's "dotted" flag) now overrides when calcDotsSnap returns 0.
// ===========================================================================
TEST_F(Tst_Importer, v0c4_dotted_note_dotctrl_bit0_drift)
{
    MasterScore* score = readEncoreScore("notes_dotted_ctrl_bit0_drift.enc");
    ASSERT_NE(score, nullptr) << "Failed to load notes_dotted_ctrl_bit0_drift.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "dotControl bit-0 dotted note with rdur drift must not corrupt: "
                     << ret.text();

    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->timesig(), Fraction(2, 4));

    Segment* first = m->first(SegmentType::ChordRest);
    ASSERT_NE(first, nullptr);
    EngravingItem* el = first->element(0);
    ASSERT_NE(el, nullptr);
    ASSERT_TRUE(el->isChord()) << "First element must be a chord, not a rest";
    Chord* c = toChord(el);
    EXPECT_EQ(c->durationType().type(), DurationType::V_EIGHTH)
        << "First note base type: eighth";
    EXPECT_EQ(c->dots(), 1)
        << "dotControl bit 0 forces 1 dot when calcDotsSnap misses due to rdur drift";
    delete score;
}

// Must import cleanly: correct measure count, no DOM corruption.
TEST_F(Tst_Importer, percussion_drum_kit_no_crash)
{
    MasterScore* score = readEncoreScore("importer_perc_bateria.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_EQ(score->nmeasures(), 36);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();
    delete score;
}

// Regression: chord symbols stored without text (tipo bit0 == 0) were silently skipped.
// chord_parsing.enc has 64 measures each with one numeric-only chord (teksto empty).
// All must now be imported as Harmony elements decoded from radiko + toniko fields.
TEST_F(Tst_Importer, numeric_chord_symbols)
{
    MasterScore* score = readEncoreScore("chord_parsing.enc");
    ASSERT_NE(score, nullptr);

    // Collect the first harmony per measure (by 0-based measure index).
    std::map<int, String> harmonyByMeasure;
    int measureIdx = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* ann : s->annotations()) {
                if (ann && ann->isHarmony()) {
                    harmonyByMeasure[measureIdx] = toHarmony(ann)->harmonyName();
                }
            }
        }
        ++measureIdx;
    }

    // chord_parsing.enc measure layout (all radiko=0 = root C, toniko varies):
    // m0: toniko=0  (major)     -> C
    // m1: toniko=1  (minor)     -> Cm
    // m2: toniko=2  (augmented) -> C+
    // m3: toniko=6  (6th)       -> C6
    // m4: toniko=24 (dom7 alt)  -> C7
    // m8: toniko=3  (dim)       -> Cdim
    // m9: toniko=12 (maj7)      -> Cmaj7
    EXPECT_FALSE(harmonyByMeasure.empty()) << "numeric chord symbols must be imported (were silently dropped)";
    EXPECT_EQ(harmonyByMeasure[0], String(u"C")) << "toniko=0 (major)";
    EXPECT_EQ(harmonyByMeasure[1], String(u"Cm")) << "toniko=1 (minor)";
    EXPECT_EQ(harmonyByMeasure[2], String(u"C+")) << "toniko=2 (augmented)";
    EXPECT_EQ(harmonyByMeasure[4], String(u"C7")) << "toniko=24 (dom7 alternate)";
    EXPECT_EQ(harmonyByMeasure[8], String(u"Cdim")) << "toniko=3 (diminished)";
    EXPECT_EQ(harmonyByMeasure[9], String(u"CMaj7")) << "toniko=12 (maj7): MuseScore normalizes 'maj' to 'Maj'";

    delete score;
}

// Regression: numeric chord with bass note (tipo bit 1 set) should produce a slash chord.
// akordo.enc measure 1 has toniko=49 (13sus4), radiko=0x25 (Ab), baso=0x13 (F#), tipo=2.
TEST_F(Tst_Importer, numeric_chord_with_bass_note)
{
    MasterScore* score = readEncoreScore("akordo.enc");
    ASSERT_NE(score, nullptr);

    Harmony* slashChord = nullptr;
    for (MeasureBase* mb = score->first(); mb && !slashChord; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s && !slashChord; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* ann : s->annotations()) {
                if (ann && ann->isHarmony()) {
                    Harmony* h = toHarmony(ann);
                    if (h->harmonyName().contains(u"/")) {
                        slashChord = h;
                    }
                }
            }
        }
    }

    ASSERT_NE(slashChord, nullptr) << "akordo.enc must have a slash chord (tipo=2, bass note present)";
    const String name = slashChord->harmonyName();
    EXPECT_TRUE(name.startsWith(u"Ab"))
        << "root should be Ab (radiko=0x25): " << name.toStdString();
    EXPECT_TRUE(name.contains(u"/F#"))
        << "bass should be F# (baso=0x13): " << name.toStdString();

    delete score;
}

// Regression: a single MEAS block whose lone REST element has mrestCount > 1
// must expand to that many MuseScore measures.
// Synthetic file: 7 MEAS blocks (notes, notes, notes, mrest=3, notes, notes, notes)
// with 2 LINE blocks (system 1 = MEAS[0..3], system 2 = MEAS[4..6]).
// Expected: 9 MuseScore measures (7 + 2 extra from expansion), measures 1-3 with
// notes, 4-6 empty, 7-9 with notes.  System 1 ends at measure 6, system 2 at 9.
TEST_F(Tst_Importer, mrest_single_block_expands_and_system_locks_correct)
{
    MasterScore* score = readEncoreScore("importer_mrest_single_block.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_mrest_single_block.enc";
    ASSERT_EQ(score->nmeasures(), 9)
        << "7 MEAS blocks + 2 extra from MEAS[3] mrestCount=3";

    auto measureAt = [](MasterScore* sc, int idx) -> Measure* {
        Measure* m = sc->firstMeasure();
        for (int i = 0; i < idx && m; ++i) {
            m = m->nextMeasure();
        }
        return m;
    };
    auto hasPitchedNotes = [](Measure* m) -> bool {
        if (!m) {
            return false;
        }
        for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            for (size_t v = 0; v < VOICES; ++v) {
                EngravingItem* el = s->element(v);
                if (el && el->isChord()) {
                    return true;
                }
            }
        }
        return false;
    };

    // Measures 1-3: notes
    EXPECT_TRUE(hasPitchedNotes(measureAt(score, 0))) << "measure 1 must have notes";
    EXPECT_TRUE(hasPitchedNotes(measureAt(score, 1))) << "measure 2 must have notes";
    EXPECT_TRUE(hasPitchedNotes(measureAt(score, 2))) << "measure 3 must have notes";
    // Measures 4-6: virtual empty measures from the single-block multi-measure rest
    EXPECT_FALSE(hasPitchedNotes(measureAt(score, 3))) << "measure 4 must be empty";
    EXPECT_FALSE(hasPitchedNotes(measureAt(score, 4))) << "measure 5 must be empty";
    EXPECT_FALSE(hasPitchedNotes(measureAt(score, 5))) << "measure 6 must be empty";
    // Measures 7-9: notes
    EXPECT_TRUE(hasPitchedNotes(measureAt(score, 6))) << "measure 7 must have notes";

    delete score;
}
