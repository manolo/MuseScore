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

namespace mu::iex::encore {
// Helper: scan `m` for the first ChordRest segment that has a chord on
// `preferTrack`; also accept `fallbackTrack` if preferred is not found.
// Sets *outTrack to the track actually used; returns nullptr if none found.
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

    // Resolve bowing marks from stand-alone ORN elements.
    // crossMeasure=true: ORN misplaced by Encore; belongs to the next measure's first chord on the sibling staff.
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
                    // The ORN voice is always 0; scan all voices of the ORN's own staff
                    // before falling back to the sibling staff. This handles the common
                    // case where notes are in voice=1+ and the ORN is stored at voice=0.
                    const track_idx_t staffBase = (pb.track / VOICES) * VOICES;
                    for (track_idx_t v = 0; v < VOICES && !c; ++v) {
                        EngravingItem* el = seg->element(staffBase + v);
                        if (el && el->isChord()) {
                            c = toChord(el);
                            useTrack = staffBase + v;
                        }
                    }
                    if (!c) {
                        // Sibling-staff fallback: same search across the adjacent staff.
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

    // Resolve fingering ORNs (0xB9..0xBD). Multiple ORNs at the same tick attach to successive notes low-to-high; fingeringCount tracks the index.
    // crossMeasure: next-measure sibling-staff chord. preferSibling: 2nd-staff chord at same tick.
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
                        // Pattern B: 2nd-staff chord takes priority.
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

    // Lock each Encore system so it always contains exactly the measures specified in
    // the LINE block, regardless of spatium. SystemLocks replace LayoutBreak::LINE because
    // they are hard constraints — the layout engine compresses note spacing within the
    // locked system rather than redistributing measures. This preserves Encore's line
    // layout without needing to reduce the staff space to unreadable sizes.
    {
        const auto& lines  = ctx.enc.lines;
        const auto& enc2ms = ctx.encToMsIdx;   // MEAS-block index → first MuseScore measure index
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
} // namespace mu::iex::encore
