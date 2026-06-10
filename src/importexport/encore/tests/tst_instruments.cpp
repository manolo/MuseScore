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
// FIX: name + MIDI scoring lets "Bass" + GM 32 (Acoustic Bass) beat the choral Bass template (program 52).
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
// FEATURE: RHYTHM staffType (=2) in LINE block routes to snare-drum regardless of name or midiProgram.
// ===========================================================================
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

// ===========================================================================
// FEATURE: EncClefType::PERC on first staff routes to drumset regardless of name or midiProgram.
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
// FEATURE: keyword fallback path. "Drums" (contains "drum") triggers step 4 and routes
// to drumset regardless of midiProgram=1 (which would otherwise give Grand Piano).
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
// FIX: percussion tracks store midiProgram=1 (Grand Piano) regardless of instrument.
// "Percusión" must reach drumset via findDrumsetTemplate (localized name), not MIDI fallback.
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
// FIX: matching is diacritics-insensitive. Template "Laúd" (id=laud) must be reached
// by files that write "Laud" (no accent), as real corpora frequently do.
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
    // TK00 varsize=2158 → offset>250 → charSize=TWO_BYTES; name read fully without probe.
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
    // TK00 varsize=112 → offset<=250 → charSize=ONE_BYTE; b1=0x00 probe detects UTF-16, upgrades to TWO_BYTES.
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
    // TK00 offset<=250 → ONE_BYTE; b1='a'!=0x00 → probe keeps ONE_BYTE (Latin-1, not UTF-16).
    MasterScore* score = readEncoreScore("instruments_tk_onebyte_name.enc");
    ASSERT_NE(score, nullptr);
    muse::Ret ret = score->sanityCheck();
    EXPECT_TRUE(ret) << "ONE_BYTE TK name file must produce a clean score";
    EXPECT_EQ(score->parts().size(), 1u)
        << "ONE_BYTE TK name file must produce exactly 1 part";
    delete score;
}

// ===========================================================================
// FIX: compact v0xC4 (no TK blocks) stores MIDI at offset 390 (step 276), not the TK formula (offset 2278).
// Also: findTemplateByMidi prefers "common" genre: MIDI 68 is Oboe (common) before Dulzaina (world).
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
// FIX: MIDI step 5 must fire even when name is empty. Old guard skipped step 5 for names < 4 chars,
// forcing Grand Piano even with a valid midiProgram. TK00 zeroed + MIDI 43 must resolve to Cello.
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
// FIX: Bb clarinet (MIDI 72, Key=0) must not fall back to Grand Piano, and
// its transposition must be zeroed (encKey=0 means 'sounds as written' in
// Encore so no chromatic shift should be applied at display time).
// ===========================================================================
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

// ===========================================================================
// FIX: nameless Bb clarinet (MIDI 72, Key=0) must not fall back to Grand Piano.
// When name is too short to trigger step 2, only step 5 (MIDI-only) can fire.
// The old transposition filter in step 5 rejected bb-clarinet (tmplChr=-2)
// whenever encKey==0, causing piano fallback.
// ===========================================================================
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

// ===========================================================================
// REGRESSION: Bb clarinet with Key=-2 (correctly configured) must still work.
// Guards against a fix for Key=0 accidentally breaking the correct-Key path.
// ===========================================================================
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

// ===========================================================================
// FIX: compact v0xC4 with short header (LINE block before offset 390) must not read MIDI from inside LINE.
// Byte 0x30=48 at offset 390 is LINE layout data; guard must fall back to Grand Piano.
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

TEST_F(Tst_Instruments, instrument_name_recovery_without_tk_block)
{
    // instrumentCount=2, 1 TK block (TK00 "Bandurria"). "Guitarra" has no TK04 header but its
    // UTF-16 LE name is at NAME_BASE + 1*NAME_STEP; importer must detect and recover it.
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
    // LINE staffData byte +19 = showByte; 0x00 means hidden. Importer must call part->setShow(false).
    MasterScore* score = readEncoreScore("instruments_staff_hidden.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());

    EXPECT_FALSE(score->parts()[0]->show())
        << "Staff with showByte=0x00 must be hidden (part->show()==false)";

    delete score;
}

TEST_F(Tst_Instruments, instrument_count_padding)
{
    // header instrumentCount=2 but only 1 TK block. Instruments vector must be padded to instrumentCount
    // so both parts are created (padded entry uses MIDI fallback).
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
    // The transposition filter is a PREFERENCE, not a hard rejection. When no
    // transposition-compatible match exists (e.g. no C-pitched Dulzaina template),
    // the function falls back to the best name+MIDI match instead of returning nullptr.
    // This prevents transposing instruments from becoming Grand Piano when the Encore
    // Key field is not set (encKey=0).
    MasterScore* score = readEncoreScore("instruments_instrument_count_padding.enc");
    ASSERT_NE(score, nullptr);
    delete score;

    using namespace mu::iex::encore;

    // encKey=0, only Castilian Dulzaina template exists (chromatic=6, no C-Dulzaina).
    // Filter prefers a compatible match; since none exists it falls back to best name match.
    const InstrumentTemplate* filtered = findEncoreInstrumentTemplate(
        QStringLiteral("Dulzaina 2"), -1, 0);
    ASSERT_NE(filtered, nullptr)
        << "When no transposition-compatible match exists, must fall back to best name match "
        "(not nullptr) to avoid Grand Piano fallback";
    EXPECT_EQ(filtered->transpose.chromatic, 6)
        << "Castilian Dulzaina (chromatic=6) is the only dulzaina template, so it is the fallback";

    // Unfiltered call must return the same result.
    const InstrumentTemplate* unfiltered = findEncoreInstrumentTemplate(
        QStringLiteral("Dulzaina 2"), -1);
    ASSERT_NE(unfiltered, nullptr);
    EXPECT_EQ(unfiltered->transpose.chromatic, 6)
        << "Unfiltered call must also find Castilian Dulzaina (chromatic=6)";
}

// ===========================================================================
// FIX: v0xC4 files with no TK blocks use fallback instruments (contentFilePos=-1,
// offset=0). The compact MIDI/Key offsets (390/367) have 0, but the standard
// large-TK offsets (2278/2255) have the real values. The importer must probe
// the large-TK positions when contentFilePos<0 (no TK blocks found).
// ===========================================================================
// ===========================================================================
// BUG FIX: instrument names from no-TK-block files recovered from fixed offsets
// ===========================================================================
TEST_F(Tst_Instruments, no_tk_blocks_name_recovered_from_fixed_offset)
{
    // instruments_no_tk_name_recovered.enc: TK00 magic zeroed, "Dulzaina" written
    // as UTF-16 LE at NAME_BASE=202. Fix: fallback "Part N" names are applied AFTER
    // readInstrumentMeta() so recoverMissingNames() can read names from the file.
    // Without the fix: name was set to "Part 1" before recovery → guard !isEmpty()
    // skipped the read → part remained "Part 1".
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
    // instruments_no_tk_name_latin1.enc: TK00 magic zeroed, "Tamboril" written
    // as Latin-1 (b0='T', b1='a' → not UTF-16, is Latin-1) at NAME_BASE=202.
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
    // instruments_no_tk_name_fallback.enc: TK00 magic zeroed, offset 202 is 0x00
    // (b0 < 0x20 → recoverMissingNames skips it). "Part 1" fallback must fire.
    MasterScore* score = readEncoreScore("instruments_no_tk_name_fallback.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Part* part = score->parts().front();
    ASSERT_NE(part, nullptr);
    const QString longName = part->longName().toQString();
    // When recovery finds nothing, the part must have a non-empty fallback name.
    EXPECT_FALSE(longName.isEmpty())
        << "Part must have a non-empty name even when name recovery fails";
    delete score;
}

TEST_F(Tst_Instruments, small_tk_midi_read_from_correct_offset)
{
    // instruments_small_tk_midi49.enc: TK00 varsize=112 (smallTK layout), MIDI 49
    // stored at contentFilePos + offset + 76 = 202 + 112 + 76 = 390.
    //
    // Without fix: readMidiPrograms uses MIDI_IN_CONTENT=60, reads at 202+60=262
    //   (within the zero-padded name area) → midiProgram=0 → Grand Piano fallback.
    // With fix: reads at 202+112+76=390 → midiProgram=49 → step 5 selects a
    //   non-Piano template (String Ensemble 1 for MIDI 49).
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
    // instruments_small_tk_key6.enc: TK00 varsize=112, key=+6 semitones at
    // contentFilePos + varSize + 53 = 202 + 112 + 53 = 367.
    //
    // Without fix: readKeyTranspositions returned early for smallTK (offset 1..250),
    //   key stayed 0, no transposition applied.
    // With fix: key=6 read from offset 367, instrument transposed +6 semitones.
    MasterScore* score = readEncoreScore("instruments_small_tk_key6.enc");
    ASSERT_NE(score, nullptr);
    ASSERT_FALSE(score->parts().empty());
    const Instrument* inst = score->parts().front()->instrument();
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->transpose().chromatic, 6)
        << "Key=6 must be read from smallTK offset (contentFilePos+varSize+53=367)";
    delete score;
}

TEST_F(Tst_Instruments, no_tk_blocks_reads_midi_and_key_from_large_tk_offsets)
{
    // instruments_no_tk_blocks_midi_key.enc: TK00 magic zeroed, MIDI=69 at 2278, Key=6 at 2255.
    // Expected: oboe template selected (MIDI 68 0-indexed = oboe), transposition +6.
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
