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

namespace mu::iex::enc {
// Find the actual chord track at tick: SLURSTART is parsed before the note, so ps.track may be stale after stream-overflow voice reassignment.
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
    const EncRoot& enc = ctx.enc;

    // Build compact-rawStaff → LINE-slot lookup (same logic as noteloop.cpp).
    // (staffWithin<<6)|instrIdx is the raw staff byte; without this, comparisons mismatch beyond the first instrument.
    std::array<int, 256> lineSlotByRawByte;
    lineSlotByRawByte.fill(-1);
    if (!enc.lines.empty()) {
        const auto& sd = enc.lines[0].staffData;
        for (int s = 0; s < static_cast<int>(sd.size()); ++s) {
            lineSlotByRawByte[static_cast<unsigned char>(sd[s].instrStaffIdx)] = s;
        }
    }
    auto emLineSlot = [&](const EncMeasureElem* em) -> int {
        const quint8 raw = (static_cast<quint8>(em->staffWithin) << 6)
                           | static_cast<quint8>(em->staffIdx);
        const int slot = lineSlotByRawByte[static_cast<unsigned char>(raw)];
        return (slot >= 0) ? slot : static_cast<int>(em->staffIdx);
    };

    // .enc has no SLURSTOP; endpoint derived from alMezuro (target measure) + xoffset heuristic.
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
        // Skipped for cross-measure v0xC4 slurs (alMezuro > 0) because xoffsets reset at barlines.
        const bool tryHeuristic = (!ctx.alMezuroIsReliable || ps.alMezuro == 0)
                                  && ps.startMeasIdx >= 0
                                  && ps.startMeasIdx < static_cast<int>(enc.measures.size());
        if (tryHeuristic) {
            const EncMeasure& startEncMeas = enc.measures[ps.startMeasIdx];
            int firstNoteXoff = -1;
            const Fraction relStartTick = ps.startTick - ctx.measuresByIdx[ps.startMeasIdx]->tick();
            // Whole-note ticks: durTicks × timeSigDen / timeSigNum (e.g. 6/8: 720×8/6=960).
            // beatTicks × timeSigDen is WRONG for compound meters.
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
                // Search all voices: slur ORN encVoice is the arc position, not necessarily the note voice.
                if (emLineSlot(em) != ps.staffIdx) {
                    continue;
                }
                if (static_cast<int>(em->tick) != startEncTick) {
                    continue;
                }
                const EncNote* en = static_cast<const EncNote*>(em);
                const int xoff = static_cast<int>(en->xoffset);
                if (en->graceType() != EncGraceType::NORMAL) {
                    // v0xC4: regular note appears before grace in binary; prefer grace xoffset
                    // as arc-start reference to avoid inflating targetEndXoff.
                    firstNoteXoff = xoff;
                    break;
                }
                if (firstNoteXoff < 0) {
                    firstNoteXoff = xoff;   // regular: tentative, keep searching
                }
            }
            if (firstNoteXoff >= 0) {
                const int pixelSpan = ps.slurXoffset2 - ps.slurXoffset;
                // Tiny pixelSpan (0-2) with note before arc start: firstNoteXoff+pixelSpan ≈ 0 matches a decoy.
                // Use slurXoffset2 directly as the arc-end target instead.
                const int targetEndXoff
                    =(pixelSpan >= 0 && pixelSpan <= 2 && firstNoteXoff < ps.slurXoffset)
                      ? ps.slurXoffset2
                      : firstNoteXoff + pixelSpan;
                // Single pass: find best later-note endpoint + detect grace/regular co-location for grace-to-main shortcut.
                {
                    int bestDist = std::numeric_limits<int>::max();
                    int bestEncTick = -1;
                    int maxXoffInMeas = -1;
                    bool hasGraceAtStart = false;
                    int regularXoffAtStart = -1;
                    for (const auto& elem : startEncMeas.elements) {
                        const EncMeasureElem* em = elem.get();
                        if (em->type != static_cast<quint8>(EncElemType::NOTE)) {
                            continue;
                        }
                        if (emLineSlot(em) != ps.staffIdx) {
                            continue;
                        }
                        const int xoff = static_cast<int>(static_cast<const EncNote*>(em)->xoffset);
                        if (xoff > maxXoffInMeas) {
                            maxXoffInMeas = xoff;
                        }
                        if (static_cast<int>(em->tick) == startEncTick) {
                            const EncNote* en = static_cast<const EncNote*>(em);
                            if (en->graceType() != EncGraceType::NORMAL) {
                                hasGraceAtStart = true;
                            } else {
                                // Gap notes often have xoffset=0; keep the best-matching regular note.
                                const int thisDist = std::abs(xoff - targetEndXoff);
                                if (regularXoffAtStart < 0
                                    || thisDist < std::abs(regularXoffAtStart - targetEndXoff)) {
                                    regularXoffAtStart = xoff;
                                }
                            }
                        }
                        // Only notes strictly after the start can be endpoints.
                        if (static_cast<int>(em->tick) <= startEncTick) {
                            continue;
                        }
                        const int dist = std::abs(xoff - targetEndXoff);
                        if (dist < bestDist) {
                            bestDist = dist;
                            bestEncTick = static_cast<int>(em->tick);
                        }
                    }
                    // Grace-to-main: grace + regular share startEncTick and regular is closest match → zero-span.
                    // If a later note is closer, resolve as grace-to-later instead.
                    if (hasGraceAtStart && regularXoffAtStart >= 0) {
                        const int regularDist = std::abs(regularXoffAtStart - targetEndXoff);
                        if (regularDist < bestDist) {
                            endTick  = ps.startTick;
                            resolved = true;
                        }
                    }
                    if (!resolved && bestEncTick > startEncTick) {
                        const Fraction endRel(bestEncTick, wt);
                        const Fraction candidate = ctx.measuresByIdx[ps.startMeasIdx]->tick()
                                                   + endRel;
                        // Snap to chord segment: grace notes steal time, shifting cumTick earlier than proportional tick.
                        Measure* sMeas = ctx.measuresByIdx[ps.startMeasIdx];
                        Segment* snappedSeg = score->tick2leftSegment(
                            candidate, false, SegmentType::ChordRest);
                        if (snappedSeg && snappedSeg->measure() == sMeas
                            && snappedSeg->tick() >= ps.startTick) {
                            bool hasChord = false;
                            for (int v = 0; v < static_cast<int>(VOICES) && !hasChord; ++v) {
                                track_idx_t t = static_cast<track_idx_t>(
                                    ps.staffIdx * VOICES + v);
                                if (snappedSeg->element(t)
                                    && snappedSeg->element(t)->isChord()) {
                                    hasChord = true;
                                }
                            }
                            endTick = hasChord ? snappedSeg->tick() : candidate;
                        } else {
                            endTick = candidate;
                        }
                        resolved = true;
                    }
                    // v0xC2 cross-measure extension: targetEndXoff exceeds all same-measure xoffsets
                    // and no same-measure note found → search up to 2 measures forward.
                    if (!resolved && !ctx.alMezuroIsReliable && bestDist > 0
                        && targetEndXoff > maxXoffInMeas
                        && bestEncTick < 0) {
                        for (int nextMIdx = ps.startMeasIdx + 1;
                             nextMIdx <= ps.startMeasIdx + 2
                             && nextMIdx < static_cast<int>(enc.measures.size())
                             && nextMIdx < static_cast<int>(ctx.measuresByIdx.size());
                             ++nextMIdx) {
                            const EncMeasure& nextEncMeas = enc.measures[nextMIdx];
                            const int nextWt
                                = (nextEncMeas.durTicks && nextEncMeas.timeSigNum
                                   && nextEncMeas.timeSigDen)
                                  ? (static_cast<int>(nextEncMeas.durTicks)
                                     * nextEncMeas.timeSigDen)
                                  / nextEncMeas.timeSigNum : 960;
                            Measure* nextMs = ctx.measuresByIdx[nextMIdx];
                            for (const auto& elem : nextEncMeas.elements) {
                                const EncMeasureElem* em = elem.get();
                                if (em->type != static_cast<quint8>(EncElemType::NOTE)) {
                                    continue;
                                }
                                if (emLineSlot(em) != ps.staffIdx) {
                                    continue;
                                }
                                const int xoff = static_cast<int>(
                                    static_cast<const EncNote*>(em)->xoffset);
                                const int dist = std::abs(xoff - targetEndXoff);
                                if (dist < bestDist) {
                                    bestDist = dist;
                                    const Fraction endRel(static_cast<int>(em->tick), nextWt);
                                    endTick = nextMs->tick() + endRel.reduced();
                                    resolved = true;
                                }
                            }
                            if (bestDist == 0) {
                                break;
                            }
                        }
                    }
                } // integrated endpoint-search block
            }
        }

        const track_idx_t startTrack = resolveChordTrack(score, ps.startTick, ps.staffIdx, ps.track);

        // Cross-measure fallback: xoffset2 resets at barlines, so it's directly comparable within the target measure.
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
                if (emLineSlot(em) != ps.staffIdx) {
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
                // Accept any voice on the same staff.
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

        if (endTick < ps.startTick) {
            continue;   // negative span: always drop
        }
        if (endTick == ps.startTick) {
            // Zero span: SLURSTART at grace tick == parent cumTick. Build grace-to-main slur with explicit elements.
            Segment* gSeg = score->tick2segment(ps.startTick, true,
                                                SegmentType::ChordRest);
            if (gSeg) {
                const track_idx_t graceTrack = resolveChordTrack(
                    score, ps.startTick, ps.staffIdx, ps.track);
                EngravingItem* el = gSeg->element(graceTrack);
                if (el && el->isChord()) {
                    const std::vector<Chord*> graces = toChord(el)->graceNotesBefore();
                    if (!graces.empty()) {
                        Slur* gSlur = Factory::createSlur(score->dummy());
                        gSlur->setTrack(graceTrack);
                        gSlur->setTrack2(graceTrack);
                        gSlur->setTick(ps.startTick);
                        // tick2 == tick signals graceToMain=true to the post-pass, preventing computeEndElement()
                        // from replacing the explicit end element with an end-of-measure chord.
                        gSlur->setTick2(ps.startTick);
                        gSlur->setStartElement(graces.front());
                        gSlur->setEndElement(el);
                        // addSpanner(false): skip computeStartElement/computeEndElement so explicit grace element is preserved.
                        score->addSpanner(gSlur, false);
                    }
                }
            }
            continue;   // handled as grace-to-main or dropped (no grace notes)
        }
        const track_idx_t endTrack = resolveChordTrack(score, endTick, ps.staffIdx, startTrack);

        Slur* slur = Factory::createSlur(score->dummy());
        slur->setTrack(startTrack);
        slur->setTrack2(endTrack);
        slur->setTick(ps.startTick);
        slur->setTick2(endTick);
        // If the chord at/after startTick has grace notes, SLURSTART was co-located with the grace in Encore; anchor to it.
        // tick2rightSegment handles grace-note tick stealing (startTick may be slightly before cumTick).
        {
            Segment* rSeg = score->tick2rightSegment(ps.startTick, false,
                                                     SegmentType::ChordRest);
            if (rSeg) {
                EngravingItem* rEl = rSeg->element(startTrack);
                if (rEl && rEl->isChord()) {
                    const std::vector<Chord*> graces = toChord(rEl)->graceNotesBefore();
                    if (!graces.empty()) {
                        slur->setStartElement(graces.front());
                    }
                }
            }
        }
        score->addElement(slur);
    }

    // Remove slurs with missing start/end note (corrupted files cause NaN in Bezier layout).
    {
        std::vector<Spanner*> toRemove;
        for (auto& [tick, spanner] : score->spannerMap().map()) {
            if (spanner->isSlur()) {
                // Grace start: computeStartElement() uses tick2segment(), which returns the regular chord,
                // not the grace sub-chord. Skip it when start element was set explicitly.
                const bool graceStart = spanner->startElement()
                                        && spanner->startElement()->isChord()
                                        && toChord(spanner->startElement())->isGrace();
                if (!graceStart) {
                    spanner->computeStartElement();
                }
                // Grace-to-main (tick == tick2): computeEndElement() fails because no segment
                // exists at the same tick through the spanner tick lookup. Grace-to-later works normally.
                const bool graceToMain = graceStart
                                         && (spanner->tick() == spanner->tick2());
                if (!graceToMain) {
                    spanner->computeEndElement();
                }
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
} // namespace mu::iex::enc
