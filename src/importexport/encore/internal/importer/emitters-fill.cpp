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

#include "emitters-internal.h"

#include <algorithm>

#include "engraving/dom/chord.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/tuplet.h"

namespace mu::iex::enc {
// Case B pickup adjustment: if measure 0 has the same timesig as measure 1 but
// the note loop placed less content than the full measure, shorten it to the
// actual cumTick. Update all subsequent measures' tick positions accordingly.
void adjustPickupMeasure(BuildCtx& ctx, Measure* measure, int measIdx)
{
    if (!ctx.opts.firstMeasureIsPickup) {
        return;
    }
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

// Pre-fill trailing silence with rests so checkMeasure does not add its own.
// InvisibleRests (default): gap rests keep the score clean.
// VisibleRests: normal rests so the user can see the empty beats.
// IrregularMeasure: no rests added; the measure actual duration is shortened to match content.
// Only applies to voices that have some content (cumTick > 0).
void fillTrailingGaps(BuildCtx& ctx, Measure* measure, Fraction measTick)
{
    const bool makeGap = (ctx.opts.underfillMeasureStrategy != UnderfillStrategy::VisibleRests
                          && ctx.opts.underfillMeasureStrategy != UnderfillStrategy::IrregularMeasure);
    const bool irregular = (ctx.opts.underfillMeasureStrategy == UnderfillStrategy::IrregularMeasure);

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
            if (irregular) {
                continue;
            }
            const track_idx_t tr = static_cast<track_idx_t>(si * VOICES + v);
            const Fraction fillTick = measTick + voicePos;
            Segment* seg = measure->getSegment(SegmentType::ChordRest, fillTick);
            if (!seg->element(tr)) {
                Rest* r = Factory::createRest(seg, TDuration(DurationType::V_MEASURE));
                r->setTicks(remaining);
                r->setTrack(tr);
                r->setGap(makeGap);
                seg->add(r);
            }
        }
    }

    if (irregular) {
        // Shrink the measure to the maximum voice content position.
        Fraction maxPos { 0, 1 };
        for (int si = 0; si < ctx.totalStaves; ++si) {
            for (voice_idx_t v = 0; v < VOICES; ++v) {
                const auto k = std::make_pair(si, static_cast<int>(v));
                if (ctx.cumTick.count(k) && ctx.cumTick.at(k) > maxPos) {
                    maxPos = ctx.cumTick.at(k);
                }
            }
        }
        if (maxPos > Fraction(0, 1) && maxPos < measure->ticks()) {
            const Fraction delta = measure->ticks() - maxPos;
            measure->setTicks(maxPos);
            for (Measure* m = measure->nextMeasure(); m; m = m->nextMeasure()) {
                m->setTick(m->tick() - delta);
            }
            for (PendingHairpin& ph : ctx.pendingHairpins) {
                if (ph.maxEndTick > measTick + maxPos) {
                    ph.maxEndTick -= delta;
                }
            }
        }
    }
}

// Maximum measure-length correction: 1/24 of a whole note (≈ one 32nd-note triplet).
// Corrections larger than this indicate genuine notation errors, not rounding noise.
static const Fraction kFillMaxDelta(1, 24);

// Fix over/undershoots up to kFillMaxDelta from non-standard gaps (cascade fills).
// Overshoot: remove smallest gap rests. Undershoot: add V_MEASURE gap rest.
void correctMeasureLength(BuildCtx& ctx, Measure* measure)
{
    const bool makeGap = (ctx.opts.underfillMeasureStrategy != UnderfillStrategy::VisibleRests);
    const Fraction mLen = measure->ticks();
    const Fraction maxDelta = kFillMaxDelta;
    for (int si = 0; si < ctx.totalStaves; ++si) {
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
            // Overshoot: remove gap rests smallest-first.
            // Skip for IrregularMeasure overfill — capMeasureLength will extend instead.
            if (voiceSum > mLen && (voiceSum - mLen) <= maxDelta
                && ctx.opts.overfillMeasureStrategy != OverfillStrategy::IrregularMeasure) {
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
            // Undershoot: add exact V_MEASURE rest for residual
            const Fraction deficit = mLen - voiceSum;
            if (deficit > Fraction(0, 1) && deficit <= maxDelta) {
                const Fraction fillTick = measure->tick() + voiceSum;
                Segment* fillSeg = measure->getSegment(SegmentType::ChordRest, fillTick);
                if (!fillSeg->element(tr)) {
                    Rest* r = Factory::createRest(fillSeg, TDuration(DurationType::V_MEASURE));
                    r->setTicks(deficit);
                    r->setTrack(tr);
                    r->setGap(makeGap);
                    fillSeg->add(r);
                }
            }
        }
    }
}

// Nuclear hard-cap: remove trailing ChordRest elements from any voice that
// still overshoots after correctMeasureLength, then fill any residual deficit
// with a rest. Guarantees no measure has wrong total duration.
// Exception: IrregularMeasure overfill extends the measure to the maximum voice
// content instead of truncating, preserving all notes and their spanner endpoints.
void capMeasureLength(BuildCtx& ctx, Measure* measure)
{
    const bool makeGap = (ctx.opts.underfillMeasureStrategy != UnderfillStrategy::VisibleRests);
    const Fraction mLen = measure->ticks();
    const Fraction measTick = measure->tick();

    if (ctx.opts.overfillMeasureStrategy == OverfillStrategy::IrregularMeasure) {
        Fraction maxVoiceSum { 0, 1 };
        for (int si = 0; si < ctx.totalStaves; ++si) {
            for (voice_idx_t v = 0; v < VOICES; ++v) {
                const track_idx_t tr = static_cast<track_idx_t>(si * VOICES + v);
                Fraction voiceSum { 0, 1 };
                for (Segment* seg = measure->first(SegmentType::ChordRest);
                     seg; seg = seg->next(SegmentType::ChordRest)) {
                    EngravingItem* el = seg->element(tr);
                    if (el) {
                        voiceSum += toChordRest(el)->actualTicks();
                    }
                }
                if (voiceSum > maxVoiceSum) {
                    maxVoiceSum = voiceSum;
                }
            }
        }
        if (maxVoiceSum > mLen) {
            const Fraction delta = maxVoiceSum - mLen;
            measure->setTicks(maxVoiceSum);
            for (Measure* m = measure->nextMeasure(); m; m = m->nextMeasure()) {
                m->setTick(m->tick() + delta);
            }
            for (PendingHairpin& ph : ctx.pendingHairpins) {
                if (ph.maxEndTick >= measTick + mLen) {
                    ph.maxEndTick += delta;
                }
            }
            // Fill all voices that fall short of the extended measure length.
            // Staves whose content stopped at the original measure length now sit
            // inside a longer measure; a visible rest covers the added time.
            for (int si = 0; si < ctx.totalStaves; ++si) {
                for (voice_idx_t v = 0; v < VOICES; ++v) {
                    const track_idx_t tr = static_cast<track_idx_t>(si * VOICES + v);
                    Fraction voiceSum { 0, 1 };
                    for (Segment* seg = measure->first(SegmentType::ChordRest);
                         seg; seg = seg->next(SegmentType::ChordRest)) {
                        EngravingItem* el = seg->element(tr);
                        if (el) {
                            voiceSum += toChordRest(el)->actualTicks();
                        }
                    }
                    if (voiceSum <= Fraction(0, 1) || voiceSum >= maxVoiceSum) {
                        continue;
                    }
                    const Fraction fillTick = measTick + voiceSum;
                    Segment* fillSeg = measure->getSegment(SegmentType::ChordRest, fillTick);
                    if (!fillSeg->element(tr)) {
                        Rest* r = Factory::createRest(fillSeg, TDuration(DurationType::V_MEASURE));
                        r->setTicks(maxVoiceSum - voiceSum);
                        r->setTrack(tr);
                        r->setGap(false);
                        fillSeg->add(r);
                    }
                }
            }
        }
        return;
    }

    for (int si = 0; si < ctx.totalStaves; ++si) {
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
                    r->setGap(makeGap);
                    fillSeg->add(r);
                }
            }
        }
    }
}
} // namespace mu::iex::enc
