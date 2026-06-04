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

    struct Entry { String text; LyricsSyllabic syll; };
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
    EXPECT_EQ(tempoTexts, (std::vector<String>{ u"Allegro", u"a tempo" }))
        << "Allegro and 'a tempo' must reach the score as TempoText, not StaffText";
    EXPECT_EQ(staffTexts, (std::vector<String>{ u"ten." }))
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
    struct Found { int measureIdx; double bps; String xmlText; };
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
        if (!mb->isMeasure()) { continue; }
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

    struct Found { String xmlText; bool followText; double bps; };
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
