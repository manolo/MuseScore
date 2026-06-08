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
bool NoteLoopMeasCtx::isTieStartAt(int si, int v, int tick) const
{
    for (int dt = 0; dt < CHORD_CLUSTER_THRESHOLD; ++dt) {
        if (tieStartSet.count({ si, v, tick - dt })) {
            return true;
        }
        if (dt > 0 && tieStartSet.count({ si, v, tick + dt })) {
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
        if (static_cast<int>(tt.currentTuplet->elements().size()) < tt.actualN) {
            track_idx_t trk = static_cast<track_idx_t>(trackKey.first) * VOICES
                              + trackKey.second;
            DurationType baseLen = tt.currentTuplet->baseLen().type();
            Fraction perNote = TDuration(baseLen).fraction()
                               * Fraction(tt.normalN, tt.actualN);
            int safety = tt.actualN + 1;
            while (tt.placedTicks < expectedTup && safety-- > 0
                   && static_cast<int>(tt.currentTuplet->elements().size()) < tt.actualN
                   && ctx.cumTick[trackKey] + perNote <= measure->ticks()) {
                Fraction restTick = measure->tick() + ctx.cumTick[trackKey];
                Segment* seg = measure->getSegment(SegmentType::ChordRest, restTick);
                if (seg->element(trk)) {
                    break;
                }
                TDuration dur(baseLen);
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

    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (mb->isMeasure()) {
            ctx.measuresByIdx.push_back(toMeasure(mb));
        }
    }

    // Slurs resolved after the pass: .enc has no SLURSTOP; end anchored at
    // last ChordRest in the alMezuro target measure (xoffset2 is layout, not tick).
    int measIdx = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
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
        ctx.cumTick.clear();
        ctx.prevMidiTick.clear();
        ctx.prevEncVoice.clear();
        ctx.lastChordPos.clear();
        ctx.graceStolenTicks.clear();
        ctx.streamOffset.clear();
        ctx.v0PitchesInMeasure.clear();

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
                    if (v >= static_cast<int>(VOICES)) {
                        v = 0;
                    } else if (e->staffWithin > 0) {
                        const int sw = static_cast<int>(e->staffWithin);
                        const int vBase = sw * (static_cast<int>(VOICES) / 2);
                        if (v >= vBase && si + sw < ctx.totalStaves) {
                            si += sw;
                            v  -= vBase;
                        }
                    }
                    mc.tieStartSet.insert({ si, v, (int)e->tick });
                }
            }
        }

        mc.validTupletGroupMember
            = computeImpliedTupletMembers(sortedElems, encMeas, ctx.totalStaves, &mc.partialEndGroup);

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
            } else if (e->staffWithin > 0) {
                const int sw    = static_cast<int>(e->staffWithin);
                const int vBase = sw * (static_cast<int>(VOICES) / 2);
                if (voice >= vBase && staffIdx < nLineStaves) {
                    const int instrIdx = lineStaffInstrIdx[staffIdx];
                    if (instrIdx >= 0
                        && instrIdx < static_cast<int>(enc.instruments.size())
                        && enc.instruments[instrIdx].nstaves > sw
                        && staffIdx + sw < ctx.totalStaves) {
                        staffIdx += sw;
                        voice    -= vBase;
                    }
                }
            }

            auto encVoiceKey = std::make_pair(staffIdx, voice);
            int msVoice = voice + ctx.streamOffset[encVoiceKey];
            if (msVoice >= static_cast<int>(VOICES)) {
                continue;  // all MuseScore voices used up
            }
            track_idx_t track = static_cast<track_idx_t>(staffIdx * VOICES + msVoice);
            auto trackKey = std::make_pair(staffIdx, msVoice);

            // faceValue-cumulative placement; near-simultaneous notes (< CHORD_MIDI_THRESHOLD) extend the chord.
            // Chord extension requires the same Encore voice; cross-voice spill must not extend.
            constexpr int CHORD_MIDI_THRESHOLD = 2 * CHORD_CLUSTER_THRESHOLD;  // = 8
            bool isNoteOrRest = (et == EncElemType::NOTE || et == EncElemType::REST);
            bool isChordExt   = isNoteOrRest && ctx.prevMidiTick.count(trackKey)
                                && ctx.prevEncVoice.count(trackKey)
                                && ctx.prevEncVoice.at(trackKey) == voice
                                && (int)e->tick - (int)ctx.prevMidiTick.at(trackKey) >= 0
                                && (int)e->tick - (int)ctx.prevMidiTick.at(trackKey)
                                < CHORD_MIDI_THRESHOLD;

            // Advance to next MuseScore voice when current is full (multi-stream overflow).
            bool dropNote = false;
            while (isNoteOrRest && !isChordExt && ctx.cumTick[trackKey] >= measure->ticks()) {
                int newOffset = ctx.streamOffset[encVoiceKey] + 1;
                if (voice + newOffset >= static_cast<int>(VOICES)) {
                    dropNote = true;  // all MuseScore voices full, skip
                    break;
                }
                ctx.streamOffset[encVoiceKey] = newOffset;
                msVoice  = voice + newOffset;
                track    = static_cast<track_idx_t>(staffIdx * VOICES + msVoice);
                trackKey = std::make_pair(staffIdx, msVoice);
                // isChordExt for the fresh voice: no ctx.prevMidiTick yet → always false
                isChordExt = false;
            }
            if (dropNote) {
                continue;
            }
            // Suppress overflow pitch if already in voice 0 (3-stream divergence dedup).
            if (msVoice > 0 && et == EncElemType::NOTE) {
                const EncNote* enChk = static_cast<const EncNote*>(e);
                const int pitchChk = static_cast<int>(enChk->semiTonePitch)
                                     + (staffIdx < static_cast<int>(ctx.staffPitchOffset.size())
                                        ? ctx.staffPitchOffset[staffIdx] : 0);
                if (ctx.v0PitchesInMeasure.count(staffIdx)
                    && ctx.v0PitchesInMeasure[staffIdx].count(pitchChk)) {
                    continue;
                }
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
                        // Also suppress when gap equals stolen grace ticks: the note after a grace group lands on the face grid by the grace amount and must not trigger a spurious rest.
                        const int stolenTicks = ctx.graceStolenTicks.count(trackKey)
                                                ? ctx.graceStolenTicks.at(trackKey) : 0;
                        if (onFaceGrid && !gracePending) {
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
                    Segment* seg = measure->getSegment(SegmentType::ChordRest, elemTick);
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
        const int encTicksPerQuarter = encMeas.beatTicks
                                       ? static_cast<int>(encMeas.beatTicks) : 240;
        const int matchThreshold = encTicksPerQuarter / 2;   // half-beat window
        for (auto& [lyTrack, entries] : ctx.pendingLyrics) {
            if (entries.empty()) {
                continue;
            }
            // Encore multi-verse: verse 1=voice 0, verse 2=voice 1. MuseScore anchors all verses to voice-0 chord via setVerse().
            const int lyStaffIdx = static_cast<int>(lyTrack) / VOICES;
            const int lyVerseNo = static_cast<int>(lyTrack) % VOICES;
            const track_idx_t chordTrack = static_cast<track_idx_t>(lyStaffIdx) * VOICES;
            std::vector<bool> consumed(entries.size(), false);
            for (Segment* s = measure->first(SegmentType::ChordRest);
                 s; s = s->next(SegmentType::ChordRest)) {
                EngravingItem* el = s->element(chordTrack);
                if (!el || !el->isChord()) {
                    continue;
                }
                // Convert segment tick (relative to measure start) to raw
                // Encore PPQ for comparison with the queued lyric ticks.
                const Fraction relTick = s->tick() - measure->tick();
                const int segEncTick = (relTick.numerator() * encTicksPerQuarter * 4)
                                       / std::max(1, relTick.denominator());
                int bestIdx = -1;
                int bestDelta = matchThreshold + 1;
                for (size_t i = 0; i < entries.size(); ++i) {
                    if (consumed[i]) {
                        continue;
                    }
                    const int delta = std::abs(entries[i].encTick - segEncTick);
                    if (delta < bestDelta) {
                        bestDelta = delta;
                        bestIdx = static_cast<int>(i);
                    }
                }
                if (bestIdx < 0) {
                    continue;
                }
                Chord* c = toChord(el);
                Lyrics* ly = Factory::createLyrics(c);
                ly->setTrack(chordTrack);
                ly->setVerse(lyVerseNo);
                ly->setXmlText(entries[bestIdx].text);
                LyricsSyllabic syll = LyricsSyllabic::SINGLE;
                if (entries[bestIdx].hyphenBefore && entries[bestIdx].hyphenAfter) {
                    syll = LyricsSyllabic::MIDDLE;
                } else if (entries[bestIdx].hyphenBefore) {
                    syll = LyricsSyllabic::END;
                } else if (entries[bestIdx].hyphenAfter) {
                    syll = LyricsSyllabic::BEGIN;
                }
                ly->setSyllabic(syll);
                c->add(ly);
                consumed[bestIdx] = true;
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

        ++measIdx;
    }

    // Apply per-measure BPM from MEAS header; emit TempoText only when BPM changes.
    // Skip when an ORN TEMPO or STAFFTEXT tempo is already present ANYWHERE in the measure,
    // not just at measTick. ORN TEMPO elements are placed at their note's tick (which may
    // differ from measTick when the measure starts with a rest), so the old per-segment
    // guard missed them and created a duplicate tempo mark.
    {
        quint16 lastBpm = 0;
        for (size_t mi = 0; mi < enc.measures.size() && mi < ctx.measuresByIdx.size(); ++mi) {
            const quint16 bpm = enc.measures[mi].bpm;
            if (bpm == 0) {
                continue;
            }
            if (mi > 0 && bpm == lastBpm) {
                continue;
            }
            Measure* m = ctx.measuresByIdx[mi];
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
