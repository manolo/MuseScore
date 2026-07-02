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
#include "engraving/dom/layoutbreak.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/volta.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/part.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftext.h"
#include "engraving/dom/stafftype.h"
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
// importPageBreaks
// structure_page_break.enc: 2 LINE blocks, both pageIdx=0 → page break after
// the last measure of the first system.
// ===========================================================================

TEST_F(Tst_Options, importPageBreaks_true_places_page_break)
{
    MasterScore* score = readEncoreScore("structure_page_break.enc");
    ASSERT_NE(score, nullptr);

    bool foundPageBreak = false;
    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        for (EngravingItem* e : m->el()) {
            if (e && e->isLayoutBreak() && toLayoutBreak(e)->isPageBreak()) {
                foundPageBreak = true;
                break;
            }
        }
        if (foundPageBreak) {
            break;
        }
    }
    EXPECT_TRUE(foundPageBreak)
        << "Default (importPageBreaks=true): score must contain at least one page break";
    delete score;
}

TEST_F(Tst_Options, importPageBreaks_false_produces_no_page_breaks)
{
    EncImportOptions opts;
    opts.importPageBreaks = false;
    MasterScore* score = readEncoreScoreWithOpts("structure_page_break.enc", opts);
    ASSERT_NE(score, nullptr);

    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        for (EngravingItem* e : m->el()) {
            EXPECT_FALSE(e && e->isLayoutBreak() && toLayoutBreak(e)->isPageBreak())
                << "importPageBreaks=false must produce no page breaks";
        }
    }
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

// A partial gap must be filled with exact-valued rests, not a whole-measure (V_MEASURE) rest:
// a V_MEASURE rest renders as a centered whole rest whatever its real duration, which is wrong
// next to notes. A fully empty measure may still hold a whole-measure rest, so only measures
// that contain a note are checked.
TEST_F(Tst_Options, visible_rests_use_exact_durations_not_whole_measure)
{
    EncImportOptions opts;
    opts.underfillMeasureStrategy = UnderfillStrategy::VisibleRests;
    MasterScore* score = readEncoreScoreWithOpts("structure_pickup_casea_sparse.enc", opts);
    ASSERT_NE(score, nullptr);

    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        bool hasNote = false;
        for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            for (track_idx_t t = 0; t < score->ntracks(); ++t) {
                const EngravingItem* e = s->element(t);
                if (e && e->isChord()) {
                    hasNote = true;
                }
            }
        }
        if (!hasNote) {
            continue;
        }
        for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            for (track_idx_t t = 0; t < score->ntracks(); ++t) {
                const EngravingItem* e = s->element(t);
                if (e && e->isRest()) {
                    EXPECT_NE(toRest(e)->durationType().type(), DurationType::V_MEASURE)
                        << "partial-gap fill must use exact rest durations, not a whole-measure rest";
                }
            }
        }
    }
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

// Regression: when firstMeasureIsPickup=false and underfillMeasureStrategy=IrregularMeasure,
// buildMeasures advanced currentTick by ts.ticks() (the explicit pickup duration) while
// setting measure->ticks(nominalTimeSig).  The mismatch made IrregularMeasure shift all
// subsequent measures by the wrong delta, placing volta brackets mid-measure instead of at
// barlines.  File: Case A pickup (ts[0]=2/4, nominal=4/4), volta on MEAS[2] and MEAS[3].
static Volta* findVolta(MasterScore* score, const String& label)
{
    for (auto& kv : score->spanner()) {
        Spanner* sp = kv.second;
        if (sp && sp->isVolta() && toVolta(sp)->beginText() == label) {
            return toVolta(sp);
        }
    }
    return nullptr;
}

static bool isAtImpliedBarline(Volta* volta, MasterScore* score)
{
    Fraction cumTick(0, 1);
    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        if (cumTick == volta->tick()) {
            return true;
        }
        cumTick += m->ticks();
    }
    return false;
}

TEST_F(Tst_Options, firstMeasure_not_pickup_irregular_volta_at_barline)
{
    EncImportOptions opts;
    opts.firstMeasureIsPickup = false;
    opts.underfillMeasureStrategy = UnderfillStrategy::IrregularMeasure;
    MasterScore* score = readEncoreScoreWithOpts("structure_pickup_casea_volta.enc", opts);
    ASSERT_NE(score, nullptr);

    Volta* v1 = findVolta(score, String(u"1."));
    Volta* v2 = findVolta(score, String(u"2."));
    ASSERT_NE(v1, nullptr) << "score must contain a '1.' volta";
    ASSERT_NE(v2, nullptr) << "score must contain a '2.' volta";

    EXPECT_TRUE(isAtImpliedBarline(v1, score))
        << "Volta '1.' tick (" << v1->tick().ticks()
        << ") must coincide with a measure barline (cumulative durations)";
    EXPECT_TRUE(isAtImpliedBarline(v2, score))
        << "Volta '2.' tick (" << v2->tick().ticks()
        << ") must coincide with a measure barline (cumulative durations)";
    delete score;
}

TEST_F(Tst_Options, firstMeasure_pickup_irregular_volta_at_barline)
{
    EncImportOptions opts;
    opts.underfillMeasureStrategy = UnderfillStrategy::IrregularMeasure;
    MasterScore* score = readEncoreScoreWithOpts("structure_pickup_casea_volta.enc", opts);
    ASSERT_NE(score, nullptr);

    Volta* v1 = findVolta(score, String(u"1."));
    Volta* v2 = findVolta(score, String(u"2."));
    ASSERT_NE(v1, nullptr) << "score must contain a '1.' volta";
    ASSERT_NE(v2, nullptr) << "score must contain a '2.' volta";

    EXPECT_TRUE(isAtImpliedBarline(v1, score))
        << "Volta '1.' tick (" << v1->tick().ticks()
        << ") must coincide with a measure barline (pickup=true, regression guard)";
    EXPECT_TRUE(isAtImpliedBarline(v2, score))
        << "Volta '2.' tick (" << v2->tick().ticks()
        << ") must coincide with a measure barline (pickup=true, regression guard)";
    delete score;
}

// ===========================================================================
// importSystemLocks
// ===========================================================================

TEST_F(Tst_Options, importSystemLocks_true_creates_system_locks)
{
    MasterScore* score = readEncoreScore("structure_system_break.enc");
    ASSERT_NE(score, nullptr);
    Measure* m0 = score->firstMeasure();
    ASSERT_NE(m0, nullptr);
    EXPECT_TRUE(m0->isStartOfSystemLock())
        << "Default: first measure must be start of a SystemLock";
    delete score;
}

TEST_F(Tst_Options, importSystemLocks_false_produces_no_system_locks)
{
    EncImportOptions opts;
    opts.importSystemLocks = false;
    MasterScore* score = readEncoreScoreWithOpts("structure_system_break.enc", opts);
    ASSERT_NE(score, nullptr);
    bool foundLock = false;
    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        if (m->isStartOfSystemLock() || m->isEndOfSystemLock()) {
            foundLock = true;
            break;
        }
    }
    EXPECT_FALSE(foundLock) << "importSystemLocks=false must produce no SystemLocks";
    delete score;
}

// ===========================================================================
// importStaffSize
// All test files in data/ have scoreSize=3, which maps to MAG 1.00 (100%).
// ===========================================================================

TEST_F(Tst_Options, importStaffSize_true_applies_encore_scale)
{
    MasterScore* score = readEncoreScore("bazo.enc");
    ASSERT_NE(score, nullptr);
    // scoreSize=3 → kScaleBySize[2] = 1.00 (100%)
    const double mag = score->staff(0)->staffType(Fraction(0, 1))->userMag();
    EXPECT_DOUBLE_EQ(mag, 1.00)
        << "importStaffSize=true (default) must apply Encore scoreSize=3 → MAG 1.00";
    delete score;
}

TEST_F(Tst_Options, importStaffSize_false_keeps_unit_scale)
{
    EncImportOptions opts;
    opts.importStaffSize = false;
    MasterScore* score = readEncoreScoreWithOpts("bazo.enc", opts);
    ASSERT_NE(score, nullptr);
    const double mag = score->staff(0)->staffType(Fraction(0, 1))->userMag();
    EXPECT_DOUBLE_EQ(mag, 1.0)
        << "importStaffSize=false must leave staff MAG at the MuseScore default (1.0)";
    delete score;
}

// ===========================================================================
// importUnsupportedArticulationsAsText
// ornaments_open_string_and_stick.enc: note 1 = 0x46 (open string, mapped),
//   note 2 = 0x47 (stick technique, unmapped).
// ===========================================================================

TEST_F(Tst_Options, unsupported_artic_default_drops_silently)
{
    MasterScore* score = readEncoreScore("ornaments_open_string_and_stick.enc");
    ASSERT_NE(score, nullptr);
    // Default: no StaffText emitted for the unmapped 0x47 byte.
    int staffTextCount = 0;
    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        for (Segment* s = m->first(SegmentType::ChordRest); s;
             s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e && e->isStaffText()) {
                    ++staffTextCount;
                }
            }
        }
    }
    EXPECT_EQ(staffTextCount, 0)
        << "Default: unsupported artic bytes must be dropped with no StaffText";
    delete score;
}

TEST_F(Tst_Options, unsupported_artic_as_text_emits_stafftext)
{
    EncImportOptions opts;
    opts.importUnsupportedArticulationsAsText = true;
    MasterScore* score = readEncoreScoreWithOpts("ornaments_open_string_and_stick.enc", opts);
    ASSERT_NE(score, nullptr);
    int staffTextCount = 0;
    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        for (Segment* s = m->first(SegmentType::ChordRest); s;
             s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e && e->isStaffText()) {
                    ++staffTextCount;
                }
            }
        }
    }
    EXPECT_GT(staffTextCount, 0)
        << "importUnsupportedArticulationsAsText=true must emit at least one StaffText for 0x47";
    delete score;
}

// ===========================================================================
// underfillMeasureStrategy = IrregularMeasure
// ===========================================================================

TEST_F(Tst_Options, underfill_irregular_measure_produces_no_gap_rests)
{
    EncImportOptions opts;
    opts.underfillMeasureStrategy = UnderfillStrategy::IrregularMeasure;
    MasterScore* score = readEncoreScoreWithOpts("structure_pickup_casea_sparse.enc", opts);
    ASSERT_NE(score, nullptr);
    int gapCount = 0;
    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        for (Segment* s = m->first(SegmentType::ChordRest); s;
             s = s->next(SegmentType::ChordRest)) {
            for (track_idx_t tr = 0; tr < score->ntracks(); ++tr) {
                EngravingItem* e = s->element(tr);
                if (e && e->isRest() && toRest(e)->isGap()) {
                    ++gapCount;
                }
            }
        }
    }
    EXPECT_EQ(gapCount, 0)
        << "IrregularMeasure must not produce any gap rests";
    delete score;
}

TEST_F(Tst_Options, underfill_irregular_measure_passes_sanity_check)
{
    EncImportOptions opts;
    opts.underfillMeasureStrategy = UnderfillStrategy::IrregularMeasure;
    MasterScore* score = readEncoreScoreWithOpts("structure_pickup_casea_sparse.enc", opts);
    ASSERT_NE(score, nullptr);
    const muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "IrregularMeasure: score failed sanity check: " << ret.text();
    delete score;
}

// A bar where only one staff has (sparse) notes and another staff is silent must NOT be
// shrunk by IrregularMeasure: the silent staff is a whole-bar rest, so the longest staff is
// the full bar. The bug measured only the note-bearing staff, shrank the whole bar, shifted
// every following measure and corrupted them. Guards both the no-shrink decision and that the
// following full bars survive intact.
TEST_F(Tst_Options, underfill_irregular_does_not_shrink_bar_with_silent_staff)
{
    EncImportOptions opts;
    opts.underfillMeasureStrategy = UnderfillStrategy::IrregularMeasure;
    MasterScore* score = readEncoreScoreWithOpts("options_underfill_irregular_empty_staff.enc", opts);
    ASSERT_NE(score, nullptr);

    const muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "silent-staff bar corrupted the score: " << ret.text();

    auto staffSum = [](Measure* m, size_t st) {
        Fraction sum(0, 1);
        for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* e = s->element(static_cast<track_idx_t>(st * VOICES));
            if (e && e->isChordRest()) {
                sum += toChordRest(e)->actualTicks();
            }
        }
        return sum;
    };

    // Bar 1 (sparse staff0 + silent staff1) keeps its nominal 4/4; both staves fill it.
    Measure* m0 = score->firstMeasure();
    ASSERT_NE(m0, nullptr);
    Measure* m1 = m0->nextMeasure();
    ASSERT_NE(m1, nullptr);
    EXPECT_EQ(m1->ticks(), Fraction(4, 4)) << "bar with a silent staff must not shrink";
    for (size_t st = 0; st < score->nstaves(); ++st) {
        EXPECT_EQ(staffSum(m1, st), Fraction(4, 4)) << "sparse bar staff " << st << " must fill the bar";
    }

    // The surrounding full bars (0, 2, 3) keep their 4/4 content intact.
    for (Measure* m = m0; m; m = m->nextMeasure()) {
        if (m == m1) {
            continue;
        }
        EXPECT_EQ(m->ticks(), Fraction(4, 4)) << "full bar must stay 4/4";
        for (size_t st = 0; st < score->nstaves(); ++st) {
            EXPECT_EQ(staffSum(m, st), Fraction(4, 4)) << "full bar staff " << st << " content lost";
        }
    }
    delete score;
}

// ===========================================================================
// overfillMeasureStrategy, reserved variants: sanity-only tests
// (StretchLastNote and IrregularMeasure are not yet fully implemented)
// ===========================================================================

TEST_F(Tst_Options, overfill_stretch_last_note_does_not_crash)
{
    EncImportOptions opts;
    opts.overfillMeasureStrategy = OverfillStrategy::StretchLastNote;
    MasterScore* score = readEncoreScoreWithOpts("bazo.enc", opts);
    ASSERT_NE(score, nullptr) << "StretchLastNote strategy must not crash during import";
    delete score;
}

TEST_F(Tst_Options, stretch_compresses_tuplet_keeps_all_notes)
{
    // notes_capped_tuplet_note.enc: 4/4 with 3 plain quarters + a 3:2 quarter triplet
    // that overflows. "Stretch last notes" preserves ALL three triplet notes by
    // compressing the tuplet bracket from a half (480) down to a quarter (240): the
    // members become eighths in a 3:2 group filling the last beat. The tuplet stays
    // intact (3 members) and the measure remains a standard 4/4.
    EncImportOptions opts;
    opts.overfillMeasureStrategy = OverfillStrategy::StretchLastNote;
    MasterScore* score = readEncoreScoreWithOpts("notes_capped_tuplet_note.enc", opts);
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "Stretch-compressed measure must pass sanity check";
    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->ticks(), m->timesig()) << "Compression keeps a standard 4/4 measure";
    Fraction sum(0, 1);
    int tupletMembers = 0;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChordRest()) {
            ChordRest* cr = toChordRest(e);
            sum += cr->actualTicks();
            if (cr->tuplet()) {
                ++tupletMembers;
            }
        }
    }
    EXPECT_EQ(tupletMembers, 3) << "All three triplet notes preserved inside the tuplet";
    EXPECT_EQ(sum, Fraction(4, 4)) << "Voice 0 sums to exactly 4/4";
    delete score;
}

TEST_F(Tst_Options, stretch_falls_back_to_irregular_for_tiny_bracket)
{
    // notes_stretch_irregular_fallback.enc: 3 plain quarters + a 3:2 HALF-note triplet
    // (natural bracket = a whole note). Only a quarter of space is left, so the largest
    // bracket that fits is < half the natural span: Stretch declines to compress and
    // falls back to IrregularMeasure, extending the bar and keeping all three members.
    EncImportOptions opts;
    opts.overfillMeasureStrategy = OverfillStrategy::StretchLastNote;
    MasterScore* score = readEncoreScoreWithOpts("notes_stretch_irregular_fallback.enc", opts);
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "Stretch irregular fallback must pass sanity check";
    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    EXPECT_GT(m->ticks(), m->timesig()) << "Fallback extends the measure past 4/4";
    int tupletMembers = 0;
    for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(0);
        if (e && e->isChordRest() && toChordRest(e)->tuplet()) {
            ++tupletMembers;
        }
    }
    EXPECT_EQ(tupletMembers, 3) << "All three half-note-triplet members preserved";
    delete score;
}

TEST_F(Tst_Options, overfill_irregular_measure_does_not_crash)
{
    EncImportOptions opts;
    opts.overfillMeasureStrategy = OverfillStrategy::IrregularMeasure;
    MasterScore* score = readEncoreScoreWithOpts("bazo.enc", opts);
    ASSERT_NE(score, nullptr) << "IrregularMeasure overfill strategy must not crash during import";
    delete score;
}

// ===========================================================================
// instrumentSearchMode
// ===========================================================================

// Piano mode: all instruments fall back to Grand Piano.
TEST_F(Tst_Options, instrumentSearchMode_piano_assigns_grand_piano_to_all)
{
    EncImportOptions opts;
    opts.instrumentSearchMode = InstrumentSearchMode::Piano;
    MasterScore* score = readEncoreScoreWithOpts("bazo.enc", opts);
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    for (const Part* part : score->parts()) {
        const Instrument* inst = part->instrument();
        ASSERT_NE(inst, nullptr);
        EXPECT_EQ(inst->id(), String(u"grand-piano"))
            << "Piano mode: every instrument must be Grand Piano";
    }
    delete score;
}

// MidiOnly mode: name matching is skipped, only MIDI program drives selection.
// bazo.enc has scoreSize=3 (75%); the instrument is usually resolved by name.
// With MidiOnly, the name-based step is bypassed so a file whose name can't
// be resolved must still produce a valid (non-crashing) result.
TEST_F(Tst_Options, instrumentSearchMode_midi_only_does_not_crash)
{
    EncImportOptions opts;
    opts.instrumentSearchMode = InstrumentSearchMode::MidiOnly;
    MasterScore* score = readEncoreScoreWithOpts("bazo.enc", opts);
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "MidiOnly mode must not produce a corrupt score: " << ret.text();
    delete score;
}

// Default mode: name+MIDI gives a better result than MidiOnly when the name matches.
TEST_F(Tst_Options, instrumentSearchMode_name_and_midi_resolves_bandurria)
{
    // instruments_abbreviated_name_bandurr.enc has name "Bandurr. I" which matches
    // "Bandurria" via substring (after punctuation stripping).
    MasterScore* score = readEncoreScore("instruments_abbreviated_name_bandurr.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    EXPECT_EQ(score->parts().front()->instrument()->id(), String(u"bandurria"))
        << "Name+MIDI default: 'Bandurr. I' must resolve to bandurria template";
    delete score;
}

// ===========================================================================
// Instrument template bracket clearing
// ===========================================================================

// Accordion template has a brace with span=2 that would overflow into the next
// part when the accordion has only 1 staff.  After clearing template brackets,
// no spurious cross-part bracket should appear.
TEST_F(Tst_Options, template_brackets_cleared_no_spurious_brace)
{
    // akordo.enc has multiple instruments; if template bracket clearing fails,
    // layout may crash or produce wrong bracket spans.
    MasterScore* score = readEncoreScore("akordo.enc");
    ASSERT_NE(score, nullptr);
    // Verify no staff has a bracket that overflows past the score's staves.
    for (staff_idx_t si = 0; si < score->nstaves(); ++si) {
        Staff* st = score->staff(si);
        ASSERT_NE(st, nullptr);
        const size_t span = st->bracketSpan(0);
        if (span > 1) {
            EXPECT_LE(si + span, score->nstaves())
                << "Bracket on staff " << si << " spans " << span
                << " but score only has " << score->nstaves() << " staves";
        }
    }
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();
    delete score;
}

TEST_F(Tst_Options, overfill_irregular_crossing_note_keeps_full_duration)
{
    // options_overfill_irregular_facevalue.enc: 4/4, single staff, Q + H + H.
    // The third note (H) starts at cumTick 3/4: only 1/4 remains before the barline.
    // Without Fix C (resolveNoteDuration + advanceCumulativeTick cap bypass in
    // emitters-note.cpp), that 1/4 remaining space would demote it to V_QUARTER.
    // IrregularMeasure must bypass the cap and preserve V_HALF, extending to 5/4.
    EncImportOptions opts;
    opts.overfillMeasureStrategy = OverfillStrategy::IrregularMeasure;
    MasterScore* score = readEncoreScoreWithOpts("options_overfill_irregular_facevalue.enc", opts);
    ASSERT_NE(score, nullptr);
    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    std::vector<ChordRest*> chords;
    for (Segment* seg = m->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
        EngravingItem* el = seg->element(0);
        if (el && el->isChord()) {
            chords.push_back(toChordRest(el));
        }
    }
    ASSERT_EQ(chords.size(), 3u) << "Q + H + H: 3 chords expected";
    EXPECT_EQ(chords[2]->durationType().type(), DurationType::V_HALF)
        << "Crossing note must keep face-value V_HALF, not be capped to V_QUARTER";
    EXPECT_EQ(m->ticks(), m->timesig() + Fraction(1, 4))
        << "Measure must extend to exactly 5/4 (one quarter past 4/4 timesig)";
    delete score;
}

TEST_F(Tst_Options, overfill_irregular_measure_extends_measure_ticks)
{
    // options_overfill_irregular_facevalue.enc: 4/4 measure with Q+H+H.
    // Note 3 (tick=720, fv=half) has realDuration=240 (gap to durTicks=960) but
    // faceValue=half, so realDuration2DurationType returns V_HALF.  With the default
    // Truncate strategy both capping points in emitters-note.cpp shrink it to a
    // quarter, keeping voiceSum=1/1 and leaving the measure at 4/4.  With
    // IrregularMeasure the caps must be skipped, cumTick reaches 5/4, and
    // capMeasureLength extends the measure so ticks() > timesig().
    EncImportOptions opts;
    opts.overfillMeasureStrategy = OverfillStrategy::IrregularMeasure;
    MasterScore* score = readEncoreScoreWithOpts("options_overfill_irregular_facevalue.enc", opts);
    ASSERT_NE(score, nullptr);
    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    EXPECT_NE(m->ticks(), m->timesig())
        << "IrregularMeasure overfill: measure ticks must differ from timesig (measure must be extended)";
    EXPECT_GT(m->ticks(), m->timesig())
        << "IrregularMeasure overfill: measure ticks must be greater than timesig";
    delete score;
}

TEST_F(Tst_Options, overfill_irregular_measure_extends_past_exact_boundary)
{
    // options_overfill_irregular_emitdrop.enc: 4/4 measure with Q+DH+Q+Q.
    // After Q+DH, cumTick = exactly measure->ticks() (4/4).  Without the fix,
    // emitters.cpp drops notes 3 and 4 at the "cumTick >= measure->ticks()" guard
    // before they ever reach the per-note caps in emitters-note.cpp.
    // With IrregularMeasure the guard must be bypassed so capMeasureLength
    // can extend the measure to 6/4 and all four notes are preserved.
    EncImportOptions opts;
    opts.overfillMeasureStrategy = OverfillStrategy::IrregularMeasure;
    MasterScore* score = readEncoreScoreWithOpts("options_overfill_irregular_emitdrop.enc", opts);
    ASSERT_NE(score, nullptr);
    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    // Count Chord elements in voice 0 to diagnose how many notes were emitted
    int chordCount = 0;
    for (Segment* seg = m->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
        EngravingItem* el = seg->element(0);  // track 0 = staff 0 voice 0
        if (el && el->isChord()) {
            ++chordCount;
        }
    }
    EXPECT_EQ(chordCount, 4)
        << "Expected 4 chords (Q+DH+Q+Q) in measure, got " << chordCount;
    EXPECT_GT(m->ticks(), m->timesig())
        << "IrregularMeasure: notes past exact measure boundary must extend the measure";
    delete score;
}

TEST_F(Tst_Options, overfill_irregular_measure_fills_short_staves)
{
    // options_overfill_irregular_twostaves.enc: 2 staves in 4/4.
    // Staff 0: Q+DH+Q+Q (extends to 6/4). Staff 1: Q+Q+Q (only 3/4).
    // After IrregularMeasure extends the measure to 6/4, staff 1 must be
    // filled with a visible rest so that sanityCheck finds no incomplete measures.
    EncImportOptions opts;
    opts.overfillMeasureStrategy = OverfillStrategy::IrregularMeasure;
    MasterScore* score = readEncoreScoreWithOpts("options_overfill_irregular_twostaves.enc", opts);
    ASSERT_NE(score, nullptr);
    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    EXPECT_GT(m->ticks(), m->timesig())
        << "Measure must be extended past 4/4 by the overfilling staff";
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret)
        << "IrregularMeasure: short staves must be filled with rests after extension: " << ret.text();
    delete score;
}

TEST_F(Tst_Options, overfill_irregular_single_staff_sanity_check)
{
    // Same fixture (single staff Q+H+H in 4/4). End-to-end test for Fix C + Fix D:
    // after the measure extends to 5/4 the single staff must be complete
    // (voice 0 sums to the extended measure length), so sanityCheck must pass.
    EncImportOptions opts;
    opts.overfillMeasureStrategy = OverfillStrategy::IrregularMeasure;
    MasterScore* score = readEncoreScoreWithOpts("options_overfill_irregular_facevalue.enc", opts);
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret)
        << "IrregularMeasure single-staff overfill must pass sanityCheck: " << ret.text();
    delete score;
}

TEST_F(Tst_Options, overfill_truncate_caps_crossing_note)
{
    // Negative regression guard for Fix C: Truncate mode must still cap the
    // note that crosses the barline. Same fixture (Q+H+H in 4/4), default strategy.
    // The third note has only 1/4 of space left, so it must NOT be V_HALF,
    // and the measure must stay at 4/4.
    MasterScore* score = readEncoreScore("options_overfill_irregular_facevalue.enc");
    ASSERT_NE(score, nullptr);
    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    std::vector<ChordRest*> chords;
    for (Segment* seg = m->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
        EngravingItem* el = seg->element(0);
        if (el && el->isChord()) {
            chords.push_back(toChordRest(el));
        }
    }
    ASSERT_GE(chords.size(), 3u) << "All 3 notes must be emitted (none before barline)";
    EXPECT_NE(chords[2]->durationType().type(), DurationType::V_HALF)
        << "Truncate mode must cap the crossing note (must not remain V_HALF)";
    EXPECT_EQ(m->ticks(), m->timesig())
        << "Truncate mode must not extend the measure past its time signature";
    delete score;
}

TEST_F(Tst_Options, overfill_truncate_drops_notes_at_barline)
{
    // Negative regression guard for Fix B: Truncate mode must drop notes whose
    // cumTick reaches the barline. options_overfill_irregular_emitdrop.enc: Q+DH+Q+Q.
    // After Q+DH, cumTick = exactly 4/4. Notes 3 and 4 must be dropped by the
    // overflow guard, leaving fewer than 4 chords, and the measure stays 4/4.
    MasterScore* score = readEncoreScore("options_overfill_irregular_emitdrop.enc");
    ASSERT_NE(score, nullptr);
    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    int chordCount = 0;
    for (Segment* seg = m->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
        EngravingItem* el = seg->element(0);
        if (el && el->isChord()) {
            ++chordCount;
        }
    }
    EXPECT_LT(chordCount, 4)
        << "Truncate: notes 3 and 4 start at/past the barline and must be dropped";
    EXPECT_EQ(m->ticks(), m->timesig())
        << "Truncate mode must not extend the measure";
    delete score;
}

// ===========================================================================
// mergeVoices
// importer_merge_voices_non_overlapping.enc: one staff, voice 0 = quarter C4 on
//   beat 1, voice 1 = quarter E4 on beat 2 (the two voices never overlap).
// importer_merge_voices_overlapping.enc: one staff, voice 0 = half C4 over beats
//   1-2, voice 1 = quarter E4 on beat 2 (the two voices overlap in time).
// ===========================================================================

// Count the distinct voices that carry at least one Chord on the given staff.
static int voicesWithChords(MasterScore* score, staff_idx_t staffIdx)
{
    bool used[VOICES] = { false, false, false, false };
    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            for (voice_idx_t v = 0; v < VOICES; ++v) {
                EngravingItem* e = s->element(staffIdx * VOICES + v);
                if (e && e->isChord()) {
                    used[v] = true;
                }
            }
        }
    }
    int count = 0;
    for (bool u : used) {
        if (u) {
            ++count;
        }
    }
    return count;
}

TEST_F(Tst_Options, mergeVoices_default_off_keeps_separate_voices)
{
    // struct fallback used by tests has mergeVoices = false, so the two
    // non-overlapping voices are left as the importer split them.
    MasterScore* score = readEncoreScore("importer_merge_voices_non_overlapping.enc");
    ASSERT_NE(score, nullptr);
    EXPECT_EQ(voicesWithChords(score, 0), 2)
        << "mergeVoices=false (test default) must keep both voices";
    delete score;
}

TEST_F(Tst_Options, mergeVoices_collapses_non_overlapping_voices)
{
    EncImportOptions opts;
    opts.mergeVoices = true;
    MasterScore* score = readEncoreScoreWithOpts("importer_merge_voices_non_overlapping.enc", opts);
    ASSERT_NE(score, nullptr);
    EXPECT_EQ(voicesWithChords(score, 0), 1)
        << "mergeVoices=true must collapse two non-overlapping voices into voice 1";
    EXPECT_TRUE(score->sanityCheck()) << "merged score must pass sanity check";
    delete score;
}

TEST_F(Tst_Options, mergeVoices_keeps_overlapping_voices)
{
    EncImportOptions opts;
    opts.mergeVoices = true;
    MasterScore* score = readEncoreScoreWithOpts("importer_merge_voices_overlapping.enc", opts);
    ASSERT_NE(score, nullptr);
    EXPECT_EQ(voicesWithChords(score, 0), 2)
        << "mergeVoices=true must leave genuinely overlapping voices untouched (all-or-nothing)";
    EXPECT_TRUE(score->sanityCheck()) << "untouched score must pass sanity check";
    delete score;
}

// Regression: a single-chord tremolo sits on the voice-1 note that mergeVoices moves
// into voice 0 (importer_merge_voices_tremolo: voice 0 quarter on beat 1, voice 1
// quarter on beat 2 carrying a per-note R16 tremolo). The voice-change path rebuilds
// the destination chord from scratch and carried over articulations, lyrics and slurs
// but never the tremolo, so the source chord (and its tremolo) was removed and the
// tremolo vanished. Enabling mergeVoices must not drop the tremolo.
TEST_F(Tst_Options, mergeVoices_preserves_single_chord_tremolo)
{
    EncImportOptions opts;
    opts.mergeVoices = true;
    MasterScore* score = readEncoreScoreWithOpts("importer_merge_voices_tremolo.enc", opts);
    ASSERT_NE(score, nullptr);
    EXPECT_TRUE(score->sanityCheck()) << "merged score must pass sanity check";
    EXPECT_EQ(voicesWithChords(score, 0), 1)
        << "mergeVoices=true must collapse the non-overlapping voices into voice 1";

    bool foundTremolo = false;
    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            for (voice_idx_t v = 0; v < VOICES; ++v) {
                EngravingItem* e = s->element(v);
                if (e && e->isChord() && toChord(e)->tremoloSingleChord()) {
                    foundTremolo = true;
                }
            }
        }
    }
    EXPECT_TRUE(foundTremolo)
        << "single-chord tremolo must survive when mergeVoices collapses its voice into voice 1";
    delete score;
}

// Regression: an upper voice that holds only rests over a bar that voice 0 already fills is
// not a real second voice. With "combine non-overlapping voices" on, such stray rests must
// be removed; the importer used to leave them (the staff has no upper-voice notes, so it was
// not even a merge candidate), showing a spurious empty second voice.
TEST_F(Tst_Options, v0c4_merge_removes_stray_upper_voice_rests)
{
    mu::iex::enc::EncImportOptions opts;
    opts.mergeVoices = true;
    MasterScore* score = readEncoreScoreWithOpts("structure_merge_stray_voice_rests.enc", opts);
    ASSERT_NE(score, nullptr) << "Failed to load structure_merge_stray_voice_rests.enc";

    int upperVoiceElems = 0;
    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            for (int v = 1; v < (int)VOICES; ++v) {
                if (s->element(v)) {
                    ++upperVoiceElems;
                }
            }
        }
    }
    EXPECT_EQ(upperVoiceElems, 0)
        << "stray upper-voice rests must be removed when merging voices";
    delete score;
}

// Regression: Encore's "voice 4" is a silent-voice placeholder that routing folds into
// voice 0. A whole-measure rest stored there (face value an eighth, but spanning the bar)
// used to be emitted as a leading eighth rest in voice 0, shifting the real notes right and
// inflating an otherwise-4/4 bar to 9/8. The importer must drop the voice-4 rest when the
// staff already carries real notes.
TEST_F(Tst_Options, v0c4_voice4_rest_dropped_when_staff_has_notes)
{
    mu::iex::enc::EncImportOptions opts;
    opts.overfillMeasureStrategy = mu::iex::enc::OverfillStrategy::IrregularMeasure;
    MasterScore* score = readEncoreScoreWithOpts("structure_voice4_rest_with_notes.enc", opts);
    ASSERT_NE(score, nullptr) << "Failed to load structure_voice4_rest_with_notes.enc";
    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->ticks(), m->timesig())
        << "the redundant voice-4 rest must not inflate the 4/4 bar to 9/8";
    EXPECT_TRUE(score->sanityCheck());
    delete score;
}

// Regression: Encore lays notes out left-to-right, so a note's xoffset column identifies its
// beat consistently across a system. A note edited in Encore can keep a stale MIDI tick that
// no longer matches its column -- it draws at the column's beat but imports one beat late. Here
// a half note drawn in the beat-1 column (xoff 8) but stored at tick 480 (beat 3) must import
// as note (beat 1) + rest (beat 3), not rest + note.
TEST_F(Tst_Options, v0c4_stale_note_tick_snaps_to_xoffset_column)
{
    MasterScore* score = readEncoreScore("structure_stale_tick_by_column.enc");
    ASSERT_NE(score, nullptr) << "Failed to load structure_stale_tick_by_column.enc";
    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);

    ChordRest* firstV1 = nullptr;
    for (Segment* s = m->first(SegmentType::ChordRest); s && !firstV1; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* e = s->element(1);   // voice 1
        if (e && e->isChordRest()) {
            firstV1 = toChordRest(e);
        }
    }
    ASSERT_NE(firstV1, nullptr) << "voice 1 must have content";
    EXPECT_EQ(firstV1->tick(), m->tick())
        << "the stale-tick half note must land on beat 1 (its xoffset column), not beat 3";
    EXPECT_TRUE(firstV1->isChord())
        << "beat 1 must carry the note, with the rest after it";
    EXPECT_TRUE(score->sanityCheck());
    delete score;
}
