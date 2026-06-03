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

#include "resolvers.h"
#include "../parser/elements.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/segment.h"

using namespace mu::engraving;

namespace mu::iex::encore {
void resolveHairpins(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;
    const EncFile& enc = ctx.enc;

    // Resolve hairpin endpoints: min(next-dynamic tick, xoffset2 clamp).
    // Cross-measure: xoffset2 < first-note xoffset means hairpin ends at the barline instead.
    for (const PendingHairpin& ph : ctx.pendingHairpins) {
        Fraction endTick = ph.maxEndTick;

        // (1) Next Dynamic on track takes priority; handles mf<f>mf chains.
        bool foundNextDynamic = false;
        for (Segment* s = score->firstSegment(SegmentType::ChordRest); s; s = s->next1(SegmentType::ChordRest)) {
            if (s->tick() <= ph.startTick) {
                continue;
            }
            if (s->tick() > ph.maxEndTick) {
                break;
            }
            bool stopHere = false;
            for (EngravingItem* ann : s->annotations()) {
                if (ann && ann->isDynamic() && ann->track() == ph.track) {
                    stopHere = true;
                    break;
                }
            }
            if (stopHere) {
                endTick = std::min(endTick, s->tick());
                foundNextDynamic = true;
                break;
            }
        }

        // (2) xoffset2 clamp: if xoffset2 < first note's xoffset in target measure, end at barline.
        // Only when no Dynamic found in step (1).
        if (!foundNextDynamic
            && ph.endMeasIdx >= 0
            && ph.endMeasIdx < static_cast<int>(enc.measures.size())) {
            const EncMeasure& endEncMeas = enc.measures[ph.endMeasIdx];
            if (endEncMeas.beatTicks && endEncMeas.timeSigDen) {
                const int xoff2 = ph.hairpinXoffset2;
                int firstNoteXoff = -1;
                for (const auto& elem : endEncMeas.elements) {
                    const EncMeasureElem* em = elem.get();
                    if (em->type != static_cast<quint8>(EncElemType::NOTE)) {
                        continue;
                    }
                    if (em->staffIdx != ph.staffIdx || em->voice != ph.encVoice) {
                        continue;
                    }
                    const int xoff = static_cast<int>(
                        static_cast<const EncNote*>(em)->xoffset);
                    if (xoff > 0 && (firstNoteXoff < 0 || xoff < firstNoteXoff)) {
                        firstNoteXoff = xoff;
                    }
                }
                if (firstNoteXoff > 0 && xoff2 < firstNoteXoff) {
                    Fraction targetMeasTick = ctx.measuresByIdx[ph.endMeasIdx]->tick();
                    endTick = std::min(endTick, targetMeasTick);
                }
            }
        }

        if (endTick <= ph.startTick) {
            continue;
        }
        Hairpin* hp = Factory::createHairpin(score->dummy()->segment());
        hp->setTrack(ph.track);
        hp->setTrack2(ph.track);
        hp->setTick(ph.startTick);
        hp->setTick2(endTick);
        hp->setHairpinType(ph.type);
        score->addElement(hp);
    }
}
} // namespace mu::iex::encore
