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

namespace mu::iex::enc {
void resolveHairpins(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;
    const EncRoot& enc = ctx.enc;

    // Resolve hairpin endpoints: min(next-dynamic tick, xoffset2 snap to note).
    // xoffset2 encodes the visual tip position; snap to the last note/rest whose xoffset <= xoff2.
    // If xoff2 is before all notes in the target measure, end at the barline (measure start tick).
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

        // (2) xoffset2 snap: find the last note/rest in the target measure with xoffset <= xoff2.
        // Mirrors the start-snap logic in snapTickByXoffset (noteloop-orn.cpp).
        // Only when no Dynamic found in step (1) and xoff2 is meaningful (> 0).
        if (!foundNextDynamic
            && ph.hairpinXoffset2 > 0
            && ph.endMeasIdx >= 0
            && ph.endMeasIdx < static_cast<int>(enc.measures.size())) {
            const EncMeasure& endEncMeas = enc.measures[ph.endMeasIdx];
            if (endEncMeas.beatTicks && endEncMeas.timeSigDen) {
                const int wholeTicks = static_cast<int>(endEncMeas.beatTicks)
                                       * static_cast<int>(endEncMeas.timeSigDen);
                const int xoff2 = ph.hairpinXoffset2;
                int bestEncTick = -1;
                int bestXoff = -1;
                int anyPositiveXoff = -1;   // sentinel: any note/rest with xoff > 0 exists
                for (const auto& elem : endEncMeas.elements) {
                    const EncMeasureElem* em = elem.get();
                    int xoff = 0;
                    if (em->type == static_cast<quint8>(EncElemType::NOTE)) {
                        xoff = static_cast<int>(static_cast<const EncNote*>(em)->xoffset);
                    } else if (em->type == static_cast<quint8>(EncElemType::REST)) {
                        xoff = static_cast<int>(static_cast<const EncRest*>(em)->xoffset);
                    } else {
                        continue;
                    }
                    if (em->staffIdx != ph.staffIdx || em->voice != ph.encVoice) {
                        continue;
                    }
                    if (xoff <= 0) {
                        continue;
                    }
                    anyPositiveXoff = xoff;
                    if (xoff <= xoff2 && xoff > bestXoff) {
                        bestXoff = xoff;
                        bestEncTick = static_cast<int>(em->tick);
                    }
                }
                Fraction targetMeasTick = ctx.measuresByIdx[ph.endMeasIdx]->tick();
                if (bestEncTick >= 0) {
                    // Snap end to the note/rest whose xoffset best matches xoff2.
                    Fraction snapEnd = targetMeasTick
                                       + Fraction(bestEncTick, wholeTicks).reduced();
                    endTick = std::min(endTick, snapEnd);
                } else if (anyPositiveXoff >= 0) {
                    // xoff2 precedes all notes with positive xoffsets: end at the barline.
                    endTick = std::min(endTick, targetMeasTick);
                }
                // else: no notes with positive xoffsets (synthetic/empty data); keep maxEndTick.
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
} // namespace mu::iex::enc
