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
#include "engraving/dom/chord.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/fingering.h"
#include "engraving/dom/note.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/systemlock.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/articulation.h"

#include <map>

using namespace mu::engraving;

namespace mu::iex::enc {
// Scan `m` for the first chord on preferTrack, falling back to fallbackTrack.
// Sets *outTrack to the track used; returns nullptr if neither is found.
static Chord* findFirstChordInMeasure(Measure* m, track_idx_t preferTrack,
                                      track_idx_t fallbackTrack,
                                      track_idx_t* outTrack)
{
    if (!m) {
        return nullptr;
    }
    Chord* preferred = nullptr;
    Chord* fallback  = nullptr;
    for (Segment* s = m->first(SegmentType::ChordRest);
         s; s = s->next(SegmentType::ChordRest)) {
        if (!preferred) {
            EngravingItem* el = s->element(preferTrack);
            if (el && el->isChord()) {
                preferred = toChord(el);
            }
        }
        if (!fallback) {
            EngravingItem* el = s->element(fallbackTrack);
            if (el && el->isChord()) {
                fallback = toChord(el);
            }
        }
        if (preferred && fallback) {
            break;
        }
    }
    if (preferred) {
        *outTrack = preferTrack;
        return preferred;
    }
    if (fallback) {
        *outTrack = fallbackTrack;
        return fallback;
    }
    return nullptr;
}

void resolveFingeringAndBowing(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;

    // Bowing tick correction: Encore sometimes stores ORN enc tick=0 even when the mark
    // visually falls on a later beat. Phase 1: adopt the tick from a same-measure ORN with a
    // matching ornXoffset (±BOW_XOFF_CLUSTER). Phase 2: match via per-measure note xoffset data.
    static constexpr int BOW_XOFF_CLUSTER = 6;
    for (PendingBowing& pb : ctx.pendingBowings) {
        if (pb.crossMeasure || pb.encTickRaw > 0) {
            continue;
        }
        // Phase 1: anchor from same-measure ORN with matching xoffset.
        bool fixed = false;
        for (const PendingBowing& anchor : ctx.pendingBowings) {
            if (&anchor == &pb || anchor.measIdx != pb.measIdx || anchor.encTickRaw == 0) {
                continue;
            }
            if (std::abs(anchor.ornXoffset - pb.ornXoffset) <= BOW_XOFF_CLUSTER) {
                pb.tick = anchor.tick;
                fixed = true;
                break;
            }
        }
        if (fixed) {
            continue;
        }
        // Phase 2: match via closest note xoffset on the same staff.
        const int staffIdx = static_cast<int>(pb.track / VOICES);
        auto it = ctx.noteXoffByMeasStaff.find({ pb.measIdx, staffIdx });
        if (it == ctx.noteXoffByMeasStaff.end()) {
            continue;
        }
        int bestTick = -1;
        int bestDiff = INT_MAX;
        for (const auto& p : it->second) {   // p = { enc_tick, note.xoffset }
            const int diff = pb.ornXoffset - p.second;
            if (diff >= 0 && diff < bestDiff) {
                bestDiff = diff;
                bestTick = p.first;
            }
        }
        if (bestTick >= 0) {
            static constexpr int wholeTicks = 960;  // buildNoteLoop uses 960 ticks/whole
            const Measure* m = (pb.measIdx >= 0 && pb.measIdx < static_cast<int>(ctx.measuresByIdx.size()))
                               ? ctx.measuresByIdx[pb.measIdx] : nullptr;
            if (m) {
                pb.tick = m->tick() + Fraction(bestTick, wholeTicks);
            }
        }
    }

    // Bowing marks: crossMeasure means Encore misplaced the ORN in the previous measure.
    for (const PendingBowing& pb : ctx.pendingBowings) {
        track_idx_t useTrack = pb.track;
        Chord* c = nullptr;

        if (pb.crossMeasure) {
            int nextIdx = pb.measIdx + 1;
            if (nextIdx >= 0 && nextIdx < static_cast<int>(ctx.measuresByIdx.size())) {
                c = findFirstChordInMeasure(ctx.measuresByIdx[nextIdx],
                                            pb.track + VOICES, pb.track, &useTrack);
            }
        } else {
            Measure* m = score->tick2measure(pb.tick);
            if (m) {
                Segment* seg = m->findSegment(SegmentType::ChordRest, pb.tick);
                if (seg) {
                    // ORN is always voice 0; scan all voices of own staff before sibling.
                    const track_idx_t staffBase = (pb.track / VOICES) * VOICES;
                    for (track_idx_t v = 0; v < VOICES && !c; ++v) {
                        EngravingItem* el = seg->element(staffBase + v);
                        if (el && el->isChord()) {
                            c = toChord(el);
                            useTrack = staffBase + v;
                        }
                    }
                    if (!c) {
                        const track_idx_t sibBase = staffBase + VOICES;
                        for (track_idx_t v = 0; v < VOICES && !c; ++v) {
                            EngravingItem* el = seg->element(sibBase + v);
                            if (el && el->isChord()) {
                                c = toChord(el);
                                useTrack = sibBase + v;
                            }
                        }
                    }
                }
            }
        }

        if (!c) {
            continue;
        }
        Articulation* art = Factory::createArticulation(c);
        art->setTrack(useTrack);
        art->setSymId(pb.symId);
        c->add(art);
    }

    // Fingering ORNs (0xB9..0xBD): multiple ORNs at the same tick attach low-to-high.
    // crossMeasure: next-measure sibling chord. preferSibling: 2nd-staff chord at same tick.
    std::map<Chord*, int> fingeringCount;
    for (const PendingOrnFingering& pf : ctx.pendingOrnFingerings) {
        track_idx_t useTrack = pf.track;
        Chord* c = nullptr;

        if (pf.crossMeasure) {
            int nextIdx = pf.measIdx + 1;
            if (nextIdx >= 0 && nextIdx < static_cast<int>(ctx.measuresByIdx.size())) {
                c = findFirstChordInMeasure(ctx.measuresByIdx[nextIdx],
                                            pf.track + VOICES, pf.track, &useTrack);
            }
        } else {
            Measure* m = score->tick2measure(pf.tick);
            if (m) {
                Segment* seg = m->findSegment(SegmentType::ChordRest, pf.tick);
                if (seg) {
                    track_idx_t sibTrack = pf.track + VOICES;
                    if (pf.preferSibling) {
                        EngravingItem* el = seg->element(sibTrack);
                        if (el && el->isChord()) {
                            c = toChord(el);
                            useTrack = sibTrack;
                        } else {
                            el = seg->element(pf.track);
                            if (el && el->isChord()) {
                                c = toChord(el);
                                useTrack = pf.track;
                            }
                        }
                    } else {
                        EngravingItem* el = seg->element(pf.track);
                        if (el && el->isChord()) {
                            c = toChord(el);
                            useTrack = pf.track;
                        } else {
                            el = seg->element(sibTrack);
                            if (el && el->isChord()) {
                                c = toChord(el);
                                useTrack = sibTrack;
                            }
                        }
                    }
                }
            }
        }

        if (!c) {
            continue;
        }
        const auto& notes = c->notes();
        if (notes.empty()) {
            continue;
        }
        int& idx = fingeringCount[c];
        Note* n = notes[std::min(idx, static_cast<int>(notes.size()) - 1)];
        ++idx;
        Fingering* f = Factory::createFingering(n);
        f->setTrack(useTrack);
        f->setXmlText(String::number(pf.fingerNum));
        n->add(f);
    }

    // SystemLocks enforce Encore's line layout as hard constraints so the engine compresses
    // spacing within the system rather than redistributing measures across lines.
    {
        const auto& lines  = ctx.enc.lines;
        const auto& enc2ms = ctx.encToMsIdx;
        const int totalMeas = static_cast<int>(ctx.measuresByIdx.size());

        for (const auto& line : lines) {
            if (line.measureCount <= 0) {
                continue;
            }
            const int firstBlock = static_cast<int>(line.start);
            const int lastBlock  = firstBlock + static_cast<int>(line.measureCount) - 1;

            if (firstBlock < 0 || lastBlock < firstBlock
                || firstBlock >= static_cast<int>(enc2ms.size())
                || lastBlock >= static_cast<int>(enc2ms.size())) {
                continue;
            }

            const int firstMsIdx = static_cast<int>(enc2ms[static_cast<size_t>(firstBlock)]);
            // Last MuseScore measure = first of the last MEAS block's range, plus however
            // many MuseScore measures that block produces (gap to next block, or to end).
            const int lastBlockMs = static_cast<int>(enc2ms[static_cast<size_t>(lastBlock)]);
            // Last MS measure = first MS index of last block's range plus the block's span.
            const int nextBlockMs = (lastBlock + 1 < static_cast<int>(enc2ms.size()))
                                    ? static_cast<int>(enc2ms[static_cast<size_t>(lastBlock + 1)])
                                    : totalMeas;
            const int lastMsIdx = nextBlockMs - 1;

            if (firstMsIdx < 0 || lastMsIdx < firstMsIdx
                || firstMsIdx >= totalMeas || lastMsIdx >= totalMeas) {
                continue;
            }

            Measure* firstM = ctx.measuresByIdx[static_cast<size_t>(firstMsIdx)];
            Measure* lastM  = ctx.measuresByIdx[static_cast<size_t>(lastMsIdx)];
            if (!firstM || !lastM) {
                continue;
            }
            ctx.score->addSystemLock(new SystemLock(firstM, lastM));
        }
    }
}
} // namespace mu::iex::enc
