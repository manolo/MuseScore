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

#include "engraving/dom/chord.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/marker.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/volta.h"

#include "testbase.h"

static const QString ENC_DIR(QString(iex_encore_tests_DATA_ROOT) + "/data/");

using namespace mu::engraving;

// ---------------------------------------------------------------------------
// Test that the importer can open each sample file without crashing
// and produce a non-empty score
// ---------------------------------------------------------------------------

class Tst_Encore : public ::testing::Test, public MTest
{
protected:
    void SetUp() override
    {
        setRootDir(ENC_DIR);
    }
};

TEST_F(Tst_Encore, bazo)
{
    MasterScore* score = readEncoreScore("bazo.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Encore, akordo)
{
    MasterScore* score = readEncoreScore("akordo.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Encore, ripetoj)
{
    MasterScore* score = readEncoreScore("ripetoj.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Encore, opeco_vochoj)
{
    MasterScore* score = readEncoreScore("opeco_vochoj.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Encore, bando)
{
    MasterScore* score = readEncoreScore("bando.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Encore, kordorkestro)
{
    MasterScore* score = readEncoreScore("kordorkestro.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

TEST_F(Tst_Encore, chord_parsing)
{
    MasterScore* score = readEncoreScore("chord_parsing.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Score has corrupted measures: " << ret.text();
    delete score;
}

TEST_F(Tst_Encore, encore_symbols)
{
    // 24-measure reference file the user authored to exercise every
    // visible symbol Encore can place: dynamics, fermatas, tremolos,
    // mordent / trill family, fingerings, technical markings, the four
    // articulation combos, Segno / Coda / To Coda, D.C. / D.S. variants,
    // dotted barlines, multiple bar styles and the full lyric machinery.
    // Provided by the user as a derivative-free demo file.
    MasterScore* score = readEncoreScore("encore_symbols.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Score has corrupted measures: " << ret.text();
    delete score;
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Synthetic files covering behaviors from the Encore 5.0 example files.
// The original example files (Beethoven.enc, Opus27.enc, etc.) are distributed
// with Encore software and cannot be included in an open-source repository.
// ---------------------------------------------------------------------------

#define ENC_SANITY_TEST(testName, fileName) \
    TEST_F(Tst_Encore, testName) { \
        MasterScore* score = readEncoreScore(fileName); \
        ASSERT_NE(score, nullptr) << "Failed to load " << fileName; \
        EXPECT_GT(score->nmeasures(), 0); \
        muse::Ret ret = score->sanityCheck(); \
        EXPECT_TRUE(ret) << "Corrupted: " << ret.text(); \
        delete score; \
    }

// Covers: tuplet=0xFF (degenerate), faceValue=0 (invalid), voice>=4, open SLURSTART
// (previously tested by Beethoven.enc and Opus 27 First Movement.enc)
ENC_SANITY_TEST(corrupted_elements,         "synthetic_v0c4_corrupted.enc")

// Covers: tiny realDuration (<15 ticks) skipped, dotted rest, 2/4 time sig
// (previously tested by Well, Licky Hear.enc)
// No sanityCheck: swing timing files produce slight measure shortfalls (by design).
TEST_F(Tst_Encore, swing_timing)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_swing.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    delete score;
}

// Covers: explicit 3:2 triplets, 3/4 time sig, multi-measure
// (previously tested by Chansonette.enc and other 3/4 files)
ENC_SANITY_TEST(explicit_triplets_3_4,      "synthetic_v0c4_triplets.enc")

// Covers: grace note filtering (fv>=4 only), ACCIACCATURA
// (previously tested by Grace.enc and Beethoven.enc)
ENC_SANITY_TEST(grace_notes,                "synthetic_v0c4_grace.enc")

// Regression for the beam-layout crash on grace + adjacent beamed eighths.
// The old importer attached grace chords to a Segment, so beam layout pulled
// them into a beam group and Chord::pagePos asserted toChord(explicitParent())
// against a Segment instead of the main Chord. readEncoreScore runs doLayout,
// so just loading this fixture proves the path is crash-free. We also assert
// the grace lives under a main chord's graceNotes() and the segment-attached
// chord is NORMAL, which is the structural invariant the fix establishes.
TEST_F(Tst_Encore, grace_with_beamed_eighths_no_layout_crash)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_grace_beam.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0c4_grace_beam.enc";
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

// Regression for the multi-stream voice switch loop. With a single switch the
// importer would happily place a note in a target voice that was already full
// (e.g. occupied by a half rest), overrunning the measure. The loop fix keeps
// switching until it finds a voice with remaining space.
TEST_F(Tst_Encore, multi_stream_switch_skips_voice_filled_by_rest)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_full_voice_skipped.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0c4_full_voice_skipped.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();
    delete score;
}

// Regression: v0xC2 MIDI-recorded measures hold notes at drift ticks (not
// aligned with the face-grid) interleaved with face-grid notes. An earlier
// version of the implicit-silence gap snap (added for the m1 voice-0
// reorder case below) snapped cumTick to the absolute Encore tick whenever
// the gap exceeded CHORD_MIDI_THRESHOLD, which mis-aligned drift positions
// and produced a zero-length rhythmic gap that aborted
// `populateRhythmicList` during layout:
//   Assertion failed: (rtick2 > rtick1), function strongestSubbeatLevelInRange
// The face-grid gate (snap only when e->tick % faceTicks == 0) makes the
// snap a no-op for drift-shifted ticks, so the file imports cleanly.
TEST_F(Tst_Encore, v0c2_multi_stream_drift_imports_cleanly)
{
    MasterScore* score = readEncoreScore("synthetic_v0c2_multi_stream_drift.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0c2_multi_stream_drift.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();
    delete score;
}

// Regression for the implicit-silence gap snap + inflated-rdur guard, with
// the per-staff Key transposition applied on top. The single staff in this
// fixture has Key = "Octave Lower" (keyTransposeSemitones = -12). m1 (3/4)
// carries:
//   - voice 0: two quarter NOTEs at Encore ticks 240 and 480 with NO
//     preceding REST element (the leading silence is encoded only via the
//     tick offset). Without the gap snap, both notes squash to beats 1-2
//     with the rest pushed to the end.
//   - voice 1: a single quarter chord (pitches 64+73) with implicit
//     trailing silence; the inflated-rdur guard keeps it a quarter
//     instead of promoting it to a dotted half ratio (720).
// Combined with Key=-12, every imported pitch sits 12 semitones below the
// binary value so MuseScore plays at the same pitch Encore does.
TEST_F(Tst_Encore, v0c4_octave_lower_implicit_silences)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_octave_lower_implicit_silences.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0c4_octave_lower_implicit_silences.enc";
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
    EXPECT_EQ(pitches, (std::set<int>{ 64 - 12, 73 - 12 }));
    delete score;
}

// End-to-end regression for the full v0xA6 fix chain. A boda-like
// synthetic combines every failure mode the real-world v0xA6 file
// exercised:
//   - 4 instruments at the v0xA6 TK strides (header end 0xA6, TK
//     blocks at 0xA6 / 0xE6 / 0x126 / 0x166)
//   - per-instrument Key bytes [0, 0, -12, -12] at content +42
//   - NOTE MIDI pitch at byte +11 (absolute, not signed offset)
//   - tuplet byte at element +7 (where v0xC4 has grace2)
//   - duplicate REST on one staff (m107-like pattern)
//   - sixteenth-triplet groups on another staff (m131-like)
// Asserts the whole pipeline lands the right pitches per staff,
// preserves both triplet groups, collapses the duplicate REST, and
// produces no spurious fingering / tremolo / fermata glyphs.
TEST_F(Tst_Encore, v0xa6_boda_like_full_pipeline)
{
    MasterScore* score = readEncoreScore("synthetic_v0xa6_boda_like.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0xa6_boda_like.enc";

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
    EXPECT_EQ(staffPitches(0), (std::vector<int>{ 88, 88, 89, 88, 86, 88, 86 }))
        << "B1 pitches survive without Key shift";

    // Staff 1 (B2, Key=0): rest + 2 eighths, no shift.
    EXPECT_EQ(staffElementCount(1), 3) << "B2 must hold rest + 2 chords";
    EXPECT_EQ(staffPitches(1), (std::vector<int>{ 76, 77 }))
        << "B2 pitches survive without Key shift";

    // Staff 2 (Laud, Key=-12): duplicate REST collapsed, m_pitch = binary -12.
    EXPECT_EQ(staffElementCount(2), 3)
        << "Laud must hold exactly rest + 2 chords after duplicate-REST dedupe";
    EXPECT_EQ(staffPitches(2), (std::vector<int>{ 76 - 12, 77 - 12 }))
        << "Laud pitches must drop by Key = -12";

    // Staff 3 (Bajo, Key=-12): 3 eighth notes shifted by -12.
    EXPECT_EQ(staffElementCount(3), 3) << "Bajo holds 3 notes";
    EXPECT_EQ(staffPitches(3), (std::vector<int>{ 57 - 12, 60 - 12, 64 - 12 }))
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

// Regression: the explicit tuplet byte lives at element offset +7 in
// v0xA6 NOTE slots (where v0xC4 has grace2). The v0xC4-shaped EncNote::
// read picks up byte +13 which lands in the padding region for v0xA6
// (= 0). Without an override the importer never sees the 0x32 (3:2)
// triplet marker on real Encore 2.x triplets, so 6 sixteenth-triplets
// collapse to 4 plain sixteenths and the excess notes spill into a
// second MuseScore voice. The fixture is a 2/8 measure carrying six
// 16th notes with tuplet = 0x32 at +7; the imported measure must hold
// six notes split into two Tuplet(3:2) groups.
TEST_F(Tst_Encore, v0xa6_triplet_byte_at_offset_7)
{
    MasterScore* score = readEncoreScore("synthetic_v0xa6_triplet_byte_at_offset_7.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0xa6_triplet_byte_at_offset_7.enc";

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
    const std::vector<int> expected{64, 65, 64, 62, 64, 62};
    EXPECT_EQ(pitches, expected) << "pitches must match the binary in order";

    // Voice 1 must be empty (no spillover).
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EXPECT_EQ(s->element(1), nullptr) << "voice 1 should stay empty";
    }
    delete score;
}

// Regression: v0xA6 sometimes stores two byte-identical REST elements
// back-to-back at the same tick / staff / voice / faceValue (observed
// roughly once per 500 rests in real Encore 2.x files). Encore renders
// the pair as a single rest, so the importer must drop the second one.
// Without the dedupe the second REST advanced cumTick past the measure
// end and the following note spilled into a second MuseScore voice,
// producing "rest, rest, note(v1)+note(v2)" instead of the user's
// "rest, note, note". The fixture is a 3/8 measure with two byte-
// identical 8th rests at tick 0 followed by two 8th notes (MIDI 64);
// after the import the first measure must hold exactly three voice-0
// elements in the right order.
TEST_F(Tst_Encore, v0xa6_duplicate_rest_collapse)
{
    MasterScore* score = readEncoreScore("synthetic_v0xa6_duplicate_rest_collapse.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0xa6_duplicate_rest_collapse.enc";

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

// Regression: v0xA6 file header ends at 0xA6 (174 bytes), not 0xC2 (194
// bytes) as in v0xC2 / v0xC4. EncHeader::read used to skip
// unconditionally to 0xC2, swallowing the TK00 block on every real
// v0xA6 file (its proper position is exactly 0xA6). The fixture places
// TK00 at 0xA6 with Key = -12 and the legacy 0xC2 slot kept untouched.
// With the buggy header end the importer skips past TK00@0xA6, fails to
// find any usable instrument metadata, and the imported pitch stays at
// the binary value (60 = C4). With the fix, TK00@0xA6 is read first,
// Key = -12 is applied, and the imported m_pitch drops to 48 (C3).
TEST_F(Tst_Encore, v0xa6_header_ends_at_0xa6)
{
    MasterScore* score = readEncoreScore("synthetic_v0xa6_header_ends_at_0xa6.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0xa6_header_ends_at_0xa6.enc";

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

// Regression: v0xA6 (Encore 2.x) files have a 174-byte (= 0xA6) file
// header (vs 194 = 0xC2 for v0xC2/v0xC4) and 64-byte TK blocks (vs
// 2158-byte TK blocks in later formats). Two independent bugs combined
// to break the per-instrument Key transposition pipeline:
//   1. EncHeader::read unconditionally skipped to 0xC2, swallowing the
//      first TK block on v0xA6 files. Every per-instrument metadata
//      field was off-by-one: instr[0] got TK01's data, etc.
//   2. The v0xC4 Key reader uses the formula PRG_BASE + n * PRG_STEP
//      which does not apply to the compact v0xA6 TK layout; instead
//      v0xA6 stores the signed-int8 Key byte at TK content offset +42.
// The fixture is a single-instrument v0xA6 file with Key = -12 patched
// at the v0xA6 location. The single note (pitch_offset = 0 -> binary
// C4 = 60) must import at m_pitch = 48 (C3) once the offset is applied.
TEST_F(Tst_Encore, v0xa6_key_transposition_octave_lower)
{
    MasterScore* score = readEncoreScore("synthetic_v0xa6_key_transposition.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0xa6_key_transposition.enc";

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

// Regression: v0xA6 NOTE elements are 10 bytes long but EncNote::read
// consumes 27 bytes from elemStart to reach articulationUp /
// articulationDown. The extra 17 bytes are read straight out of the next
// slot's preamble (tick, typeVoice, size, faceValue, ...). Whatever lands
// in those two byte slots is then fed to fingering, articulation,
// tremolo, fermata and open-string lookups. Real-world v0xA6 scores
// generated thousands of spurious fingerings ("4" glyphs above every
// note) and tremolos. v0xA6 NOTEs carry NO articulation data; EncNote
// must zero the fields when the slot size is too small to actually hold
// them. The fixture is two consecutive quarter notes in a v0xA6 2/4
// measure, the simplest layout where the next slot's faceValue (= 3)
// lands on the previous note's articulationUp byte; the imported score
// must hold zero fingerings, zero articulations, and zero tremolos.
TEST_F(Tst_Encore, v0xa6_no_spurious_articulation_glyphs)
{
    MasterScore* score = readEncoreScore("synthetic_v0xa6_no_spurious_tremolo.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0xa6_no_spurious_tremolo.enc";

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

// Regression: the implicit-silence gap snap used to convert each element's
// Encore tick to a Fraction with denominator `4 * beatTicks`. That formula
// assumed beatTicks always equalled 240 (the quarter), which is true only
// for x/4 meters. Real Encore files in x/8 store beatTicks = 120 (the
// eighth); 4 * 120 = 480 gave a whole-note denominator half the correct
// value, the snap pushed cumTick to twice the intended Fraction, and the
// measure overflowed by one beat. Fix: wholeTicks = beatTicks * timeSigDen
// (= 960 across the corpus). The fixture is a 3/8 measure with beatTicks
// = 120 and a single NOTE at Encore tick 240 (beat 3) with no preceding
// REST -- the implicit-silence pattern that triggers the snap. The
// imported measure must carry the leading-silence shape (rest, rest,
// note) without spilling past the bar line.
TEST_F(Tst_Encore, v0c4_gap_snap_eighth_meter)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_gap_snap_eighth_meter.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0c4_gap_snap_eighth_meter.enc";
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

// Regression: compact-TK files (TK varsize = 112, e.g. v0xC4 SATB choir
// scores saved by Encore 5.0.2) do NOT follow the
// PRG_BASE + n * PRG_STEP layout the importer uses to locate per-staff
// MIDI program and Key transposition bytes. Any non-zero byte at the
// formula-derived Key offset would mis-shift every pitch on that staff.
// The fixture leaves the byte at PRG_BASE - 23 set to +8 and asserts the
// imported pitch stays at the binary value, not 76 + 8 = 84.
TEST_F(Tst_Encore, v0c4_compact_tk_ignores_key_byte)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_compact_tk_ignores_key_byte.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0c4_compact_tk_ignores_key_byte.enc";
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

// Regression: Encore can leave trailing MEAS blocks in the file from
// prior edits that it does not render. The rendered count lives in the
// file header measureCount field at offset 0x34. Mamae_eu_quero-Bateria
// stores 56 MEAS blocks but rendered measureCount = 36; importing all 56
// produced 20 ghost measures of stale content past the real end of the
// piece. The importer must stop appending MEAS blocks once it has loaded
// header.measureCount of them. The fixture writes measureCount = 2 with
// 6 MEAS blocks on disk and asserts the imported score has exactly 2
// measures.
TEST_F(Tst_Encore, v0c4_header_measure_count_truncates_ghost_measures)
{
    MasterScore* score = readEncoreScore(
        "synthetic_v0c4_header_measure_count_truncates_ghost_measures.enc");
    ASSERT_NE(score, nullptr)
        << "Failed to load header_measure_count_truncates_ghost_measures.enc";
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

// Regression: Encore tags every measure inside a volta with the same
// repeatAlternative bitmask (m1=0x01, m2=0x01, m3=0x02). The importer
// previously created one Volta per measure (3 voltas), and never set
// begin-text on them, so the bracket rendered without "1." or "2." above.
// Two fixes coalesce equal-bitmask runs into one Volta and set begin-text
// from the endings list. The fixture exercises both: 3 measures with alt
// bits 1/1/2 must import as exactly 2 voltas whose texts are "1." and "2."
TEST_F(Tst_Encore, v0c4_volta_coalesce_and_numbered_text)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_volta_coalesce_and_text.enc");
    ASSERT_NE(score, nullptr)
        << "Failed to load synthetic_v0c4_volta_coalesce_and_text.enc";

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

// Regression: Encore stores the source measure of "To Coda" with coda-
// byte = 0x85 (CODA1) and the destination measure carrying the Coda
// glyph with coda-byte = 0x89 (CODA2). The importer used to map both to
// MarkerType::CODA, losing the distinction so the user saw a duplicate
// Coda glyph where Encore showed the "To Coda" text label. The fixture
// places 0x85 on m1 and 0x89 on m2 and asserts the imported markers are
// TOCODA then CODA in order.
TEST_F(Tst_Encore, v0c4_to_coda_distinct_from_coda)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_to_coda_vs_coda_marker.enc");
    ASSERT_NE(score, nullptr)
        << "Failed to load synthetic_v0c4_to_coda_vs_coda_marker.enc";

    std::vector<MarkerType> orderedTypes;
    int measureIdx = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        ++measureIdx;
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

// Regression: many octave-transposing instruments (laud, classical guitar,
// electric bass, ...) are written in Encore with a plain G clef even
// though their MuseScore instrument template carries the octave-bassa
// variant (G8_VB). When the Encore Key transposition matches the template
// clef's octave decoration, the importer overrides the staff clef with
// the template's so the notes sit at the same staff position the user saw
// in Encore. The fixture is a Laud staff with Key = -12 and a plain G in
// Encore; the imported staff must carry G8_VB.
TEST_F(Tst_Encore, v0c4_octave_bassa_clef_override)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_octave_bassa_clef_override.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0c4_octave_bassa_clef_override.enc";
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

    // The Key = -12 stored in the binary must also apply: written pitch 76
    // becomes m_pitch 64. Combined with the G8_VB clef, the note sits at the
    // E5 staff position the user saw in Encore while sounding E4.
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

// Regression: when the matched MuseScore template has a DISTINCT pair of
// clefs (concertClef = F8_VB, transposingClef = F + transposeChromatic =
// -12, e.g. the bass-guitar / 5-string-electric-bass templates), the
// importer prefers the PLAIN transposing clef over the octave-decorated
// concert clef. The instrument's transposeChromatic still places the
// noteheads at the same staff position the concert clef would render
// them at, but the staff carries Encore's plain-F glyph.
TEST_F(Tst_Encore, v0c4_bass_guitar_transposing_clef)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_bass_guitar_transposing_clef.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0c4_bass_guitar_transposing_clef.enc";
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
    EXPECT_EQ(toClef(clefEl)->clefType(), ClefType::F)
        << "staff must carry the template's plain transposing F clef, "
           "NOT the F8_VB concert clef";

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

// Regression: Encore's Staff Sheet exposes a per-instrument "Key" dropdown
// that adds a chromatic transposition at playback time (range 2 Octaves
// Higher .. Major 20th Lower). The value lives in the binary as a signed
// int8 at PRG_BASE - 23 + n * PRG_STEP and the importer adds it to every
// NOTE's semiTonePitch before setPitch so the resulting MuseScore m_pitch
// matches what Encore plays. The fixture carries two instruments with
// DIFFERENT Key values: staff 0 = Sounds as Written (0), staff 1 = Octave
// Lower (-12). Both staves play the same pitch A4 (binary 69) at m1 beat
// 1; the importer must apply the offset PER STAFF, leaving staff 0 at 69
// and shifting staff 1 to 57.
TEST_F(Tst_Encore, v0c4_key_transposition_per_staff)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_key_per_staff.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0c4_key_per_staff.enc";
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

// Regression for the voice >= VOICES filter combined with the short-name
// instrument-template fallback. The fixture mirrors an SATB choir layout:
// four parts labelled S / C / T / B (1 char each), where the bass staff
// stores its notes with voice = 4 in the typeVoice byte while the lyric
// elements stay on voice 0. Previously the importer accepted voice = 4
// only for ORNAMENT (system-level marks) and silently dropped any other
// element with voice >= VOICES, so the bass staff imported empty. After
// the fix every out-of-range voice maps to voice 0 on the same staff,
// letting the chords land and the lyric-attachment pass anchor onto them.
// Separately, short instrument names (1-3 chars) skip the name+MIDI
// matcher and the MIDI-only fallback (both unreliable in compact-TK
// files); every part falls through to the neutral Grand Piano template,
// preserving the original Encore label as the part's long name.
TEST_F(Tst_Encore, v0c4_satb_short_names_with_voice4_bass_lyrics)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_satb_short_names_voice4_lyrics.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0c4_satb_short_names_voice4_lyrics.enc";
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

// Regression for the non-tuplet branch of the second cap. An explicit-but-
// isolated tuplet note that gets treated as plain (tupAdv != remaining) had
// its chord placed with the face value, but the cumTick advance was capped
// to fit the remaining measure space. Without updating the chord's ticks the
// voice overran by face - capped. The fix always sets the chord duration to
// the capped value.
TEST_F(Tst_Encore, isolated_explicit_tuplet_caps_chord_ticks)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_isolated_explicit_tuplet_capped.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0c4_isolated_explicit_tuplet_capped.enc";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();
    delete score;
}

// Regression for treating a rest as a chord-extension anchor. Previously both
// notes and rests updated prevMidiTick, so a note arriving at the same MIDI
// tick as a recent rest was mis-detected as a chord extension; the importer
// then silently replaced the rest's segment with the note's chord while
// cumTick still carried the rest's contribution, producing voice/cumTick
// mismatch and a measure that fails sanityCheck.
TEST_F(Tst_Encore, rest_does_not_anchor_chord_extension)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_rest_not_chord_anchor.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();
    delete score;
}

// Regression for the rest path's second cap. A rest arriving with a tuplet
// still open had its first cap skipped; the tuplet then closed (rest has no
// tuplet bytes); the second cap shortened the cumTick advance but left the
// rest's ticks at the uncapped face value. cr->actualTicks() then exceeded
// the actual cumTick advance and the voice overran the measure.
TEST_F(Tst_Encore, rest_caps_its_ticks_when_advance_is_capped)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_rest_caps_in_open_tuplet.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();
    delete score;
}

// Regression for the duplicate `if (tt.inTuplet()) tt.placedTicks += advance;`
// in the rest path. The bug double-counted a rest's contribution to the
// tuplet's placedTicks, masking the tuplet's undershoot and skipping the
// closeTuplet shrink. checkMeasure then reported the resulting non-standard
// gap as Incomplete measure. The fixture has a 3:2 quarter triplet whose
// first member is a quarter rest, followed by two eighth-note chords; total
// content sums to 1/3 of the 2/4 measure (= 1/6 + 1/12 + 1/12) and the
// tuplet must shrink to 1/3 so the remaining 1/6 can be filled by checkMeasure.
TEST_F(Tst_Encore, rest_in_tuplet_does_not_double_count_placed_ticks)
{
    MasterScore* score = readEncoreScore("synthetic_v0c4_rest_in_tuplet.enc");
    ASSERT_NE(score, nullptr) << "Failed to load synthetic_v0c4_rest_in_tuplet.enc";
    EXPECT_GT(score->nmeasures(), 0);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();
    delete score;
}

// Cross-cutting sanity coverage for the alMezuro-based spanner endpoint
// resolution (hairpins + slurs) and the partial-tuplet TDuration guard.
// The dedicated behavior tests live in tst_encore_features.cpp; these
// entries make sure each fixture stays green under the simpler "load +
// nmeasures > 0 + sanityCheck" gate that the rest of Tst_Encore uses.
ENC_SANITY_TEST(multi_measure_hairpin,      "synthetic_v0c4_multi_measure_hairpin.enc")
ENC_SANITY_TEST(multi_measure_slur,         "synthetic_v0c4_multi_measure_slur.enc")
ENC_SANITY_TEST(partial_quarter_triplet,    "synthetic_v0c4_partial_quarter_triplet.enc")
ENC_SANITY_TEST(lyrics_attach,              "synthetic_v0c4_lyrics.enc")
ENC_SANITY_TEST(lyrics_variable_length,     "synthetic_v0c4_lyrics_variable.enc")
ENC_SANITY_TEST(lyrics_two_verses,          "synthetic_v0c4_lyrics_two_verses.enc")
ENC_SANITY_TEST(lyrics_hyphenated_words,    "synthetic_v0c4_lyrics_hyphenated_words.enc")
ENC_SANITY_TEST(tie_start_flag_byte6,       "synthetic_v0c4_tie_start_flag_byte6.enc")
ENC_SANITY_TEST(articulations_extended,     "synthetic_v0c4_articulations.enc")
ENC_SANITY_TEST(articulations_combo,        "synthetic_v0c4_articulations_combo.enc")
ENC_SANITY_TEST(trill_mordent,              "synthetic_v0c4_trill_mordent.enc")
ENC_SANITY_TEST(tremolos,                   "synthetic_v0c4_tremolos.enc")
ENC_SANITY_TEST(fermatas,                   "synthetic_v0c4_fermatas.enc")
ENC_SANITY_TEST(technical,                  "synthetic_v0c4_technical.enc")
ENC_SANITY_TEST(trill_spanner,              "synthetic_v0c4_trill_spanner.enc")
ENC_SANITY_TEST(staccato_orn,               "synthetic_v0c4_staccato_orn.enc")
ENC_SANITY_TEST(section_markers,            "synthetic_v0c4_section_markers.enc")
ENC_SANITY_TEST(jump_marks,                 "synthetic_v0c4_jump_marks.enc")
ENC_SANITY_TEST(jump_marks_all,             "synthetic_v0c4_jump_marks_all.enc")
ENC_SANITY_TEST(tie_direction_fc,           "synthetic_v0c4_tie_dir_fc.enc")
ENC_SANITY_TEST(keychange_to_c,             "synthetic_v0c4_keychange_to_c.enc")
ENC_SANITY_TEST(staff_text,                 "synthetic_v0c4_staff_text.enc")
ENC_SANITY_TEST(titl_headers_footers,       "synthetic_v0c4_titl_headers_footers.enc")
ENC_SANITY_TEST(arpeggio,                   "synthetic_v0c4_arpeggio.enc")
ENC_SANITY_TEST(staff_text_placement,       "synthetic_v0c4_staff_text_placement.enc")
ENC_SANITY_TEST(dynamics_size16,            "synthetic_v0c4_dynamics.enc")
ENC_SANITY_TEST(dynamics_full,              "synthetic_v0c4_dynamics_full.enc")
ENC_SANITY_TEST(wedgestart_at_measure_end,  "synthetic_v0c4_wedgestart_at_measure_end.enc")
ENC_SANITY_TEST(double_barline_multi_staff, "synthetic_v0c4_double_barline_multi_staff.enc")
