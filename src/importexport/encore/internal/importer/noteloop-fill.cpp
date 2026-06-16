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

#include "noteloop-internal.h"

#include <algorithm>

#include "engraving/dom/chord.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/tuplet.h"

namespace mu::iex::encore {

// Case B pickup adjustment: if measure 0 has the same timesig as measure 1 but
// the note loop placed less content than the full measure, shorten it to the
// actual cumTick. Update all subsequent measures' tick positions accordingly.
void adjustPickupMeasure(BuildCtx& ctx, Measure* measure, int measIdx)
{
    if (measIdx != 0 || measure->timesig() != measure->ticks()) {
        return;
    }
    Fraction maxCumTick { 0, 1 };
    for (auto& [key, ct] : ctx.cumTick) {
        if (ct > maxCumTick) {
            maxCumTick = ct;
        }
    }
    if (maxCumTick <= Fraction(0, 1) || maxCumTick >= measure->ticks()) {
        return;
    }
    const Fraction delta = measure->ticks() - maxCumTick;
    measure->setTicks(maxCumTick);
    for (Measure* m = measure->nextMeasure(); m; m = m->nextMeasure()) {
        m->setTick(m->tick() - delta);
    }
    // Any PendingHairpin whose maxEndTick reaches past the shortened measure 0 must
    // be adjusted by the same delta so resolution searches the correct range.
    const Fraction m0End = measure->tick() + maxCumTick;
    for (PendingHairpin& ph : ctx.pendingHairpins) {
        if (ph.maxEndTick > m0End) {
            ph.maxEndTick -= delta;
        }
    }
}

// Pre-fill trailing silence with invisible gap rests so checkMeasure does not
// add visible rests for space that was never encoded in the Encore file.
// Only applies to voices that have some content (cumTick > 0).
void fillTrailingGaps(BuildCtx& ctx, Measure* measure, Fraction measTick)
{
    for (int si = 0; si < ctx.totalStaves; ++si) {
        for (voice_idx_t v = 0; v < VOICES; ++v) {
            const auto key = std::make_pair(si, static_cast<int>(v));
            if (!ctx.cumTick.count(key)) {
                continue;
            }
            const Fraction voicePos = ctx.cumTick.at(key);
            if (voicePos <= Fraction(0, 1)) {
                continue;
            }
            const Fraction remaining = measure->ticks() - voicePos;
            if (remaining <= Fraction(0, 1)) {
                continue;
            }
            const track_idx_t tr = static_cast<track_idx_t>(si * VOICES + v);
            const Fraction fillTick = measTick + voicePos;
            Segment* seg = measure->getSegment(SegmentType::ChordRest, fillTick);
            if (!seg->element(tr)) {
                Rest* r = Factory::createRest(seg, TDuration(DurationType::V_MEASURE));
                r->setTicks(remaining);
                r->setTrack(tr);
                r->setGap(true);
                seg->add(r);
            }
        }
    }
}

// Fix over/undershoots up to 1/24 from non-standard gaps (cascade fills).
// Overshoot: remove smallest gap rests. Undershoot: add V_MEASURE gap rest.
void correctMeasureLength(Measure* measure, int totalStaves)
{
    const Fraction mLen = measure->ticks();
    const Fraction maxDelta(1, 24);
    for (int si = 0; si < totalStaves; ++si) {
        for (voice_idx_t v = 0; v < VOICES; ++v) {
            track_idx_t tr = static_cast<track_idx_t>(si * VOICES + v);
            Fraction voiceSum(0, 1);
            bool hasContent = false;
            std::vector<Rest*> gapRests;
            for (Segment* seg = measure->first(SegmentType::ChordRest);
                 seg; seg = seg->next(SegmentType::ChordRest)) {
                EngravingItem* el = seg->element(tr);
                if (!el) {
                    continue;
                }
                hasContent = true;
                ChordRest* cr = toChordRest(el);
                voiceSum += cr->actualTicks();
                if (el->isRest() && toRest(el)->isGap()) {
                    gapRests.push_back(toRest(el));
                }
            }
            if (!hasContent) {
                continue;
            }
            // Overshoot: remove gap rests smallest-first
            if (voiceSum > mLen && (voiceSum - mLen) <= maxDelta) {
                std::stable_sort(gapRests.begin(), gapRests.end(),
                                 [](Rest* a, Rest* b) {
                    return a->actualTicks() < b->actualTicks();
                });
                for (Rest* gr : gapRests) {
                    if (voiceSum <= mLen) {
                        break;
                    }
                    Fraction at = gr->actualTicks();
                    voiceSum -= at;
                    Segment* gseg = gr->segment();
                    gseg->remove(gr);
                    delete gr;
                }
            }
            // Undershoot: add exact V_MEASURE gap rest for residual
            const Fraction deficit = mLen - voiceSum;
            if (deficit > Fraction(0, 1) && deficit <= maxDelta) {
                const Fraction fillTick = measure->tick() + voiceSum;
                Segment* fillSeg = measure->getSegment(SegmentType::ChordRest, fillTick);
                if (!fillSeg->element(tr)) {
                    Rest* r = Factory::createRest(fillSeg, TDuration(DurationType::V_MEASURE));
                    r->setTicks(deficit);
                    r->setTrack(tr);
                    r->setGap(true);
                    fillSeg->add(r);
                }
            }
        }
    }
}

// Nuclear hard-cap: remove trailing ChordRest elements from any voice that
// still overshoots after correctMeasureLength, then fill any residual deficit
// with an invisible gap rest. Guarantees no measure has wrong total duration.
void capMeasureLength(Measure* measure, int totalStaves)
{
    const Fraction mLen = measure->ticks();
    for (int si = 0; si < totalStaves; ++si) {
        for (voice_idx_t v = 0; v < VOICES; ++v) {
            const track_idx_t tr = static_cast<track_idx_t>(si * VOICES + v);
            std::vector<ChordRest*> crs;
            Fraction voiceSum(0, 1);
            for (Segment* seg = measure->first(SegmentType::ChordRest);
                 seg; seg = seg->next(SegmentType::ChordRest)) {
                EngravingItem* el = seg->element(tr);
                if (!el) {
                    continue;
                }
                ChordRest* cr = toChordRest(el);
                crs.push_back(cr);
                voiceSum += cr->actualTicks();
            }
            if (voiceSum <= mLen || crs.empty()) {
                continue;
            }
            while (voiceSum > mLen && !crs.empty()) {
                ChordRest* last = crs.back();
                crs.pop_back();
                voiceSum -= last->actualTicks();
                if (last->tuplet()) {
                    last->tuplet()->remove(last);
                    last->setTuplet(nullptr);
                }
                Segment* lseg = last->segment();
                lseg->remove(last);
                delete last;
            }
            const Fraction deficit = mLen - voiceSum;
            if (deficit > Fraction(0, 1)) {
                const Fraction fillTick = measure->tick() + voiceSum;
                Segment* fillSeg = measure->getSegment(SegmentType::ChordRest, fillTick);
                if (!fillSeg->element(tr)) {
                    Rest* r = Factory::createRest(fillSeg, TDuration(DurationType::V_MEASURE));
                    r->setTicks(deficit);
                    r->setTrack(tr);
                    r->setGap(true);
                    fillSeg->add(r);
                }
            }
        }
    }
}

} // namespace mu::iex::encore
