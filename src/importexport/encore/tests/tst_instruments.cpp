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

// Instrument matching: name/MIDI/clef routing to templates, drumset detection, TK name decoding and transposition.

#include <gtest/gtest.h>

#include "engraving/dom/chord.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftype.h"
#include "engraving/dom/tuplet.h"

#include "engraving/dom/instrtemplate.h"
#include "importexport/encore/internal/importer/mappers.h"

#include "testbase.h"

static const QString ENC_DIR(QString(iex_encore_tests_DATA_ROOT) + "/data/");

using namespace mu::engraving;

class Tst_Instruments : public ::testing::Test, public MTest
{
protected:
    void SetUp() override { setRootDir(ENC_DIR); }
};

// Name + MIDI scoring lets "Bass" + GM 32 (Acoustic Bass) beat the choral Bass template (program 52).
TEST_F(Tst_Instruments, instrument_name_midi_tiebreaks_to_acoustic_bass)
{
    MasterScore* score = readEncoreScore("instruments_instr_bass_midi_tiebreak.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->id(), String(u"acoustic-bass"))
        << "Bass + program 32 must reach the instrumental template, not the choral voice";
    delete score;
}

// v0xA6 MIDI program lives at TK content+52. MIDI 119 is in the GM percussive range (>=113) and must route to a drumset.
// See ENCORE_FORMAT.md ### MIDI program and Key by layout.
TEST_F(Tst_Instruments, v0xa6_reads_midi_program_from_tk_block)
{
    MasterScore* score = readEncoreScore("instruments_v0xa6_midi_program.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_NE(inst->drumset(), nullptr)
        << "v0xA6 MIDI 119 (>=113) must be read from TK+52 and route to a drumset, not Grand Piano";
    delete score;
}

// A name/MIDI match may land on a tablature template variant, but a normal Encore clef must swap it to the standard-notation sibling.
TEST_F(Tst_Instruments, tab_template_swapped_to_standard_when_clef_not_tab)
{
    MasterScore* score = readEncoreScore("instruments_tab_template_forced_standard.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->staves().empty());
    const StaffType* st = score->staff(0)->staffType(Fraction(0, 1));
    ASSERT_NE(st, nullptr);
    EXPECT_NE(st->group(), StaffGroup::TAB)
        << "Encore stores a normal clef, so the tablature template must be swapped to standard notation";
    delete score;
}

// EncClefType::TAB must swap a standard "Classical Guitar" match to the tablature sibling.
TEST_F(Tst_Instruments, standard_template_swapped_to_tablature_when_clef_tab)
{
    MasterScore* score = readEncoreScore("instruments_tab_clef_keeps_tablature.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->staves().empty());
    const StaffType* st = score->staff(0)->staffType(Fraction(0, 1));
    ASSERT_NE(st, nullptr);
    EXPECT_EQ(st->group(), StaffGroup::TAB)
        << "EncClefType::TAB must select the tablature template variant";
    delete score;
}

// RHYTHM staffType (=2) in the LINE block routes to snare-drum regardless of name or midiProgram.
TEST_F(Tst_Instruments, instrument_rhythm_staff_routes_to_snare_drum)
{
    MasterScore* score = readEncoreScore("instruments_rhythm_staff_snare.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->id(), String(u"snare-drum"))
        << "RHYTHM staffType must route to snare-drum template (step 6), not Grand Piano";
    EXPECT_NE(inst->drumset(), nullptr)
        << "Snare-drum instrument must carry a drumset";
    delete score;
}

// EncClefType::PERC on the first staff routes to drumset regardless of name or midiProgram.
TEST_F(Tst_Instruments, instrument_perc_clef_routes_to_drumset)
{
    MasterScore* score = readEncoreScore("instruments_instr_perc_clef_drumset.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->id(), String(u"drumset"))
        << "PERC clef must override name+MIDI and route to drumset (primary path)";
    delete score;
}

// Keyword fallback: "Drums" (contains "drum") routes to drumset despite midiProgram=1 (Grand Piano).
TEST_F(Tst_Instruments, instrument_name_drums_english_routes_to_drumset)
{
    MasterScore* score = readEncoreScore("instruments_instr_drums_name_drumset.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->id(), String(u"drumset"))
        << "'Drums' must reach drumset via findDrumsetTemplate, not fall back to piano";
    delete score;
}

// Percussion tracks store midiProgram=1 (Grand Piano), so the localized name "Percusion" must reach drumset by name, not MIDI.
TEST_F(Tst_Instruments, instrument_name_routes_percussion_to_drumset)
{
    MasterScore* score = readEncoreScore("instruments_instr_percussion_drumset.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->id(), String(u"drumset"))
        << "Encore 'Percusión' must reach the drum-kit template, not the MIDI piano fallback";
    delete score;
}

// Matching is diacritics-insensitive: "Laud" (no accent) must reach the "Laud" template (id=laud).
TEST_F(Tst_Instruments, instrument_name_diacritics_insensitive_match)
{
    MasterScore* score = readEncoreScore("instruments_instr_laud_accent.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->id(), String(u"laud"))
        << "Encore 'Laud' must match the laud template whose trackName carries the diacritic";
    delete score;
}

// TK block instrument name encoding: UTF-16 probe for v0xC4. See ENCORE_FORMAT.md ## Instrument block.

TEST_F(Tst_Instruments, tk_utf16_name_charsize_reads_full_name)
{
    // TK00 varsize=2158 (offset>250) selects charSize=TWO_BYTES; name reads fully without a probe.
    MasterScore* score = readEncoreScore("instruments_tk_utf16_name.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());

    const String longName = score->parts()[0]->longName();
    EXPECT_EQ(longName, String(u"Bandurria"))
        << "UTF-16 TK name (charSize=TWO_BYTES by offset) must be fully decoded";

    delete score;
}

TEST_F(Tst_Instruments, tk_probe_upgrades_onebyte_to_utf16)
{
    // TK00 varsize=112 (offset<=250) selects ONE_BYTE; the b1=0x00 probe detects UTF-16 and upgrades to TWO_BYTES.
    MasterScore* score = readEncoreScore("instruments_tk_probe_utf16.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());

    const String longName = score->parts()[0]->longName();
    EXPECT_EQ(longName, String(u"Bandurria"))
        << "Probe must upgrade ONE_BYTE charSize to TWO_BYTES for UTF-16 content";

    delete score;
}

TEST_F(Tst_Instruments, tk_probe_keeps_onebyte_for_latin1)
{
    // TK00 offset<=250 selects ONE_BYTE; b1='a' (not 0x00) keeps ONE_BYTE (Latin-1, not UTF-16).
    MasterScore* score = readEncoreScore("instruments_tk_onebyte_name.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "ONE_BYTE TK name file must produce a clean score";
    EXPECT_EQ(score->parts().size(), 1u)
        << "ONE_BYTE TK name file must produce exactly 1 part";
    delete score;
}

// Compact v0xC4 (no TK blocks) reads MIDI from the compact area, and findTemplateByMidi prefers the "common"
// genre (MIDI 68 is Oboe before Dulzaina). See ENCORE_FORMAT.md ### No-TK-block files.
TEST_F(Tst_Instruments, compact_no_tk_midi_reads_from_compact_area)
{
    MasterScore* score = readEncoreScore("instruments_compact_no_tk_midi_oboe.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->id(), String(u"oboe"))
        << "Compact file (no TK blocks): MIDI 69 must read from compact offset and resolve to oboe";
    delete score;
}

// MIDI-only matching must fire even when the name is empty: TK00 zeroed + MIDI 43 must resolve to Cello, not Grand Piano.
TEST_F(Tst_Instruments, instrument_empty_name_midi_resolves_to_cello)
{
    MasterScore* score = readEncoreScore("instruments_instr_empty_name_midi_cello.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->id(), String(u"violoncello"))
        << "Empty name + MIDI 43 (Cello) must resolve to violoncello via step5, not Grand Piano";
    delete score;
}

// A matched template with no <trackName> (e.g. the bare "recorder" template) must not leave the part with a blank
// track name; the importer derives one from the template id.
TEST_F(Tst_Instruments, instrument_recorder_midi75_keeps_track_name)
{
    MasterScore* score = readEncoreScore("instruments_instr_recorder_midi75_trackname.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->id(), String(u"recorder"))
        << "MIDI 75 must resolve to the recorder template via step5";
    EXPECT_EQ(inst->trackName(), String(u"Recorder"))
        << "recorder template has no trackName; importer must derive the sounding "
        "instrument name from the template id, not from the Encore part label";
    EXPECT_EQ(inst->nameAsPlainText(), String(u"Txistu"))
        << "the Encore instrument name stays as the part long name";
    delete score;
}

// Bb clarinet (MIDI 72, Key=0) must resolve to bb-clarinet with transposition zeroed: encKey=0 means "sounds as written".
// See ENCORE_FORMAT.md ## Key encoding.
TEST_F(Tst_Instruments, instrument_clarinet_midi72_key0_resolves_not_piano)
{
    MasterScore* score = readEncoreScore("instruments_instr_clarinet_midi72_key0.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->id(), String(u"bb-clarinet"))
        << "Clarinete + MIDI 72 + Key=0 must resolve to bb-clarinet (step2), not Grand Piano";
    EXPECT_EQ(inst->transpose().chromatic, 0)
        << "encKey=0 means Encore stores written pitch with no shift; template transposition "
        "must be zeroed so notes display at their Encore written pitch";
    delete score;
}

// Nameless Bb clarinet (MIDI 72, Key=0): the MIDI-only path must still resolve bb-clarinet (tmplChr=-2), not Grand Piano.
TEST_F(Tst_Instruments, instrument_empty_name_midi_clarinet_resolves_not_piano)
{
    MasterScore* score = readEncoreScore("instruments_instr_empty_name_midi_clarinet.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->id(), String(u"bb-clarinet"))
        << "Empty name + MIDI 72 + Key=0 must resolve to bb-clarinet (step5), not Grand Piano";
    delete score;
}

// Regression: a correctly configured Bb clarinet (Key=-2) must still resolve, so the Key=0 fix does not break it.
TEST_F(Tst_Instruments, instrument_clarinet_midi72_key_neg2_resolves_correctly)
{
    MasterScore* score = readEncoreScore("instruments_instr_clarinet_midi72_key_neg2.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->id(), String(u"bb-clarinet"))
        << "Clarinete + MIDI 72 + Key=-2 must still resolve to bb-clarinet via transposition filter";
    delete score;
}

// Compact v0xC4 with a short header (LINE block before offset 390): byte at 390 is LINE layout, not MIDI, so fall back to Grand Piano.
TEST_F(Tst_Instruments, compact_short_header_ignores_line_data_as_midi)
{
    MasterScore* score = readEncoreScore("instruments_compact_short_header_no_midi.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_NE(inst->id(), String(u"timpani"))
        << "Byte 0x30=48 inside LINE block must NOT select Timpani; no MIDI program in pre-LINE area";
    delete score;
}

TEST_F(Tst_Instruments, instrument_name_recovery_without_tk_block)
{
    // instrumentCount=2 but 1 TK block: "Guitarra" has no TK header, its name is recovered from NAME_BASE + 1*NAME_STEP.
    MasterScore* score = readEncoreScore("instruments_name_recovery.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_GE(score->parts().size(), 2u)
        << "Both instruments must be created (1 TK block + 1 recovered)";

    const String part1Name = score->parts()[1]->longName();
    EXPECT_EQ(part1Name, String(u"Guitarra"))
        << "Instrument name must be recovered from formula position, not 'Soprano Guitar'";

    delete score;
}

TEST_F(Tst_Instruments, staff_hidden_flag)
{
    // LINE staffData showByte 0x00 means hidden. See ENCORE_FORMAT.md ### LINE staff entry (30 bytes).
    MasterScore* score = readEncoreScore("instruments_staff_hidden.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());

    EXPECT_FALSE(score->parts()[0]->show())
        << "Staff with showByte=0x00 must be hidden (part->show()==false)";

    delete score;
}

TEST_F(Tst_Instruments, instrument_count_padding)
{
    // instrumentCount=2 but only 1 TK block: the vector must be padded so both parts are created (padded entry uses MIDI fallback).
    MasterScore* score = readEncoreScore("instruments_instrument_count_padding.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "score must be clean: " << ret.text();

    EXPECT_EQ(score->parts().size(), 2u)
        << "Both instruments must be imported even when only 1 TK block exists";

    delete score;
}

TEST_F(Tst_Instruments, transposition_filter_prefers_compatible_key)
{
    // The transposition filter is a preference, not a hard rejection: with no compatible match it falls back to the
    // best name+MIDI match instead of nullptr, so transposing instruments do not become Grand Piano when encKey=0.
    MasterScore* score = readEncoreScore("instruments_instrument_count_padding.enc");
    ASSERT_NE(score, nullptr);
    delete score;

    using namespace mu::iex::enc;

    // encKey=0 and only Castilian Dulzaina (chromatic=6) exists, so the filter falls back to it.
    const InstrumentTemplate* filtered = findEncoreInstrumentTemplate(
        QStringLiteral("Dulzaina 2"), -1, 0);
    ASSERT_NE(filtered, nullptr)
        << "When no transposition-compatible match exists, must fall back to best name match "
        "(not nullptr) to avoid Grand Piano fallback";
    EXPECT_EQ(filtered->transpose.chromatic, 6)
        << "Castilian Dulzaina (chromatic=6) is the only dulzaina template, so it is the fallback";

    const InstrumentTemplate* unfiltered = findEncoreInstrumentTemplate(
        QStringLiteral("Dulzaina 2"), -1);
    ASSERT_NE(unfiltered, nullptr);
    EXPECT_EQ(unfiltered->transpose.chromatic, 6)
        << "Unfiltered call must also find Castilian Dulzaina (chromatic=6)";
}

// A name unique to a single template ("Dulzaina") must win over the MIDI program (69, Oboe). Ambiguous substrings
// ("Bajo") still defer to MIDI: see instrument_name_midi_tiebreaks_to_acoustic_bass and the tuba test.
TEST_F(Tst_Instruments, unique_name_match_not_overridden_by_midi)
{
    MasterScore* score = readEncoreScore("instruments_unique_name_beats_midi.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_TRUE(inst->id().contains(String(u"dulzaina")))
        << "unique name 'Dulzaina' must win over the Oboe MIDI program; got "
        << inst->id().toStdString();
    delete score;
}

// Last-resort fuzzy (edit-distance) name match: "Clarynet" (one substitution from "Clarinet") has no substring or
// MIDI match, so only the fuzzy pass can map it to a clarinet instead of Grand Piano.
TEST_F(Tst_Instruments, fuzzy_name_match_typo)
{
    MasterScore* score = readEncoreScore("instruments_fuzzy_name_match.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_TRUE(inst->id().contains(String(u"clarinet")))
        << "typo 'Clarynet' must fuzzy-match a clarinet, not fall back to piano; got "
        << inst->id().toStdString();
    delete score;
}

// No-TK-block files recover instrument names from fixed offsets. See ENCORE_FORMAT.md ### No-TK-block files.
TEST_F(Tst_Instruments, no_tk_blocks_name_recovered_from_fixed_offset)
{
    // "Dulzaina" (UTF-16 LE) at NAME_BASE=202. The "Part N" fallback must be applied after name recovery, else the
    // !isEmpty() guard skips the read and the part stays "Part 1".
    MasterScore* score = readEncoreScore("instruments_no_tk_name_recovered.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Part* part = score->parts().front();
    ASSERT_NE(part, nullptr);
    const QString longName = part->longName().toQString();
    EXPECT_EQ(longName, QString("Dulzaina"))
        << "Instrument name 'Dulzaina' stored at NAME_BASE=202 (UTF-16) must be "
        "recovered when TK blocks are absent; without fix it stays 'Part 1'";
    delete score;
}

TEST_F(Tst_Instruments, no_tk_blocks_name_recovered_latin1_encoding)
{
    // "Tamboril" written as Latin-1 (b1='a', not 0x00) at NAME_BASE=202.
    MasterScore* score = readEncoreScore("instruments_no_tk_name_latin1.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Part* part = score->parts().front();
    ASSERT_NE(part, nullptr);
    const QString longName = part->longName().toQString();
    EXPECT_EQ(longName, QString("Tamboril"))
        << "Latin-1 instrument name at NAME_BASE=202 must be recovered when TK blocks absent";
    delete score;
}

TEST_F(Tst_Instruments, no_tk_blocks_name_falls_back_to_part_n_when_not_recoverable)
{
    // Offset 202 is 0x00 (b0 < 0x20), so recovery skips it and the "Part 1" fallback must fire.
    MasterScore* score = readEncoreScore("instruments_no_tk_name_fallback.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Part* part = score->parts().front();
    ASSERT_NE(part, nullptr);
    const QString longName = part->longName().toQString();
    EXPECT_FALSE(longName.isEmpty())
        << "Part must have a non-empty name even when name recovery fails";
    delete score;
}

TEST_F(Tst_Instruments, small_tk_midi_read_from_correct_offset)
{
    // smallTK layout (varsize=112): MIDI 49 lives at contentFilePos+offset+76=390, not the content+60 position that
    // lands in the zero-padded name area. See ENCORE_FORMAT.md ### MIDI program and Key by layout.
    MasterScore* score = readEncoreScore("instruments_small_tk_midi49.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_NE(inst->id(), String(u"grand-piano"))
        << "MIDI 49 must be read from smallTK offset (contentFilePos+offset+76=390), "
        "not zero-padded name area; result must not be Grand Piano fallback";
    delete score;
}

TEST_F(Tst_Instruments, small_tk_key_read_from_correct_offset)
{
    // smallTK layout (varsize=112): key=+6 lives at contentFilePos+varSize+53=367; the reader must not skip smallTK.
    MasterScore* score = readEncoreScore("instruments_small_tk_key6.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->transpose().chromatic, 6)
        << "Key=6 must be read from smallTK offset (contentFilePos+varSize+53=367)";
    delete score;
}

TEST_F(Tst_Instruments, total_size_tk_midi_read_from_content_offset)
{
    // Encore 4.x total-block-size TK layout: varSize is the whole block, stride equals varSize, and MIDI is at
    // content[60]. See ENCORE_FORMAT.md ### MIDI program and Key by layout. Here TK00 MIDI=49, TK01 MIDI=34.
    MasterScore* score = readEncoreScore("instruments_total_size_tk_two_instrs.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_GE(static_cast<int>(score->parts().size()), 2)
        << "File has 2 instruments; both must be parsed";
    const Instrument* inst0 = score->parts()[0]->instrument();
    const Instrument* inst1 = score->parts()[1]->instrument();
    ASSERT_NE(inst0, nullptr);
    ASSERT_NE(inst1, nullptr);
    EXPECT_NE(inst0->id(), String(u"grand-piano"))
        << "MIDI=49 must be read from total-size TK content[60]; must not fall back to Grand Piano";
    EXPECT_NE(inst1->id(), String(u"grand-piano"))
        << "MIDI=34 must be read from total-size TK content[60]; must not fall back to Grand Piano";
    delete score;
}

// An empty name on a real TK block is authoritative: the importer must fall back to "Part N", not probe the formula offset.
TEST_F(Tst_Instruments, tk_empty_name_is_authoritative_not_recovered)
{
    // TK01 is a real block with an empty name; "ZZTOP" is planted at the formula recovery offset (2360) as a decoy.
    MasterScore* score = readEncoreScore("instruments_tk_empty_name_authoritative.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_GE(static_cast<int>(score->parts().size()), 2)
        << "File declares 2 instruments; both must appear as parts";
    EXPECT_EQ(score->parts()[1]->longName(), String(u"Part 2"))
        << "An empty name on a real TK block is authoritative; instrument 1 "
           "must fall back to 'Part 2', not recover garbage from the formula offset";
    delete score;
}

TEST_F(Tst_Instruments, no_tk_blocks_reads_midi_and_key_from_large_tk_offsets)
{
    // No TK blocks: MIDI=69 (oboe) at large-TK offset 2278, Key=6 at 2255. See ENCORE_FORMAT.md ### No-TK-block files.
    MasterScore* score = readEncoreScore("instruments_no_tk_blocks_midi_key.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->id(), String(u"oboe"))
        << "MIDI=69 (oboe) must be read from large-TK offset 2278 even when TK blocks are absent";
    EXPECT_EQ(inst->transpose().chromatic, 6)
        << "Key=6 must be read from large-TK offset 2255 and applied as transposition";
    delete score;
}

// A drumset staff imported via the GM Percussive range (113-128) must get a percussion clef, not the clef stored in
// the LINE block. Fixture: name that matches no template, prg=116 (Taiko Drum).
TEST_F(Tst_Instruments, gm_perc_range_drumset_staff_gets_perc_clef)
{
    MasterScore* score = readEncoreScore("instruments_gm_perc_range_taiko.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->staves().empty());

    const Staff* st = score->staff(0);
    ASSERT_NE(st, nullptr);

    Measure* m0 = score->firstMeasure();
    ASSERT_NE(m0, nullptr);
    Segment* cs = m0->findSegment(SegmentType::HeaderClef, m0->tick());
    ASSERT_NE(cs, nullptr) << "First measure must have a header clef segment";

    bool foundPerc = false;
    for (EngravingItem* el : cs->elist()) {
        if (el && el->isClef()) {
            const Clef* clef = toClef(el);
            if (clef->clefType() == ClefType::PERC || clef->clefType() == ClefType::PERC2) {
                foundPerc = true;
            }
        }
    }
    EXPECT_TRUE(foundPerc)
        << "Drumset staff (GM prg=116) must use percussion clef, "
        "not the LINE-block clef (C3L/C4L/F)";

    delete score;
}

// Genuine simultaneous chord tones on a percussion staff must survive the MIDI artifact filter via two bypasses:
// first note on staff (savedPrevMidiTick<0) and chord extension (isChordExt=true). Fixture: prg=116, H@0 (pit=60)
// and H@5 (pit=64), where the tick diff of 5 would otherwise flag note@0 as an artifact.
TEST_F(Tst_Instruments, gm_perc_chord_notes_not_dropped_by_artifact_filter)
{
    MasterScore* score = readEncoreScore("instruments_gm_perc_chord_notes.enc");
    ASSERT_NE(score, nullptr);

    Measure* m0 = score->firstMeasure();
    ASSERT_NE(m0, nullptr);

    Segment* firstSeg = m0->first(SegmentType::ChordRest);
    ASSERT_NE(firstSeg, nullptr);
    EngravingItem* el = firstSeg->element(0);
    ASSERT_NE(el, nullptr);
    ASSERT_TRUE(el->isChord());

    const Chord* chord = toChord(el);
    EXPECT_EQ(static_cast<int>(chord->notes().size()), 2)
        << "Both chord notes (pit=60 and pit=64) must survive the MIDI artifact "
        "filter: note@0 is the first on-staff note (bypass: savedPrevMidiTick<0) "
        "and note@5 is a chord extension (bypass: isChordExt=true)";

    delete score;
}

TEST_F(Tst_Instruments, gm_perc_range_midi_program_routes_to_drumset)
{
    MasterScore* score = readEncoreScore("instruments_gm_perc_range_taiko.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->id(), String(u"drumset"))
        << "MIDI program 116 (Taiko Drum, GM Percussive range 113-128) must route "
        "to drumset even when the instrument name matches nothing";
    EXPECT_NE(inst->drumset(), nullptr)
        << "Drumset instrument must carry a drumset object";
    delete score;
}

// encKey=0 ("sounds as written") must zero the template's octave transposition too, not just non-octave ones.
// See ENCORE_FORMAT.md ## Key encoding.
TEST_F(Tst_Instruments, key0_zeroes_octave_template_transposition)
{
    // "Bajo" + MIDI 33 resolves to acoustic-bass (transposeChromatic=-12); encKey=0 must zero the -12.
    MasterScore* score = readEncoreScore("instruments_bass_enckey0_no_octave_transpos.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->transpose().chromatic, 0)
        << "encKey=0 (sounds as written) must zero the template's octave transposition (-12) "
        "so notes display at Encore's written pitch, not one octave higher";

    // The note pitch must stay at the Encore-stored value (A2=45), with no octave shift from the template.
    Measure* m = score->firstMeasure();
    ASSERT_NE(m, nullptr);
    Segment* seg = m->first(SegmentType::ChordRest);
    ASSERT_NE(seg, nullptr);
    EngravingItem* el = seg->element(0);
    ASSERT_TRUE(el && el->isChord());
    EXPECT_EQ(toChord(el)->notes().front()->pitch(), 45)
        << "Concert pitch A2(45) must be stored unchanged; octave shift from template must not apply";

    delete score;
}

// findTemplateByMidi() must use only a template's first channel, so MIDI 44 (Tremolo Strings) does not resolve to
// acoustic-bass (which carries program 44 only in its tremolo secondary channel). Fixture resolves MIDI 69 to oboe.
TEST_F(Tst_Instruments, midi44_does_not_resolve_to_acoustic_bass_via_tremolo_channel)
{
    MasterScore* score = readEncoreScore("instruments_compact_no_tk_midi_oboe.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->id(), String(u"oboe"))
        << "MIDI 69 must resolve to oboe; acoustic-bass must not match via tremolo channel";
    delete score;
}

// Trailing punctuation must be stripped from the name needle so "Bandurr. I" matches "Bandurria" via contains().
TEST_F(Tst_Instruments, abbreviated_name_with_trailing_dot_matches_bandurria)
{
    MasterScore* score = readEncoreScore("instruments_abbreviated_name_bandurr.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->id(), String(u"bandurria"))
        << "Name 'Bandurr. I' must resolve to bandurria after punctuation stripping";
    delete score;
}

// v0xC2 files without a ~~~~ block use a compact table (entries at NAME_BASE+n*112, MIDI at 262+n*112), not the
// ~~~~-block stride, so names and MIDI must not shift by one entry. See ENCORE_FORMAT.md ### No-TK-block files.
TEST_F(Tst_Instruments, c2_no_tilde_compact_instr1_name_not_duplicated_to_instr0)
{
    // [0] no name, MIDI=49; [1] "Guitarra", MIDI=25; [2] no name, MIDI=57.
    MasterScore* score = readEncoreScore("instruments_c2_no_tilde_compact_names_midi.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_EQ(score->parts().size(), 3u);
    // Part 1 must have the name "Guitarra" (not shifted to part 0).
    EXPECT_EQ(score->parts()[1]->longName(), String(u"Guitarra"))
        << "Name 'Guitarra' must be assigned to instrument [1], not [0]";
    // Part 0 must NOT have "Guitarra" (would indicate the old shift bug).
    EXPECT_NE(score->parts()[0]->longName(), String(u"Guitarra"))
        << "Instrument [0] must not receive instrument [1]'s name";
    delete score;
}

TEST_F(Tst_Instruments, c2_no_tilde_compact_midi_assigned_to_correct_instr)
{
    // MIDI programs must not shift by one entry: [0] MIDI=49, [1] MIDI=25, [2] MIDI=57, so [0] and [1] stay distinct.
    MasterScore* score = readEncoreScore("instruments_c2_no_tilde_compact_names_midi.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_EQ(score->parts().size(), 3u);
    const String id0 = score->parts()[0]->instrument()->id();
    const String id1 = score->parts()[1]->instrument()->id();
    const String id2 = score->parts()[2]->instrument()->id();
    EXPECT_NE(id0, id1) << "Instrument [0] must not match instrument [1] (MIDI shifted bug)";
    EXPECT_NE(id1, id2) << "Instrument [1] must not match instrument [2]";
    delete score;
}

// v0xC2 ~~~~-block files with a primary block (printable ASCII at NAME_BASE+n*NAME_STEP) must read MIDI at block+60,
// not skip it (midiProgram=0). Fixture: single instrument, MIDI=25 at 262 (-> Classical Guitar, not Grand Piano).
TEST_F(Tst_Instruments, c2_tilde_primary_block_midi_read_from_offset_60)
{
    MasterScore* score = readEncoreScore("instruments_c2_tilde_primary_block_midi.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_EQ(score->parts().size(), 1u);
    EXPECT_NE(score->parts()[0]->instrument()->id(), String(u"grand-piano"))
        << "Primary-block MIDI=25 at block+60 must be read; Grand Piano means midiProgram stayed 0";
    delete score;
}

TEST_F(Tst_Instruments, no_tk_blocks_large_tk_layout_reads_all_instrument_names)
{
    // No TK blocks but a LINE block past 2278 triggers the large-TK stride (2158) in name recovery, so instrument 1
    // reads "Cello" at 202+2158=2360, not zero bytes at 202+112=314. "Oboe" is at NAME_BASE=202.
    MasterScore* score = readEncoreScore("instruments_no_tk_large_tk_two_names.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_GE(static_cast<int>(score->parts().size()), 2)
        << "File declares 2 instruments; both must appear as parts";
    const QString name0 = score->parts()[0]->longName().toQString();
    const QString name1 = score->parts()[1]->longName().toQString();
    EXPECT_EQ(name0, QString("Oboe"))
        << "Instrument 0 name must be read from NAME_BASE=202 with step=2158";
    EXPECT_EQ(name1, QString("Cello"))
        << "Instrument 1 name must be read from NAME_BASE+2158=2360 (not 202+112=314)";
    delete score;
}

// A trailing ordinal after a separator ("Trumpet-1") must be stripped so the base name still matches, not the MIDI fallback.
TEST_F(Tst_Instruments, instrument_name_trailing_number_after_dash_stripped)
{
    MasterScore* score = readEncoreScore("instruments_name_trailing_number.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts()[0]->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_TRUE(inst->id().contains(String(u"trumpet")))
        << "\"Trumpet-1\" must match a Trumpet (trailing \"-1\" stripped), not the MIDI fallback; got "
        << inst->id().toStdString();
    delete score;
}

// Names split on '-' as well as spaces, so "French-Horn" yields the needle "horn" and matches the Horn template.
TEST_F(Tst_Instruments, instrument_name_splits_on_dash_separator)
{
    MasterScore* score = readEncoreScore("instruments_name_dash_separator.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts()[0]->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_TRUE(inst->id().contains(String(u"horn")))
        << "\"French-Horn\" must split on '-' and match a Horn, not the MIDI fallback; got "
        << inst->id().toStdString();
    delete score;
}

// A weak substring name match must defer to MIDI: "Contrabass" + MIDI 59 (Tuba) must not pick the treble "Contrabass Bugle".
TEST_F(Tst_Instruments, instrument_weak_substring_name_defers_to_midi)
{
    MasterScore* score = readEncoreScore("instruments_weak_name_defers_to_midi.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts()[0]->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_NE(inst->id(), String(u"contrabass-bugle"))
        << "weak substring name match must not pick the treble bugle";
    EXPECT_TRUE(inst->id().contains(String(u"tuba")))
        << "must defer to MIDI program 59 (Tuba); got " << inst->id().toStdString();
    delete score;
}

// The importer must rebuild the MIDI mapping so every part has a valid channel and port; a channel of -1 makes
// Part::midiPort() index m_midiMapping[-1] and crash MusicXML export.
TEST_F(Tst_Instruments, midi_mapping_is_built_for_every_part)
{
    MasterScore* score = readEncoreScore("instruments_compact_tk_ignores_key_byte.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    for (const Part* part : score->parts()) {
        const Instrument* inst = part->instrument();
        ASSERT_NE(inst, nullptr);
        EXPECT_GE(inst->channel(0)->channel(), 0)
            << "import must assign a MIDI channel; -1 makes midiPort() index out of bounds";
        EXPECT_GE(part->midiPort(), 0)
            << "midiPort() must be valid so MusicXML export does not crash";
    }
    delete score;
}

// Name-confidence matcher functions, exercised directly. A confident name match (exact, unique, or an unambiguous
// normalized/plural/fuzzy match) must be reported confident so the weak-name -> MIDI override does not discard it.
TEST_F(Tst_Instruments, plural_name_depluralized_and_unique)
{
    using namespace mu::iex::enc;
    bool exact = false, unique = false;
    // "Bandurrias" (plural) must collapse to the singular stem and match the unique Bandurria template confidently.
    const InstrumentTemplate* t = findEncoreInstrumentTemplate(
        QStringLiteral("Bandurrias"), -1, ENC_KEY_NO_FILTER, &exact, &unique);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->id, String(u"bandurria"));
    EXPECT_TRUE(exact || unique)
        << "a depluralized stem must match the Bandurria template confidently, not weakly";
}

TEST_F(Tst_Instruments, attached_digit_name_normalized)
{
    using namespace mu::iex::enc;
    bool exact = false, unique = false;
    // "Bandurria1" (digit attached, no separator) must normalize to "Bandurria".
    const InstrumentTemplate* t = findEncoreInstrumentTemplate(
        QStringLiteral("Bandurria1"), -1, ENC_KEY_NO_FILTER, &exact, &unique);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->id, String(u"bandurria"));
    EXPECT_TRUE(exact || unique) << "attached part number must be stripped to a confident match";
}

TEST_F(Tst_Instruments, fuzzy_unique_name_reported_unique)
{
    using namespace mu::iex::enc;
    bool exact = false, unique = false;
    // "Acordeon" fuzzy-matches only "accordion"; a single close template is confident.
    const InstrumentTemplate* t = findEncoreInstrumentTemplate(
        QStringLiteral("Acordeon"), -1, ENC_KEY_NO_FILTER, &exact, &unique);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->id, String(u"accordion"));
    EXPECT_TRUE(unique) << "a fuzzy match that is the only template above threshold must be unique";
}

TEST_F(Tst_Instruments, ambiguous_name_still_not_unique)
{
    using namespace mu::iex::enc;
    bool exact = false, unique = false;
    // Regression guard: "Guitarra" contains-matches many guitar templates, so it must stay
    // non-unique and keep deferring to the MIDI program (Classical Guitar via MIDI 25).
    const InstrumentTemplate* t = findEncoreInstrumentTemplate(
        QStringLiteral("Guitarra"), -1, ENC_KEY_NO_FILTER, &exact, &unique);
    ASSERT_NE(t, nullptr);
    EXPECT_FALSE(exact);
    EXPECT_FALSE(unique) << "an ambiguous substring name must remain weak so MIDI can correct it";
}

// Class B: a configured MIDI program that no template carries as its primary sound must fall
// back to the nearest template in the same General MIDI family, not to Grand Piano.
TEST_F(Tst_Instruments, gm_family_fallback_for_unmapped_program)
{
    using namespace mu::iex::enc;
    constexpr int kPizzicatoStrings0 = 45;   // GM 46, 0-indexed; Strings family (40..47)
    EXPECT_EQ(findTemplateByMidi(kPizzicatoStrings0), nullptr)
        << "precondition: no template has Pizzicato Strings as its primary program";
    const InstrumentTemplate* t = findTemplateByMidiFamily(kPizzicatoStrings0);
    ASSERT_NE(t, nullptr);
    ASSERT_FALSE(t->channel.empty());
    const int prog = t->channel.front().program();
    EXPECT_GE(prog, 40);
    EXPECT_LE(prog, 47) << "family fallback must stay within the Strings family, not Grand Piano";
}

// SCO5 (big-endian macOS Encore 5) frames block sizes big-endian except the TK instrument blocks, whose size stays
// little-endian; reading TK size big-endian yields 0 and drops every name after the first. See ENCORE_FORMAT.md ### Magics and byte order.
TEST_F(Tst_Instruments, sco5_tk_block_instrument_names)
{
    MasterScore* score = readEncoreScore("instruments_sco5_tk_names.enc");
    ASSERT_NE(score, nullptr) << "Failed to load instruments_sco5_tk_names.enc";
    ASSERT_EQ(score->parts().size(), 2u) << "expected 2 instruments";
    EXPECT_EQ(score->parts().at(0)->longName(), String(u"CORNETA 1"));
    EXPECT_EQ(score->parts().at(1)->longName(), String(u"TROMPETA 2"))
        << "second TK-block name must import, not fall back to a default";
    delete score;
}
