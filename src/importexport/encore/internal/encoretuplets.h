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

#ifndef MU_IMPORTEXPORT_ENCORETUPLETS_H
#define MU_IMPORTEXPORT_ENCORETUPLETS_H

#include <set>

#include "engraving/dom/durationtype.h"
#include "engraving/types/fraction.h"
#include "engraving/types/types.h"

#include "encoreelements.h"

namespace mu::engraving {
class Measure;
class Tuplet;
}

namespace mu::iex::encore {

// Tuplet state tracker (one per staff+voice combination).
// With faceValue-cumulative placement, groups close when the accumulated face-value
// sum reaches actualN * baseLen.  This handles mixed-duration brackets (e.g. a 3:2
// triplet of 8th+8th+16th+16th whose face values sum to 3/8 = 3×(1/8)).
struct TupletTracker {
    mu::engraving::Tuplet* currentTuplet { nullptr };
    int actualN       { 0 };        // ratio numerator (e.g. 3 for 3:2)
    int normalN       { 0 };        // ratio denominator (e.g. 2 for 3:2)
    mu::engraving::Fraction placedTicks   { 0, 1 };// cumulative cumTick advances while in this group
    mu::engraving::Fraction faceTicks     { 0, 1 };// cumulative FACE-VALUE sum of notes in this group
    mu::engraving::Fraction fullFaceSum   { 0, 1 };// target = baseLen * actualN (group closes when reached)

    bool inTuplet()  const { return currentTuplet != nullptr; }
    bool groupFull() const;

    void closeTuplet();

    mu::engraving::Tuplet* startTuplet(mu::engraving::Measure* measure,
                                       mu::engraving::Fraction tick,
                                       int aN, int normalN_,
                                       mu::engraving::DurationType baseType,
                                       mu::engraving::track_idx_t track_);

    // Duration advance per note within this tuplet group
    mu::engraving::Fraction noteAdvance(mu::engraving::DurationType baseType) const;
};

// Pre-compute which elements belong to COMPLETE tuplet groups (both implied and explicit).
//
// Implied tuplets (v0xC2): only valid when exactly actualN consecutive chord-groups in
// the same (staffIdx, voice) all have the same detectImpliedTuplet ratio.
// Single isolated notes with a "matching" rdur are MIDI swing drift, not real tuplets.
//
// Explicit tuplets: notes with a standard tup byte (3:2, 5:4, 6:4) are valid when
// exactly actualN consecutive chord-groups have the SAME tup byte.  Isolated notes
// at the tail of a longer run (e.g. note 4 of a 3:2 group in besamemucho) are
// marked as invalid so they are treated as plain notes — preventing partial tuplets
// that confuse checkMeasure.
//
// Returns a set of element pointers that are validated group members.
std::set<const EncMeasureElem*> computeImpliedTupletMembers(
    const MeasureElemRefVec& sortedElems,
    const EncMeasure& encMeas,
    int totalStaves);

} // namespace mu::iex::encore

#endif // MU_IMPORTEXPORT_ENCORETUPLETS_H
