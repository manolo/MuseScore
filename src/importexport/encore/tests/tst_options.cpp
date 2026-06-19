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

// ===========================================================================
// overfillMeasureStrategy — reserved variants: sanity-only tests
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
