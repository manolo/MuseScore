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
#ifndef MU_IMPORTEXPORT_ENC_IMPORT_RESOLVERS_H
#define MU_IMPORTEXPORT_ENC_IMPORT_RESOLVERS_H

#include "ctx.h"

#include "engraving/dom/chord.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/segment.h"

namespace mu::iex::enc {
void resolveAll(BuildCtx& ctx);
void resolveSlurs(BuildCtx& ctx);
void resolveHairpins(BuildCtx& ctx);
void resolveOrnaments(BuildCtx& ctx);
void resolveFingeringAndBowing(BuildCtx& ctx);

// Tick → measure → ChordRest segment → Chord lookup; shared by all resolver files.
inline mu::engraving::Chord* findChordAt(mu::engraving::MasterScore* score,
                                         mu::engraving::Fraction tick,
                                         mu::engraving::track_idx_t track)
{
    using namespace mu::engraving;
    Measure* m = score->tick2measure(tick);
    if (!m) {
        return nullptr;
    }
    Segment* seg = m->findSegment(SegmentType::ChordRest, tick);
    if (!seg) {
        return nullptr;
    }
    EngravingItem* el = seg->element(track);
    if (!el || !el->isChord()) {
        return nullptr;
    }
    return toChord(el);
}
} // namespace mu::iex::enc

#endif // MU_IMPORTEXPORT_ENC_IMPORT_RESOLVERS_H
