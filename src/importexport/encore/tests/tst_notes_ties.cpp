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
#include "engraving/dom/drumset.h"
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


static Measure* measureAt(MasterScore* score, int n)
{
    int idx = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        if (idx == n) {
            return toMeasure(mb);
        }
        ++idx;
    }
    return nullptr;
}

class Tst_NotesTies : public ::testing::Test, public MTest
{
protected:
    void SetUp() override { setRootDir(ENC_DIR); }
};

TEST_F(Tst_NotesTies, tie_direction_fc_creates_tie)
{
    MasterScore* score = readEncoreScore("notes_tie_dir_fc.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int tieCount = 0;
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
                if (n->tieFor()) {
                    ++tieCount;
                }
            }
        }
    }
    EXPECT_EQ(tieCount, 1) << "expected one tie from the 0xfc TIE element";
    delete score;
}

TEST_F(Tst_NotesTies, tie_direction_02_creates_tie)
{
    MasterScore* score = readEncoreScore("notes_tie_dir_02.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int tieCount = 0;
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
                if (n->tieFor()) {
                    ++tieCount;
                }
            }
        }
    }
    EXPECT_EQ(tieCount, 1) << "expected one tie from the 0x02 TIE dir element";
    delete score;
}

TEST_F(Tst_NotesTies, tie_18byte_intra_chord_arc_no_spurious_tie)
{
    // 4 duplicate 18-byte TIE@0 with arcX1==arcX2==12 (same as POPURRI).
    // Two chord notes p60+p62 at tick=0. Same pitches again at tick=480.
    // Fix: arcX1==arcX2 → isTieStart=false → no ties created.
    MasterScore* score = readEncoreScore("notes_tie_intra_chord_arc_no_spurious.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "intra-chord arc test must produce clean score: " << ret.text();

    int tieCount = 0;
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
                if (n->tieFor()) {
                    ++tieCount;
                }
            }
        }
    }
    EXPECT_EQ(tieCount, 0)
        << "18-byte TIE with arcX1==arcX2 is an intra-chord arc; must produce NO forward ties";
    delete score;
}

TEST_F(Tst_NotesTies, tie_18byte_real_forward_still_creates_tie)
{
    // 18-byte TIE@0 with arcX1=12, arcX2=50 (different x positions → real forward tie).
    // dirByte=0x02 → isTieStart=true. C4@tick=0 must be tied to C4@tick=480.
    MasterScore* score = readEncoreScore("notes_tie_18byte_real_forward.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "18-byte real forward tie must produce clean score: " << ret.text();

    int tieCount = 0;
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
                if (n->tieFor()) {
                    ++tieCount;
                }
            }
        }
    }
    EXPECT_EQ(tieCount, 1)
        << "18-byte TIE with arcX1!=arcX2 is a real forward tie; must still create one Tie";
    delete score;
}

TEST_F(Tst_NotesTies, tie_direction_03_creates_tie)
{
    MasterScore* score = readEncoreScore("notes_tie_dir_03.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int tieCount = 0;
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
                if (n->tieFor()) {
                    ++tieCount;
                }
            }
        }
    }
    EXPECT_EQ(tieCount, 1) << "expected one tie from the 0x03 TIE dir element";
    delete score;
}

TEST_F(Tst_NotesTies, tie_start_flag_on_byte6_creates_tie)
{
    MasterScore* score = readEncoreScore("notes_tie_start_flag_byte6.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    int tieCount = 0;
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
                if (n->tieFor()) {
                    ++tieCount;
                }
            }
        }
    }
    EXPECT_EQ(tieCount, 1)
        << "expected one tie from the +6=0x80 TIE-start-flag element";
    delete score;
}

TEST_F(Tst_NotesTies, tie_source_position_partial_chord)
{
    // notes_tie_partial_chord_source_position.enc: one measure.
    // At tick=0: TIE(18-byte, dir=0x04, flag=0x80, sourcePosition=5) + C#4(pos=0) + A4(pos=5).
    // Later in the same measure: C#4(pos=0) and A4(pos=5) as potential tie receivers.
    //
    // Bug (before fix): isTieStartAt returns true for the entire chord tick, so BOTH
    //   C#4 and A4 register tie-starts. C#4 gets a spurious tie to its later occurrence.
    // Fix: sourcePosition=5 at TIE byte +14 matches only A4 (pos=5). C#4 (pos=0) gets
    //   no tie; A4 gets exactly one tie.
    MasterScore* score = readEncoreScore("notes_tie_partial_chord_source_position.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "partial-chord source-position tie must produce clean score: " << ret.text();

    // Collect all notes with tieFor, keyed by pitch
    std::map<int, int> tiesByPitch;
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
                if (n->tieFor()) {
                    tiesByPitch[n->pitch()]++;
                }
            }
        }
    }

    EXPECT_EQ(tiesByPitch.count(61), 0)
        << "C#4 (pitch=61, pos=0) must NOT get a tie: sourcePosition=5 does not match pos=0";
    EXPECT_EQ(tiesByPitch.count(69), 1)
        << "A4 (pitch=69, pos=5) must get exactly one tie: sourcePosition=5 matches pos=5";

    delete score;
}

TEST_F(Tst_NotesTies, tie_crossmeasure_arcxx_equal_with_startflag)
{
    // notes_tie_crossmeasure_arcxx_equal.enc: two 4/4 measures. Measure 1
    // has a whole note C4 with an 18-byte TIE element where arcX1==arcX2==12
    // (zero horizontal extent) but startFlag=0x80 (explicit tie-start bit).
    // Measure 2 has a whole note C4.
    //
    // Without fix: arcX1==arcX2 override sets isTieStart=false → no tie.
    // With fix: startFlag=0x80 prevents the override → tie created.
    MasterScore* score = readEncoreScore("notes_tie_crossmeasure_arcxx_equal.enc");
    ASSERT_NE(score, nullptr);

    std::vector<Note*> notes;
    for (MeasureBase* mb = score->first(); mb && notes.size() < 2; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(0);
            if (el && el->isChord()) {
                Chord* c = toChord(el);
                if (!c->notes().empty()) {
                    notes.push_back(c->notes().front());
                }
            }
        }
    }
    ASSERT_GE(notes.size(), 2u);
    ASSERT_EQ(notes[0]->pitch(), 60);
    ASSERT_EQ(notes[1]->pitch(), 60);

    ASSERT_NE(notes[0]->tieFor(), nullptr)
        << "cross-measure tie (arcX1==arcX2, startFlag=0x80) must not be suppressed";
    EXPECT_EQ(notes[0]->tieFor()->endNote(), notes[1])
        << "tie must connect the whole note in measure 1 to the whole note in measure 2";

    delete score;
}

TEST_F(Tst_NotesTies, tie_element_creates_mscore_tie)
{
    // C4 quarter at tick=0 and C4 quarter at tick=240 with TIE element: must link via a Tie.
    // Bug: TIE elements (type=3) were discarded. Fix: EncTie + pendingTieNote creates Factory::createTie().
    MasterScore* score = readEncoreScore("notes_tie.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Tie test must produce clean score: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    // Find the first chord (the tie-start note)
    Chord* first = nullptr;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            first = toChord(e);
            break;
        }
    }
    ASSERT_NE(first, nullptr) << "Must have a first chord";
    ASSERT_GE(first->notes().size(), 1u);

    Note* startNote = first->notes()[0];
    EXPECT_EQ(startNote->pitch(), 60) << "First note must be C4 (pitch=60)";

    // The note must have a tieFor() pointing to the next note
    ASSERT_NE(startNote->tieFor(), nullptr)
        << "TIE element must create a Tie from the first C4 to the second C4";
    EXPECT_NE(startNote->tieFor()->endNote(), nullptr)
        << "Tie must link to an end note";
    if (startNote->tieFor()->endNote()) {
        EXPECT_EQ(startNote->tieFor()->endNote()->pitch(), 60)
            << "Tie end note must also be C4";
    }
    delete score;
}

TEST_F(Tst_NotesTies, sf_tiestart_not_filtered_by_rdur)
{
    // 64th note at tick=0 with rdur=11 (<15) and a TIE element: must not be filtered.
    // Fix: 64th/128th notes with a TIE element at their tick bypass the rdur<15 filter.
    MasterScore* score = readEncoreScore("notes_sf_tiestart.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "sf tiestart test must produce clean score: " << ret.text();

    Measure* m = measureAt(score, 0);
    ASSERT_NE(m, nullptr);

    // First ChordRest must be the 64th note
    Chord* first = nullptr;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChord()) {
            first = toChord(e);
            break;
        }
    }
    ASSERT_NE(first, nullptr) << "Must have a first chord (the 64th)";
    EXPECT_EQ(first->durationType().type(), DurationType::V_64TH)
        << "64th note with TIE element must not be filtered by rdur<15";
    ASSERT_GE(first->notes().size(), 1u);

    Note* sfNote = first->notes()[0];
    ASSERT_NE(sfNote->tieFor(), nullptr)
        << "64th note must have an outgoing tie to the Q note";
    if (sfNote->tieFor()) {
        Note* endNote = sfNote->tieFor()->endNote();
        ASSERT_NE(endNote, nullptr);
        EXPECT_EQ(endNote->chord()->durationType().type(), DurationType::V_QUARTER)
            << "Tie end must be the Q note";
    }
    delete score;
}

