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
#include "noteloop-internal.h"
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
#include "engraving/editing/transpose.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/marker.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/drumset.h"
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
bool NoteLoopMeasCtx::isTieStartAt(int si, int v, int tick, int notePosition) const
{
    auto checkAt = [&](int t) {
        auto range = tieStartSet.equal_range({ si, v, t });
        for (auto it = range.first; it != range.second; ++it) {
            // notePosition<0 = match any; sourcePosition<0 = any note in chord
            if (notePosition < 0 || it->second < 0 || it->second == static_cast<int8_t>(notePosition)) {
                return true;
            }
        }
        return false;
    };
    for (int dt = 0; dt < CHORD_CLUSTER_THRESHOLD; ++dt) {
        if (checkAt(tick - dt)) {
            return true;
        }
        if (dt > 0 && checkAt(tick + dt)) {
            return true;
        }
    }
    return false;
}

void NoteLoopMeasCtx::closeTupletWithFill(BuildCtx& ctx, TupletTracker& tt,
                                          std::pair<int, int> trackKey)
{
    if (!tt.inTuplet() || tt.placedTicks <= Fraction(0, 1)) {
        tt.closeTuplet();
        return;
    }
    const Fraction expectedTup = TDuration(tt.currentTuplet->baseLen()).fraction()
                                 * tt.currentTuplet->ratio().denominator();
    TDuration snap(tt.placedTicks, true /*truncate*/);
    const bool fitsTD = snap.isValid() && snap.fraction() == tt.placedTicks;
    if (tt.placedTicks < expectedTup && !fitsTD) {
        const bool countShort = (static_cast<int>(tt.currentTuplet->elements().size()) < tt.actualN);
        // Mixed-duration group: element count matches actualN but face-value sum is still short
        // (e.g. {Q,Q,8th}/3:2 needs a 4th 8th to reach fullFaceSum=3Q, but the note falls exactly
        // at the measure boundary and Encore omits it from the element stream).
        const bool faceShort = (tt.fullFaceSum > Fraction(0, 1) && tt.faceTicks < tt.fullFaceSum);
        if (countShort || faceShort) {
            track_idx_t trk = static_cast<track_idx_t>(trackKey.first) * VOICES
                              + trackKey.second;
            DurationType baseLen = tt.currentTuplet->baseLen().type();
            Fraction perNote = TDuration(baseLen).fraction()
                               * Fraction(tt.normalN, tt.actualN);
            DurationType fillDurType = baseLen;
            // For the mixed-duration case compute fill from remaining face value, which is
            // smaller than baseLen and produces an advance that fits the remaining measure space.
            if (faceShort && !countShort) {
                const Fraction remFace = tt.fullFaceSum - tt.faceTicks;
                TDuration remDur(remFace, true /*truncate*/);
                if (remDur.isValid() && remDur.fraction() == remFace) {
                    fillDurType = remDur.type();
                    perNote = remFace * Fraction(tt.normalN, tt.actualN);
                }
            }
            int safety = tt.actualN + 1;
            while (tt.placedTicks < expectedTup && safety-- > 0
                   && (static_cast<int>(tt.currentTuplet->elements().size()) < tt.actualN
                       || (faceShort && tt.faceTicks < tt.fullFaceSum))
                   && ctx.cumTick[trackKey] + perNote <= measure->ticks()) {
                Fraction restTick = measure->tick() + ctx.cumTick[trackKey];
                Segment* seg = measure->getSegment(SegmentType::ChordRest, restTick);
                if (seg->element(trk)) {
                    break;
                }
                TDuration dur(fillDurType);
                Rest* rest = Factory::createRest(seg, dur);
                rest->setTrack(trk);
                rest->setTicks(dur.fraction());
                rest->setVisible(false);
                rest->setTuplet(tt.currentTuplet);
                tt.currentTuplet->add(rest);
                seg->add(rest);
                tt.placedTicks += perNote;
                ctx.cumTick[trackKey] += perNote;
            }
        }
    }
    tt.closeTuplet();
}

// Render tempo text. displayBpm is the beat-unit BPM that Encore shows the user.
// beatTicks=360 means the beat unit is a dotted quarter; beatTicks=240 is a quarter.
// For the MEAS header, bpm is QPM so displayBpm = bpm*2/3 when beatTicks=360.
// For ORN TEMPO, eo->tempo is already the displayed beat-unit value.
String tempoXmlText(int displayBpm, int beatTicks)
{
    if (beatTicks == 360) {
        return String(u"<sym>metNoteQuarterUp</sym><sym>space</sym><sym>metAugmentationDot</sym> = %1").arg(displayBpm);
    }
    return String(u"<sym>metNoteQuarterUp</sym> = %1").arg(displayBpm);
}

void buildNoteLoop(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;
    const EncFile& enc = ctx.enc;

    // Per-LINE-staff lookup for multi-staff instrument detection (see voice >= VOICES below).
    const int nLineStaves = (!enc.lines.empty())
                            ? static_cast<int>(enc.lines[0].staffData.size()) : 0;
    std::vector<int> lineStaffInstrIdx(nLineStaves, -1);
    std::vector<int> lineStaffWithin(nLineStaves, 0);
    for (int s = 0; s < nLineStaves; ++s) {
        lineStaffInstrIdx[s] = static_cast<int>(enc.lines[0].staffData[s].instrumentIndex());
        lineStaffWithin[s]   = static_cast<int>(enc.lines[0].staffData[s].staffIndex());
    }

    // Compact rawStaff encoding: the raw staff byte in a note element carries
    // (staffWithin<<6)|instrIdx, the same format as instrStaffIdx in LINE blocks.
    // Build a reverse map so we can translate the compact byte to a LINE slot index
    // before applying the voice>=VOICES / staffWithin routing below.
    // This handles multi-instrument files where earlier instruments have >1 staff
    // (e.g. piano+organ both grand staff: organ has instrIdx=1 but LINE slot 2).
    std::array<int, 256> lineSlotByRawByte;
    lineSlotByRawByte.fill(-1);
    for (int s = 0; s < nLineStaves; ++s) {
        const quint8 key = enc.lines[0].staffData[s].instrStaffIdx;
        lineSlotByRawByte[static_cast<unsigned char>(key)] = s;
    }

    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (mb->isMeasure()) {
            ctx.measuresByIdx.push_back(toMeasure(mb));
        }
    }

    // Slurs resolved after the pass: .enc has no SLURSTOP; end anchored at
    // last ChordRest in the alMezuro target measure (xoffset2 is layout, not tick).

    // Key sig in an empty measure (rest-only, no NOTE elements) breaks MuseScore's
    // multi-measure rest condensation.  Defer such key sigs to the first subsequent
    // measure that contains pitched notes; standard engraving practice places the
    // key change at the barline just before the notes resume.
    auto hasPitchedNotes = [](const EncMeasure& m) {
        for (const auto& elem : m.elements) {
            if (static_cast<EncElemType>(elem->type) == EncElemType::NOTE) {
                return true;
            }
        }
        return false;
    };

    struct DeferredKeySig {
        Key writtenKey;
        Key concertKey;
        int staffIdx { 0 };
    };
    std::vector<DeferredKeySig> pendingKeySigs;

    auto hasSingleRest = [](const EncMeasure& m) -> bool {
        return m.elements.size() == 1
               && static_cast<EncElemType>(m.elements[0]->type) == EncElemType::REST;
    };

    auto measDisplayCount = [&hasPitchedNotes, &hasSingleRest](
        const EncMeasure& m, const EncMeasure* prev, const EncMeasure* next) -> int {
        if (m.elements.size() != 1) {
            return 1;
        }
        const EncMeasureElem* e = m.elements[0].get();
        if (static_cast<EncElemType>(e->type) != EncElemType::REST) {
            return 1;
        }
        const int cnt = static_cast<int>(static_cast<const EncRest*>(e)->mrestCount);
        if (cnt <= 1) {
            return 1;
        }
        if (prev && hasSingleRest(*prev)) {
            return 1;
        }
        if (!next || !hasPitchedNotes(*next)) {
            return 1;
        }
        return cnt;
    };

    // Map enc.measures[i] → index into ctx.measuresByIdx (MuseScore measure index).
    // Required because single-block multi-measure rests expand 1 MEAS to N MuseScore measures,
    // so the indices diverge after the first such block.
    int measSkip = 0;
    size_t msIdxCounter = 0;

    int measIdx = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }

        if (measSkip > 0) {
            --measSkip;
            ++msIdxCounter;
            // Fill the virtual empty measure that was created by a single-block
            // multi-measure rest expansion so the sanity check passes.
            Measure* vm = toMeasure(mb);
            const Fraction vmTick = vm->tick();
            const Fraction vmLen  = vm->ticks();
            for (int si = 0; si < ctx.totalStaves; ++si) {
                const track_idx_t tr = static_cast<track_idx_t>(si) * VOICES;
                Segment* seg = vm->getSegment(SegmentType::ChordRest, vmTick);
                Rest* r = Factory::createRest(seg, TDuration(DurationType::V_MEASURE));
                r->setTicks(vmLen);
                r->setTrack(tr);
                seg->add(r);
            }
            continue;
        }

        if (measIdx >= static_cast<int>(enc.measures.size())) {
            break;
        }
        Measure* measure = toMeasure(mb);
        const EncMeasure& encMeas = enc.measures[measIdx];
        const Fraction measTick = measure->tick();

        NoteLoopMeasCtx mc;
        mc.measure = measure;
        mc.encMeas = &encMeas;
        mc.measTick = measTick;
        mc.measIdx = measIdx;
        mc.nLineStaves = nLineStaves;
        mc.lineStaffInstrIdx = &lineStaffInstrIdx;
        mc.lineStaffWithin = &lineStaffWithin;

        for (auto& [key, tt] : ctx.tuplets) {
            if (tt.inTuplet()) {
                tt.closeTuplet();
            }
        }
        ctx.tuplets.clear();
        for (auto& [key, tt] : ctx.innerTuplets) {
            if (tt.inTuplet()) {
                tt.closeTuplet();
            }
        }
        ctx.innerTuplets.clear();
        ctx.cumTick.clear();
        ctx.prevMidiTick.clear();
        ctx.prevEncVoice.clear();
        ctx.lastChordPos.clear();
        ctx.graceStolenTicks.clear();

        // Delete unattached grace chords explicitly: not in score tree, so no auto-cleanup.
        for (auto& [key, vec] : ctx.pendingGraces) {
            for (Chord* gc : vec) {
                LOGW() << "Encore import: discarding dangling grace chord at measure " << measIdx
                       << " (staff " << key.first << ", voice " << key.second << ")";
                delete gc;
            }
        }
        ctx.pendingGraces.clear();

        // Repeat navigation marks
        EncRepeatType rt = encMeas.repeatMark();
        if (rt != EncRepeatType::NONE) {
            addRepeatMark(score, measure, rt);
        }

        // Volta: equal-bitmask consecutive measures coalesce into one spanning Volta.
        if (encMeas.repeatAlternative != 0) {
            if (ctx.activeVolta && ctx.activeVoltaBits == encMeas.repeatAlternative) {
                ctx.activeVolta->setTick2(measTick + measure->ticks());
            } else {
                Volta* volta = Factory::createVolta(score->dummy());
                volta->setVoltaType(Volta::Type::CLOSED);
                volta->setTrack(0);
                volta->setTrack2(0);
                volta->setTick(measTick);
                volta->setTick2(measTick + measure->ticks());
                std::vector<int> endings;
                for (int b = 0; b < 8; ++b) {
                    if (encMeas.repeatAlternative & (1 << b)) {
                        endings.push_back(b + 1);
                    }
                }
                volta->setEndings(endings);
                // setText needed: without it the bracket renders blank even with setEndings.
                String voltaText;
                for (int number : endings) {
                    if (!voltaText.empty()) {
                        voltaText += u", ";
                    }
                    voltaText += String::number(number);
                }
                voltaText += u".";
                volta->setText(voltaText);
                score->addElement(volta);
                ctx.activeVolta = volta;
                ctx.activeVoltaBits = encMeas.repeatAlternative;
            }
        } else {
            ctx.activeVolta = nullptr;
            ctx.activeVoltaBits = 0;
        }

        // Apply any key sig that was deferred from a preceding rest-only measure.
        if (!pendingKeySigs.empty() && hasPitchedNotes(encMeas)) {
            for (const DeferredKeySig& dks : pendingKeySigs) {
                Staff* dksStaff = score->staff(dks.staffIdx);
                if (!dksStaff) {
                    continue;
                }
                KeySigEvent ke;
                ke.setConcertKey(dks.concertKey);
                ke.setKey(dks.writtenKey);
                dksStaff->setKey(measTick, ke);
                Segment* seg = measure->getSegment(SegmentType::KeySig, measTick);
                KeySig* ks = Factory::createKeySig(seg);
                ks->setTrack(dks.staffIdx * VOICES);
                ks->setKey(dks.concertKey, dks.writtenKey);
                seg->add(ks);
            }
            pendingKeySigs.clear();
        }

        // Sort: tick asc, then ORNs before notes, then tuplet notes before non-tuplet.
        // Last rule: when tup note and tie-artifact share a tick, tup note sets the duration.
        MeasureElemRefVec sortedElems;
        sortedElems.reserve(encMeas.elements.size());
        for (const auto& elem : encMeas.elements) {
            sortedElems.push_back(elem.get());
        }
        std::stable_sort(sortedElems.begin(), sortedElems.end(),
                         [](const EncMeasureElem* a, const EncMeasureElem* b) {
            if (a->tick != b->tick) {
                return a->tick < b->tick;
            }
            bool aIsNote = (static_cast<EncElemType>(a->type) == EncElemType::NOTE
                            || static_cast<EncElemType>(a->type) == EncElemType::REST);
            bool bIsNote = (static_cast<EncElemType>(b->type) == EncElemType::NOTE
                            || static_cast<EncElemType>(b->type) == EncElemType::REST);
            if (aIsNote != bIsNote) {
                return !aIsNote;  // non-notes before notes
            }
            if (!aIsNote) {
                return false;     // both non-notes: preserve stable order
            }
            // Among notes at same tick: tuplet notes before non-tuplet notes
            bool aTuplet = a->tupletByte() != 0;
            bool bTuplet = b->tupletByte() != 0;
            if (aTuplet != bTuplet) {
                return aTuplet;   // tuplet note first
            }
            // Shortest faceValue first at same tick so the chord root drives cumTick by the minimum step.
            const quint8 aFv = static_cast<EncElemType>(a->type) == EncElemType::NOTE
                               ? (static_cast<const EncNote*>(a)->faceValue & 0x0F)
                               : (static_cast<const EncRest*>(a)->faceValue & 0x0F);
            const quint8 bFv = static_cast<EncElemType>(b->type) == EncElemType::NOTE
                               ? (static_cast<const EncNote*>(b)->faceValue & 0x0F)
                               : (static_cast<const EncRest*>(b)->faceValue & 0x0F);
            if (aFv != bFv) {
                return aFv > bFv;  // higher number = shorter duration = comes first
            }
            return false;         // stable for equal keys
        });

        // Collect TIE-START positions using routed (staffIdx, voice) so that
        // ties on bit6-encoded second-staff notes are found correctly.
        for (const EncMeasureElem* e : sortedElems) {
            if (static_cast<EncElemType>(e->type) == EncElemType::TIE) {
                const EncTie* et = static_cast<const EncTie*>(e);
                if (et->isTieStart) {
                    int si = static_cast<int>(e->staffIdx);
                    int v  = static_cast<int>(e->voice);
                    const quint8 tiRawByte = (static_cast<quint8>(e->staffWithin) << 6)
                                             | static_cast<quint8>(e->staffIdx);
                    const int tiOrigSW = static_cast<int>(e->staffWithin);
                    bool tiResolved = false;
                    if (lineSlotByRawByte[tiRawByte] >= 0) {
                        si = lineSlotByRawByte[tiRawByte];
                        tiResolved = true;
                        // Remap voice for notes that used the per-staff half-range encoding.
                        if (tiOrigSW > 0 && v < static_cast<int>(VOICES)) {
                            const int vBase = tiOrigSW * (static_cast<int>(VOICES) / 2);
                            if (v >= vBase) {
                                v -= vBase;
                            }
                        }
                    }
                    if (v >= static_cast<int>(VOICES)) {
                        v = 0;
                    } else if (!tiResolved && tiOrigSW > 0) {
                        const int sw = tiOrigSW;
                        const int vBase = sw * (static_cast<int>(VOICES) / 2);
                        if (v >= vBase && si + sw < ctx.totalStaves) {
                            si += sw;
                            v  -= vBase;
                        }
                    }
                    mc.tieStartSet.insert({ { si, v, (int)e->tick }, et->sourcePosition });
                }
            }
        }

        mc.overrideGroupRatios.clear();
        mc.validTupletGroupMember
            = computeImpliedTupletMembers(sortedElems, encMeas, ctx.totalStaves,
                                          &mc.partialEndGroup, &mc.nestedInfos,
                                          &mc.overrideGroupRatios);
        mc.nestedByInnerFirst.clear();
        mc.nestedByInnerLast.clear();
        mc.innerGroupMembers.clear();
        for (const NestedTupletInfo& ni : mc.nestedInfos) {
            if (ni.innerFirst) {
                mc.nestedByInnerFirst[ni.innerFirst] = &ni;
            }
            if (ni.innerLast) {
                mc.nestedByInnerLast[ni.innerLast] = &ni;
            }
            // Collect all sorted elements between innerFirst and innerLast (inclusive).
            if (ni.innerFirst && ni.innerLast) {
                bool inInner = false;
                for (const EncMeasureElem* em2 : sortedElems) {
                    if (em2 == ni.innerFirst) {
                        inInner = true;
                    }
                    if (inInner) {
                        mc.innerGroupMembers.insert(em2);
                    }
                    if (em2 == ni.innerLast) {
                        break;
                    }
                }
            }
        }

        for (const EncMeasureElem* em : sortedElems) {
            const EncElemType et2 = static_cast<EncElemType>(em->type);
            if (et2 == EncElemType::NOTE) {
                mc.noteTicks.insert(static_cast<int>(em->tick));
                if (em->voice >= static_cast<int>(VOICES)) {
                    mc.voice4NoteTicks.insert(static_cast<int>(em->tick));
                } else {
                    mc.v0NoteCountAtTick[static_cast<int>(em->tick)]++;
                    mc.maxVoice0Tick = std::max(mc.maxVoice0Tick, static_cast<int>(em->tick));
                }
                // Detect scale string number anchors (au in 0x39..0x40)
                const EncNote* enPre = static_cast<const EncNote*>(em);
                if (encArticByteToScaleStringNumber(enPre->articulationUp) > 0
                    || encArticByteToScaleStringNumber(enPre->articulationDown) > 0) {
                    mc.hasScaleStringAnchors = true;
                }
            } else if (et2 == EncElemType::ORNAMENT) {
                const EncOrnament* eo2 = static_cast<const EncOrnament*>(em);
                const EncOrnamentType ot2 = eo2->ornType();
                if (ot2 >= EncOrnamentType::FINGER_1 && ot2 <= EncOrnamentType::FINGER_5) {
                    mc.ornFingCountAtTick[static_cast<int>(em->tick)]++;
                }
            }
        }

        for (const EncMeasureElem* e : sortedElems) {
            EncElemType et = static_cast<EncElemType>(e->type);

            // Skip notes/rests at or beyond measure end; ornaments at durTicks are end-of-measure markers (e.g. hairpin start on the barline) and must not be dropped.
            if ((et == EncElemType::NOTE || et == EncElemType::REST)
                && e->tick >= encMeas.durTicks) {
                continue;
            }
            if (et == EncElemType::ORNAMENT && e->tick > encMeas.durTicks) {
                // Past-durTicks dynamics/STAFFTEXT are Encore end-of-measure markers; let through and clamp to the last segment.
                const EncOrnament* eoFilt = static_cast<const EncOrnament*>(e);
                const EncOrnamentType ot = eoFilt->ornType();
                const bool isDyn = (ot >= EncOrnamentType::DYN_PPP
                                    && ot <= EncOrnamentType::DYN_FP)
                                   || ot == EncOrnamentType::DYN_FZ
                                   || ot == EncOrnamentType::DYN_SF;
                const bool isText = (ot == EncOrnamentType::STAFFTEXT);
                if (!isDyn && !isText) {
                    continue;
                }
            }

            int staffIdx = static_cast<int>(e->staffIdx);
            int voice    = static_cast<int>(e->voice);
            const bool isNoteOrRest = (et == EncElemType::NOTE || et == EncElemType::REST);

            // Translate compact rawStaff encoding to LINE slot index.
            // The raw staff byte uses (staffWithin<<6)|instrIdx (same format as instrStaffIdx
            // in LINE blocks). The lineSlotByRawByte map resolves it to the global LINE slot,
            // handling multi-instrument files where earlier instruments have >1 staff.
            // When the lookup succeeds:
            //   - staffIdx is updated to the LINE slot (staff routing done).
            //   - voice remap for notes/rests is still applied when origStaffWithin > 0 and
            //     voice is in the per-staff half-range (matches case-B remap semantics).
            //   - Case B (staffWithin routing) is then skipped to avoid double-routing.
            const quint8 rawNoteStaff = (static_cast<quint8>(e->staffWithin) << 6)
                                        | static_cast<quint8>(e->staffIdx);
            const int origStaffWithin  = static_cast<int>(e->staffWithin);
            bool rawByteResolved = false;
            if (lineSlotByRawByte[rawNoteStaff] >= 0) {
                staffIdx = lineSlotByRawByte[rawNoteStaff];
                rawByteResolved = true;
                // Apply voice remap when the original encoding used staffWithin>0 and voice
                // falls in the upper half-range (vBase..vBase+VOICES/2-1). This preserves the
                // case-B voice-remapping semantics for single-instrument grand-staff files.
                if (isNoteOrRest && origStaffWithin > 0 && voice < static_cast<int>(VOICES)) {
                    const int vBase = origStaffWithin * (static_cast<int>(VOICES) / 2);
                    if (voice >= vBase) {
                        voice -= vBase;
                    }
                }
            }

            if (staffIdx >= ctx.totalStaves) {
                continue;
            }
            // Multi-staff routing: two encodings exist.
            // (A) voice >= VOICES: out-of-band voice slot (voice=4+); route to staffIdx+1, voice=0.
            // (B) staffWithin > 0 (high 2 bits of raw staff byte): route to staffIdx+staffWithin,
            //     remap voice down by staffWithin*(VOICES/2) so each staff uses voices 0..VOICES/2-1.
            if (voice >= static_cast<int>(VOICES)) {
                if (staffIdx < nLineStaves) {
                    const int instrIdx = lineStaffInstrIdx[staffIdx];
                    if (instrIdx >= 0
                        && instrIdx < static_cast<int>(enc.instruments.size())
                        && enc.instruments[instrIdx].nstaves > 1
                        && lineStaffWithin[staffIdx] + 1 < enc.instruments[instrIdx].nstaves
                        && staffIdx + 1 < ctx.totalStaves) {
                        staffIdx += 1;
                    }
                }
                voice = 0;
            } else if (!rawByteResolved && origStaffWithin > 0) {
                // staffWithin: route to staffIdx+sw.
                // Notes/rests: also check voice is in the expected range, then remap.
                // Standalone ORNs: always have voice=0; route by staffWithin alone, no voice remap.
                const int sw    = origStaffWithin;
                const int vBase = sw * (static_cast<int>(VOICES) / 2);
                const bool voiceInRange = !isNoteOrRest || voice >= vBase;
                if (voiceInRange && staffIdx < nLineStaves) {
                    const int instrIdx = lineStaffInstrIdx[staffIdx];
                    if (instrIdx >= 0
                        && instrIdx < static_cast<int>(enc.instruments.size())
                        && enc.instruments[instrIdx].nstaves > sw
                        && staffIdx + sw < ctx.totalStaves) {
                        staffIdx += sw;
                        if (isNoteOrRest) {
                            voice -= vBase;
                        }
                    }
                }
            }

            auto encVoiceKey = std::make_pair(staffIdx, voice);
            int msVoice = voice;
            if (msVoice >= static_cast<int>(VOICES)) {
                continue;  // voice out of range
            }
            track_idx_t track = static_cast<track_idx_t>(staffIdx * VOICES + msVoice);
            auto trackKey = std::make_pair(staffIdx, msVoice);

            // faceValue-cumulative placement; near-simultaneous notes (< CHORD_MIDI_THRESHOLD) extend the chord.
            // Chord extension requires the same Encore voice; cross-voice spill must not extend.
            constexpr int CHORD_MIDI_THRESHOLD = 2 * CHORD_CLUSTER_THRESHOLD;  // = 8
            bool isChordExt   = isNoteOrRest && ctx.prevMidiTick.count(trackKey)
                                && ctx.prevEncVoice.count(trackKey)
                                && ctx.prevEncVoice.at(trackKey) == voice
                                && (int)e->tick - (int)ctx.prevMidiTick.at(trackKey) >= 0
                                && (int)e->tick - (int)ctx.prevMidiTick.at(trackKey)
                                < CHORD_MIDI_THRESHOLD;

            // Drop notes that arrive after the voice is full: no multi-stream overflow to
            // the next MuseScore voice. Overflow notes are MIDI recording artifacts that
            // belong to neither voice and would produce nonsensical music if routed to v1.
            if (isNoteOrRest && !isChordExt && ctx.cumTick[trackKey] >= measure->ticks()) {
                continue;
            }

            const int savedPrevMidiTick = ctx.prevMidiTick.count(trackKey)
                                          ? ctx.prevMidiTick.at(trackKey) : -1;
            const bool hadLastChordPos = ctx.lastChordPos.count(trackKey);
            const Fraction savedLastChordPos = hadLastChordPos
                                               ? ctx.lastChordPos.at(trackKey) : Fraction(-1, 1);

            Fraction elemTick;
            {
                if (isChordExt) {
                    elemTick = ctx.lastChordPos.count(trackKey) ? ctx.lastChordPos.at(trackKey)
                               : measTick;
                } else {
                    // Gap-snap: when binary tick is on the face grid and gap > CHORD_MIDI_THRESHOLD, advance cumTick to reveal implicit rests.
                    // Live-recorded ticks (sub-grid values) never trigger the snap, preserving multi-stream placement.
                    if (isNoteOrRest) {
                        quint8 elemFv = 0;
                        if (et == EncElemType::NOTE) {
                            elemFv = static_cast<const EncNote*>(e)->faceValue;
                        } else if (et == EncElemType::REST) {
                            elemFv = static_cast<const EncRest*>(e)->faceValue;
                        }
                        const int faceTicks = faceValue2ticks(elemFv);
                        const bool onFaceGrid = faceTicks > 0
                                                && ((int)e->tick % faceTicks) == 0;
                        // Suppress gap-snap when a grace is pending (v0xA6 grace ticks land on the face grid but represent no real time).
                        const bool gracePending = !ctx.pendingGraces[trackKey].empty();
                        // Suppress inside an active tuplet: tuplet elements are placed by
                        // cumTick advance (not by MIDI tick), so any apparent gap between
                        // enc tick and cumTick is an artifact of the tuplet's internal timing
                        // rather than a real rest. Firing gap-snap inside a tuplet would
                        // create spurious rests and misalign subsequent notes.
                        const bool inActiveTuplet = ctx.tuplets.count(trackKey)
                                                    && ctx.tuplets.at(trackKey).inTuplet();
                        // Also suppress when gap equals stolen grace ticks: the note after a grace group lands on the face grid by the grace amount and must not trigger a spurious rest.
                        const int stolenTicks = ctx.graceStolenTicks.count(trackKey)
                                                ? ctx.graceStolenTicks.at(trackKey) : 0;
                        if (onFaceGrid && !gracePending && !inActiveTuplet) {
                            // wholeTicks = beatTicks * timeSigDen; using 4*beatTicks is wrong for x/8 meters. Fall back to 960 when the header has no time signature.
                            const int wholeTicks
                                = (encMeas.beatTicks && encMeas.timeSigDen)
                                  ? encMeas.beatTicks * encMeas.timeSigDen
                                  : 960;
                            const Fraction encTickFrac((int)e->tick, wholeTicks);
                            if (encTickFrac > ctx.cumTick[trackKey]) {
                                const Fraction gap = encTickFrac - ctx.cumTick[trackKey];
                                const int gapEncTicks
                                    = (gap.numerator() * wholeTicks)
                                      / std::max(1, gap.denominator());
                                // Keep stolenTicks across all subsequent notes so each grace-displaced note gets suppression; deficit shows as a trailing rest at measure end.
                                const bool gapIsGraceArtifact
                                    = (stolenTicks > 0 && gapEncTicks <= stolenTicks);
                                if (gapEncTicks > CHORD_MIDI_THRESHOLD && !gapIsGraceArtifact
                                    && encTickFrac < measure->ticks()) {
                                    ctx.cumTick[trackKey] = encTickFrac;
                                }
                            }
                        }
                    }
                    elemTick = measTick + ctx.cumTick[trackKey];
                    if (isNoteOrRest) {
                        ctx.lastChordPos[trackKey] = elemTick;
                    }
                    // Only notes set prevMidiTick (chord-extension anchor). A rest is a separator: same-tick note after a rest is a fresh cluster, not an extension.
                    if (et == EncElemType::NOTE) {
                        ctx.prevMidiTick[trackKey] = e->tick;
                        ctx.prevEncVoice[trackKey] = voice;
                    }
                }
            }

            // -- Build per-element context for dispatched handlers --
            NoteElemCtx ec;
            ec.e = e;
            ec.et = et;
            ec.staffIdx = staffIdx;
            ec.voice = voice;
            ec.msVoice = msVoice;
            ec.track = track;
            ec.trackKey = trackKey;
            ec.encVoiceKey = encVoiceKey;
            ec.isChordExt = isChordExt;
            ec.isNoteOrRest = isNoteOrRest;
            ec.elemTick = elemTick;
            ec.savedPrevMidiTick = savedPrevMidiTick;
            ec.hadLastChordPos = hadLastChordPos;
            ec.savedLastChordPos = savedLastChordPos;

            auto handleChordSym = [&]() {
                const EncChordSym* ecs = static_cast<const EncChordSym*>(e);
                const QString raw = ecs->chordName();
                if (!raw.isEmpty()) {
                    // CHD elements in live-recorded Encore files often carry a MIDI timing
                    // offset from the note they annotate.  The key insight is that Encore
                    // renders chord symbols at BEAT positions, not at individual note ticks.
                    // Algorithm: floor the CHD tick to the start of the beat that contains it,
                    // then attach the harmony to the FIRST ChordRest segment within that beat
                    // that precedes the CHD.  This handles:
                    //   - Small drift (e.g. tick=6 for a beat-1 chord, distance 6t from note=0)
                    //   - Large drift (e.g. tick=87 for a beat-1 chord, distance 87t from note=0)
                    //   - Near-miss to a subdivison note (e.g. tick=62 in a measure with notes
                    //     at tick=0 AND tick=60; without the beat-floor the chord would snap to
                    //     the second 16th note instead of beat 1).
                    const int wt = (encMeas.beatTicks && encMeas.timeSigDen)
                                   ? encMeas.beatTicks * encMeas.timeSigDen : 960;
                    const int bt = static_cast<int>(encMeas.beatTicks ? encMeas.beatTicks : 240);
                    const int chdEncTick = static_cast<int>(e->tick);
                    const int beatStart  = (chdEncTick / bt) * bt;  // floor to beat boundary
                    const Fraction beatStartFrac(beatStart, wt);
                    const Fraction chdFrac(chdEncTick, wt);
                    Segment* seg = nullptr;
                    for (Segment* s = measure->first(SegmentType::ChordRest); s;
                         s = s->next(SegmentType::ChordRest)) {
                        const Fraction sRel = s->tick() - measTick;
                        if (sRel < beatStartFrac) {
                            continue;   // before the beat that contains this CHD
                        }
                        if (sRel > chdFrac) {
                            break;      // past the CHD tick
                        }
                        if (!seg) {
                            seg = s;    // take the FIRST segment in [beatStart, chdTick]
                        }
                    }
                    if (!seg) {
                        // Fallback: any segment at or before the CHD tick
                        for (Segment* s = measure->first(SegmentType::ChordRest); s;
                             s = s->next(SegmentType::ChordRest)) {
                            if (s->tick() - measTick <= chdFrac) {
                                seg = s;
                            } else {
                                break;
                            }
                        }
                    }
                    if (!seg) {
                        seg = measure->getSegment(SegmentType::ChordRest, elemTick);
                    }
                    Harmony* h = Factory::createHarmony(score->dummy()->segment());
                    h->setTrack(track);
                    h->setHarmony(String(raw));
                    seg->add(h);
                }
            };

            auto handleLyric = [&]() {
                const EncLyric* el = static_cast<const EncLyric*>(e);
                const String text(el->text);
                auto& queue = ctx.pendingLyrics[track];
                if (text == u"-") {
                    if (!queue.empty()) {
                        queue.back().hyphenAfter = true;
                    }
                    ctx.nextLyricHyphenBefore[track] = true;
                } else if (text.isEmpty()) {
                    ctx.nextLyricHyphenBefore[track] = false;
                } else {
                    PendingLyric pl;
                    pl.encTick = static_cast<int>(e->tick);
                    pl.text = text;
                    auto it = ctx.nextLyricHyphenBefore.find(track);
                    pl.hyphenBefore = (it != ctx.nextLyricHyphenBefore.end()) && it->second;
                    pl.hyphenAfter = false;
                    ctx.nextLyricHyphenBefore[track] = false;
                    queue.push_back(std::move(pl));
                }
            };

            auto handleKeyChange = [&]() {
                const EncKeyChange* ekc = static_cast<const EncKeyChange*>(e);
                Staff* staff = score->staff(staffIdx);
                if (!staff) {
                    return;
                }
                Key writtenKey = Key(encKeyToFifths(ekc->tipo));
                Interval v = Interval(ctx.staffPitchOffset[staffIdx]);
                Key concertKey = v.isZero() ? writtenKey : Transpose::transposeKey(writtenKey, v);

                if (!hasPitchedNotes(encMeas)) {
                    // Placing a KeySig in a rest-only measure breaks MuseScore's MMRest
                    // condensation.  Defer to the next measure that contains notes.
                    pendingKeySigs.push_back({ writtenKey, concertKey, staffIdx });
                    return;
                }

                KeySigEvent ke;
                ke.setConcertKey(concertKey);
                ke.setKey(writtenKey);
                staff->setKey(elemTick, ke);
                Segment* seg = measure->getSegment(SegmentType::KeySig, elemTick);
                KeySig* ks = Factory::createKeySig(seg);
                ks->setTrack(track);
                ks->setKey(concertKey, writtenKey);
                seg->add(ks);
            };

            switch (static_cast<EncElemType>(e->type)) {
            case EncElemType::NOTE:      handleNote(ctx, mc, ec);
                break;
            case EncElemType::REST:      handleRest(ctx, mc, ec);
                break;
            case EncElemType::CHORD:     handleChordSym();
                break;
            case EncElemType::LYRIC:     handleLyric();
                break;
            case EncElemType::ORNAMENT:  handleOrnament(ctx, mc, ec);
                break;
            case EncElemType::KEYCHANGE: handleKeyChange();
                break;
            default: break;
            }
        }  // end element for-loop

        // Finalize open tuplets before checkMeasure: closeTupletWithFill handles partial groups; closeTuplet handles the rest.
        for (auto& [key, tt] : ctx.tuplets) {
            mc.closeTupletWithFill(ctx, tt, key);
        }

        // Attach queued lyrics to the nearest chord segment (within half a beat). Encore PPQ from beatTicks; unmatched end-of-measure syllables are discarded.
        // In compound meters (6/8, 9/8, 12/8) beatTicks = ticks per dotted-quarter beat = 1.5 quarter notes.
        // The segEncTick formula uses ticks-per-quarter, so scale down by 2/3 for compound time.
        // matchThreshold stays at beatTicks/2 (half a beat in ENC ticks) regardless of meter.
        const bool isCompoundMeter = (encMeas.timeSigDen == 8 || encMeas.timeSigDen == 16)
                                     && encMeas.timeSigNum >= 6
                                     && (encMeas.timeSigNum % 3) == 0;
        const int beatTicksVal = encMeas.beatTicks ? static_cast<int>(encMeas.beatTicks) : 240;
        const int encTicksPerQuarter = isCompoundMeter ? beatTicksVal * 2 / 3 : beatTicksVal;
        // Compound meters need a wider window: Encore places lyrics slightly before their note.
        const int matchThreshold = isCompoundMeter ? beatTicksVal * 2 / 3 : beatTicksVal / 2;
        for (auto& [lyTrack, entries] : ctx.pendingLyrics) {
            if (entries.empty()) {
                continue;
            }
            // Encore multi-verse: verse 1=voice 0, verse 2=voice 1. MuseScore anchors all verses to voice-0 chord via setVerse().
            const int lyStaffIdx = static_cast<int>(lyTrack) / VOICES;
            const int lyVerseNo = static_cast<int>(lyTrack) % VOICES;
            const track_idx_t chordTrack = static_cast<track_idx_t>(lyStaffIdx) * VOICES;

            // Build (segEncTick, Chord*) table for this measure and verse track.
            std::vector<std::pair<int, Chord*> > noteTickPairs;
            for (Segment* s = measure->first(SegmentType::ChordRest);
                 s; s = s->next(SegmentType::ChordRest)) {
                EngravingItem* el = s->element(chordTrack);
                if (!el || !el->isChord()) {
                    continue;
                }
                const Fraction relTick = s->tick() - measure->tick();
                const int segEncTick = (relTick.numerator() * encTicksPerQuarter * 4)
                                       / std::max(1, relTick.denominator());
                noteTickPairs.emplace_back(segEncTick, toChord(el));
            }

            // Lyrics-first assignment: for each lyric in tick order, claim the nearest
            // available note within the threshold. This correctly handles cases where
            // syllables are unevenly spaced relative to note positions — a note-first
            // greedy pass would let a later syllable steal the note from an earlier one.
            std::vector<bool> noteConsumed(noteTickPairs.size(), false);
            for (const auto& pl : entries) {
                int bestNoteIdx = -1;
                int bestDelta = matchThreshold + 1;
                for (size_t ni = 0; ni < noteTickPairs.size(); ++ni) {
                    if (noteConsumed[ni]) {
                        continue;
                    }
                    const int delta = std::abs(noteTickPairs[ni].first - pl.encTick);
                    if (delta < bestDelta) {
                        bestDelta = delta;
                        bestNoteIdx = static_cast<int>(ni);
                    }
                }
                if (bestNoteIdx < 0) {
                    continue;
                }
                noteConsumed[bestNoteIdx] = true;
                Chord* c = noteTickPairs[bestNoteIdx].second;
                Lyrics* ly = Factory::createLyrics(c);
                ly->setTrack(chordTrack);
                ly->setVerse(lyVerseNo);
                ly->setXmlText(pl.text);
                LyricsSyllabic syll = LyricsSyllabic::SINGLE;
                if (pl.hyphenBefore && pl.hyphenAfter) {
                    syll = LyricsSyllabic::MIDDLE;
                } else if (pl.hyphenBefore) {
                    syll = LyricsSyllabic::END;
                } else if (pl.hyphenAfter) {
                    syll = LyricsSyllabic::BEGIN;
                }
                ly->setSyllabic(syll);
                c->add(ly);
            }
            // Lyric ticks are measure-relative; unmatched leftovers cannot anchor in a later measure, so discard them.
            entries.clear();
        }
        // ctx.nextLyricHyphenBefore survives bar lines so a trailing hyphen (e.g. "RO -") carries into the next measure's first syllable.

        // Fill remaining gaps; faceValue-cumulative placement ensures they represent genuinely missing rests, not drift.
        for (int si = 0; si < ctx.totalStaves; ++si) {
            measure->checkMeasure(static_cast<staff_idx_t>(si));
        }

        // Post-checkMeasure: fix over/undershoots ≤ 1/24 from non-standard gaps (cascade fills). Overshoot: remove smallest gap rests. Undershoot: add V_MEASURE gap rest for the deficit.
        {
            const Fraction mLen_fix = measure->ticks();
            const Fraction maxDelta(1, 24);
            for (int si = 0; si < ctx.totalStaves; ++si) {
                for (voice_idx_t v = 0; v < VOICES; ++v) {
                    track_idx_t tr = static_cast<track_idx_t>(si * VOICES + v);
                    Fraction voiceSum(0, 1);
                    bool hasContent = false;
                    std::vector<Rest*> gapRests;
                    for (Segment* seg = measure->first(SegmentType::ChordRest);
                         seg; seg = seg->next(SegmentType::ChordRest)) {
                        EngravingItem* el = seg->element(tr);
                        if (!el) {
                            continue;
                        }
                        hasContent = true;
                        ChordRest* cr = toChordRest(el);
                        voiceSum += cr->actualTicks();
                        if (el->isRest() && toRest(el)->isGap()) {
                            gapRests.push_back(toRest(el));
                        }
                    }
                    if (!hasContent) {
                        continue;
                    }

                    // Overshoot: remove gap rests smallest-first
                    if (voiceSum > mLen_fix && (voiceSum - mLen_fix) <= maxDelta) {
                        std::stable_sort(gapRests.begin(), gapRests.end(),
                                         [](Rest* a, Rest* b){
                            return a->actualTicks() < b->actualTicks();
                        });
                        for (Rest* gr : gapRests) {
                            if (voiceSum <= mLen_fix) {
                                break;
                            }
                            Fraction at = gr->actualTicks();
                            voiceSum -= at;
                            Segment* gseg = gr->segment();
                            gseg->remove(gr);
                            delete gr;
                        }
                    }

                    // Undershoot: add exact V_MEASURE gap rest for residual
                    const Fraction deficit = mLen_fix - voiceSum;
                    if (deficit > Fraction(0, 1) && deficit <= maxDelta) {
                        const Fraction fillTick = measure->tick() + voiceSum;
                        Segment* fillSeg = measure->getSegment(
                            SegmentType::ChordRest, fillTick);
                        if (!fillSeg->element(tr)) {
                            Rest* r = Factory::createRest(
                                fillSeg, TDuration(DurationType::V_MEASURE));
                            r->setTicks(deficit);
                            r->setTrack(tr);
                            r->setGap(true);
                            fillSeg->add(r);
                        }
                    }
                }
            }
        }

        // Nuclear hard-cap: if any voice still overshoots by any amount (the 1/24 pass
        // only handles gap rests, and the Tuplet.ticks fix may not cover all edge cases),
        // remove trailing ChordRest elements until voiceSum <= measureLen, then fill
        // any remaining deficit with an invisible V_MEASURE gap rest. This guarantees
        // the importer never produces a measure with wrong total duration.
        {
            const Fraction mLen_cap = measure->ticks();
            for (int si = 0; si < ctx.totalStaves; ++si) {
                for (voice_idx_t v = 0; v < VOICES; ++v) {
                    const track_idx_t tr = static_cast<track_idx_t>(si * VOICES + v);
                    // Collect all ChordRests in order; compute voiceSum.
                    std::vector<ChordRest*> crs;
                    Fraction voiceSum(0, 1);
                    for (Segment* seg = measure->first(SegmentType::ChordRest);
                         seg; seg = seg->next(SegmentType::ChordRest)) {
                        EngravingItem* el = seg->element(tr);
                        if (!el) {
                            continue;
                        }
                        ChordRest* cr = toChordRest(el);
                        crs.push_back(cr);
                        voiceSum += cr->actualTicks();
                    }
                    if (voiceSum <= mLen_cap || crs.empty()) {
                        continue;
                    }
                    // Remove from the end (last placed) until we fit.
                    while (voiceSum > mLen_cap && !crs.empty()) {
                        ChordRest* last = crs.back();
                        crs.pop_back();
                        voiceSum -= last->actualTicks();
                        // Detach from tuplet if needed.
                        if (last->tuplet()) {
                            last->tuplet()->remove(last);
                            last->setTuplet(nullptr);
                        }
                        Segment* lseg = last->segment();
                        lseg->remove(last);
                        delete last;
                    }
                    // Fill any residual deficit with an invisible gap rest.
                    const Fraction deficit2 = mLen_cap - voiceSum;
                    if (deficit2 > Fraction(0, 1)) {
                        const Fraction fillTick2 = measure->tick() + voiceSum;
                        Segment* fillSeg2 = measure->getSegment(
                            SegmentType::ChordRest, fillTick2);
                        if (!fillSeg2->element(tr)) {
                            Rest* r2 = Factory::createRest(
                                fillSeg2, TDuration(DurationType::V_MEASURE));
                            r2->setTicks(deficit2);
                            r2->setTrack(tr);
                            r2->setGap(true);
                            fillSeg2->add(r2);
                        }
                    }
                }
            }
        }

        const EncMeasure* prevMeas = (measIdx > 0) ? &enc.measures[measIdx - 1] : nullptr;
        const EncMeasure* nextMeas = (measIdx + 1 < static_cast<int>(enc.measures.size()))
                                     ? &enc.measures[measIdx + 1] : nullptr;
        measSkip = measDisplayCount(encMeas, prevMeas, nextMeas) - 1;
        ++msIdxCounter;
        ++measIdx;
    }

    // Apply per-measure BPM from MEAS header; emit TempoText only when BPM changes.
    // Skip when an ORN TEMPO or STAFFTEXT tempo is already present ANYWHERE in the measure,
    // not just at measTick. ORN TEMPO elements are placed at their note's tick (which may
    // differ from measTick when the measure starts with a rest), so the old per-segment
    // guard missed them and created a duplicate tempo mark.
    {
        quint16 lastBpm = 0;
        for (size_t mi = 0; mi < enc.measures.size(); ++mi) {
            const quint16 bpm = enc.measures[mi].bpm;
            if (bpm == 0) {
                continue;
            }
            if (mi > 0 && bpm == lastBpm) {
                continue;
            }
            const size_t msI = (mi < ctx.encToMsIdx.size()) ? ctx.encToMsIdx[mi] : mi;
            if (msI >= ctx.measuresByIdx.size()) {
                continue;
            }
            Measure* m = ctx.measuresByIdx[msI];
            const Fraction measTick = m->tick();
            Segment* seg = m->getSegment(SegmentType::ChordRest, measTick);
            if (!seg) {
                continue;
            }
            bool hasExisting = false;
            for (Segment* s = m->first(SegmentType::ChordRest); s && !hasExisting;
                 s = s->next(SegmentType::ChordRest)) {
                for (EngravingItem* e : s->annotations()) {
                    if (e && e->isTempoText()) {
                        hasExisting = true;
                        break;
                    }
                }
            }
            if (!hasExisting) {
                // Detect dotted-quarter beat: MEAS header beatTicks=360, OR compound time sig
                // (6/8, 9/8, 12/8). Old fixtures store beatTicks=240 even for 6/8, so keep
                // the timesig fallback for backward compatibility.
                const quint16 rawBeatTicks = enc.measures[mi].beatTicks;
                const Fraction mts = m->timesig();
                const bool cmpd = (rawBeatTicks == 360)
                                  || (mts.denominator() == 8
                                      && mts.numerator() % 3 == 0
                                      && mts.numerator() > 3);
                const double bps = bpm / 60.0;    // bpm is QPM; correct for playback
                const int displayBpm = cmpd ? (bpm * 2 + 1) / 3 : static_cast<int>(bpm);
                TempoText* tt = Factory::createTempoText(seg);
                tt->setTrack(0);
                tt->setTempo(BeatsPerSecond(bps));
                tt->setXmlText(tempoXmlText(displayBpm, cmpd ? 360 : 240));
                tt->setFollowText(true);
                seg->add(tt);
                score->setTempo(measTick, BeatsPerSecond(bps));
            }
            lastBpm = bpm;
        }
    }

    // Discard any grace chords still queued after the final measure (no main chord
    // followed them). They have no parent in the score tree, so delete them here.
    for (auto& [key, vec] : ctx.pendingGraces) {
        for (Chord* gc : vec) {
            LOGW() << "Encore import: discarding dangling grace chord at end of score"
                   << " (staff " << key.first << ", voice " << key.second << ")";
            delete gc;
        }
    }
    ctx.pendingGraces.clear();
}
} // namespace mu::iex::encore
