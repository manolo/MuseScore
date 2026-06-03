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
#include "engraving/dom/layoutbreak.h"
#include "engraving/dom/note.h"
#include "engraving/dom/masterscore.h"
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
                    EngravingItem* el = seg->element(pb.track);
                    if (el && el->isChord()) {
                        c = toChord(el);
                        useTrack = pb.track;
                    } else {
                        // Sibling-staff fallback for same-measure ORNs.
                        track_idx_t sibTrack = pb.track + VOICES;
                        el = seg->element(sibTrack);
                        if (el && el->isChord()) {
                            c = toChord(el);
                            useTrack = sibTrack;
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

    // Add LINE breaks from the Encore LINE block; one EncLine = one system.
    {
        const auto& lines = ctx.enc.lines;
        int cumMeas = 0;
        for (int li = 0; li + 1 < static_cast<int>(lines.size()); ++li) {
            cumMeas += lines[li].measureCount;
            const int lastIdx = cumMeas - 1;
            if (lastIdx < 0 || lastIdx >= static_cast<int>(ctx.measuresByIdx.size())) {
                continue;
            }
            Measure* m = ctx.measuresByIdx[lastIdx];
            if (!m) {
                continue;
            }
            LayoutBreak* lb = Factory::createLayoutBreak(m);
            lb->setLayoutBreakType(LayoutBreakType::LINE);
            lb->setTrack(0);
            m->add(lb);
        }
    }
}
} // namespace mu::iex::encore
