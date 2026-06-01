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
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/tuplet.h"

#include "engraving/dom/instrtemplate.h"
#include "importexport/encore/internal/importer/mapping.h"

#include "testbase.h"

static const QString ENC_DIR(QString(iex_encore_tests_DATA_ROOT) + "/data/");

using namespace mu::engraving;

class Tst_Instruments : public ::testing::Test, public MTest
{
protected:
    void SetUp() override { setRootDir(ENC_DIR); }
};

// ===========================================================================
// FIX: findEncoreInstrumentTemplate scores name + MIDI program together so
// "Bass" + GM program 32 (Acoustic Bass) wins over the choral Bass voice
// template that matches the name exactly but ships with a Choir Aahs
// channel (program 52).
// ===========================================================================
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

// ===========================================================================
// FEATURE: percussion detection — primary path (PERC clef).
// An instrument with EncClefType::PERC on its first staff must be routed to
// the drumset template regardless of its name or midiProgram. This is the
// language-agnostic, binary-level detection that requires no keyword list.
// ===========================================================================
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

// ===========================================================================
// FEATURE: percussion detection — keyword fallback path.
// An instrument named "Drums" (English) with midiProgram=1 must reach the
// drumset template. The word "drum" triggers the keyword fallback (step 4),
// which routes it to the canonical drumset template.
// ===========================================================================
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

// ===========================================================================
// FIX: percussion tracks store midiProgram=1 (Grand Piano) regardless of
// the actual instrument. Spanish "Percusión" must reach the drumset template
// via findDrumsetTemplate (localized name match), not the MIDI fallback.
// ===========================================================================
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

// ===========================================================================
// FIX: instrument matching is now diacritics-insensitive. The Spanish
// folk lute template id="laud" ships with trackName="Laúd"; an Encore file
// that writes the name without the accent (real corpora frequently do)
// must still resolve to it.
// ===========================================================================
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

// ===========================================================================
// FEATURE: TK block instrument name encoding (UTF-16 probe for v0xC4)
// ===========================================================================

TEST_F(Tst_Instruments, tk_utf16_name_charsize_reads_full_name)
{
    // instruments_tk_utf16_name.enc: TK00 varsize=2158 → offset=2158>250
    // → charSize()=TWO_BYTES.  Content is UTF-16 LE "Bandurria".
    // charSize already picks TWO_BYTES; name is read fully without probe.
    // Represents v0xC4 files from older Encore versions with offset>250.
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
    // instruments_tk_probe_utf16.enc: TK00 varsize=112 → offset=112<=250
    // → charSize()=ONE_BYTE; content is UTF-16 LE "Bandurria"
    // (b0=0x42='B', b1=0x00 → probe detects UTF-16, upgrades to TWO_BYTES).
    // Without the probe fix (old forceUtf16=always), this also worked, but
    // with wrong results for ONE_BYTE files.  The probe must detect correctly.
    // Represents Encore 5.0.2 v0xC4 files (e.g. pachbel.enc resaved).
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
    // instruments_tk_onebyte_name.enc: TK00 varsize=112 → offset=112<=250
    // → charSize()=ONE_BYTE; content is Latin-1 "Bandurria 1"
    // (b0=0x42='B', b1=0x61='a'!=0x00 → probe keeps ONE_BYTE, not UTF-16).
    // Regression: a naive forceUtf16=true would misread "Bandurria 1" as
    // UTF-16 pairs, producing garbled instrument names.
    // Verify the file imports cleanly with the correct part count.
    MasterScore* score = readEncoreScore("instruments_tk_onebyte_name.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "ONE_BYTE TK name file must produce a clean score";
    EXPECT_EQ(score->parts().size(), 1u)
        << "ONE_BYTE TK name file must produce exactly 1 part";
    delete score;
}

// ===========================================================================
// FIX: compact v0xC4 files (no TK blocks) store the MIDI program in a
// different location (offset 390, step 276) than TK-based files (offset
// 2278, step 2158). Without this fix the byte at the TK-derived offset is
// read, which is arbitrary file data (e.g. 0x58=88 instead of 69=Oboe).
//
// Fixture: instruments_compact_no_tk_midi_oboe.enc
//   - Real v0xC4 file with no TK blocks; MIDI byte 69 (1-indexed) at 0x186.
//   - The compact offset formula reads 69 correctly → Oboe.
//   - The old TK formula reads 88 → no template match → Grand Piano.
//
// Additionally: findTemplateByMidi must prefer the "common" genre among
// templates sharing the same program. MIDI 68 (0-indexed) is shared by
// Oboe (common genre) and Castilian Dulzaina (world genre). Without the
// tiebreaker, searchTemplateForMidiProgram returns Dulzaina first.
// ===========================================================================
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

// ===========================================================================
// FIX: MIDI program lookup (step 5) must fire even when the instrument name
// is empty (length 0). The short-name guard previously blocked step 5 for
// all names shorter than 4 characters, including the empty string, forcing
// a Grand Piano fallback even when a valid midiProgram was available.
// Fixture: instruments_instr_empty_name_midi_cello.enc
//   - TK00 name zeroed → instr.name = ""
//   - MIDI byte at PRG_BASE (offset 2278) = 43 (1-indexed) → GM program 42 = Cello
// Expected: Violoncello template (id="violoncello"), not Grand Piano.
// ===========================================================================
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

// ===========================================================================
// FIX: compact v0xC4 files with a short header (LINE blocks starting before
// offset 390) must NOT read their instrument's MIDI program from inside the
// LINE block data.  The byte at offset 390 is LINE layout data whose value
// happens to be 0x30 = 48 (GM Timpani); without the guard the importer would
// select Timpani instead of the correct fallback (Grand Piano).
//
// Fixture: instruments_compact_short_header_no_midi.enc
//   - Compact v0xC4 (no TK blocks); first LINE block at offset 194.
//   - Byte at compact formula offset 390 (inside LINE) = 0x30 = 48.
//   - Expected: Grand Piano fallback (no valid MIDI in pre-LINE area).
// ===========================================================================
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

// ===========================================================================
// BUG FIX: missing instruments when Encore 5.0.2 v0xC4 omits TK blocks
// ===========================================================================

// ===========================================================================
// FEATURE: Staff visibility flag (showByte at LINE staffData offset +19)
// ===========================================================================

// ===========================================================================
// FEATURE: Instrument name recovery for instruments without TK block header
// ===========================================================================

TEST_F(Tst_Instruments, instrument_name_recovery_without_tk_block)
{
    // instruments_name_recovery.enc: instrumentCount=2, 1 TK block (TK00
    // "Bandurria").  "Guitarra" is stored as UTF-16 LE at the formula offset
    // NAME_BASE + 1*NAME_STEP = 202 + 2158 = 2360, with no TK04 header.
    // This matches pachbel.enc where Encore 5.0.2 omits TK04 but still writes
    // the name content at the formula-derived position.
    //
    // The importer must scan each padded (name-empty) instrument at its
    // formula offset, detect UTF-16, and recover "Guitarra".
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
    // instruments_staff_hidden.enc: SKELETON_PRE LINE block patched so
    // staff 0 showByte = 0x00 (hidden).  Binary-diff verified: Encore stores
    // the visibility flag at byte +19 of each 30-byte EncLineStaffData entry
    // (3rd byte of the 3-byte skip after pageIdx).
    //
    // The importer must call part->setShow(false) for hidden staves.
    MasterScore* score = readEncoreScore("instruments_staff_hidden.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());

    EXPECT_FALSE(score->parts()[0]->show())
        << "Staff with showByte=0x00 must be hidden (part->show()==false)";

    delete score;
}

TEST_F(Tst_Instruments, instrument_count_padding)
{
    // instruments_instrument_count_padding.enc: header instrumentCount=2
    // but only 1 TK block (TK00).  Encore 5.0.2 can omit TK blocks for some
    // instruments (e.g. pachbel.enc has 5 instruments but only 4 TK blocks —
    // Guitarra has no TK block).
    //
    // Bug: instruments.size()=1 < instrumentCount=2 → only 1 part created.
    // Fix: pad instruments vector to instrumentCount with empty entries.
    //      Both instruments are then created; the padded one uses the MIDI
    //      program fallback if available.
    MasterScore* score = readEncoreScore("instruments_instrument_count_padding.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "score must be clean: " << ret.text();

    EXPECT_EQ(score->parts().size(), 2u)
        << "Both instruments must be imported even when only 1 TK block exists";

    delete score;
}

TEST_F(Tst_Instruments, transposition_filter_rejects_mismatched_key)
{
    // Verify that findEncoreInstrumentTemplate with encKeySemitones=0 rejects
    // templates whose non-octave chromatic transposition does not match.
    // "Castilian Dulzaina" has chromatic=6: it must be filtered out when
    // Encore says the instrument is a C-instrument (encKey=0).
    //
    // The unfiltered call (ENC_KEY_NO_FILTER) must still find the template so
    // existing behaviour is preserved for callers that opt out of the filter.
    MasterScore* score = readEncoreScore("instruments_instrument_count_padding.enc");
    ASSERT_NE(score, nullptr);
    delete score;

    using namespace mu::iex::encore;

    const InstrumentTemplate* filtered = findEncoreInstrumentTemplate(
        QStringLiteral("Dulzaina 2"), -1, 0);
    EXPECT_EQ(filtered, nullptr)
        << "Transposing dulzaina template (chromatic=6) must be rejected when encKey=0";

    const InstrumentTemplate* unfiltered = findEncoreInstrumentTemplate(
        QStringLiteral("Dulzaina 2"), -1);
    ASSERT_NE(unfiltered, nullptr);
    EXPECT_EQ(unfiltered->transpose.chromatic, 6)
        << "Unfiltered call must find Castilian Dulzaina (chromatic=6)";
}
