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

// Post-pass: place pending ornaments, fermatas, tremolos, trills, arpeggios and breaths.

#include <cstdlib>
#include <tuple>
#include <limits>

#include "resolvers.h"
#include "../parser/elem.h"
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
#include "engraving/dom/vibrato.h"
#include "engraving/dom/guitarbend.h"
#include "engraving/editing/editmeasurerepeat.h"
#include "engraving/editing/transaction/transaction.h"

using namespace mu::engraving;

namespace mu::iex::enc {
static void resolveArpeggios(MasterScore* score,
                             const std::vector<PendingArpeggio>& pendingArpeggios)
{
    // ORN precedes chord in MEAS order, so deferred to resolve phase.
    for (const PendingArpeggio& pa : pendingArpeggios) {
        Chord* c = findChordAt(score, pa.tick, pa.track);
        if (!c || c->arpeggio()) {
            continue;
        }
        Arpeggio* arp = Factory::createArpeggio(c);
        arp->setTrack(pa.track);
        arp->setArpeggioType(ArpeggioType::NORMAL);
        c->add(arp);
    }
}

// The tremolo's stored tick and voice are unreliable, so try the exact segment, then the last chord
// on that track in the measure, then any voice of the staff.
static Chord* findChordForTremolo(MasterScore* score, const PendingOrnTremolo& pt)
{
    // staffIdx/msVoice come from the file; reject an out-of-range staff before deriving tracks.
    if (!validTrack(score, static_cast<track_idx_t>(pt.staffIdx) * VOICES)) {
        return nullptr;
    }
    const track_idx_t trTrack = static_cast<track_idx_t>(pt.staffIdx * VOICES + pt.msVoice);
    Measure* m = score->tick2measure(pt.tick);
    if (!m) {
        m = score->tick2measure(pt.measTick);
    }
    if (!m) {
        return nullptr;
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
        return nullptr;
    }
    EngravingItem* el = seg->element(resolvedTrack);
    if (!el || !el->isChord()) {
        return nullptr;
    }
    return toChord(el);
}

static void resolveSingleChordTremolos(MasterScore* score,
                                       const std::vector<PendingOrnTremolo>& pendingOrnTremolos)
{
    for (const PendingOrnTremolo& pt : pendingOrnTremolos) {
        Chord* c = findChordForTremolo(score, pt);
        if (!c) {
            continue;
        }
        // Encore places the tremolo ORN after the tied-from note; walk back to tie start.
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
}

static void resolveMarkers(MasterScore* score,
                           const std::vector<PendingMarker>& pendingMarkers)
{
    for (const PendingMarker& pm : pendingMarkers) {
        Measure* m = score->tick2measure(pm.tick);
        if (!m) {
            continue;
        }
        Marker* mk = Factory::createMarker(m);
        mk->setMarkerType(pm.type);
        mk->setTrack(0);
        m->add(mk);
    }
}

// True when seg already carries a Fermata with this symId (a per-note artic byte may have
// produced the same glyph; dedup against it).
static bool segmentHasFermata(const Segment* seg, SymId symId)
{
    for (EngravingItem* e : seg->annotations()) {
        if (e->isFermata() && toFermata(e)->symId() == symId) {
            return true;
        }
    }
    return false;
}

// True when chord c already carries an Articulation with this symId (dedup against a
// per-note artic byte that produced the same glyph).
static bool chordHasArticulation(const Chord* c, SymId symId)
{
    for (Articulation* a : c->articulations()) {
        if (a->symId() == symId) {
            return true;
        }
    }
    return false;
}

static void resolveFermatas(MasterScore* score,
                            const std::vector<PendingFermata>& pendingFermatas)
{
    for (const PendingFermata& pf : pendingFermatas) {
        Chord* c = findChordAt(score, pf.tick, pf.track);
        if (!c) {
            continue;
        }
        Segment* seg = c->segment();
        if (segmentHasFermata(seg, pf.symId)) {
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
}

static void resolveStaccatos(MasterScore* score,
                             const std::vector<PendingStaccato>& pendingStaccatos)
{
    // Dedup: artic byte 0x1D produces the same glyph.
    for (const PendingStaccato& ps : pendingStaccatos) {
        Chord* c = findChordAt(score, ps.tick, ps.track);
        if (!c) {
            continue;
        }
        if (chordHasArticulation(c, SymId::articStaccatoAbove)
            || chordHasArticulation(c, SymId::articStaccatoBelow)) {
            continue;
        }
        Articulation* art = Factory::createArticulation(c);
        art->setTrack(ps.track);
        art->setSymId(SymId::articStaccatoAbove);
        c->add(art);
    }
}

// True when a non-alt trill on the same track starts before pt, so pt's alt glyph sits within
// that earlier trill's span (Ornament glyph only, no new spanner).
static bool hasEarlierTrillStart(const std::vector<PendingTrill>& pendingTrills, const PendingTrill& pt)
{
    for (const PendingTrill& other : pendingTrills) {
        if (!other.isAlt && other.track == pt.track && other.tick < pt.tick) {
            return true;
        }
    }
    return false;
}

static void resolveTrillsWithSpans(MasterScore* score,
                                   const std::vector<PendingTrill>& pendingTrills,
                                   std::map<track_idx_t, std::vector<Fraction> >& pendingTrillEnds,
                                   const std::vector<Measure*>& measuresByIdx)
{
    // (A) TRILL_ALT within a prior TRILL_START span: Ornament glyph only.
    // (B) TRILL_ALT standalone: spanner on note duration.
    // (C) TRILL_START: spanner when endpoint found; glyph otherwise.
    for (const PendingTrill& pt : pendingTrills) {
        Fraction trillTick = pt.tick;
        Chord* trillChord = findChordAt(score, trillTick, pt.track);
        if (!trillChord) {
            // TRILL_SIMPLE may land on a rest tick; snap forward to next chord in measure.
            if (pt.isAlt) {
                Measure* m = score->tick2measure(pt.tick);
                if (m) {
                    for (Segment* s = m->first(SegmentType::ChordRest); s;
                         s = s->next(SegmentType::ChordRest)) {
                        if (s->tick() < pt.tick) {
                            continue;
                        }
                        EngravingItem* el = s->element(pt.track);
                        if (el && el->isChord()) {
                            trillChord = toChord(el);
                            trillTick = s->tick();
                            break;
                        }
                    }
                }
            }
            if (!trillChord) {
                continue;
            }
        }

        const bool altWithinSpan = pt.isAlt && hasEarlierTrillStart(pendingTrills, pt);
        const bool standaloneAlt = pt.isAlt && !altWithinSpan;

        Fraction endTick;
        bool hasSpan = !altWithinSpan;

        if (hasSpan) {
            hasSpan = false;
            // A trill end pairs within its own measure only; longer spans use the explicit forward count. Without
            // the bound, a standalone terminal trill is swallowed into a huge wrong span.
            Measure* startMeas = score->tick2measure(trillTick);
            const Fraction startMeasEnd = startMeas
                                          ? startMeas->tick() + startMeas->ticks()
                                          : Fraction(std::numeric_limits<int>::max(), 1);
            auto it = pendingTrillEnds.find(pt.track);
            if (it != pendingTrillEnds.end()) {
                auto& endVec = it->second;
                for (auto eit = endVec.begin(); eit != endVec.end(); ++eit) {
                    if (*eit > trillTick && *eit < startMeasEnd) {
                        endTick = *eit;
                        hasSpan = true;
                        endVec.erase(eit);
                        break;
                    }
                }
            }
            // Cross-measure span via alMezuro field.
            if (!hasSpan && pt.alMezuro > 0) {
                const size_t endMeasIdx = pt.measIdx + static_cast<size_t>(pt.alMezuro);
                if (endMeasIdx < measuresByIdx.size()) {
                    Measure* endMeas = measuresByIdx[endMeasIdx];
                    if (endMeas) {
                        endTick = endMeas->endTick();
                        hasSpan = true;
                    }
                }
            }
        }

        if (pt.isSimple) {
            hasSpan = false;
        } else if (standaloneAlt && (!hasSpan || endTick <= trillTick)) {
            const Fraction noteDuration = trillChord->actualTicks();
            if (!noteDuration.isZero()) {
                endTick = trillChord->tick() + noteDuration;
                hasSpan = true;
            }
        }

        if (hasSpan && endTick > trillTick) {
            Trill* trill = Factory::createTrill(score->dummy());
            trill->setTrack(pt.track);
            trill->setTrack2(pt.track);
            trill->setTick(trillTick);
            trill->setTick2(endTick);
            trill->setTrillType(TrillType::TRILL_LINE);
            score->addElement(trill);
        } else {
            const SymId sid = pt.isSimple ? pt.simpleSymId : SymId::ornamentTrill;
            bool alreadyHas = false;
            for (Articulation* a : trillChord->articulations()) {
                if (a && a->isOrnament() && toOrnament(a)->symId() == sid) {
                    alreadyHas = true;
                    break;
                }
            }
            if (!alreadyHas) {
                Ornament* orn = Factory::createOrnament(trillChord);
                orn->setTrack(pt.track);
                orn->setSymId(sid);
                trillChord->add(orn);
            }
        }
    }
}

static void resolveUnconsumedTrillEnds(MasterScore* score,
                                       std::map<track_idx_t, std::vector<Fraction> >& pendingTrillEnds)
{
    // TRILL_END markers not consumed by TRILL_START: create a spanner on the note's duration.
    for (auto& [trTrack, endTicks] : pendingTrillEnds) {
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
    pendingTrillEnds.clear();
}

static void resolveBreaths(MasterScore* score,
                           const std::vector<PendingBreath>& pendingBreaths)
{
    // pb.tick is the following note; attach after the preceding chord.
    // If pb.tick is at a measure boundary, that chord is in the prior measure.
    for (const PendingBreath& pb : pendingBreaths) {
        Measure* m = score->tick2measure(pb.tick);
        if (m && m->tick() == pb.tick) {
            MeasureBase* prevBase = m->prev();
            while (prevBase && !prevBase->isMeasure()) {
                prevBase = prevBase->prev();
            }
            if (prevBase) {
                m = toMeasure(prevBase);
            }
        }
        if (!m) {
            continue;
        }
        Chord* prevChord = nullptr;
        for (Segment* s = m->first(SegmentType::ChordRest); s;
             s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(pb.track);
            if (el && el->isChord()) {
                Chord* c = toChord(el);
                if (c->tick() + c->actualTicks() <= pb.tick) {
                    prevChord = c;
                }
            }
        }
        const Fraction breathTick = prevChord
                                    ? prevChord->tick() + prevChord->actualTicks()
                                    : pb.tick;
        Measure* breathMeasure = prevChord ? prevChord->measure() : m;
        if (!breathMeasure) {
            continue;
        }
        Segment* seg = breathMeasure->getSegment(SegmentType::Breath, breathTick);
        Breath* breath = Factory::createBreath(seg);
        breath->setTrack(pb.track);
        breath->setSymId(pb.symId);
        breath->setPlacement(PlacementV::ABOVE);
        breath->setPropertyFlags(Pid::PLACEMENT, PropertyFlags::UNSTYLED);
        seg->add(breath);
    }
}

static void resolveMeasureRepeats(MasterScore* score,
                                  const std::vector<PendingMeasureRepeat>& pendingMeasureRepeats)
{
    // Replace measure content with "%" symbol.
    for (const PendingMeasureRepeat& pmr : pendingMeasureRepeats) {
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
        EditMeasureRepeat::addMeasureRepeat(score->transactionManager()->currentOrDummyTransaction(), score, m->tick(), track, 1);
        m->setMeasureRepeatCount(1, static_cast<staff_idx_t>(pmr.staffIdx));
    }
}

// The wavy line states the column it starts at and the one it ends at, and the notes standing in
// those columns are its ends. It goes on the notation staff, whose clone the tab staff is.
static void resolveVibratos(BuildCtx& ctx)
{
    const EncRoot& enc = ctx.enc;
    for (const PendingVibrato& pv : ctx.pendingVibratos) {
        if (pv.measIdx < 0 || static_cast<size_t>(pv.measIdx) >= enc.measures.size()) {
            continue;
        }
        const size_t msIdx = static_cast<size_t>(pv.measIdx) < ctx.encToMsIdx.size()
                             ? ctx.encToMsIdx[static_cast<size_t>(pv.measIdx)] : static_cast<size_t>(pv.measIdx);
        if (msIdx >= ctx.measuresByIdx.size() || !ctx.measuresByIdx[msIdx]) {
            continue;
        }
        const EncMeasure& em = enc.measures[static_cast<size_t>(pv.measIdx)];
        const int wholeTicks = encWholeNoteTicks(em);
        // The line ends where its column falls, and the note nearest that column is the one it means.
        const auto tickAtColumn = [&](int column) {
            int best = -1, bestGap = -1;
            forEachStaffNoteXoff(em, pv.staffIdx, /*includeRests*/ false, /*lineSlotByRawByte*/ nullptr,
                                 [&](const EncMeasureElem* elem, int xoff) {
                if (xoff <= 0) {
                    return true;
                }
                const int gap = std::abs(xoff - column);
                if (best < 0 || gap < bestGap) {
                    bestGap = gap;
                    best = static_cast<int>(elem->tick);
                }
                return true;
            });
            return best;
        };
        const int startEnc = tickAtColumn(pv.startColumn);
        const int endEnc = tickAtColumn(pv.endColumn);
        if (startEnc < 0 || endEnc < startEnc) {
            continue;
        }
        const Fraction measTick = ctx.measuresByIdx[msIdx]->tick();
        const track_idx_t track = static_cast<track_idx_t>(pv.staffIdx) * VOICES;
        if (!validTrack(ctx.score, track)) {
            continue;
        }
        // Where the note landed, not where its Encore tick says: a live-recorded tick is not the
        // position the emitters gave it. The notes are on record from the tab pass.
        const auto placeOf = [&](int encTick) {
            Fraction tick = measTick + Fraction(encTick, wholeTicks).reduced();
            const Chord* chord = nullptr;
            auto it = ctx.notesByMeasStaff.find({ pv.measIdx, pv.staffIdx });
            if (it != ctx.notesByMeasStaff.end()) {
                for (const auto& [noteEncTick, note] : it->second) {
                    if (noteEncTick == encTick && note->chord()) {
                        chord = note->chord();
                        tick = chord->tick();
                        break;
                    }
                }
            }
            if (!chord) {
                chord = findChordAt(ctx.score, tick, track);
            }
            return std::make_pair(tick, chord);
        };
        const auto [startTick, startChord] = placeOf(startEnc);
        auto [endTick, endChord] = placeOf(endEnc);
        if (endChord) {
            endTick += endChord->actualTicks();   // the line covers the note it ends on
        }
        if (endTick <= startTick) {
            continue;
        }
        Vibrato* vib = Factory::createVibrato(ctx.score->dummy());
        vib->setTrack(track);
        vib->setTrack2(track);
        vib->setTick(startTick);
        vib->setTick2(endTick);
        vib->setVibratoType(VibratoType::GUITAR_VIBRATO);
        ctx.score->addElement(vib);
    }
}

// The note a bend reaches: the next one struck on the same track, the nearest in pitch when that
// beat carries a chord.
static Note* nextNoteOnTrack(const Note* note)
{
    const Chord* chord = note->chord();
    const track_idx_t track = chord->track();
    for (Segment* seg = chord->segment()->next(SegmentType::ChordRest); seg;
         seg = seg->next(SegmentType::ChordRest)) {
        EngravingItem* el = seg->element(track);
        if (!el) {
            continue;
        }
        if (!el->isChord()) {
            return nullptr;   // a rest ends the run: nothing to bend into
        }
        Note* best = nullptr;
        for (Note* n : toChord(el)->notes()) {
            if (!best || std::abs(n->pitch() - note->pitch()) < std::abs(best->pitch() - note->pitch())) {
                best = n;
            }
        }
        return best;
    }
    return nullptr;
}

// A bend is the arrow that joins a note to the one above it, which is what Encore leaves written
// whenever the string is struck again while bent. Struck again at the pitch it started from, the
// second note is where the string comes back to rest and the mark belongs to it, drawn as the curve
// into a note that a scoop is. Pulled down instead of up, which the recorded pitch wheel is the only
// thing to say, the bend stays on its own note the same way.
//
// How far it goes is not in the file: the mark carries no size, the wheel is a curve drawn by hand
// that reaches wherever the drawing reached, and the word beside it is whatever language the author
// wrote in. Joined notes measure themselves and the rest take the size MuseScore gives them, and no
// note, duration or fret the file states is touched either way.
static void resolveGuitarBends(BuildCtx& ctx)
{
    static constexpr int kFarthestBend = 5;          // a fourth, as far as a string is pulled
    const EncRoot& enc = ctx.enc;
    // Both staves of a guitar pair draw the same bend, a few pixels apart, and the one on the staff
    // that holds the notes is the one worth keeping: a tablature staff files all its marks at the
    // head of the measure, where the stream no longer says which note they belong to. Two marks on
    // one staff are never that pair, however close they are drawn: they are two bends.
    static constexpr int kSameMarkColumns = 24;
    std::vector<PendingBend> sorted = ctx.pendingBends;
    std::sort(sorted.begin(), sorted.end(), [](const PendingBend& a, const PendingBend& b) {
        return std::tie(a.staffIdx, a.measIdx, a.column) < std::tie(b.staffIdx, b.measIdx, b.column);
    });
    std::vector<PendingBend> bends;
    for (size_t i = 0; i < sorted.size();) {
        size_t j = i + 1;
        while (j < sorted.size() && sorted[j].staffIdx == sorted[i].staffIdx
               && sorted[j].measIdx == sorted[i].measIdx
               && sorted[j].markStaffIdx != sorted[j - 1].markStaffIdx
               && std::abs(sorted[j].column - sorted[j - 1].column) <= kSameMarkColumns) {
            ++j;
        }
        size_t pick = i;
        for (size_t k = i; k < j; ++k) {
            if (sorted[k].markStaffIdx == sorted[k].staffIdx) {
                pick = k;
                break;
            }
        }
        bends.push_back(sorted[pick]);
        i = j;
    }

    for (const PendingBend& pb : bends) {
        if (pb.measIdx < 0 || static_cast<size_t>(pb.measIdx) >= enc.measures.size()) {
            continue;
        }
        const EncMeasure& em = enc.measures[static_cast<size_t>(pb.measIdx)];
        // Encore files a mark next to the group of elements its note belongs to, before the group in
        // format 3.05 and inside it from 4.20 on, so the neighbour whose column is nearest names the
        // note however the generation ordered them. Its own column cannot: a mark is often drawn a
        // note's width away from what it decorates. See ENCORE_FORMAT.md 8.2 note 6.
        int bendEncTick = -1, destEncTick = -1;
        {
            const auto columnOf = [](const EncMeasureElem* elem) {
                if (const auto* note = dynamic_cast<const EncNote*>(elem)) {
                    return static_cast<int>(note->xoffset);
                }
                if (const auto* rest = dynamic_cast<const EncRest*>(elem); rest && rest->isTabFingering) {
                    return static_cast<int>(rest->xoffset);
                }
                return -1;
            };
            int before = -1;
            bool ownStaffHasNoteBefore = false;
            for (size_t k = 0; k < em.elements.size(); ++k) {
                const EncMeasureElem* elem = em.elements[k].get();
                const auto* orn = dynamic_cast<const EncOrnament*>(elem);
                if (!orn || orn->tipo != pb.kind || static_cast<int>(orn->xoffset) != pb.column
                    || static_cast<int>(elem->staffIdx) != pb.markStaffIdx) {
                    const int col = columnOf(elem);
                    if (col >= 0) {
                        before = col;
                        if (static_cast<int>(elem->staffIdx) == pb.markStaffIdx) {
                            ownStaffHasNoteBefore = true;
                        }
                    } else if (orn && static_cast<int>(elem->staffIdx) == pb.markStaffIdx) {
                        ownStaffHasNoteBefore = false;   // a mark, not a note: the run is still at the head
                    }
                    continue;
                }
                if (!ownStaffHasNoteBefore) {
                    break;   // filed at the head of the measure: only its own column names a note
                }
                int after = -1;
                for (size_t j = k + 1; j < em.elements.size() && after < 0; ++j) {
                    after = columnOf(em.elements[j].get());
                }
                int group = -1;
                if (before >= 0 && after >= 0) {
                    group = std::abs(before - pb.column) <= std::abs(after - pb.column) ? before : after;
                } else {
                    group = before >= 0 ? before : after;
                }
                if (group < 0) {
                    break;
                }
                int gap = -1;
                forEachStaffNoteXoff(em, pb.staffIdx, /*includeRests*/ false, /*lineSlotByRawByte*/ nullptr,
                                     [&](const EncMeasureElem* n, int xoff) {
                    if (xoff <= 0) {
                        return true;
                    }
                    const int d = std::abs(xoff - group);
                    if (bendEncTick < 0 || d < gap) {
                        gap = d;
                        bendEncTick = static_cast<int>(n->tick);
                    }
                    return true;
                });
                int destGap = -1;
                forEachStaffNoteXoff(em, pb.staffIdx, /*includeRests*/ false, /*lineSlotByRawByte*/ nullptr,
                                     [&](const EncMeasureElem* n, int xoff) {
                    const int tick = static_cast<int>(n->tick);
                    if (xoff <= 0 || tick <= bendEncTick) {
                        return true;
                    }
                    if (destEncTick < 0 || tick - bendEncTick < destGap) {
                        destGap = tick - bendEncTick;
                        destEncTick = tick;
                    }
                    return true;
                });
                break;
            }
        }
        if (bendEncTick < 0) {
            // A mark filed at the head of the measure has no note before it; its column names one.
            int bestGap = -1;
            forEachStaffNoteXoff(em, pb.staffIdx, /*includeRests*/ false, /*lineSlotByRawByte*/ nullptr,
                                 [&](const EncMeasureElem* elem, int xoff) {
                if (xoff <= 0) {
                    return true;
                }
                const int gap = std::abs(xoff - pb.column);
                if (bendEncTick < 0 || gap < bestGap) {
                    bestGap = gap;
                    bendEncTick = static_cast<int>(elem->tick);
                }
                return true;
            });
            if (bendEncTick < 0) {
                continue;
            }
            int destGap = -1;
            forEachStaffNoteXoff(em, pb.staffIdx, /*includeRests*/ false, /*lineSlotByRawByte*/ nullptr,
                                 [&](const EncMeasureElem* elem, int xoff) {
                const int tick = static_cast<int>(elem->tick);
                if (xoff <= 0 || tick <= bendEncTick) {
                    return true;
                }
                const int gap = tick - bendEncTick;
                if (destEncTick < 0 || gap < destGap) {
                    destGap = gap;
                    destEncTick = tick;
                }
                return true;
            });
        }
        Note* note = nullptr;
        int noteGap = -1;
        auto it = ctx.notesByMeasStaff.find({ pb.measIdx, pb.staffIdx });
        if (it != ctx.notesByMeasStaff.end()) {
            for (const auto& [noteEncTick, n] : it->second) {
                const int gap = std::abs(noteEncTick - bendEncTick);
                if (!note || gap < noteGap) {
                    noteGap = gap;
                    note = n;
                }
            }
        }
        if (!note || !note->chord()) {
            continue;
        }
        // Which way it goes is the one thing the wheel does say, and says plainly: a bend that pulls
        // the string down is written nowhere else. Its depth is not read, only its sign.
        static constexpr int kWheelAtRest = 400;
        bool pullsDown = false;
        auto wit = ctx.wheelByMeasStaff.find({ pb.measIdx, pb.staffIdx });
        if (wit != ctx.wheelByMeasStaff.end()) {
            for (const auto& [tick, value] : wit->second) {
                if (tick >= bendEncTick && std::abs(value) > kWheelAtRest) {
                    pullsDown = value < 0;
                    break;
                }
            }
        }
        // A tie is one sounding note written twice, and the bend belongs to the end of it: the string
        // is struck, held, and only then pulled. Starting on the first of the pair would draw the
        // arrow across the tie, which says the note bends into itself.
        while (const Tie* tie = note->tieFor()) {
            if (!tie->endNote() || tie->endNote() == note) {
                break;
            }
            note = tie->endNote();
        }
        // The note the bend reaches is the one struck after it, and joining the two is the arrow a
        // guitarist reads: MuseScore frets it on the string the first was bent on and takes the size
        // from the distance between them. A mark with nothing above it to reach stays on its note as
        // a dip, which needs no second note and so leaves the music the file states untouched.
        Note* dest = nextNoteOnTrack(note);
        // Struck again at the pitch it started from, the second note is where the string comes back
        // to rest, and the mark belongs there. Pulled down, the bend never leaves its own note.
        const bool joins = dest && !pullsDown && dest->pitch() > note->pitch()
                           && dest->pitch() - note->pitch() <= kFarthestBend;
        const bool returns = dest && !pullsDown && !joins && dest->pitch() == note->pitch();
        // A prebend is a string already pulled when it is struck, which MuseScore writes as a small
        // note below and an arrow into the real one, adding the small note itself.
        const EncOrnamentType kind = static_cast<EncOrnamentType>(pb.kind);
        if (kind == EncOrnamentType::GUITAR_PREBEND || kind == EncOrnamentType::GUITAR_PREBEND_RELEASE) {
            ctx.score->addGuitarBend(GuitarBendType::PRE_BEND, note, nullptr);
        } else if (joins) {
            ctx.score->addGuitarBend(GuitarBendType::BEND, note, dest);
        } else {
            ctx.score->addGuitarBend(GuitarBendType::SCOOP, returns ? dest : note, nullptr);
        }
    }
}

void resolveOrnaments(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;
    resolveArpeggios(score, ctx.pendingArpeggios);
    resolveSingleChordTremolos(score, ctx.pendingOrnTremolos);
    resolveMarkers(score, ctx.pendingMarkers);
    resolveFermatas(score, ctx.pendingFermatas);
    resolveStaccatos(score, ctx.pendingStaccatos);
    resolveTrillsWithSpans(score, ctx.pendingTrills, ctx.pendingTrillEnds, ctx.measuresByIdx);
    resolveUnconsumedTrillEnds(score, ctx.pendingTrillEnds);
    resolveBreaths(score, ctx.pendingBreaths);
    resolveMeasureRepeats(score, ctx.pendingMeasureRepeats);
    resolveVibratos(ctx);
    resolveGuitarBends(ctx);
}
} // namespace mu::iex::enc
