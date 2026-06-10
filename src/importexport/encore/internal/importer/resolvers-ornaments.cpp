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
#include "engraving/dom/fermata.h"
#include "engraving/dom/breath.h"
#include "engraving/dom/measurerepeat.h"
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

    // Standalone fermata ORNs (tipo 0xCC/0xCD); attach to segment, not chord.
    for (const PendingFermata& pf : ctx.pendingFermatas) {
        Chord* c = findChordAt(score, pf.tick, pf.track);
        if (!c) {
            continue;
        }
        Segment* seg = c->segment();
        bool alreadyHas = false;
        for (EngravingItem* e : seg->annotations()) {
            if (e->isFermata() && toFermata(e)->symId() == pf.symId) {
                alreadyHas = true;
                break;
            }
        }
        if (alreadyHas) {
            continue;
        }
        const bool isBelow = (pf.symId == SymId::fermataBelow
                              || pf.symId == SymId::fermataShortBelow
                              || pf.symId == SymId::fermataLongBelow);
        Fermata* fermata = Factory::createFermata(seg);
        fermata->setTrack(pf.track);
        fermata->setSymId(pf.symId);
        fermata->setPlacement(isBelow ? PlacementV::BELOW : PlacementV::ABOVE);
        fermata->setPropertyFlags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
        seg->add(fermata);
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

        // Three cases:
        //  (A) TRILL_ALT within a TRILL_START span: secondary marker → Ornament glyph always.
        //  (B) TRILL_ALT standalone (no prior START on same track): spanner on note duration.
        //  (C) TRILL_START: spanner when explicit endpoint found; glyph if no endpoint.
        const bool altWithinSpan = pt.isAlt && [&]() {
            for (const PendingTrill& other : ctx.pendingTrills) {
                if (!other.isAlt && other.track == pt.track && other.tick < pt.tick) {
                    return true;
                }
            }
            return false;
        }();
        const bool standaloneAlt = pt.isAlt && !altWithinSpan;

        Fraction endTick;
        bool hasSpan = !altWithinSpan;  // case A: never try to build a span

        if (hasSpan) {
            hasSpan = false;
            // Check for a TRILL_END marker on this track; erase consumed entry.
            auto it = ctx.pendingTrillEnds.find(pt.track);
            if (it != ctx.pendingTrillEnds.end()) {
                auto& endVec = it->second;
                for (auto eit = endVec.begin(); eit != endVec.end(); ++eit) {
                    if (*eit > pt.tick) {
                        endTick = *eit;
                        hasSpan = true;
                        endVec.erase(eit);
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

        // Case B: standalone TRILL_ALT with no explicit endpoint → span the note's duration.
        if (standaloneAlt && (!hasSpan || endTick <= pt.tick)) {
            const Fraction noteDuration = trillChord->actualTicks();
            if (!noteDuration.isZero()) {
                endTick = trillChord->tick() + noteDuration;
                hasSpan = true;
            }
        }

        if (hasSpan && endTick > pt.tick) {
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

    // Standalone TRILL_END entries (not consumed by any TRILL_START):
    // Create a Trill spanner covering the note's duration.
    for (auto& [trTrack, endTicks] : ctx.pendingTrillEnds) {
        for (const Fraction& eTick : endTicks) {
            Chord* c = findChordAt(score, eTick, trTrack);
            if (!c) {
                continue;
            }
            const Fraction noteDuration = c->actualTicks();
            if (noteDuration.isZero()) {
                continue;
            }
            Trill* trill = Factory::createTrill(score->dummy());
            trill->setTrack(trTrack);
            trill->setTrack2(trTrack);
            trill->setTick(c->tick());
            trill->setTick2(c->tick() + noteDuration);
            trill->setTrillType(TrillType::TRILL_LINE);
            score->addElement(trill);
        }
    }
    ctx.pendingTrillEnds.clear();

    // Breath marks and caesuras (tipo 0xA7/0xA8).
    // In standard notation, breath/caesura is placed AFTER the preceding note.
    // The Encore ORN is at the tick of the note it precedes; we attach to the Breath
    // segment at (prev_note_tick + prev_note_ticks), i.e., after the note at that tick.
    for (const PendingBreath& pb : ctx.pendingBreaths) {
        Chord* c = findChordAt(score, pb.tick, pb.track);
        if (!c) {
            continue;
        }
        Measure* m = c->measure();
        if (!m) {
            continue;
        }
        const Fraction breathTick = c->tick() + c->ticks();
        Segment* seg = m->getSegment(SegmentType::Breath, breathTick);
        Breath* breath = Factory::createBreath(seg);
        breath->setTrack(pb.track);
        breath->setSymId(pb.symId);
        breath->setPlacement(PlacementV::ABOVE);
        breath->setPropertyFlags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
        seg->add(breath);
    }

    // Measure repeats (tipo 0xA3): replace measure content with "%" symbol.
    for (const PendingMeasureRepeat& pmr : ctx.pendingMeasureRepeats) {
        Measure* m = score->tick2measure(pmr.measTick);
        if (!m) {
            continue;
        }
        const track_idx_t track = static_cast<track_idx_t>(pmr.staffIdx) * VOICES;
        Segment* firstSeg = m->first(SegmentType::ChordRest);
        if (!firstSeg) {
            continue;
        }
        Staff* st = score->staff(static_cast<staff_idx_t>(pmr.staffIdx));
        if (!st) {
            continue;
        }
        score->makeGap(firstSeg, track, m->stretchedLen(st), nullptr);
        score->addMeasureRepeat(m->tick(), track, 1);
        m->setMeasureRepeatCount(1, static_cast<staff_idx_t>(pmr.staffIdx));
    }
}
} // namespace mu::iex::encore
