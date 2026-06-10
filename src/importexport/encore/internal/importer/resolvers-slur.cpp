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
#include "engraving/dom/slur.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/note.h"
#include "log.h"

using namespace mu::engraving;

namespace mu::iex::encore {
// Return the track of the first Chord at tick on staffIdx (any voice), or fallback if none.
// Corrects slur tracks after stream-overflow voice reassignment: SLURSTART is parsed before the note, so ps.track may be stale.
static track_idx_t resolveChordTrack(MasterScore* score, Fraction tick, int staffIdx, track_idx_t fallback)
{
    Segment* seg = score->tick2segment(tick, false, SegmentType::ChordRest);
    if (!seg) {
        return fallback;
    }
    for (int v = 0; v < static_cast<int>(VOICES); ++v) {
        track_idx_t t = static_cast<track_idx_t>(staffIdx * VOICES + v);
        EngravingItem* el = seg->element(t);
        if (el && el->isChord()) {
            return t;
        }
    }
    return fallback;
}

void resolveSlurs(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;
    const EncFile& enc = ctx.enc;

    // Resolve slur intents: .enc has no SLURSTOP; endpoint from alMezuro.
    // Anchor on last ChordRest in the target measure on this track.
    for (const PendingSlur& ps : ctx.pendingSlurs) {
        // When alMezuro is not a reliable measure count (e.g. v0xC2), clamp to
        // the start measure so the same-measure xoffset heuristic handles it.
        int clampedEndMeasIdx = ps.endMeasIdx;
        if (!ctx.alMezuroIsReliable && ps.startMeasIdx >= 0
            && ps.startMeasIdx < static_cast<int>(ctx.measuresByIdx.size())) {
            clampedEndMeasIdx = ps.startMeasIdx;
        }
        if (clampedEndMeasIdx < 0
            || clampedEndMeasIdx >= static_cast<int>(ctx.measuresByIdx.size())) {
            continue;
        }
        Measure* endMeas = ctx.measuresByIdx[clampedEndMeasIdx];
        Fraction endTick;
        bool resolved = false;

        // Same-measure heuristic: find the note closest to first_note_xoff + pixelSpan.
        // Applied when alMezuro is unreliable or zero; skipped for cross-measure v0xC4
        // slurs (alMezuro > 0) because xoffsets reset at barlines.
        const bool tryHeuristic = (!ctx.alMezuroIsReliable || ps.alMezuro == 0)
                                  && ps.startMeasIdx >= 0
                                  && ps.startMeasIdx < static_cast<int>(enc.measures.size());
        if (tryHeuristic) {
            const EncMeasure& startEncMeas = enc.measures[ps.startMeasIdx];
            int firstNoteXoff = -1;
            // Find the first note on this track whose Encore tick matches the slur's start tick.
            const Fraction relStartTick = ps.startTick - ctx.measuresByIdx[ps.startMeasIdx]->tick();
            // Whole-note ticks: durTicks × timeSigDen / timeSigNum (e.g. 6/8: 720×8/6=960).
            // beatTicks × timeSigDen is WRONG for compound meters (e.g. 6/8 gives 360×8=2880≠960).
            const int wt = (startEncMeas.durTicks && startEncMeas.timeSigNum && startEncMeas.timeSigDen)
                           ? (static_cast<int>(startEncMeas.durTicks) * startEncMeas.timeSigDen)
                           / startEncMeas.timeSigNum : 960;
            const int startEncTick = (relStartTick.numerator() * wt)
                                     / std::max(1, relStartTick.denominator());
            for (const auto& elem : startEncMeas.elements) {
                const EncMeasureElem* em = elem.get();
                if (em->type != static_cast<quint8>(EncElemType::NOTE)) {
                    continue;
                }
                // Search all voices: the slur ORN's encVoice is where the arc is drawn, not necessarily where the notes are.
                if (em->staffIdx != ps.staffIdx) {
                    continue;
                }
                if (static_cast<int>(em->tick) != startEncTick) {
                    continue;
                }
                firstNoteXoff = static_cast<int>(static_cast<const EncNote*>(em)->xoffset);
                break;
            }
            if (firstNoteXoff >= 0) {
                const int pixelSpan = ps.slurXoffset2 - ps.slurXoffset;
                const int targetEndXoff = firstNoteXoff + pixelSpan;
                int bestDist = std::numeric_limits<int>::max();
                int bestEncTick = -1;
                for (const auto& elem : startEncMeas.elements) {
                    const EncMeasureElem* em = elem.get();
                    if (em->type != static_cast<quint8>(EncElemType::NOTE)) {
                        continue;
                    }
                    if (em->staffIdx != ps.staffIdx) {
                        continue;
                    }
                    // Only consider notes strictly after the start — the start note
                    // cannot be its own slur endpoint.
                    if (static_cast<int>(em->tick) <= startEncTick) {
                        continue;
                    }
                    const int xoff = static_cast<int>(static_cast<const EncNote*>(em)->xoffset);
                    const int dist = std::abs(xoff - targetEndXoff);
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestEncTick = static_cast<int>(em->tick);
                    }
                }
                if (bestEncTick > startEncTick) {
                    const Fraction endRel(bestEncTick, wt);
                    endTick = ctx.measuresByIdx[ps.startMeasIdx]->tick() + endRel;
                    resolved = true;
                }
            }
        }

        // Resolve the actual track from the placed chord (may differ from ps.track
        // when a stream-overflow moved the note to a higher MuseScore voice).
        const track_idx_t startTrack = resolveChordTrack(score, ps.startTick, ps.staffIdx, ps.track);

        // Cross-measure: use xoffset2 to find the end note by proximity (xoffset2 resets at barlines, directly comparable within the target measure).
        if (!resolved && clampedEndMeasIdx < static_cast<int>(enc.measures.size())) {
            const EncMeasure& endEncMeas = enc.measures[clampedEndMeasIdx];
            const int wholeTicks = (endEncMeas.durTicks && endEncMeas.timeSigNum && endEncMeas.timeSigDen)
                                   ? (static_cast<int>(endEncMeas.durTicks) * endEncMeas.timeSigDen)
                                   / endEncMeas.timeSigNum : 960;
            int bestDist = std::numeric_limits<int>::max();
            int bestEncTick = -1;
            for (const auto& elem : endEncMeas.elements) {
                const EncMeasureElem* em = elem.get();
                if (em->type != static_cast<quint8>(EncElemType::NOTE)) {
                    continue;
                }
                if (em->staffIdx != ps.staffIdx) {
                    continue;
                }
                const int xoff = static_cast<int>(static_cast<const EncNote*>(em)->xoffset);
                const int dist = std::abs(xoff - ps.slurXoffset2);
                if (dist <= bestDist) {
                    bestDist = dist;
                    bestEncTick = static_cast<int>(em->tick);
                }
            }
            if (bestEncTick >= 0 && wholeTicks > 0) {
                const Fraction candidate = endMeas->tick()
                                           + Fraction(bestEncTick, wholeTicks).reduced();
                if (candidate > ps.startTick) {
                    endTick = candidate;
                    resolved = true;
                }
            }
        }

        if (!resolved) {
            Segment* lastSeg = nullptr;
            for (Segment* s = endMeas->first(SegmentType::ChordRest); s;
                 s = s->next(SegmentType::ChordRest)) {
                // Accept any chord on the same staff (any voice).
                bool hasChord = false;
                for (int v = 0; v < static_cast<int>(VOICES); ++v) {
                    track_idx_t t = static_cast<track_idx_t>(ps.staffIdx * VOICES + v);
                    if (s->element(t) && s->element(t)->isChord()) {
                        hasChord = true;
                        break;
                    }
                }
                if (hasChord) {
                    lastSeg = s;
                }
            }
            if (!lastSeg) {
                continue;
            }
            endTick = lastSeg->tick();
        }

        if (endTick <= ps.startTick) {
            continue;   // zero or negative span: drop
        }
        const track_idx_t endTrack = resolveChordTrack(score, endTick, ps.staffIdx, startTrack);

        Slur* slur = Factory::createSlur(score->dummy());
        slur->setTrack(startTrack);
        slur->setTrack2(endTrack);
        slur->setTick(ps.startTick);
        slur->setTick2(endTick);
        score->addElement(slur);
    }

    // Remove slurs whose start or end note doesn't exist (corrupted files).
    // Such slurs cause NaN in Bezier layout.
    {
        std::vector<Spanner*> toRemove;
        for (auto& [tick, spanner] : score->spannerMap().map()) {
            if (spanner->isSlur()) {
                spanner->computeStartElement();
                spanner->computeEndElement();
                if (!spanner->startElement() || !spanner->endElement()) {
                    toRemove.push_back(spanner);
                }
            }
        }
        for (Spanner* sp : toRemove) {
            score->removeElement(sp);
        }
    }
}
} // namespace mu::iex::encore
