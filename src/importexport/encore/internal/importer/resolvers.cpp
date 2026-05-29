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

#include "ctx.h"
#include "import.h"
#include "../parser/elements.h"
#include "mapping.h"
#include "../parser/ticks.h"
#include "tuplets.h"
#include <algorithm>
#include <memory>
#include <map>
#include <set>
#include <vector>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include "engraving/dom/arpeggio.h"
#include "engraving/dom/box.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/fermata.h"
#include "engraving/dom/fingering.h"
#include "engraving/dom/ornament.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/jump.h"
#include "engraving/dom/key.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/marker.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/part.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftext.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/text.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/volta.h"
#include "engraving/engravingerrors.h"
#include "log.h"

namespace mu::iex::encore {

void resolveAll(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;
    const EncFile& enc = ctx.enc;
    // Resolve slur intents: .enc has no SLURSTOP; endpoint from alMezuro.
    // Anchor on last ChordRest in the target measure on this track.
    for (const PendingSlur& ps : ctx.pendingSlurs) {
        if (ps.endMeasIdx < 0 || ps.endMeasIdx >= static_cast<int>(ctx.measuresByIdx.size())) {
            continue;
        }
        Measure* endMeas = ctx.measuresByIdx[ps.endMeasIdx];
        Fraction endTick;
        bool resolved = false;

        // Same-measure slur: pixel-span heuristic.
        // target_end_xoff = first_note_xoff + (slurXoffset2 - slurXoffset).
        // Find the note whose xoff is closest to that target.
        // Cross-measure (alMezuro > 0): xoffsets reset at barline; fall back to last ChordRest.
        if (ps.alMezuro == 0 && ps.startMeasIdx >= 0
            && ps.startMeasIdx < static_cast<int>(enc.measures.size())) {
            const EncMeasure& startEncMeas = enc.measures[ps.startMeasIdx];
            int firstNoteXoff = -1;
            // Find the first note on this track whose Encore tick matches the slur's start tick.
            const Fraction relStartTick = ps.startTick - ctx.measuresByIdx[ps.startMeasIdx]->tick();
            const int startEncTick = (relStartTick.numerator() * startEncMeas.beatTicks
                                      * startEncMeas.timeSigDen)
                                     / std::max(1, relStartTick.denominator());
            for (const auto& elem : startEncMeas.elements) {
                const EncMeasureElem* em = elem.get();
                if (em->type != static_cast<quint8>(EncElemType::NOTE)) {
                    continue;
                }
                if (em->staffIdx != ps.staffIdx || em->voice != ps.encVoice) {
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
                    if (em->staffIdx != ps.staffIdx || em->voice != ps.encVoice) {
                        continue;
                    }
                    const int xoff = static_cast<int>(static_cast<const EncNote*>(em)->xoffset);
                    const int dist = std::abs(xoff - targetEndXoff);
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestEncTick = static_cast<int>(em->tick);
                    }
                }
                if (bestEncTick >= 0 && bestEncTick > startEncTick) {
                    const Fraction endRel(bestEncTick,
                                          startEncMeas.beatTicks * startEncMeas.timeSigDen);
                    endTick = ctx.measuresByIdx[ps.startMeasIdx]->tick() + endRel;
                    resolved = true;
                }
            }
        }

        if (!resolved) {
            Segment* lastSeg = nullptr;
            for (Segment* s = endMeas->first(SegmentType::ChordRest); s;
                 s = s->next(SegmentType::ChordRest)) {
                if (s->element(ps.track)) {
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
        Slur* slur = Factory::createSlur(score->dummy());
        slur->setTrack(ps.track);
        slur->setTrack2(ps.track);
        slur->setTick(ps.startTick);
        slur->setTick2(endTick);
        score->addElement(slur);
    }

    // Resolve hairpin intents. Endpoint = min(next-dynamic tick, xoffset2 clamp).
    // Same-measure: next Dynamic on the track marks the end.
    // Cross-measure: xoffset2 < first-note xoffset means hairpin ends at the barline.
    for (const PendingHairpin& ph : ctx.pendingHairpins) {
        Fraction endTick = ph.maxEndTick;

        // (1) Next Dynamic on track takes priority; handles mf<f>mf chains.
        bool foundNextDynamic = false;
        for (Segment* s = score->firstSegment(SegmentType::ChordRest); s; s = s->next1(SegmentType::ChordRest)) {
            if (s->tick() <= ph.startTick) {
                continue;
            }
            if (s->tick() > ph.maxEndTick) {
                break;
            }
            bool stopHere = false;
            for (EngravingItem* ann : s->annotations()) {
                if (ann && ann->isDynamic() && ann->track() == ph.track) {
                    stopHere = true;
                    break;
                }
            }
            if (stopHere) {
                endTick = std::min(endTick, s->tick());
                foundNextDynamic = true;
                break;
            }
        }

        // (2) xoffset2 clamp: if xoffset2 < first note's xoffset in target measure, end at barline.
        // Only when no Dynamic found in step (1).
        if (!foundNextDynamic
            && ph.endMeasIdx >= 0
            && ph.endMeasIdx < static_cast<int>(enc.measures.size())) {
            const EncMeasure& endEncMeas = enc.measures[ph.endMeasIdx];
            if (endEncMeas.beatTicks && endEncMeas.timeSigDen) {
                const int xoff2 = ph.hairpinXoffset2;
                int firstNoteXoff = -1;
                for (const auto& elem : endEncMeas.elements) {
                    const EncMeasureElem* em = elem.get();
                    if (em->type != static_cast<quint8>(EncElemType::NOTE)) {
                        continue;
                    }
                    if (em->staffIdx != ph.staffIdx || em->voice != ph.encVoice) {
                        continue;
                    }
                    const int xoff = static_cast<int>(
                        static_cast<const EncNote*>(em)->xoffset);
                    if (xoff > 0 && (firstNoteXoff < 0 || xoff < firstNoteXoff)) {
                        firstNoteXoff = xoff;
                    }
                }
                if (firstNoteXoff > 0 && xoff2 < firstNoteXoff) {
                    Fraction targetMeasTick = ctx.measuresByIdx[ph.endMeasIdx]->tick();
                    endTick = std::min(endTick, targetMeasTick);
                }
            }
        }

        if (endTick <= ph.startTick) {
            continue;
        }
        Hairpin* hp = Factory::createHairpin(score->dummy()->segment());
        hp->setTrack(ph.track);
        hp->setTrack2(ph.track);
        hp->setTick(ph.startTick);
        hp->setTick2(endTick);
        hp->setHairpinType(ph.type);
        score->addElement(hp);
    }

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

    // Resolve ORN-based single-chord tremolos (tipo 0xAF / 0xEF).
    // Find the chord at the stored tick; if none, search backwards in the
    // source measure for the latest chord (handles Encore placement at
    // tick == durTicks on long notes).
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
            // Fall back: search backwards in the measure that owns the source
            // ORN tick (pt.measTick). Encore can place the tremolo ORN at
            // tick == durTicks (after the last note), so the tick2measure
            // lookup above can land in the NEXT measure (which has only a
            // whole-rest filler and no chord). Re-anchor the search in the
            // source measure using pt.measTick.
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
        if (!seg || !seg->element(trTrack)) {
            continue;
        }
        EngravingItem* el = seg->element(trTrack);
        if (!el || !el->isChord()) {
            continue;
        }
        Chord* c = toChord(el);
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

    // Resolve staccato intents: add articStaccatoAbove if not already present
    // (artic byte 0x1D produces the same glyph; skip to avoid duplicates).
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

    // Resolve trill intents: same deferred pattern as ARPEGGIO.
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
        Chord* c = toChord(el);
        Ornament* orn = Factory::createOrnament(c);
        orn->setTrack(pt.track);
        orn->setSymId(SymId::ornamentTrill);
        c->add(orn);
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
