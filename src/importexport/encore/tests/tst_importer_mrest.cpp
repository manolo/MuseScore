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

class Tst_ImporterMrest : public ::testing::Test, public MTest
{
protected:
    void SetUp() override
    {
        setRootDir(ENC_DIR);
    }
};

// Regression: a single MEAS block whose lone REST element has mrestCount > 1
// must expand to that many MuseScore measures.
// Synthetic file: 7 MEAS blocks (notes, notes, notes, mrest=3, notes, notes, notes)
// with 2 LINE blocks (system 1 = MEAS[0..3], system 2 = MEAS[4..6]).
// Expected: 9 MuseScore measures (7 + 2 extra from expansion), measures 1-3 with
// notes, 4-6 empty, 7-9 with notes.  System 1 ends at measure 6, system 2 at 9.
TEST_F(Tst_ImporterMrest, mrest_single_block_expands_and_system_locks_correct)
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

// ===========================================================================
// Regression: mrestCount expansion must not require the successor MEAS block
// to contain pitched notes. A block with mrestCount=3 followed by a plain
// rest measure (not notes) must still expand to 3 MuseScore measures.
// Without the fix, encMeasDisplayCount collapsed the mrest to 1 because
// encMeasHasPitchedNotes(next) returned false for the rest successor.
//
// Synthetic file: 5 real MEAS blocks [note, note, mrest=3, rest, note] plus
// 1 filler block added by assemble() = 6 blocks total.
// Expected: 2 + 3 + 1 + 1 + 1(filler) = 8 MuseScore measures.
// Without fix: 2 + 1 + 1 + 1 + 1(filler) = 6 measures (loses 2 from unexpanded mrest).
// ===========================================================================
TEST_F(Tst_ImporterMrest, mrest_single_block_expands_when_successor_is_rest)
{
    MasterScore* score = readEncoreScore("importer_mrest_followed_by_rest.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_mrest_followed_by_rest.enc";
    ASSERT_EQ(score->nmeasures(), 8)
        << "5 real MEAS blocks + 2 extra from mrestCount=3 + 1 filler; without fix only 6";

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

    // Measures 1-2: notes
    EXPECT_TRUE(hasPitchedNotes(measureAt(score, 0))) << "measure 1 must have notes";
    EXPECT_TRUE(hasPitchedNotes(measureAt(score, 1))) << "measure 2 must have notes";
    // Measures 3-5: virtual empty measures from the mrest expansion (key regression case)
    EXPECT_FALSE(hasPitchedNotes(measureAt(score, 2))) << "measure 3 (mrest) must be empty";
    EXPECT_FALSE(hasPitchedNotes(measureAt(score, 3))) << "measure 4 (mrest) must be empty";
    EXPECT_FALSE(hasPitchedNotes(measureAt(score, 4))) << "measure 5 (mrest) must be empty";
    // Measure 6: the single-rest successor (not a note measure — this is what triggered the bug)
    EXPECT_FALSE(hasPitchedNotes(measureAt(score, 5))) << "measure 6 (single rest) must be empty";
    // Measure 7: note
    EXPECT_TRUE(hasPitchedNotes(measureAt(score, 6))) << "measure 7 must have notes";
    // Measure 8: filler (empty measure added by assemble())
    EXPECT_FALSE(hasPitchedNotes(measureAt(score, 7))) << "measure 8 (filler) must be empty";

    delete score;
}

// ===========================================================================
// Regression: a single MEAS block with mrestCount > 1 must expand even when
// the immediately preceding MEAS block is a plain single-measure rest
// (mrestCount == 1).  Before the fix, encMeasHasSingleRest(*prev) incorrectly
// returned true for any REST element (ignoring mrestCount), so the guard
// suppressed expansion whenever the predecessor happened to be a rest measure.
//
// Synthetic file: [rest][rest][mrest=7][note]  (assemble() pads 4 blocks to min 6)
// Expected: 1 + 1 + 7 + 1 + 1(pad) + 1(pad) = 12 MuseScore measures.
// Without fix: 1 + 1 + 1 + 1 + 1 + 1 = 6 (the mrest=7 collapses to 1).
// ===========================================================================
TEST_F(Tst_ImporterMrest, mrest_single_block_expands_when_preceded_by_rest)
{
    MasterScore* score = readEncoreScore("importer_mrest_preceded_by_rest.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_mrest_preceded_by_rest.enc";
    ASSERT_EQ(score->nmeasures(), 12)
        << "4 MEAS blocks + 2 pad + 6 extra from mrestCount=7; without fix only 6 measures";

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

    // Measures 1-2: plain single-measure rests (the predecessor case that triggered the bug)
    EXPECT_FALSE(hasPitchedNotes(measureAt(score, 0))) << "measure 1 must be empty";
    EXPECT_FALSE(hasPitchedNotes(measureAt(score, 1))) << "measure 2 must be empty";
    // Measures 3-9: the 7-measure multi-measure rest expanded correctly
    for (int i = 2; i < 9; ++i) {
        EXPECT_FALSE(hasPitchedNotes(measureAt(score, i)))
            << "measure " << (i + 1) << " must be empty (mrest expansion)";
    }
    // Measure 10: note after the multi-measure rest
    EXPECT_TRUE(hasPitchedNotes(measureAt(score, 9))) << "measure 10 must have notes";
    // Measures 11-12: padding empty measures added by assemble()
    EXPECT_FALSE(hasPitchedNotes(measureAt(score, 10))) << "measure 11 (pad) must be empty";
    EXPECT_FALSE(hasPitchedNotes(measureAt(score, 11))) << "measure 12 (pad) must be empty";

    delete score;
}

// ===========================================================================
// Regression: a multi-staff file stores one REST element per staff inside the
// mrest MEAS block, so m.elements.size() > 1.  The old guard
// 'if (m.elements.size() != 1) return 1' incorrectly collapsed the block to
// a single measure for any file with more than one staff.
//
// Synthetic file: 2-staff, [mrest=7 (2 staves)][note (2 staves)]
// Expected: 7 + 1 = 8 MuseScore measures.
// Without fix: 1 + 1 = 2 (both staves' REST counted as size==2, suppressed).
// ===========================================================================
TEST_F(Tst_ImporterMrest, mrest_single_block_expands_for_multi_staff_file)
{
    MasterScore* score = readEncoreScore("importer_mrest_multistaff.enc");
    ASSERT_NE(score, nullptr) << "Failed to load importer_mrest_multistaff.enc";
    ASSERT_EQ(score->nmeasures(), 8)
        << "mrest=7 across 2 staves must expand to 7 measures + 1 note; without fix only 2";

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

    // Measures 1-7: virtual empty measures from the 2-staff mrest expansion
    for (int i = 0; i < 7; ++i) {
        EXPECT_FALSE(hasPitchedNotes(measureAt(score, i)))
            << "measure " << (i + 1) << " must be empty (mrest expansion)";
    }
    // Measure 8: note measure (both staves)
    EXPECT_TRUE(hasPitchedNotes(measureAt(score, 7))) << "measure 8 must have notes";

    delete score;
}

// ===========================================================================
// createMultiMeasureRests style flag: only set when the Encore file has at
// least one MEAS block whose lone REST element carries mrestCount > 1.
// Before the fix the flag was always true; files with only individual rests
// were incorrectly getting MMRest display (collapsing separate rest measures).
// ===========================================================================
TEST_F(Tst_ImporterMrest, mmrest_flag_off_when_file_has_no_mrest_blocks)
{
    // bazo.enc has no multi-measure rest blocks — only chord/note content.
    MasterScore* score = readEncoreScore("bazo.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_FALSE(score->style().styleB(Sid::createMultiMeasureRests))
        << "createMultiMeasureRests must be FALSE when the file has no mrestCount>1 blocks";
    delete score;
}

TEST_F(Tst_ImporterMrest, mmrest_flag_on_when_file_has_mrest_block)
{
    // importer_mrest_single_block.enc has a REST with mrestCount=3.
    MasterScore* score = readEncoreScore("importer_mrest_single_block.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->style().styleB(Sid::createMultiMeasureRests))
        << "createMultiMeasureRests must be TRUE when the file contains mrestCount>1 blocks";
    delete score;
}
