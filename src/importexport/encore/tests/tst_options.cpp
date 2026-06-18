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

#include "engraving/compat/scoreaccess.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/stafftext.h"
#include "engraving/dom/tempotext.h"
#include "engraving/style/style.h"
#include "engraving/types/fraction.h"

#include "../internal/importer/import-options.h"

#include "testbase.h"

static const QString ENC_DIR(QString(iex_encore_tests_DATA_ROOT) + "/data/");

using namespace mu::engraving;
using namespace mu::iex::enc;

class Tst_Options : public ::testing::Test, public MTest
{
protected:
    void SetUp() override { setRootDir(ENC_DIR); }
};

// ===========================================================================
// importPageLayout
// ===========================================================================

TEST_F(Tst_Options, importPageLayout_false_keeps_ms_default_top_margin)
{
    MasterScore* ref = compat::ScoreAccess::createMasterScoreWithBaseStyle(nullptr);
    const double defaultTop = ref->style().styleD(Sid::pageOddTopMargin);
    delete ref;

    EncImportOptions opts;
    opts.importPageLayout = false;
    MasterScore* score = readEncoreScoreWithOpts("bazo.enc", opts);
    ASSERT_NE(score, nullptr);
    EXPECT_DOUBLE_EQ(score->style().styleD(Sid::pageOddTopMargin), defaultTop);
    delete score;
}

TEST_F(Tst_Options, importPageLayout_true_overrides_default_top_margin)
{
    // bazo_top_100 encodes top margin = 100 pt; this must differ from any reasonable default.
    MasterScore* ref = compat::ScoreAccess::createMasterScoreWithBaseStyle(nullptr);
    const double defaultTop = ref->style().styleD(Sid::pageOddTopMargin);
    delete ref;

    MasterScore* score = readEncoreScore("bazo_top_100.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_NE(score->style().styleD(Sid::pageOddTopMargin), defaultTop)
        << "bazo_top_100 must produce a top margin different from the MS default";
    delete score;
}

// ===========================================================================
// importTempoTextSemantic
// ===========================================================================

// text_stafftext_tempo_promotion has "Allegro" as a STAFFTEXT element.
// Default: promoted to TempoText.  With importTempoTextSemantic=false: stays StaffText.
TEST_F(Tst_Options, importTempoTextSemantic_false_keeps_italian_term_as_stafftext)
{
    EncImportOptions opts;
    opts.importTempoTextSemantic = false;
    MasterScore* score = readEncoreScoreWithOpts("text_stafftext_tempo_promotion.enc", opts);
    ASSERT_NE(score, nullptr);

    bool foundAllegroAsStaffText = false;
    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        for (Segment* s = m->first(SegmentType::ChordRest); s;
             s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (!e) {
                    continue;
                }
                if (e->isTempoText()) {
                    const String text = toTempoText(e)->plainText();
                    EXPECT_FALSE(text.contains(String(u"Allegro"), muse::CaseInsensitive))
                        << "Italian term must not be promoted to TempoText when semantic mode is off";
                }
                if (e->isStaffText()
                    && toStaffText(e)->plainText().contains(String(u"Allegro"),
                                                            muse::CaseInsensitive)) {
                    foundAllegroAsStaffText = true;
                }
            }
        }
    }
    EXPECT_TRUE(foundAllegroAsStaffText)
        << "Allegro should remain as StaffText when importTempoTextSemantic=false";
    delete score;
}

// Default opts: "Allegro" is promoted to TempoText (existing behavior, regression guard).
TEST_F(Tst_Options, importTempoTextSemantic_true_promotes_italian_term_to_tempotext)
{
    MasterScore* score = readEncoreScore("text_stafftext_tempo_promotion.enc");
    ASSERT_NE(score, nullptr);

    bool foundAllegroAsTempoText = false;
    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        for (Segment* s = m->first(SegmentType::ChordRest); s;
             s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e && e->isTempoText()
                    && toTempoText(e)->plainText().contains(String(u"Allegro"),
                                                            muse::CaseInsensitive)) {
                    foundAllegroAsTempoText = true;
                }
            }
        }
    }
    EXPECT_TRUE(foundAllegroAsTempoText)
        << "Allegro must be promoted to TempoText under default (semantic=true) opts";
    delete score;
}

// ===========================================================================
// underfillMeasureStrategy
// ===========================================================================

// Default (InvisibleRests): trailing silence in underfull voices becomes gap rests.
// VisibleRests: no gap rests should be present.
TEST_F(Tst_Options, underfill_default_creates_gap_rests)
{
    // structure_pickup_casea_sparse has sparse voices, producing gap rests by default.
    MasterScore* score = readEncoreScore("structure_pickup_casea_sparse.enc");
    ASSERT_NE(score, nullptr);

    int gapCount = 0;
    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        for (Segment* s = m->first(SegmentType::ChordRest); s;
             s = s->next(SegmentType::ChordRest)) {
            for (track_idx_t track = 0; track < score->ntracks(); ++track) {
                EngravingItem* e = s->element(track);
                if (e && e->isRest() && toRest(e)->isGap()) {
                    ++gapCount;
                }
            }
        }
    }
    EXPECT_GT(gapCount, 0)
        << "Default InvisibleRests must produce at least one gap rest in this file";
    delete score;
}

TEST_F(Tst_Options, underfill_visible_rests_produces_no_gap_rests)
{
    EncImportOptions opts;
    opts.underfillMeasureStrategy = UnderfillStrategy::VisibleRests;
    MasterScore* score = readEncoreScoreWithOpts("structure_pickup_casea_sparse.enc", opts);
    ASSERT_NE(score, nullptr);

    int gapCount = 0;
    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        for (Segment* s = m->first(SegmentType::ChordRest); s;
             s = s->next(SegmentType::ChordRest)) {
            for (track_idx_t track = 0; track < score->ntracks(); ++track) {
                EngravingItem* e = s->element(track);
                if (e && e->isRest() && toRest(e)->isGap()) {
                    ++gapCount;
                }
            }
        }
    }
    EXPECT_EQ(gapCount, 0)
        << "VisibleRests strategy must not produce any gap (invisible) rests";
    delete score;
}

// ===========================================================================
// firstMeasureIsPickup
// ===========================================================================

// Default: first measure is shortened to the pickup duration (1/4 for this file).
// firstMeasureIsPickup=false: first measure keeps its full nominal duration.
TEST_F(Tst_Options, firstMeasure_default_is_shortened_to_pickup)
{
    MasterScore* score = readEncoreScore("structure_pickup_measure.enc");
    ASSERT_NE(score, nullptr);
    Measure* m0 = score->firstMeasure();
    ASSERT_NE(m0, nullptr);
    EXPECT_NE(m0->ticks(), m0->timesig())
        << "Default: first measure must be shortened as pickup";
    delete score;
}

TEST_F(Tst_Options, firstMeasure_not_pickup_keeps_full_nominal_duration)
{
    EncImportOptions opts;
    opts.firstMeasureIsPickup = false;
    MasterScore* score = readEncoreScoreWithOpts("structure_pickup_measure.enc", opts);
    ASSERT_NE(score, nullptr);
    Measure* m0 = score->firstMeasure();
    ASSERT_NE(m0, nullptr);
    EXPECT_EQ(m0->ticks(), m0->timesig())
        << "firstMeasureIsPickup=false: first measure must retain full nominal duration";
    delete score;
}
