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

#ifndef MU_IMPORTEXPORT_ENC_IMPORT_OPTIONS_H
#define MU_IMPORTEXPORT_ENC_IMPORT_OPTIONS_H

namespace mu {
namespace iex {
namespace enc {
enum class UnderfillStrategy {
    InvisibleRests,    // gap rests (invisible)
    VisibleRests,      // normal visible rests
    IrregularMeasure,  // set actual measure duration to match content (shipped default)
};

enum class OverfillStrategy {
    Truncate,          // remove trailing notes/rests
    StretchLastNote,   // compress the trailing tuplet / notes to fit
    IrregularMeasure,  // set actual measure duration to match content (shipped default)
};

enum class InstrumentSearchMode {
    NameAndMidi,  // name matching + MIDI fallback (shipped default)
    MidiOnly,     // skip name matching, use only MIDI program
    Piano,        // assign Grand Piano to all instruments
};

struct EncImportOptions {
    // Layout group
    bool importPageLayout = true;   // apply page size and margins from the Encore file
    bool importPageBreaks = true;   // apply page breaks derived from the Encore LINE blocks
    bool importStaffSize  = true;   // apply staff size scaling from the Encore file

    // Layout group (continued)
    bool importSystemLocks = true;  // lock each system to Encore's line measure count

    // Text / content group
    bool importTempoTextSemantic              = true;   // map Italian tempo terms to BPM values
    bool importUnsupportedArticulationsAsText = false;  // emit unknown artic bytes as staff text

    // Instrument search
    InstrumentSearchMode instrumentSearchMode = InstrumentSearchMode::NameAndMidi;

    // Measure correction group. These struct values are the in-code fallback used by tests;
    // the shipped (GUI) default for both is IrregularMeasure (see enc-importconfiguration.cpp).
    UnderfillStrategy underfillMeasureStrategy = UnderfillStrategy::InvisibleRests;
    OverfillStrategy overfillMeasureStrategy  = OverfillStrategy::Truncate;
    bool firstMeasureIsPickup = true;  // shorten first measure as pickup; false = pad with rests

    // Voice consolidation. When a staff splits its content across several voices
    // that never sound at the same time, collapse them back into voice 1. A staff
    // is only collapsed when ALL of its voices fit into voice 1 with no timing
    // change (notes that genuinely overlap leave the staff untouched). This struct
    // value is the in-code fallback used by tests (off, so existing fixtures keep
    // their voices); the shipped (GUI) default is true (see enc-importconfiguration.cpp).
    bool mergeVoices = false;
};
} // namespace enc
} // namespace iex
} // namespace mu

#endif // MU_IMPORTEXPORT_ENC_IMPORT_OPTIONS_H
