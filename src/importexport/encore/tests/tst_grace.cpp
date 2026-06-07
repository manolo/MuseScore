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

#include "engraving/dom/articulation.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/ornament.h"
#include "engraving/dom/segment.h"

#include "testbase.h"

static const QString ENC_DIR(QString(iex_encore_tests_DATA_ROOT) + "/data/");

using namespace mu::engraving;

class Tst_Grace : public ::testing::Test, public MTest
{
protected:
    void SetUp() override { setRootDir(ENC_DIR); }
};

#define ENC_SANITY_TEST(testName, fileName) \
    TEST_F(Tst_Grace, testName) { \
        MasterScore* score = readEncoreScore(fileName); \
        ASSERT_NE(score, nullptr) << "Failed to load " << fileName; \
        EXPECT_GT(score->nmeasures(), 0); \
        muse::Ret ret = score->sanityCheck(); \
        EXPECT_TRUE(ret) << "Corrupted: " << ret.text(); \
        delete score; \
    }

// Regression: grace chords attached to a Segment caused beam layout to assert in Chord::pagePos.
// readEncoreScore runs doLayout; crash-free load + structural invariant (grace in graceNotes(), segment chord NORMAL).
TEST_F(Tst_Grace, grace_with_beamed_eighths_no_layout_crash)
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

// BUG: notes with grace1 & 0x30 == 0x30 (both the 0x20 and 0x10 bits set) were
// misread as appoggiaturas and, having no principal chord to attach to, discarded.
// Only 0x20 (appoggiatura) and 0x10 (inner grace) are grace markers; 0x30 is a
// normal note. The fixture has four such quarter notes; all must survive as
// normal chords. Before the fix the measure held only a rest.
TEST_F(Tst_Grace, grace1_0x30_is_normal_note_not_discarded)
{
    MasterScore* score = readEncoreScore("importer_grace1_0x30_normal_notes.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_GT(score->nmeasures(), 0);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "Corrupted: " << ret.text();

    int normalChords = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->elist()) {
                if (e && e->isChord() && toChord(e)->noteType() == NoteType::NORMAL) {
                    ++normalChords;
                    EXPECT_TRUE(toChord(e)->graceNotes().empty())
                        << "grace1==0x30 notes must not be attached as grace notes";
                }
            }
        }
    }
    EXPECT_EQ(normalChords, 4)
        << "All four grace1==0x30 notes must be imported as normal chords, not discarded";
    delete score;
}

// FIX: the grace path applied articulations to a still-detached grace chord, where it could
// only create plain Articulations. A trill byte therefore produced a plain Articulation
// (isOrnament()==false) instead of an Ornament. Articulations are now applied after the grace
// is attached, sharing the main note path, so ornaments/fermatas/dedup work on grace notes.
// Fixture: a normal note + an acciaccatura grace carrying artic byte 0x04 (ornamentTrill).
TEST_F(Tst_Grace, grace_articulation_becomes_ornament)
{
    MasterScore* score = readEncoreScore("grace_ornament.enc");
    ASSERT_NE(score, nullptr);

    bool foundOrnamentOnGrace = false;
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
                for (Chord* gc : toChord(e)->graceNotes()) {
                    for (Articulation* a : gc->articulations()) {
                        if (a->isOrnament()) {
                            foundOrnamentOnGrace = true;
                        }
                    }
                }
            }
        }
    }
    EXPECT_TRUE(foundOrnamentOnGrace)
        << "trill artic byte on a grace note must become an Ornament, not a plain Articulation";

    delete score;
}

// Covers: grace note filtering (fv>=4 only), ACCIACCATURA
ENC_SANITY_TEST(grace_notes, "notes_grace.enc")
