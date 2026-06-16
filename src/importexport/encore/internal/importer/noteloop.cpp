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

namespace mu::iex::enc {
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
        // Mixed-duration group: count matches actualN but face-value sum is short (e.g. {Q,Q,8th}/3:2 missing a 4th 8th at measure boundary).
        const bool faceShort = (tt.fullFaceSum > Fraction(0, 1) && tt.faceTicks < tt.fullFaceSum);
        if (countShort || faceShort) {
            track_idx_t trk = static_cast<track_idx_t>(trackKey.first) * VOICES
                              + trackKey.second;
            DurationType baseLen = tt.currentTuplet->baseLen().type();
            Fraction perNote = TDuration(baseLen).fraction()
                               * Fraction(tt.normalN, tt.actualN);
            DurationType fillDurType = baseLen;
            // Mixed-duration fill: derive from remaining face value rather than baseLen.
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

struct DeferredKeySig {
    Key writtenKey;
    Key concertKey;
    int staffIdx { 0 };
};

static bool hasPitchedNotes(const EncMeasure& m)
{
    for (const auto& elem : m.elements) {
        if (static_cast<EncElemType>(elem->type) == EncElemType::NOTE) {
            return true;
        }
    }
    return false;
}

static bool hasMultiRest(const EncMeasure& m)
{
    if (m.elements.empty()) {
        return false;
    }
    for (const auto& ep : m.elements) {
        if (static_cast<EncElemType>(ep->type) != EncElemType::REST) {
            return false;
        }
    }
    return static_cast<const EncRest*>(m.elements[0].get())->mrestCount > 1;
}

static int measDisplayCount(const EncMeasure& m, const EncMeasure* prev)
{
    if (m.elements.empty()) {
        return 1;
    }
    for (const auto& ep : m.elements) {
        if (static_cast<EncElemType>(ep->type) != EncElemType::REST) {
            return 1;
        }
    }
    const int cnt = static_cast<int>(static_cast<const EncRest*>(m.elements[0].get())->mrestCount);
    if (cnt <= 1) {
        return 1;
    }
    if (prev && hasMultiRest(*prev)) {
        return 1;
    }
    return cnt;
}

static void sortMeasureElements(const EncMeasure& encMeas, MeasureElemRefVec& sortedElems)
{
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
        // Shortest faceValue first: chord root drives cumTick by minimum step.
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
}

static void collectTieStartPositions(const MeasureElemRefVec& sortedElems,
                                     const std::array<int, 256>& lineSlotByRawByte,
                                     int totalStaves, NoteLoopMeasCtx& mc)
{
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
                    if (v >= vBase && si + sw < totalStaves) {
                        si += sw;
                        v  -= vBase;
                    }
                }
                mc.tieStartSet.insert({ { si, v, (int)e->tick }, et->sourcePosition });
            }
        }
    }
}

static void scanMeasureMetadata(const MeasureElemRefVec& sortedElems, NoteLoopMeasCtx& mc)
{
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
}

// Returns false if the element should be skipped entirely (tick out of range).
static bool shouldIncludeElement(const EncMeasureElem* e, const EncMeasure& encMeas)
{
    const EncElemType et = static_cast<EncElemType>(e->type);
    // Notes/rests at or beyond measure end are dropped; ORNs at durTicks are end-of-measure markers
    // (e.g. hairpin on barline) and must not be dropped.
    if ((et == EncElemType::NOTE || et == EncElemType::REST)
        && e->tick >= encMeas.durTicks) {
        return false;
    }
    if (et == EncElemType::ORNAMENT && e->tick > encMeas.durTicks) {
        // Past-durTicks dynamics/STAFFTEXT: let through and clamp to the last segment.
        const EncOrnament* eoFilt = static_cast<const EncOrnament*>(e);
        const EncOrnamentType ot = eoFilt->ornType();
        const bool isDyn = (ot >= EncOrnamentType::DYN_PPP
                            && ot <= EncOrnamentType::DYN_FP)
                           || ot == EncOrnamentType::DYN_FZ
                           || ot == EncOrnamentType::DYN_SF;
        const bool isText = (ot == EncOrnamentType::STAFFTEXT);
        if (!isDyn && !isText) {
            return false;
        }
    }
    return true;
}

// Returns false if the element should be skipped (staffIdx out of range or voice invalid).
// On success, fills staffIdx, voice, msVoice, track, trackKey, encVoiceKey.
static bool routeElementStaffVoice(
    const EncMeasureElem* e,
    bool isNoteOrRest,
    const std::array<int, 256>& lineSlotByRawByte,
    const NoteLoopMeasCtx& mc,
    const BuildCtx& ctx,
    int& staffIdx,
    int& voice,
    int& msVoice,
    track_idx_t& track,
    std::pair<int, int>& trackKey,
    std::pair<int, int>& encVoiceKey)
{
    const EncRoot& enc = ctx.enc;
    const int nLineStaves = mc.nLineStaves;
    const std::vector<int>& lineStaffInstrIdx = *mc.lineStaffInstrIdx;
    const std::vector<int>& lineStaffWithin   = *mc.lineStaffWithin;

    staffIdx = static_cast<int>(e->staffIdx);
    voice    = static_cast<int>(e->voice);

    // Translate rawStaff byte (staffWithin<<6)|instrIdx to LINE slot; apply case-B voice remap when origStaffWithin > 0.
    const quint8 rawNoteStaff = (static_cast<quint8>(e->staffWithin) << 6)
                                | static_cast<quint8>(e->staffIdx);
    const int origStaffWithin  = static_cast<int>(e->staffWithin);
    bool rawByteResolved = false;
    if (lineSlotByRawByte[rawNoteStaff] >= 0) {
        staffIdx = lineSlotByRawByte[rawNoteStaff];
        rawByteResolved = true;
        if (isNoteOrRest && origStaffWithin > 0 && voice < static_cast<int>(VOICES)) {
            const int vBase = origStaffWithin * (static_cast<int>(VOICES) / 2);
            if (voice >= vBase) {
                voice -= vBase;
            }
        }
    }

    if (staffIdx >= ctx.totalStaves) {
        return false;
    }
    // Multi-staff routing:
    // (A) voice >= VOICES: route to staffIdx+1, voice=0.
    // (B) staffWithin > 0: route to staffIdx+sw, remap voice down by sw*(VOICES/2).
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
        // Standalone ORNs always have voice=0 and route by staffWithin alone.
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

    encVoiceKey = std::make_pair(staffIdx, voice);
    msVoice = voice;
    if (msVoice >= static_cast<int>(VOICES)) {
        return false;  // voice out of range
    }
    track    = static_cast<track_idx_t>(staffIdx * VOICES + msVoice);
    trackKey = std::make_pair(staffIdx, msVoice);
    return true;
}

// Returns the MuseScore tick where this element should be placed.
// Has side effects on ctx: updates prevMidiTick, lastChordPos, prevRestTick, noteXoffByMeasStaff.
static Fraction computeElementTick(
    const EncMeasureElem* e,
    bool isNoteOrRest,
    bool isChordExt,
    int voice,
    int staffIdx,
    std::pair<int, int> trackKey,
    const Measure* measure,
    Fraction measTick,
    BuildCtx& ctx,
    const NoteLoopMeasCtx& mc)
{
    constexpr int CHORD_MIDI_THRESHOLD = 2 * CHORD_CLUSTER_THRESHOLD;  // = 8
    const EncElemType et = static_cast<EncElemType>(e->type);
    if (isChordExt) {
        return ctx.lastChordPos.count(trackKey) ? ctx.lastChordPos.at(trackKey) : measTick;
    }

    // Gap-snap: when binary tick is on the face grid and gap > CHORD_MIDI_THRESHOLD, advance cumTick
    // to reveal implicit rests. Live-recorded ticks (sub-grid values) never trigger the snap,
    // preserving multi-stream placement.
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
        // Suppress gap-snap: (a) grace pending (v0xA6 ticks on face grid but no real time),
        // (b) inside active tuplet (apparent gap is tuplet-internal timing artifact),
        // (c) gap equals stolen grace ticks (grace-displaced note must not fire spurious rest).
        const bool gracePending = !ctx.pendingGraces[trackKey].empty();
        const bool inActiveTuplet = ctx.tuplets.count(trackKey)
                                    && ctx.tuplets.at(trackKey).inTuplet();
        const int stolenTicks = ctx.graceStolenTicks.count(trackKey)
                                ? ctx.graceStolenTicks.at(trackKey) : 0;
        if (onFaceGrid && !gracePending && !inActiveTuplet) {
            // Hardcode 960 ticks/whole: the beatTicks*timeSigDen formula breaks for
            // non-standard beatTicks (e.g. 2/2 with beatTicks=240 gives 480).
            static constexpr int wholeTicks = 960;
            const Fraction encTickFrac((int)e->tick, wholeTicks);
            if (encTickFrac > ctx.cumTick[trackKey]) {
                const Fraction gap = encTickFrac - ctx.cumTick[trackKey];
                const int gapEncTicks
                    = (gap.numerator() * wholeTicks)
                      / std::max(1, gap.denominator());
                const bool gapIsGraceArtifact
                    = (stolenTicks > 0 && gapEncTicks <= stolenTicks);
                if (gapEncTicks > CHORD_MIDI_THRESHOLD && !gapIsGraceArtifact
                    && encTickFrac < measure->ticks()) {
                    ctx.cumTick[trackKey] = encTickFrac;
                }
            }
        }
    }

    const Fraction elemTick = measTick + ctx.cumTick[trackKey];
    if (isNoteOrRest) {
        ctx.lastChordPos[trackKey] = elemTick;
    }
    // Rests don't set prevMidiTick: a note after a rest is a fresh cluster.
    if (et == EncElemType::NOTE) {
        ctx.prevMidiTick[trackKey] = e->tick;
        ctx.prevEncVoice[trackKey] = voice;
        // Record note xoffset for bowing-mark cluster resolution.
        const auto* en = static_cast<const EncNote*>(e);
        auto& vec = ctx.noteXoffByMeasStaff[{ mc.measIdx, staffIdx }];
        const int encTick = static_cast<int>(e->tick);
        const int xoff = static_cast<int>(en->xoffset);
        bool already = false;
        for (const auto& p : vec) { if (p.first == encTick) { already = true; break; } }
        if (!already) { vec.push_back({ encTick, xoff }); }
    } else if (et == EncElemType::REST) {
        ctx.prevRestTick[trackKey] = static_cast<int>(e->tick);
    }
    return elemTick;
}

static void initLineStaffMappings(
    const EncRoot& enc,
    int nLineStaves,
    std::vector<int>& lineStaffInstrIdx,
    std::vector<int>& lineStaffWithin,
    std::array<int, 256>& lineSlotByRawByte)
{
    lineStaffInstrIdx.assign(nLineStaves, -1);
    lineStaffWithin.assign(nLineStaves, 0);
    for (int s = 0; s < nLineStaves; ++s) {
        lineStaffInstrIdx[s] = static_cast<int>(enc.lines[0].staffData[s].instrumentIndex());
        lineStaffWithin[s]   = static_cast<int>(enc.lines[0].staffData[s].staffIndex());
    }

    // Raw staff byte carries (staffWithin<<6)|instrIdx (same as instrStaffIdx in LINE blocks).
    // Reverse map translates it to a LINE slot before voice routing.
    // Needed for multi-instrument files where earlier instruments have >1 staff (e.g. piano+organ: organ instrIdx=1 but LINE slot 2).
    lineSlotByRawByte.fill(-1);
    for (int s = 0; s < nLineStaves; ++s) {
        const quint8 key = enc.lines[0].staffData[s].instrStaffIdx;
        lineSlotByRawByte[static_cast<unsigned char>(key)] = s;
    }
}

static void fillExpandedMrestMeasure(Measure* vm, int totalStaves)
{
    const Fraction vmTick = vm->tick();
    const Fraction vmLen  = vm->ticks();
    for (int si = 0; si < totalStaves; ++si) {
        const track_idx_t tr = static_cast<track_idx_t>(si) * VOICES;
        Segment* seg = vm->getSegment(SegmentType::ChordRest, vmTick);
        Rest* r = Factory::createRest(seg, TDuration(DurationType::V_MEASURE));
        r->setTicks(vmLen);
        r->setTrack(tr);
        seg->add(r);
    }
}

static void coalesceVolta(BuildCtx& ctx, Measure* measure,
                          const EncMeasure& encMeas, Fraction measTick)
{
    if (encMeas.repeatAlternative != 0) {
        if (ctx.activeVolta && ctx.activeVoltaBits == encMeas.repeatAlternative) {
            ctx.activeVolta->setTick2(measTick + measure->ticks());
        } else {
            Volta* volta = Factory::createVolta(ctx.score->dummy());
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
            // setText required: setEndings alone leaves the bracket blank.
            String voltaText;
            for (int number : endings) {
                if (!voltaText.empty()) {
                    voltaText += u", ";
                }
                voltaText += String::number(number);
            }
            voltaText += u".";
            volta->setText(voltaText);
            ctx.score->addElement(volta);
            ctx.activeVolta = volta;
            ctx.activeVoltaBits = encMeas.repeatAlternative;
        }
    } else {
        ctx.activeVolta = nullptr;
        ctx.activeVoltaBits = 0;
    }
}

static void resetPerMeasureState(BuildCtx& ctx, int measIdx)
{
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
    ctx.prevRestTick.clear();
    ctx.graceStolenTicks.clear();

    // Unattached grace chords are not in the score tree and need explicit deletion.
    for (auto& [key, vec] : ctx.pendingGraces) {
        for (Chord* gc : vec) {
            LOGW() << "Encore import: discarding dangling grace chord at measure " << measIdx
                   << " (staff " << key.first << ", voice " << key.second << ")";
            delete gc;
        }
    }
    ctx.pendingGraces.clear();
}

static void buildNestedTupletMaps(NoteLoopMeasCtx& mc,
                                  const MeasureElemRefVec& sortedElems)
{
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
}

static void handleKeyChange(BuildCtx& ctx, const NoteLoopMeasCtx& mc,
                            const NoteElemCtx& ec, const EncMeasureElem* e,
                            std::vector<DeferredKeySig>& pendingKeySigs)
{
    MasterScore* score = ctx.score;
    const EncKeyChange* ekc = static_cast<const EncKeyChange*>(e);
    Staff* staff = score->staff(ec.staffIdx);
    if (!staff) {
        return;
    }
    Key writtenKey = Key(encKeyToFifths(ekc->tipo));
    Interval v = Interval(ctx.staffPitchOffset[ec.staffIdx]);
    Key concertKey = v.isZero() ? writtenKey : Transpose::transposeKey(writtenKey, v);

    if (!hasPitchedNotes(*mc.encMeas)) {
        // Placing a KeySig in a rest-only measure breaks MuseScore's MMRest
        // condensation.  Defer to the next measure that contains notes.
        pendingKeySigs.push_back({ writtenKey, concertKey, ec.staffIdx });
        return;
    }

    KeySigEvent ke;
    ke.setConcertKey(concertKey);
    ke.setKey(writtenKey);
    staff->setKey(ec.elemTick, ke);
    Segment* seg = mc.measure->getSegment(SegmentType::KeySig, ec.elemTick);
    KeySig* ks = Factory::createKeySig(seg);
    ks->setTrack(ec.track);
    ks->setKey(concertKey, writtenKey);
    seg->add(ks);
}

void buildNoteLoop(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;
    const EncRoot& enc = ctx.enc;

    const int nLineStaves = (!enc.lines.empty())
                            ? static_cast<int>(enc.lines[0].staffData.size()) : 0;
    std::vector<int> lineStaffInstrIdx;
    std::vector<int> lineStaffWithin;
    std::array<int, 256> lineSlotByRawByte;
    initLineStaffMappings(enc, nLineStaves, lineStaffInstrIdx, lineStaffWithin, lineSlotByRawByte);

    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (mb->isMeasure()) {
            ctx.measuresByIdx.push_back(toMeasure(mb));
        }
    }

    // Slurs resolved after the pass: .enc has no SLURSTOP; end anchored at last ChordRest in the alMezuro target measure.

    std::vector<DeferredKeySig> pendingKeySigs;

    // measSkip tracks the expansion of multi-measure rests (1 enc MEAS → N MuseScore measures).
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
            fillExpandedMrestMeasure(toMeasure(mb), ctx.totalStaves);
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

        resetPerMeasureState(ctx, measIdx);

        EncRepeatType rt = encMeas.repeatMark();
        if (rt != EncRepeatType::NONE) {
            addRepeatMark(score, measure, rt);
        }

        // Consecutive measures with equal repeatAlternative bitmask coalesce into one Volta.
        coalesceVolta(ctx, measure, encMeas, measTick);

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

        // Sort: tick asc, ORNs before notes, tuplet notes before non-tuplet (ensures tup note sets duration at shared tick).
        MeasureElemRefVec sortedElems;
        sortMeasureElements(encMeas, sortedElems);

        // Collect TIE-START positions using routed (staffIdx, voice) so bit6-encoded second-staff notes resolve correctly.
        collectTieStartPositions(sortedElems, lineSlotByRawByte, ctx.totalStaves, mc);

        mc.overrideGroupRatios.clear();
        mc.validTupletGroupMember
            = computeImpliedTupletMembers(sortedElems, encMeas, ctx.totalStaves,
                                          &mc.partialEndGroup, &mc.nestedInfos,
                                          &mc.overrideGroupRatios);
        buildNestedTupletMaps(mc, sortedElems);

        scanMeasureMetadata(sortedElems, mc);

        for (const EncMeasureElem* e : sortedElems) {
            EncElemType et = static_cast<EncElemType>(e->type);
            const bool isNoteOrRest = (et == EncElemType::NOTE || et == EncElemType::REST);

            if (!shouldIncludeElement(e, encMeas)) {
                continue;
            }

            int staffIdx = 0, voice = 0, msVoice = 0;
            track_idx_t track = 0;
            std::pair<int, int> trackKey, encVoiceKey;
            if (!routeElementStaffVoice(e, isNoteOrRest, lineSlotByRawByte, mc, ctx,
                                        staffIdx, voice, msVoice, track, trackKey, encVoiceKey)) {
                continue;
            }

            // Near-simultaneous notes (< CHORD_MIDI_THRESHOLD) extend the chord; same Encore voice required.
            constexpr int CHORD_MIDI_THRESHOLD = 2 * CHORD_CLUSTER_THRESHOLD;  // = 8
            bool isChordExt = isNoteOrRest && ctx.prevMidiTick.count(trackKey)
                              && ctx.prevEncVoice.count(trackKey)
                              && ctx.prevEncVoice.at(trackKey) == voice
                              && (int)e->tick - (int)ctx.prevMidiTick.at(trackKey) >= 0
                              && (int)e->tick - (int)ctx.prevMidiTick.at(trackKey)
                              < CHORD_MIDI_THRESHOLD;
            // REST-REST dedup: two Encore voices routing to the same MuseScore voice at the same tick; second REST would double-advance cumTick.
            if (!isChordExt && et == EncElemType::REST
                && ctx.prevRestTick.count(trackKey)
                && ctx.prevRestTick.at(trackKey) == static_cast<int>(e->tick)) {
                continue;
            }

            // Drop overflow notes when voice is full; MIDI artifacts must not spill to the next MuseScore voice.
            if (isNoteOrRest && !isChordExt && ctx.cumTick[trackKey] >= measure->ticks()) {
                continue;
            }

            const int savedPrevMidiTick = ctx.prevMidiTick.count(trackKey)
                                          ? ctx.prevMidiTick.at(trackKey) : -1;
            const bool hadLastChordPos = ctx.lastChordPos.count(trackKey);
            const Fraction savedLastChordPos = hadLastChordPos
                                               ? ctx.lastChordPos.at(trackKey) : Fraction(-1, 1);

            const Fraction elemTick = computeElementTick(e, isNoteOrRest, isChordExt, voice,
                                                         staffIdx, trackKey, measure, measTick,
                                                         ctx, mc);

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

            switch (static_cast<EncElemType>(e->type)) {
            case EncElemType::NOTE:      handleNote(ctx, mc, ec);
                break;
            case EncElemType::REST:      handleRest(ctx, mc, ec);
                break;
            case EncElemType::CHORD:     handleChordSym(ctx, mc, ec);
                break;
            case EncElemType::LYRIC:     enqueueLyric(ctx, static_cast<const EncLyric*>(e), track);
                break;
            case EncElemType::ORNAMENT:  handleOrnament(ctx, mc, ec);
                break;
            case EncElemType::KEYCHANGE: handleKeyChange(ctx, mc, ec, e, pendingKeySigs);
                break;
            default: break;
            }
        }  // end element for-loop

        for (auto& [key, tt] : ctx.tuplets) {
            mc.closeTupletWithFill(ctx, tt, key);
        }

        attachPendingLyrics(ctx, measure, encMeas, measTick);

        adjustPickupMeasure(ctx, measure, measIdx);

        fillTrailingGaps(ctx, measure, measTick);

        for (int si = 0; si < ctx.totalStaves; ++si) {
            measure->checkMeasure(static_cast<staff_idx_t>(si));
        }

        correctMeasureLength(measure, ctx.totalStaves);
        capMeasureLength(measure, ctx.totalStaves);

        const EncMeasure* prevMeas = (measIdx > 0) ? &enc.measures[measIdx - 1] : nullptr;
        measSkip = measDisplayCount(encMeas, prevMeas) - 1;
        ++msIdxCounter;
        ++measIdx;
    }

    applyMeasureBpmMarks(ctx);

    // Dangling graces after the final measure have no score-tree parent; delete explicitly.
    for (auto& [key, vec] : ctx.pendingGraces) {
        for (Chord* gc : vec) {
            LOGW() << "Encore import: discarding dangling grace chord at end of score"
                   << " (staff " << key.first << ", voice " << key.second << ")";
            delete gc;
        }
    }
    ctx.pendingGraces.clear();
}
} // namespace mu::iex::enc
