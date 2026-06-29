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
// Resize `measure` to `newLen` and propagate the change. Shrinking or extending a measure
// moves the absolute tick of everything that follows it, so the following measures shift by
// the signed delta and so does every pending element that caches an absolute tick. Only
// pending hairpins used to be shifted; pending slurs, ornaments, markers and the rest carry
// absolute ticks too and were silently corrupted by a measure resize. Forward ticks at or
// after the measure's content boundary shift; ticks inside the kept content never move.
static void resizeMeasureAndShift(BuildCtx& ctx, Measure* measure, Fraction newLen)
{
    const Fraction oldLen = measure->ticks();
    const Fraction delta = newLen - oldLen;
    if (delta == Fraction(0, 1)) {
        return;
    }
    const Fraction measTick = measure->tick();
    // Content boundary: positions before it are inside the kept part of the measure and never
    // move; positions at/after it belong to following measures and shift by delta. Shrinking
    // removes the [newEnd, oldEnd) span, so a tick exactly at the new end stays put (strict >);
    // extending inserts space at the old end, so a tick there is the next downbeat and moves (>=).
    const bool shrinking = delta < Fraction(0, 1);
    const Fraction boundary = measTick + std::min(oldLen, newLen);

    measure->setTicks(newLen);
    for (Measure* m = measure->nextMeasure(); m; m = m->nextMeasure()) {
        m->setTick(m->tick() + delta);
    }

    auto shift = [&](Fraction& t) {
        const bool past = shrinking ? (t > boundary) : (t >= boundary);
        if (past) {
            t += delta;
        }
    };

    for (PendingHairpin& p : ctx.pendingHairpins) {
        shift(p.startTick);
        shift(p.maxEndTick);
    }
    for (PendingSlur& p : ctx.pendingSlurs) {
        shift(p.startTick);
    }
    for (PendingArpeggio& p : ctx.pendingArpeggios) {
        shift(p.tick);
    }
    for (PendingOrnTremolo& p : ctx.pendingOrnTremolos) {
        shift(p.tick);
        shift(p.measTick);
    }
    for (PendingTrill& p : ctx.pendingTrills) {
        shift(p.tick);
    }
    for (auto& [tr, ends] : ctx.pendingTrillEnds) {
        for (Fraction& e : ends) {
            shift(e);
        }
    }
    for (PendingStaccato& p : ctx.pendingStaccatos) {
        shift(p.tick);
    }
    for (PendingFermata& p : ctx.pendingFermatas) {
        shift(p.tick);
    }
    for (PendingBreath& p : ctx.pendingBreaths) {
        shift(p.tick);
    }
    for (PendingMeasureRepeat& p : ctx.pendingMeasureRepeats) {
        shift(p.measTick);
    }
    for (PendingBowing& p : ctx.pendingBowings) {
        shift(p.tick);
    }
    for (PendingOrnFingering& p : ctx.pendingOrnFingerings) {
        shift(p.tick);
    }
    for (PendingOttava& p : ctx.pendingOttavas) {
        shift(p.startTick);
    }
    for (PendingMarker& p : ctx.pendingMarkers) {
        shift(p.tick);
    }
}

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
    resizeMeasureAndShift(ctx, measure, maxCumTick);
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
        // Shrink the measure to the longest staff's content, but ONLY when every staff is
        // genuinely short. A staff with no content is a whole-bar rest = the full nominal
        // length, so it is the longest staff and the bar is not short: leave the measure at
        // its nominal length (the short staves are filled to it by checkMeasure). Measuring
        // only the note-bearing staves used to shrink such bars and shift every following
        // measure, corrupting them.
        Fraction maxPos { 0, 1 };
        bool anyStaffSilent = false;
        for (int si = 0; si < ctx.totalStaves && !anyStaffSilent; ++si) {
            Fraction staffLen { 0, 1 };
            for (voice_idx_t v = 0; v < VOICES; ++v) {
                const auto k = std::make_pair(si, static_cast<int>(v));
                if (ctx.cumTick.count(k) && ctx.cumTick.at(k) > staffLen) {
                    staffLen = ctx.cumTick.at(k);
                }
            }
            if (staffLen <= Fraction(0, 1)) {
                anyStaffSilent = true;   // whole-bar rest: this staff fills the nominal length
            } else if (staffLen > maxPos) {
                maxPos = staffLen;
            }
        }
        if (!anyStaffSilent && maxPos > Fraction(0, 1) && maxPos < measure->ticks()) {
            resizeMeasureAndShift(ctx, measure, maxPos);
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
            std::vector<ChordRest*> crs;
            Fraction voiceSum = collectVoice(measure, tr, crs);
            if (crs.empty()) {
                continue;   // no content in this voice
            }
            std::vector<Rest*> gapRests;
            for (ChordRest* cr : crs) {
                if (cr->isRest() && toRest(cr)->isGap()) {
                    gapRests.push_back(toRest(cr));
                }
            }
            // Overshoot: remove gap rests smallest-first.
            // Skip for IrregularMeasure overfill, capMeasureLength will extend instead.
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

// Extend the measure to the maximum voice content (IrregularMeasure behavior), shifting
// later measures and pending hairpins, and filling short voices with a visible rest.
// Used by the IrregularMeasure strategy and as the Stretch fallback when a tuplet cannot
// be compressed enough to be musical.
void extendMeasureIrregular(BuildCtx& ctx, Measure* measure)
{
    const Fraction mLen = measure->ticks();
    const Fraction measTick = measure->tick();

    std::vector<ChordRest*> crs;
    Fraction maxVoiceSum { 0, 1 };
    for (int si = 0; si < ctx.totalStaves; ++si) {
        for (voice_idx_t v = 0; v < VOICES; ++v) {
            const Fraction voiceSum = collectVoice(measure, static_cast<track_idx_t>(si * VOICES + v), crs);
            if (voiceSum > maxVoiceSum) {
                maxVoiceSum = voiceSum;
            }
        }
    }
    if (maxVoiceSum > mLen) {
        resizeMeasureAndShift(ctx, measure, maxVoiceSum);
        // Fill all voices that fall short of the extended measure length.
        // Staves whose content stopped at the original measure length now sit
        // inside a longer measure; a visible rest covers the added time.
        for (int si = 0; si < ctx.totalStaves; ++si) {
            for (voice_idx_t v = 0; v < VOICES; ++v) {
                const track_idx_t tr = static_cast<track_idx_t>(si * VOICES + v);
                const Fraction voiceSum = collectVoice(measure, tr, crs);
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
        extendMeasureIrregular(ctx, measure);
        return;
    }

    for (int si = 0; si < ctx.totalStaves; ++si) {
        for (voice_idx_t v = 0; v < VOICES; ++v) {
            const track_idx_t tr = static_cast<track_idx_t>(si * VOICES + v);
            std::vector<ChordRest*> crs;
            Fraction voiceSum = collectVoice(measure, tr, crs);
            if (voiceSum <= mLen || crs.empty()) {
                continue;
            }
            while (voiceSum > mLen && !crs.empty()) {
                ChordRest* last = crs.back();
                // A tuplet is atomic: dissolve it whole (members revert to plain face value)
                // rather than removing one member and leaving an invalid partial tuplet, then
                // re-collect and keep trimming the now-plain notes.
                if (last->tuplet()) {
                    dissolveTuplet(last->tuplet());
                    voiceSum = collectVoice(measure, tr, crs);
                    continue;
                }
                crs.pop_back();
                voiceSum -= last->actualTicks();
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
