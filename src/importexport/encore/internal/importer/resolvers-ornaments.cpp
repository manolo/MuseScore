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
        Chord* c = findChordAt(score, pa.tick, pa.track);
        if (!c) {
            continue;
        }
        if (c->arpeggio()) {
            continue;
        }
        Arpeggio* arp = Factory::createArpeggio(c);
        arp->setTrack(pa.track);
        arp->setArpeggioType(ArpeggioType::NORMAL);
        c->add(arp);
    }

    // Single-chord tremolos (0xAF/0xEF). See ENCORE_FORMAT.md §Ornament element.
    // Encore may place ORN at durTicks or in voice 0 even when the note is in another voice.
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
            Measure* srcMeas = score->tick2measure(pt.measTick);
            if (!srcMeas) {
                srcMeas = m;
            }
            seg = nullptr;
            for (Segment* s = srcMeas->first(SegmentType::ChordRest); s;
                 s = s->next(SegmentType::ChordRest)) {
                if (s->element(trTrack) && s->element(trTrack)->isChord()) {
                    seg = s;
                }
            }
        }
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
            continue;
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
        Chord* c = findChordAt(score, ps.tick, ps.track);
        if (!c) {
            continue;
        }
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
        Chord* trillChord = findChordAt(score, pt.tick, pt.track);
        if (!trillChord) {
            continue;
        }

        // TRILL_ALT → Ornament glyph; TRILL_START → Trill spanner when endpoint known, else Ornament.
        // Endpoints: TRILL_END on same track, or alMezuro target measure. See ENCORE_FORMAT.md §Ornament element.
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
            Ornament* orn = Factory::createOrnament(trillChord);
            orn->setTrack(pt.track);
            orn->setSymId(SymId::ornamentTrill);
            trillChord->add(orn);
        }
    }
    ctx.pendingTrillEnds.clear();
}
} // namespace mu::iex::encore
