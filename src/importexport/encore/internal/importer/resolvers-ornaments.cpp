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
#include "engraving/dom/arpeggio.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/dom/ornament.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/note.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/marker.h"
#include "engraving/dom/articulation.h"
#include "engraving/dom/trill.h"

using namespace mu::engraving;

namespace mu::iex::encore {
void resolveOrnaments(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;

    // Resolve arpeggio intents: ORN written before chord in MEAS, so attach now.
    for (const PendingArpeggio& pa : ctx.pendingArpeggios) {
        Measure* m = score->tick2measure(pa.tick);
        if (!m) {
            continue;
        }
        Segment* seg = m->findSegment(SegmentType::ChordRest, pa.tick);
        if (!seg) {
            continue;
        }
        EngravingItem* el = seg->element(pa.track);
        if (!el || !el->isChord()) {
            continue;
        }
        Chord* c = toChord(el);
        if (c->arpeggio()) {
            continue;   // chord already carries an arpeggio
        }
        Arpeggio* arp = Factory::createArpeggio(c);
        arp->setTrack(pa.track);
        arp->setArpeggioType(ArpeggioType::NORMAL);
        c->add(arp);
    }

    // Resolve ORN-based single-chord tremolos (0xAF / 0xEF). Encore may place the ORN at durTicks; fall back to backwards search if no chord at the exact tick.
    // Tremolo ORNs are in voice 0 regardless of the actual note voice; widen to all voices when needed.
    for (const PendingOrnTremolo& pt : ctx.pendingOrnTremolos) {
        const track_idx_t trTrack = static_cast<track_idx_t>(pt.staffIdx * VOICES + pt.msVoice);
        // Try exact tick match first.
        Measure* m = score->tick2measure(pt.tick);
        if (!m) {
            m = score->tick2measure(pt.measTick);
        }
        if (!m) {
            continue;
        }
        Segment* seg = m->findSegment(SegmentType::ChordRest, pt.tick);
        if (!seg || !seg->element(trTrack) || !seg->element(trTrack)->isChord()) {
            // Encore may place tremolo ORN at durTicks; tick2measure lands in the next measure. Re-anchor to the source measure.
            Measure* srcMeas = score->tick2measure(pt.measTick);
            if (!srcMeas) {
                srcMeas = m;
            }
            seg = nullptr;
            for (Segment* s = srcMeas->first(SegmentType::ChordRest); s;
                 s = s->next(SegmentType::ChordRest)) {
                if (s->element(trTrack) && s->element(trTrack)->isChord()) {
                    seg = s;   // keep updating: we want the LAST chord
                }
            }
        }
        // ORN voice 0 is the norm even when notes live in a different voice; widen to all staff voices if no match.
        track_idx_t resolvedTrack = trTrack;
        if (!seg || !seg->element(resolvedTrack) || !seg->element(resolvedTrack)->isChord()) {
            Measure* srcMeas = score->tick2measure(pt.measTick);
            if (!srcMeas) {
                srcMeas = m;
            }
            if (srcMeas) {
                for (int v = 0; v < static_cast<int>(VOICES) && !seg; ++v) {
                    const track_idx_t altTrack = static_cast<track_idx_t>(pt.staffIdx * VOICES + v);
                    for (Segment* s = srcMeas->first(SegmentType::ChordRest); s;
                         s = s->next(SegmentType::ChordRest)) {
                        if (s->element(altTrack) && s->element(altTrack)->isChord()) {
                            seg = s;
                            resolvedTrack = altTrack;
                        }
                    }
                }
            }
        }
        if (!seg || !seg->element(resolvedTrack)) {
            continue;
        }
        EngravingItem* el = seg->element(resolvedTrack);
        if (!el || !el->isChord()) {
            continue;
        }
        Chord* c = toChord(el);
        // If the resolved chord is the tied-to continuation, walk back to the tie start.
        // Encore places the tremolo ORN after the tied-from note in the stream, so the
        // ORN tick lands on the continuation chord; the tremolo belongs on the first note.
        if (!c->notes().empty() && c->notes().front()->tieBack()) {
            Chord* prev = c->notes().front()->tieBack()->startNote()->chord();
            if (prev) {
                c = prev;
            }
        }
        if (c->tremoloSingleChord()) {
            continue;   // already has a tremolo from the articulation byte
        }
        TremoloSingleChord* trem = Factory::createTremoloSingleChord(c);
        trem->setTremoloType(pt.tremType);
        c->add(trem);
    }

    // Resolve SEGNO / CODA section markers on their measures.
    for (const PendingMarker& pm : ctx.pendingMarkers) {
        Measure* m = score->tick2measure(pm.tick);
        if (!m) {
            continue;
        }
        Marker* mk = Factory::createMarker(m);
        mk->setMarkerType(pm.type);
        mk->setTrack(0);
        m->add(mk);
    }

    // Add staccato unless already present (artic byte 0x1D produces the same glyph).
    for (const PendingStaccato& ps : ctx.pendingStaccatos) {
        Measure* m = score->tick2measure(ps.tick);
        if (!m) {
            continue;
        }
        Segment* seg = m->findSegment(SegmentType::ChordRest, ps.tick);
        if (!seg) {
            continue;
        }
        EngravingItem* el = seg->element(ps.track);
        if (!el || !el->isChord()) {
            continue;
        }
        Chord* c = toChord(el);
        bool alreadyHas = false;
        for (Articulation* a : c->articulations()) {
            if (a->symId() == SymId::articStaccatoAbove
                || a->symId() == SymId::articStaccatoBelow) {
                alreadyHas = true;
                break;
            }
        }
        if (alreadyHas) {
            continue;
        }
        Articulation* art = Factory::createArticulation(c);
        art->setTrack(ps.track);
        art->setSymId(SymId::articStaccatoAbove);
        c->add(art);
    }

    // Resolve trill intents.
    // When Encore marks a trill span (TRILL_END in the same measure, or alMezuro>0),
    // create a Trill spanner (tr + wavy line). Otherwise create only the Ornament glyph.
    for (const PendingTrill& pt : ctx.pendingTrills) {
        Measure* m = score->tick2measure(pt.tick);
        if (!m) {
            continue;
        }
        Segment* seg = m->findSegment(SegmentType::ChordRest, pt.tick);
        if (!seg) {
            continue;
        }
        EngravingItem* el = seg->element(pt.track);
        if (!el || !el->isChord()) {
            continue;
        }

        // TRILL_ALT (0x37): secondary trill mark within a span → always Ornament glyph.
        // TRILL_START (0x36): create Trill spanner when the span endpoint is known:
        //   1. TRILL_END in the same measure (first end tick > pt.tick on this track).
        //   2. alMezuro > 0: span to the end of the target measure.
        //   3. Neither: fall back to Ornament glyph.
        Fraction endTick;
        bool hasSpan = !pt.isAlt;

        if (hasSpan) {
            hasSpan = false;
            // Check for a TRILL_END marker on this track
            auto it = ctx.pendingTrillEnds.find(pt.track);
            if (it != ctx.pendingTrillEnds.end()) {
                for (const Fraction& endT : it->second) {
                    if (endT > pt.tick) {
                        endTick = endT;
                        hasSpan = true;
                        break;
                    }
                }
            }

            // Cross-measure span via alMezuro
            if (!hasSpan && pt.alMezuro > 0) {
                const size_t endMeasIdx = pt.measIdx + static_cast<size_t>(pt.alMezuro);
                if (endMeasIdx < ctx.measuresByIdx.size()) {
                    Measure* endMeas = ctx.measuresByIdx[endMeasIdx];
                    if (endMeas) {
                        endTick = endMeas->endTick();
                        hasSpan = true;
                    }
                }
            }
        }

        if (hasSpan && endTick > pt.tick) {
            // Create Trill spanner (renders tr + wavy line from startTick to endTick).
            Trill* trill = Factory::createTrill(score->dummy());
            trill->setTrack(pt.track);
            trill->setTrack2(pt.track);
            trill->setTick(pt.tick);
            trill->setTick2(endTick);
            trill->setTrillType(TrillType::TRILL_LINE);
            score->addElement(trill);
        } else {
            // No span info — keep the original Ornament glyph behaviour.
            Chord* c = toChord(el);
            Ornament* orn = Factory::createOrnament(c);
            orn->setTrack(pt.track);
            orn->setSymId(SymId::ornamentTrill);
            c->add(orn);
        }
    }
    ctx.pendingTrillEnds.clear();
}
} // namespace mu::iex::encore
