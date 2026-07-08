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
#pragma once

#include "engraving/types/fraction.h"

namespace mu::iex::enc {
struct EncMeasure;

// Encore ticks per whole note for a measure. Encore stores element ticks on a fixed
// 960-ticks-per-whole grid (kEncWholeTicks). When the measure carries a usable time
// signature this is re-derived as durTicks x timeSigDen / timeSigNum, which yields the
// same 960 for a well-formed bar and stays correct for compound meters, where
// beatTicks x timeSigDen does NOT. Falls back to kEncWholeTicks otherwise. Single source
// of truth for the "ticks per whole note" conversion used when snapping spanners and
// ornaments to Encore element ticks.
int encWholeNoteTicks(const EncMeasure& measure);

// ---------------------------------------------------------------------------
// Coordinate-based anchoring helpers (shared)
//
// Encore draws an ornament/spanner glyph at an x-position (`xoffset`) whose origin
// differs from the note `xoffset` origin by a per-file constant (it is NOT zero). So a
// raw xoffset value cannot be compared directly against note xoffsets. Two patterns
// recur across the importer and live here so dynamics, tempo marks, hairpins, trills and
// slurs all resolve positions the same way. See ENCORE_IMPORTER.md "Coordinate-based
// anchoring of ornaments and spanners"; the xoffset column itself is ENCORE_FORMAT.md
// §Chord column (xoffset).
// ---------------------------------------------------------------------------

// START anchor. Trusts the element's own tick (`defaultTick`); only when the glyph is
// drawn to the LEFT of the note at that tick (its xoffset is smaller) does it snap back to
// the latest preceding chord/rest on the same staff whose xoffset is <= the glyph xoffset.
// Used for the start of dynamics, tempo marks, hairpins, trills and slurs.
mu::engraving::Fraction snapStartTickByXoffset(
    mu::engraving::Fraction defaultTick, const EncMeasure& encMeas,
    int staffIdx, int ornXoffset, mu::engraving::Fraction measTick);
}
