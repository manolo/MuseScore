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
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/segment.h"

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
