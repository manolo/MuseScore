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

#include "../internal/importer/noteloop-internal.h"
#include "testbase.h"

static const QString ENC_DIR(QString(iex_encore_tests_DATA_ROOT) + "/data/");

using namespace mu::engraving;

class Tst_Text : public ::testing::Test, public MTest
{
protected:
    void SetUp() override { setRootDir(ENC_DIR); }
};

// ===========================================================================
// FIX: Encore "-" LYRIC elements are hyphen continuation markers; filter them out and tag adjacent
// syllables with LyricsSyllabic. LaMorenaDeMiCopla m18 "JU - LIO RO -" reproduced the off-by-one shift.
// ===========================================================================
TEST_F(Tst_Text, lyrics_hyphen_separators_dropped_and_set_syllabic)
{
    MasterScore* score = readEncoreScore("text_lyrics_hyphenated_words.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    struct Entry {
        String text;
        LyricsSyllabic syll;
    };
    std::vector<Entry> seen;
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
            for (Lyrics* ly : toChord(el)->lyrics()) {
                seen.push_back({ ly->plainText(), ly->syllabic() });
            }
        }
    }
    ASSERT_EQ(seen.size(), 3u);
    EXPECT_EQ(seen[0].text, String(u"JU"));
    EXPECT_EQ(seen[0].syll, LyricsSyllabic::BEGIN)
        << "JU is followed by a hyphen continuation";
    EXPECT_EQ(seen[1].text, String(u"LIO"));
    EXPECT_EQ(seen[1].syll, LyricsSyllabic::END)
        << "LIO closes the JU-LIO word";
    EXPECT_EQ(seen[2].text, String(u"RO"));
    EXPECT_EQ(seen[2].syll, LyricsSyllabic::BEGIN)
        << "RO is followed by a hyphen continuation past the bar";
    delete score;
}

TEST_F(Tst_Text, lyrics_two_verses_on_voice_0_chord)
{
    MasterScore* score = readEncoreScore("text_lyrics_two_verses.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<String> verse0;
    std::vector<String> verse1;
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
            for (Lyrics* ly : toChord(el)->lyrics()) {
                if (ly->verse() == 0) {
                    verse0.push_back(ly->plainText());
                } else if (ly->verse() == 1) {
                    verse1.push_back(ly->plainText());
                }
            }
        }
    }
    std::vector<String> expectedV0 = { u"JU", u"LIO", u"RO", u"ME" };
    std::vector<String> expectedV1 = { u"Co", u"mo", u"ca", u"pa" };
    EXPECT_EQ(verse0, expectedV0);
    EXPECT_EQ(verse1, expectedV1);
    delete score;
}

TEST_F(Tst_Text, lyrics_variable_length_with_empty_placeholder)
{
    MasterScore* score = readEncoreScore("text_lyrics_variable.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<String> expected = { u"JU", u"LIO", u"RO" };
    std::vector<String> seen;
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
            for (Lyrics* ly : toChord(el)->lyrics()) {
                seen.push_back(ly->plainText());
            }
        }
    }
    EXPECT_EQ(seen, expected);
    delete score;
}

TEST_F(Tst_Text, lyrics_attached_to_chords)
{
    MasterScore* score = readEncoreScore("text_lyrics.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<String> expected = { u"do", u"re", u"mi", u"fa" };
    std::vector<String> seen;
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
            Chord* c = toChord(el);
            for (Lyrics* ly : c->lyrics()) {
                seen.push_back(ly->plainText());
            }
        }
    }
    EXPECT_EQ(seen, expected);
    delete score;
}

// ===========================================================================
// FIX: EncLyric::read() detects per-element encoding; some v0xC4 files store lyrics as Latin-1 (one byte/char).
// Reading as UTF-16 LE produced spurious CJK code units.
// ===========================================================================
TEST_F(Tst_Text, lyrics_latin1_text_decoded_as_one_byte_per_char)
{
    MasterScore* score = readEncoreScore("text_lyrics_latin1.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<String> seen;
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
            for (Lyrics* ly : toChord(el)->lyrics()) {
                seen.push_back(ly->plainText());
            }
        }
    }
    ASSERT_EQ(seen.size(), 2u);
    EXPECT_EQ(seen[0], String(u"txã"));
    EXPECT_EQ(seen[1], String(u"nã"));
    delete score;
}

// ===========================================================================
// FIX: STAFFTEXT matching Italian tempo terms is promoted to TempoText.
// Relative markings ("a tempo") get TempoText without absolute BPS; non-tempo strings stay StaffText.
// ===========================================================================
TEST_F(Tst_Text, staff_text_promoted_to_tempo_for_italian_terms)
{
    MasterScore* score = readEncoreScore("text_stafftext_tempo_promotion.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<String> tempoTexts;
    std::vector<String> staffTexts;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (!e) {
                    continue;
                }
                if (e->isTempoText()) {
                    tempoTexts.push_back(toTempoText(e)->plainText());
                } else if (e->isStaffText()) {
                    staffTexts.push_back(toStaffText(e)->plainText());
                }
            }
        }
    }
    EXPECT_EQ(tempoTexts, (std::vector<String> { u"Allegro", u"a tempo" }))
        << "Allegro and 'a tempo' must reach the score as TempoText, not StaffText";
    EXPECT_EQ(staffTexts, (std::vector<String> { u"ten." }))
        << "Non-tempo words remain plain StaffText";
    delete score;
}

TEST_F(Tst_Text, staff_text_promoted_to_tempo_sets_tempo_map)
{
    MasterScore* score = readEncoreScore("text_stafftext_tempo_promotion.enc");
    ASSERT_NE(score, nullptr);

    // Allegro at measure 0 must use the palette default of 144 BPM (= 2.4 BPS).
    const Fraction tick0(0, 1);
    EXPECT_NEAR(score->tempo(tick0).val, 144.0 / 60.0, 1e-6)
        << "Allegro at tick 0 must set the tempo to 144 BPM";
    delete score;
}

// ===========================================================================
// FIX: TITL multi-line slots of the same category join with \n; headers/footers stack by alignment byte.
// Also: Encore #P/#D/#T tokens are rewritten to MuseScore macros $P/$D/$m.
// ===========================================================================
TEST_F(Tst_Text, multi_slot_text_joined_with_newlines)
{
    MasterScore* score = readEncoreScore("text_multi_slot_stacked_text.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    EXPECT_EQ(score->metaTag(u"composer"),
              u"Vicente Paiva e Jararáca\nAdapt.: Sgt Solano\nBanda de Música do CRPO/VRS")
        << "the three author slots must join into one newline-separated string";
    // Two center-aligned header lines must stack into a single Sid value.
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::oddHeaderC),
              u"Top line one\nTop line two");
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::evenHeaderC),
              u"Top line one\nTop line two");
    // The two footers have different alignments (left + right) and must
    // therefore stay on separate Sids.
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::oddFooterL),  u"Left footer");
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::oddFooterR),  u"Right footer");
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::evenFooterL), u"Left footer");
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::evenFooterR), u"Right footer");
    delete score;
}

// ===========================================================================
// FIX: some files write the TITL block twice. EncTitle::read() clears slot vectors on each pass
// so the second block replaces the first instead of doubling composer/header/footer lines.
// ===========================================================================
TEST_F(Tst_Text, duplicate_titl_block_does_not_double_lines)
{
    MasterScore* score = readEncoreScore("text_duplicate_titl_block.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    // The fixture writes the same TITL block twice; the composer must
    // appear exactly once.
    EXPECT_EQ(score->metaTag(u"composer"), u"Sole Composer")
        << "second TITL block must replace the first instead of appending";
    EXPECT_EQ(score->metaTag(u"workTitle"), u"Duped TITL");
    delete score;
}

// ===========================================================================
// FIX: MEAS header BPM at offset 0 was read but never used, forcing 120 bpm on all imports.
// Post-pass emits TempoText for the first measure and each BPM change, and calls Score::setTempo.
// ===========================================================================
TEST_F(Tst_Text, measure_header_bpm_drives_initial_tempo_and_changes)
{
    MasterScore* score = readEncoreScore("text_tempo_changes.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    // Collect every TempoText in the score with its host measure index.
    struct Found {
        int measureIdx;
        double bps;
        String xmlText;
    };
    std::vector<Found> seen;
    int mi = -1;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        ++mi;
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e && e->isTempoText()) {
                    TempoText* tt = toTempoText(e);
                    seen.push_back({ mi, tt->tempo().val, tt->xmlText() });
                }
            }
        }
    }
    // [100,60,100,60,200,200]: emit TempoText for m1..m5 (initial + changes); m6 same as m5, no mark.
    ASSERT_EQ(seen.size(), 5u);
    EXPECT_EQ(seen[0].measureIdx, 0);
    EXPECT_NEAR(seen[0].bps, 100.0 / 60.0, 1e-6);
    EXPECT_EQ(seen[0].xmlText, u"<sym>metNoteQuarterUp</sym> = 100");
    EXPECT_EQ(seen[1].measureIdx, 1);
    EXPECT_NEAR(seen[1].bps, 60.0 / 60.0, 1e-6);
    EXPECT_EQ(seen[2].measureIdx, 2);
    EXPECT_NEAR(seen[2].bps, 100.0 / 60.0, 1e-6);
    EXPECT_EQ(seen[3].measureIdx, 3);
    EXPECT_NEAR(seen[3].bps, 60.0 / 60.0, 1e-6);
    EXPECT_EQ(seen[4].measureIdx, 4);
    EXPECT_NEAR(seen[4].bps, 200.0 / 60.0, 1e-6);

    // Tempo map must reflect the same changes (sampled at each measure start).
    Measure* m = score->firstMeasure();
    std::vector<int> expected = { 100, 60, 100, 60, 200, 200 };
    for (int i = 0; i < 6 && m; ++i, m = m->nextMeasure()) {
        EXPECT_NEAR(score->tempo(m->tick()).val, expected[i] / 60.0, 1e-6)
            << "measure " << i << " expected " << expected[i] << " BPM";
    }
    delete score;
}

// ===========================================================================
// FIX: ORN TEMPO byte is beat-unit BPM, not quarter-note BPM. Compound meters (6/8, 9/8, 12/8) beat = dotted quarter;
// multiply by 3/2. 6/8 TEMPO=80 → BPS=2.0 (120 qBPM); old code gave 80/60=1.333 (♩.=53).
// ===========================================================================
TEST_F(Tst_Text, orn_tempo_compound_meter_dotted_quarter_bpm)
{
    MasterScore* score = readEncoreScore("text_tempo_orn_compound_68.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    TempoText* tt = nullptr;
    for (MeasureBase* mb = score->first(); mb && !tt; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s && !tt; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e && e->isTempoText()) {
                    tt = toTempoText(e);
                    break;
                }
            }
        }
    }
    ASSERT_NE(tt, nullptr) << "No TempoText found in score";
    EXPECT_NEAR(tt->tempo().val, 120.0 / 60.0, 1e-6)
        << "ORN TEMPO=80 in 6/8 must produce quarterBpm=120 (BPS=2.0), not 80/60";
    EXPECT_EQ(tt->xmlText(),
              u"<sym>metNoteQuarterUp</sym><sym>space</sym><sym>metAugmentationDot</sym> = 80")
        << "Compound-meter tempo must use dotted-quarter sym tags; displayed value must be 80";

    delete score;
}

// ===========================================================================
// BUG FIX: MEAS-header and ORN tempo texts used a raw Unicode note symbol
// (U+2669 "♩") in their xmlText.  TempoText::updateTempo() matches against
// TempoPattern strings that use <sym>metNoteQuarterUp</sym>, so the Unicode
// form never matched and editing the displayed BPM had no effect on playback.
// Fix: tempoXmlText() now emits <sym> tags; all numeric BPM TempoTexts also
// get followText=true so MuseScore keeps the tempo map in sync on edit.
// ===========================================================================
TEST_F(Tst_Text, tempo_text_uses_sym_tags_and_follow_text_enabled)
{
    MasterScore* score = readEncoreScore("ornaments_tempo_sym_followtext.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    struct Found {
        String xmlText;
        bool followText;
        double bps;
    };
    std::vector<Found> seen;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e && e->isTempoText()) {
                    TempoText* tt = toTempoText(e);
                    seen.push_back({ tt->xmlText(), tt->followText(), tt->tempo().val });
                }
            }
        }
    }
    // The fixture has 2 custom measures plus fill measures; at least 2 TempoTexts expected.
    ASSERT_GE(seen.size(), 2u);

    // m1: 4/4 bpm=100 -> simple meter -> quarter sym
    EXPECT_EQ(seen[0].xmlText, u"<sym>metNoteQuarterUp</sym> = 100")
        << "Simple-meter tempo must use metNoteQuarterUp sym tag, not raw unicode";
    EXPECT_TRUE(seen[0].followText)
        << "MEAS-header TempoText must have followText=true so BPM edits update playback";
    EXPECT_NEAR(seen[0].bps, 100.0 / 60.0, 1e-6);

    // m2: 6/8 bpm=80 beat-unit (dotted quarter) -> compound meter
    EXPECT_EQ(seen[1].xmlText,
              u"<sym>metNoteQuarterUp</sym><sym>space</sym><sym>metAugmentationDot</sym> = 80")
        << "Compound-meter tempo must use dotted-quarter sym tags, not raw unicode";
    EXPECT_TRUE(seen[1].followText)
        << "Compound-meter TempoText must have followText=true";
    // MEAS header bpm=120 (quarter-note BPM); BPS = 120/60 = 2.0
    EXPECT_NEAR(seen[1].bps, 120.0 / 60.0, 1e-6);

    delete score;
}

TEST_F(Tst_Text, header_footer_tokens_translated_to_mscore_macros)
{
    MasterScore* score = readEncoreScore("text_header_footer_tokens.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    // header[0] right-aligned -> oddHeaderR + evenHeaderR.
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::oddHeaderR), u"Page $P")
        << "#P must be rewritten to $P (page number on every page)";
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::evenHeaderR), u"Page $P");
    // header[1] center-aligned -> oddHeaderC + evenHeaderC.
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::oddHeaderC), u"Created $D")
        << "#D must be rewritten to $D (creation date)";
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::evenHeaderC), u"Created $D");
    // footer[0] left-aligned -> oddFooterL + evenFooterL.
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::oddFooterL), u"Time $m")
        << "#T must be rewritten to $m (best MuseScore equivalent for 'time')";
    EXPECT_EQ(score->style().styleSt(mu::engraving::Sid::evenFooterL), u"Time $m");
    delete score;
}

// ===========================================================================
// FIX: STAFFTEXT 0x1E (ORN tind byte +32) indexes into the TEXT block for the display string.
// Importer reads TEXT block entries and creates StaffText via the tind-derived index.
// ===========================================================================

TEST_F(Tst_Text, staff_text_resolved_via_text_block)
{
    MasterScore* score = readEncoreScore("text_staff_text.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    // "Allegretto" is promoted to TempoText; the remaining entries stay as StaffText.
    std::vector<String> expected = { u"cresc.", u"dimin.", u"ten." };
    std::vector<String> seen;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e && e->isStaffText()) {
                    seen.push_back(toStaffText(e)->plainText());
                }
            }
        }
    }
    EXPECT_EQ(seen, expected);
    delete score;
}

// ===========================================================================
// FEATURE: STAFFTEXT placement from ORN yoffset: positive keeps ABOVE; negative (Cartesian below staff) maps to BELOW.
// ===========================================================================
TEST_F(Tst_Text, staff_text_placement_from_yoffset)
{
    MasterScore* score = readEncoreScore("text_staff_text_placement.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::map<String, PlacementV> placements;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e && e->isStaffText()) {
                    StaffText* st = toStaffText(e);
                    placements[st->plainText()] = st->placement();
                }
            }
        }
    }
    ASSERT_EQ(placements.size(), 2u) << "two STAFFTEXT elements expected";
    EXPECT_EQ(placements[u"Above"], PlacementV::ABOVE)
        << "yoffset=+10 must keep default ABOVE placement";
    EXPECT_EQ(placements[u"ten"], PlacementV::BELOW)
        << "yoffset=-10 (Cartesian below) must map to PlacementV::BELOW";
    delete score;
}

// ===========================================================================
// UNIT: tempoXmlText() — pure-function tests, no score needed.
// Verifies that the note symbol is always emitted as a <sym> tag (not raw
// Unicode), that beatTicks=360 (dotted quarter) uses the dotted-quarter variant,
// and that beatTicks=240 (quarter) uses the plain quarter.
// displayBpm is always the beat-unit BPM as displayed; no conversion happens inside.
// ===========================================================================
TEST(Tst_TempoXmlText, simple_meter_quarter_sym)
{
    using namespace mu::iex::encore;
    // beatTicks=240 (quarter beat): quarter sym
    EXPECT_EQ(tempoXmlText(120, 240),
              String(u"<sym>metNoteQuarterUp</sym> = 120"));
    EXPECT_EQ(tempoXmlText(80, 240),
              String(u"<sym>metNoteQuarterUp</sym> = 80"));
    EXPECT_EQ(tempoXmlText(100, 240),
              String(u"<sym>metNoteQuarterUp</sym> = 100"));
}

TEST(Tst_TempoXmlText, dotted_quarter_beat_sym)
{
    using namespace mu::iex::encore;
    // beatTicks=360 (dotted-quarter beat): dotted-quarter sym; displayBpm is already the beat-unit value.
    // (For MEAS BPM the caller converts QPM to displayBpm via bpm*2/3 before calling tempoXmlText.)
    EXPECT_EQ(tempoXmlText(80, 360),
              String(u"<sym>metNoteQuarterUp</sym><sym>space</sym><sym>metAugmentationDot</sym> = 80"));
    // beatTicks=360 for 3/8 (dotted-quarter beat, previously incorrectly treated as quarter)
    EXPECT_EQ(tempoXmlText(80, 360),
              String(u"<sym>metNoteQuarterUp</sym><sym>space</sym><sym>metAugmentationDot</sym> = 80"));
}

TEST(Tst_TempoXmlText, non_dotted_eighth_denominators_use_quarter_sym)
{
    using namespace mu::iex::encore;
    // beatTicks=120 (eighth-note beat, e.g. 7/8 with eighth as beat unit): quarter sym
    EXPECT_EQ(tempoXmlText(156, 120),
              String(u"<sym>metNoteQuarterUp</sym> = 156"));
    // Other non-dotted beat values also use quarter sym
    EXPECT_EQ(tempoXmlText(120, 240),
              String(u"<sym>metNoteQuarterUp</sym> = 120"));
}

// ===========================================================================
// FIX: 3/8 pieces with beatTicks=360 (dotted-quarter beat) played at 2/3 speed.
// Old compound check: `numerator > 3` excluded 3/8 (numerator=3). Fix: also
// check beatTicks==360 from the MEAS header so 3/8 files get the 1.5x BPS
// adjustment the same as 6/8.
// Expected: ORN TEMPO=80 in 3/8 (beatTicks=360) → BPS = 80*1.5/60 = 2.0.
// ===========================================================================
TEST_F(Tst_Text, orn_tempo_3_8_dotted_quarter_bps_correct)
{
    MasterScore* score = readEncoreScore("text_orn_tempo_3_8_dotted_quarter.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    TempoText* tt = nullptr;
    for (MeasureBase* mb = score->first(); mb && !tt; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s && !tt; s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e && e->isTempoText()) {
                    tt = toTempoText(e);
                    break;
                }
            }
        }
    }
    ASSERT_NE(tt, nullptr) << "No TempoText found in score";
    EXPECT_NEAR(tt->tempo().val, 80.0 * 1.5 / 60.0, 1e-5)
        << "ORN TEMPO=80 in 3/8 (beatTicks=360) must give BPS=2.0 (dotted-quarter 80), "
        "not 1.333 (plain quarter 80)";
    EXPECT_EQ(tt->xmlText(),
              u"<sym>metNoteQuarterUp</sym><sym>space</sym><sym>metAugmentationDot</sym> = 80")
        << "Display must show dotted-quarter=80, not quarter=80";

    delete score;
}

// ===========================================================================
// FIX: When ORN TEMPO is placed at a later tick (first NOTE, not first REST),
// the MEAS-header BPM guard only checked the segment at measTick (the rest
// segment). It missed the ORN TEMPO, creating two conflicting tempo marks.
// Fix: widen guard to scan all segments in the measure.
// Expected: only ONE TempoText, from ORN TEMPO=63 (not the MEAS BPM=160).
// ===========================================================================
TEST_F(Tst_Text, meas_bpm_wins_over_conflicting_orn_tempo_at_later_tick)
{
    // text_meas_bpm_suppressed_by_orn_tempo_later_tick.enc: 4/4, MEAS bpm=160,
    // quarter REST at tick=0, ORN TEMPO=63 at tick=240 (after the rest).
    // The ORN TEMPO BPM (63) conflicts with the MEAS header BPM (160).
    // MEAS header BPM is authoritative; ORN TEMPO is a visual annotation and is
    // suppressed when it disagrees with the header.
    // Expected: exactly ONE TempoText with BPS=160/60 (from MEAS header).
    MasterScore* score = readEncoreScore("text_meas_bpm_suppressed_by_orn_tempo_later_tick.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<double> bpsValues;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest); s;
             s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e && e->isTempoText()) {
                    bpsValues.push_back(toTempoText(e)->tempo().val);
                }
            }
        }
    }

    EXPECT_EQ(bpsValues.size(), 1u)
        << "Only ONE TempoText must exist; conflicting ORN TEMPO=63 is suppressed "
        "when MEAS header BPM=160 disagrees";

    if (!bpsValues.empty()) {
        EXPECT_NEAR(bpsValues[0], 160.0 / 60.0, 1e-5)
            << "The surviving TempoText must be the MEAS header BPM (160/60), not the ORN TEMPO (63/60)";
    }

    delete score;
}

// ===========================================================================
// BUG FIX: ORN TEMPO misplaced one system before its intended measure
// ===========================================================================

TEST_F(Tst_Text, orn_tempo_mismatch_with_header_bpm_suppressed)
{
    // text_orn_tempo_mismatch_suppressed.enc: 2 content measures.
    // M1: header BPM=249, ORN TEMPO=80 (BPM conflicts with header → misplaced ornament).
    // M2: header BPM=80, no ORN TEMPO.
    //
    // Without fix: ORN TEMPO at M1 creates TempoText BPM=80 at M1 (wrong position),
    //   and the !hasExisting guard in the header-BPM loop blocks M2's correct TempoText.
    // With fix: ORN TEMPO suppressed because 80 != encMeas.bpm=249; header-BPM loop
    //   creates TempoText BPM=80 at M2.
    MasterScore* score = readEncoreScore("text_orn_tempo_mismatch_suppressed.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    struct TempoAtTick {
        Fraction tick;
        double bps;
    };
    std::vector<TempoAtTick> found;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest); s;
             s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e && e->isTempoText()) {
                    found.push_back({ s->tick(), toTempoText(e)->tempo().val });
                }
            }
        }
    }

    // No TempoText at measure 1 (tick=0) with BPM=80 — the misplaced ornament must be suppressed
    for (const auto& t : found) {
        if (t.tick == Fraction(0, 1)) {
            EXPECT_FALSE(std::abs(t.bps - 80.0 / 60.0) < 1e-4)
                << "ORN TEMPO=80 at M1 (header BPM=249) must be suppressed (misplaced ornament)";
        }
    }

    // TempoText BPM=80 must appear somewhere after tick=0 (at measure 2)
    bool foundM2 = false;
    for (const auto& t : found) {
        if (t.tick > Fraction(0, 1) && std::abs(t.bps - 80.0 / 60.0) < 1e-4) {
            foundM2 = true;
        }
    }
    EXPECT_TRUE(foundM2)
        << "Header BPM=80 must create TempoText at M2 when misplaced ORN TEMPO at M1 is suppressed";

    delete score;
}

// ===========================================================================
// FIX: v0xC2 lyric text was read at element offset +20 instead of +18, dropping
// the first two bytes of every syllable (e.g. "ver"→"r", "dad"→"d", "Es"→"").
// Root cause: the 9-byte skip after the kie field should be 7 bytes for v0xC2.
// Fix: EncFormatReader_V0xC4::lyricTextGapAfterKie() returns 7 when !m_hasMetaTables.
//
// FIX: lyric matching used a note-first greedy algorithm that let later syllables
// steal the nearest note before earlier ones could claim it. Switched to lyrics-first.
//
// FIX: segEncTick formula used encTicksPerQuarter = beatTicks regardless of meter.
// For compound meters (6/8, 9/8) beatTicks represents a dotted-quarter beat (= 1.5
// quarter notes), so encTicksPerQuarter must be beatTicks * 2/3.
// Test file: J-RONDA.ENC (v0xC2, 6/8, "Jota de ronda" - Spanish folk tune).
// Before fixes: 24 garbled single-char fragments. After: 56 complete syllables.
// ===========================================================================

static std::vector<String> collectAllLyrics(MasterScore* score)
{
    std::vector<String> lyrics;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        for (Segment* s = toMeasure(mb)->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            for (track_idx_t t = 0; t < score->ntracks(); ++t) {
                EngravingItem* el = s->element(t);
                if (!el || !el->isChord()) {
                    continue;
                }
                for (Lyrics* ly : toChord(el)->lyrics()) {
                    lyrics.push_back(ly->plainText());
                }
            }
        }
    }
    return lyrics;
}

TEST_F(Tst_Text, lyrics_v0xc2_text_offset_full_words)
{
    MasterScore* score = readEncoreScore("jronda_68_lyrics.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << ret.text();

    std::vector<String> all = collectAllLyrics(score);
    // Before the v0xC2 +18 fix, the importer returned only 24 garbled single-char
    // fragments. After the fix, all ~56 syllables are imported.
    EXPECT_GE(all.size(), 50u)
        << "expected ~56 lyrics after v0xC2 offset fix";

    auto contains = [&](const String& s) {
        return std::find(all.begin(), all.end(), s) != all.end();
    };
    EXPECT_TRUE(contains(u"Es")) << "'Es' must be present (was '' before fix)";
    EXPECT_TRUE(contains(u"ver")) << "'ver' must be present (was 'r' before fix)";
    EXPECT_TRUE(contains(u"dad")) << "'dad' must be present (was 'd' before fix)";
    EXPECT_TRUE(contains(u"que")) << "'que' must be present (was 'e' before fix)";
    EXPECT_TRUE(contains(u"ri")) << "'ri' must be present";
    EXPECT_TRUE(contains(u"lim")) << "'lim' must be present (was 'm' before fix)";
    EXPECT_TRUE(contains(u"pia")) << "'pia' must be present (was 'a' before fix)";

    // These single-char garbled fragments must not appear after the fix.
    EXPECT_FALSE(contains(u"r")) << "garbled fragment 'r' must not appear after offset fix";
    EXPECT_FALSE(contains(u"d")) << "garbled fragment 'd' must not appear after offset fix";

    delete score;
}

TEST_F(Tst_Text, lyrics_compound_meter_all_syllables_matched)
{
    MasterScore* score = readEncoreScore("jronda_68_lyrics.enc");
    ASSERT_NE(score, nullptr);

    std::vector<String> all = collectAllLyrics(score);
    // In 6/8 (compound meter, beatTicks=360), the segEncTick formula must use
    // encTicksPerQuarter = beatTicks * 2/3 = 240 rather than beatTicks = 360.
    // Using 360 inflates note positions (beat 2 appeared at segEncTick=540 instead of
    // 360), placing them out of range of the lyrics. Before the compound-meter fix,
    // several syllables in every 6/8 measure were dropped.
    // Count occurrences of "Es", "ver", "dad" to verify all three refrain occurrences
    // are imported (the refrain "Es ver-dad..." repeats three times in J-RONDA).
    int countEs = 0, countVer = 0, countDad = 0;
    for (const String& s : all) {
        if (s == u"Es") {
            ++countEs;
        }
        if (s == u"ver") {
            ++countVer;
        }
        if (s == u"dad") {
            ++countDad;
        }
    }
    EXPECT_GE(countEs, 2)
        << "'Es' must appear at least twice; before compound-meter fix: 0 or 1 occurrences.";
    EXPECT_GE(countVer, 3)
        << "'ver' must appear at least three times; before compound-meter fix: 0 occurrences.";
    EXPECT_GE(countDad, 3)
        << "'dad' must appear at least three times; before compound-meter fix: 0 occurrences.";

    delete score;
}
