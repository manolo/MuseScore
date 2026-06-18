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
    InvisibleRests,    // gap rests (invisible) — current default
    VisibleRests,      // normal visible rests
    IrregularMeasure,  // set actual measure duration to match content
};

enum class OverfillStrategy {
    Truncate,          // remove trailing notes/rests — current default
    StretchLastNote,   // extend last note duration to fill to the barline
    IrregularMeasure,  // set actual measure duration to match content
};

struct EncImportOptions {
    // Layout group
    bool importPageLayout = true;   // apply page size and margins from the Encore file
    bool importPageBreaks = true;   // import page breaks (requires future implementation)
    bool importStaffSize  = true;   // apply staff size scaling from the Encore file

    // Text / content group
    bool importTempoTextSemantic              = true;   // map Italian tempo terms to BPM values
    bool importUnsupportedArticulationsAsText = false;  // emit unknown artic bytes as staff text

    // Measure correction group
    UnderfillStrategy underfillMeasureStrategy = UnderfillStrategy::InvisibleRests;
    OverfillStrategy overfillMeasureStrategy  = OverfillStrategy::Truncate;
    bool firstMeasureIsPickup = true;  // shorten first measure as pickup; false = pad with rests
};
} // namespace enc
} // namespace iex
} // namespace mu

#endif // MU_IMPORTEXPORT_ENC_IMPORT_OPTIONS_H
