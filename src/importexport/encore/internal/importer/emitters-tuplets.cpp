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

// STUB: TupletTracker helpers. Real implementation replaces this in B10.

#include "emitters-tuplets.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/segment.h"

using namespace mu::engraving;

namespace mu::iex::enc {

bool TupletTracker::groupFull() const
{
    return false;
}

bool fitsTDuration(const Fraction&)
{
    return false;
}

void TupletTracker::closeTuplet()
{
    currentTuplet = nullptr;
}

Tuplet* TupletTracker::startTuplet(Measure*, Fraction, int, int, DurationType, track_idx_t)
{
    return nullptr;
}

Fraction TupletTracker::noteAdvance(DurationType) const
{
    return Fraction(0, 1);
}

} // namespace mu::iex::enc
