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
// Render tempo text for `quarterBpm` (quarter-note BPM). For compound meters
// (6/8, 9/8, 12/8) Encore displays the dotted-quarter BPM, so convert.
static String tempoXmlText(int quarterBpm, const Fraction& timeSig)
{
    bool compound = timeSig.denominator() == 8
                    && timeSig.numerator() % 3 == 0
                    && timeSig.numerator() > 3;
    if (compound) {
        int dottedBpm = (quarterBpm * 2 + 1) / 3;
        return String(u"♩. = %1").arg(dottedBpm);
    }
    return String(u"♩ = %1").arg(quarterBpm);
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

    // --------------- Notes, rests, ornaments, chord symbols ---------------
    // measuresByIdx: resolves hairpin/slur end measure from alMezuro offset.
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

        // Fill a partial tuplet with invisible rests so chord-sum reaches canonical
        // (baseLen * normalN). Without this, sanityCheck and beam layout assert.
        auto closeTupletWithFill = [&](TupletTracker& tt,
                                       std::pair<int, int> trackKey) {
            if (!tt.inTuplet() || tt.placedTicks <= Fraction(0, 1)) {
                tt.closeTuplet();
                return;
            }
            const Fraction expectedTup = TDuration(tt.currentTuplet->baseLen()).fraction()
                                         * tt.currentTuplet->ratio().denominator();
            TDuration snap(tt.placedTicks, true /*truncate*/);
            const bool fitsTD = snap.isValid()
                                && snap.fraction() == tt.placedTicks;
            if (tt.placedTicks < expectedTup && !fitsTD) {
                // Strictly partial: fewer than actualN notes placed; fill with invisible rests.
                // Mixed-value: all actualN placed but face sum < fullFaceSum; leave the gap.
                if (static_cast<int>(tt.currentTuplet->elements().size())
                    < tt.actualN) {
                    track_idx_t trk = static_cast<track_idx_t>(trackKey.first) * VOICES
                                      + trackKey.second;
                    DurationType baseLen = tt.currentTuplet->baseLen().type();
                    Fraction perNote = TDuration(baseLen).fraction()
                                       * Fraction(tt.normalN, tt.actualN);
                    int safety = tt.actualN + 1;
                    while (tt.placedTicks < expectedTup && safety-- > 0
                           && static_cast<int>(tt.currentTuplet->elements().size())
                           < tt.actualN
                           && cumTick[trackKey] + perNote <= measure->ticks()) {
                        Fraction restTick = measure->tick() + cumTick[trackKey];
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
                        cumTick[trackKey] += perNote;
                    }
                }
            }
            tt.closeTuplet();
        };

        // Reset per-measure state.
        for (auto& [key, tt] : tuplets) {
            if (tt.inTuplet()) {
                tt.closeTuplet();
            }
        }
        tuplets.clear();
        cumTick.clear();
        prevMidiTick.clear();
        prevEncVoice.clear();
        lastChordPos.clear();
        v0xA6GraceStolenTicks.clear();
        streamOffset.clear();
        v0PitchesInMeasure.clear();

        // Delete unattached grace chords explicitly: not in score tree, so no auto-cleanup.
        for (auto& [key, vec] : pendingGraces) {
            for (Chord* gc : vec) {
                LOGW() << "Encore import: discarding dangling grace chord at measure " << measIdx
                       << " (staff " << key.first << ", voice " << key.second << ")";
                delete gc;
            }
        }
        pendingGraces.clear();

        // Repeat navigation marks
        EncRepeatType rt = encMeas.repeatMark();
        if (rt != EncRepeatType::NONE) {
            addRepeatMark(score, measure, rt);
        }

        // Volta: equal-bitmask consecutive measures coalesce into one spanning Volta.
        if (encMeas.repeatAlternative != 0) {
            if (activeVolta && activeVoltaBits == encMeas.repeatAlternative) {
                activeVolta->setTick2(measTick + measure->ticks());
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
                activeVolta = volta;
                activeVoltaBits = encMeas.repeatAlternative;
            }
        } else {
            activeVolta = nullptr;
            activeVoltaBits = 0;
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
            // Among non-tuplet same-tick notes: shortest faceValue first so the
            // chord root drives cumTick by the minimum step. When the same tick
            // carries a quarter and an eighth (common in guitar rhythmic patterns),
            // putting the eighth first ensures the 16th subdivisions that follow
            // can be placed at their correct positions.
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

        // Collect TIE-START positions (isTieStart==true). Arc-only markers (incoming
        // endpoint) are excluded; including them would falsely mark notes as tie-senders.
        std::set<std::tuple<int, int, int> > tieStartSet;
        for (const EncMeasureElem* e : sortedElems) {
            if (static_cast<EncElemType>(e->type) == EncElemType::TIE) {
                const EncTie* et = static_cast<const EncTie*>(e);
                if (et->isTieStart) {
                    tieStartSet.insert({ (int)e->staffIdx, (int)e->voice, (int)e->tick });
                }
            }
        }
        // Match within CHORD_CLUSTER_THRESHOLD; strict < so TIE@476 does not match tick=480.
        auto isTieStart = [&](int si, int v, int tick) -> bool {
            for (int dt = 0; dt < CHORD_CLUSTER_THRESHOLD; ++dt) {
                if (tieStartSet.count({ si, v, tick - dt })) {
                    return true;
                }
                if (dt > 0 && tieStartSet.count({ si, v, tick + dt })) {
                    return true;
                }
            }
            return false;
        };

        // Pre-compute which elements belong to COMPLETE tuplet groups.
        // - Explicit tuplets (all formats): groups of exactly actualN consecutive notes
        //   with the same standard tup byte.  Isolated tail notes (e.g. note 4 after a
        //   3:2 triplet group) are excluded to avoid partial-tuplet checkMeasure issues.
        // - Implied tuplets (v0xC2 only): groups of exactly actualN notes with matching
        //   detectImpliedTuplet ratio; isolated swing-timing notes are excluded.
        // Both sets are merged into validTupletGroupMember.
        // partialEndGroup tracks elements added by the measure-end partial-group exception
        // only (groups whose rdur sum fills the measure but face values would overflow);
        // Fix 2 and Fix 3 apply only to these notes, not to complete groups.
        std::set<const EncMeasureElem*> partialEndGroup;
        std::set<const EncMeasureElem*> validTupletGroupMember
            = computeImpliedTupletMembers(sortedElems, encMeas, ctx.totalStaves, &partialEndGroup);
        // Alias for implied-tuplet checks (v0xC2 guard applied in the element loop).
        const auto& impliedGroupMember = validTupletGroupMember;

        // Raw Encore ticks that have at least one NOTE (any voice) in this measure.
        // BOWING ORNs at a tick absent from this set have no corresponding note and
        // belong to the first chord of the next measure on the sibling staff.
        std::set<int> noteTicks;
        // Grand-staff fingering routing uses finer per-voice counts.
        // voice4NoteTicks: ticks with a voice>=VOICES (2nd staff) note.
        // v0NoteCountAtTick: number of voice=0 notes per raw tick.
        // ornFingCountAtTick: number of FINGER_1..5 ORNs per raw tick.
        // maxVoice0Tick: last raw tick carrying a voice=0 note.
        std::set<int> voice4NoteTicks;
        std::map<int, int> v0NoteCountAtTick;
        std::map<int, int> ornFingCountAtTick;
        int maxVoice0Tick = -1;
        for (const EncMeasureElem* em : sortedElems) {
            const EncElemType et2 = static_cast<EncElemType>(em->type);
            if (et2 == EncElemType::NOTE) {
                noteTicks.insert(static_cast<int>(em->tick));
                if (em->voice >= static_cast<int>(VOICES)) {
                    voice4NoteTicks.insert(static_cast<int>(em->tick));
                } else {
                    v0NoteCountAtTick[static_cast<int>(em->tick)]++;
                    maxVoice0Tick = std::max(maxVoice0Tick, static_cast<int>(em->tick));
                }
            } else if (et2 == EncElemType::ORNAMENT) {
                const EncOrnament* eo2 = static_cast<const EncOrnament*>(em);
                const EncOrnamentType ot2 = eo2->ornType();
                if (ot2 >= EncOrnamentType::FINGER_1 && ot2 <= EncOrnamentType::FINGER_5) {
                    ornFingCountAtTick[static_cast<int>(em->tick)]++;
                }
            }
        }

        // Tracks filtered MIDI artifact notes that are tie-senders (grace1 low nibble == 1).
        // When such a note is filtered by the rdur<15 check, its continuation note (same
        // pitch, same voice, grace1 low == 2) should also be filtered — Encore hides both.
        std::set<std::tuple<int, int, int> > filteredTieSenderPitches;

        for (const EncMeasureElem* e : sortedElems) {
            EncElemType et = static_cast<EncElemType>(e->type);

            // Skip notes/rests strictly at or beyond measure end (they would
            // write past the bar). Ornaments at `tick == durTicks` mark the
            // end-of-measure boundary (e.g. a WEDGESTART that visually starts
            // on the bar line); they belong to the current measure and must
            // not be discarded, otherwise hairpins like the f -> p diminuendo
            // that spans m1 -> m3 in Beethoven Plectro disappear silently.
            if ((et == EncElemType::NOTE || et == EncElemType::REST)
                && e->tick >= encMeas.durTicks) {
                continue;
            }
            if (et == EncElemType::ORNAMENT && e->tick > encMeas.durTicks) {
                // Dynamics and STAFFTEXT ornaments stored at a tick past the
                // measure end are Encore's "end-of-measure" markers (observed
                // in sirena.enc m21: a 2/4 measure with the 1st-volta "pp"
                // dynamic + "la 2ª" stafftext at tick=960 instead of 480).
                // Let them through; the per-case clamp later attaches them
                // to the last existing segment of the current measure.
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
            // voice=4 is Encore's out-of-band slot for system ornaments and,
            // in single-instrument grand-staff scores, for the second staff's
            // notes (all elements share staffIdx=0, second staff uses voice=4).
            // Detect the multi-staff case via LINE data and advance staffIdx;
            // otherwise clamp to voice 0 of the same staff (choral/ornament case).
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
            }

            // Determine which MuseScore voice to use for this Encore (staffIdx, voice) slot.
            // streamOffset starts at 0 and increments when a recording stream overflows
            // the current MuseScore voice (see "multi-stream voice assignment" below).
            auto encVoiceKey = std::make_pair(staffIdx, voice);
            int msVoice = voice + streamOffset[encVoiceKey];
            if (msVoice >= static_cast<int>(VOICES)) {
                continue;  // all MuseScore voices used up
            }
            track_idx_t track = static_cast<track_idx_t>(staffIdx * VOICES + msVoice);
            // trackKey keys all per-voice state (cumTick, prevMidiTick, lastChordPos, tuplets).
            auto trackKey = std::make_pair(staffIdx, msVoice);

            // faceValue-cumulative placement: compute MuseScore position from accumulated
            // written durations, not from the MIDI tick position.
            // Same MIDI tick (or near-simultaneous within CHORD_MIDI_THRESHOLD) in the
            // same MuseScore voice = chord extension (same cumulative position).
            constexpr int CHORD_MIDI_THRESHOLD = 2 * CHORD_CLUSTER_THRESHOLD;  // = 8
            bool isNoteOrRest = (et == EncElemType::NOTE || et == EncElemType::REST);
            // Only treat as a chord extension if the previous note at this trackKey
            // came from the SAME Encore voice. Otherwise a multi-stream spill from
            // another encVoice can falsely chord-extend this note's encVoice.
            bool isChordExt   = isNoteOrRest && prevMidiTick.count(trackKey)
                                && prevEncVoice.count(trackKey)
                                && prevEncVoice.at(trackKey) == voice
                                && (int)e->tick - (int)prevMidiTick.at(trackKey) >= 0
                                && (int)e->tick - (int)prevMidiTick.at(trackKey)
                                < CHORD_MIDI_THRESHOLD;

            // Multi-stream voice assignment (Option B):
            // When a non-chord note arrives and the current MuseScore voice is already full,
            // this note belongs to a second (or third…) simultaneous recording stream encoded
            // in the same Encore voice slot.  Assign it to the next MuseScore voice.
            // Multi-stream voice assignment loop: keep switching voices until we find
            // one with remaining space or exhaust all VOICES. A single switch is not
            // enough because the target voice may also be full (e.g. a prior rest filled
            // it). Without the loop, a note can be placed in a full voice and overflow
            // the measure.
            bool dropNote = false;
            while (isNoteOrRest && !isChordExt && cumTick[trackKey] >= measure->ticks()) {
                int newOffset = streamOffset[encVoiceKey] + 1;
                if (voice + newOffset >= static_cast<int>(VOICES)) {
                    dropNote = true;  // all MuseScore voices full, skip
                    break;
                }
                streamOffset[encVoiceKey] = newOffset;
                msVoice  = voice + newOffset;
                track    = static_cast<track_idx_t>(staffIdx * VOICES + msVoice);
                trackKey = std::make_pair(staffIdx, msVoice);
                // isChordExt for the fresh voice: no prevMidiTick yet → always false
                isChordExt = false;
            }
            if (dropNote) {
                continue;
            }
            // Suppress stream-duplicate overflow notes: any note routed to voice 1+
            // whose pitch already exists in voice 0 for this staff/measure is a
            // recording-stream artifact. Encore records 3 MIDI streams per instrument
            // into the same voice; when one stream diverges in timing beyond
            // CHORD_MIDI_THRESHOLD it is treated as a new note rather than a chord
            // extension, fills cumTick, and triggers an overflow. Since Encore has no
            // real secondary voices, the overflow note is always a duplicate.
            if (msVoice > 0 && et == EncElemType::NOTE) {
                const EncNote* enChk = static_cast<const EncNote*>(e);
                const int pitchChk = static_cast<int>(enChk->semiTonePitch)
                                     + (staffIdx < static_cast<int>(ctx.staffPitchOffset.size())
                                        ? ctx.staffPitchOffset[staffIdx] : 0);
                if (v0PitchesInMeasure.count(staffIdx)
                    && v0PitchesInMeasure[staffIdx].count(pitchChk)) {
                    continue;
                }
            }

            // Save prevMidiTick/lastChordPos so we can restore them if we later decide
            // to skip the note (implied tuplet group that doesn't fit in remaining
            // measure space) or route it to the grace queue (graces must not look like
            // a chord root to the next note's chord-extension check).
            const int savedPrevMidiTick = prevMidiTick.count(trackKey)
                                          ? prevMidiTick.at(trackKey) : -1;
            const bool hadLastChordPos = lastChordPos.count(trackKey);
            const Fraction savedLastChordPos = hadLastChordPos
                                               ? lastChordPos.at(trackKey) : Fraction(-1, 1);

            Fraction elemTick;
            {
                if (isChordExt) {
                    elemTick = lastChordPos.count(trackKey) ? lastChordPos.at(trackKey)
                               : measTick;
                } else {
                    // Honor implicit gaps encoded by Encore's absolute tick: when
                    // the binary places a NOTE/REST significantly later than the
                    // cumulative sum of face-value advances AND the tick lands on
                    // the user's notation grid (a multiple of the element's face
                    // value in Encore ticks), snap cumTick to the Encore tick.
                    //
                    // Encore does NOT always emit explicit REST elements for
                    // leading or interior silences (e.g. a 3/4 measure encoded
                    // as two NOTE elements at ticks 240 and 480, with the
                    // leading quarter rest implicit). Without the snap, those
                    // notes get squashed to beats 1-2 instead of 2-3 and the
                    // trailing rest moves to the end, altering the timing.
                    //
                    // The face-grid gate avoids interfering with live-recorded
                    // files where every NOTE drifts by a few ticks from its
                    // notated position and the cumTick approach is what aligns
                    // the playback grid. Live recordings carry sub-grid ticks
                    // like 41, 105, 199 that never match `tick % faceTicks ==
                    // 0`, so the snap stays off and the cumulative-face
                    // placement keeps the multi-stream and implied-tuplet
                    // bookkeeping intact.
                    //
                    // The 8-Encore-tick threshold (= CHORD_MIDI_THRESHOLD)
                    // ignores micro-drift inside a chord cluster while still
                    // catching any intentional silence: the smallest non-
                    // degenerate face-value advance is the 64th (15 Encore
                    // ticks), so a real grid-aligned silence is always strictly
                    // above the threshold.
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
                        // Suppress the gap-snap when a grace note is queued.
                        // In v0xA6, grace notes occupy real tick positions that
                        // push subsequent notes forward; their ticks often land
                        // on the face grid and would otherwise trigger a gap rest
                        // equal to the grace's face value. The grace is visual
                        // only -- no real time passes -- so cumTick should NOT
                        // advance here.
                        const bool gracePending = !pendingGraces[trackKey].empty();
                        // Also suppress when the gap matches stolen grace ticks:
                        // after a grace group flushes onto the first regular note,
                        // the NEXT regular note may be on the face grid with a gap
                        // equal to the grace duration. Without this, the snap
                        // creates a spurious rest = grace face value (boda.enc m87:
                        // 32nd grace steals 30 ticks, note4 (16th at tick=180) is
                        // on the 30-tick grid, snap fires → Rest 32nd inserted).
                        const int stolenTicks = v0xA6GraceStolenTicks.count(trackKey)
                                                ? v0xA6GraceStolenTicks.at(trackKey) : 0;
                        if (onFaceGrid && !gracePending) {
                            // wholeTicks = ticks per whole note. Encore stores
                            // beatTicks per time-signature beat (= 240 for x/4,
                            // 120 for x/8, etc.), so wholeTicks = beatTicks *
                            // timeSigDen. Using `4 * beatTicks` only works when
                            // the denominator is 4; in x/8 it returned half the
                            // correct value and the snap pushed cumTick well
                            // beyond the measure end (regression on v0xA6 3/8
                            // and 6/8 scores). Fall back to 960 (= 240 * 4)
                            // when the header carries no time signature.
                            const int wholeTicks
                                = (encMeas.beatTicks && encMeas.timeSigDen)
                                  ? encMeas.beatTicks * encMeas.timeSigDen
                                  : 960;
                            const Fraction encTickFrac((int)e->tick, wholeTicks);
                            if (encTickFrac > cumTick[trackKey]) {
                                const Fraction gap = encTickFrac - cumTick[trackKey];
                                const int gapEncTicks
                                    = (gap.numerator() * wholeTicks)
                                      / std::max(1, gap.denominator());
                                // Suppress if the gap matches recently stolen grace ticks.
                                // Keep stolenTicks for ALL subsequent notes (don't erase)
                                // so each note displaced by the grace gets the suppression.
                                // The remaining time deficit shows as a trailing rest at
                                // measure end (less disruptive than an inter-note rest).
                                const bool gapIsGraceArtifact
                                    = (stolenTicks > 0 && gapEncTicks <= stolenTicks);
                                if (gapEncTicks > CHORD_MIDI_THRESHOLD && !gapIsGraceArtifact
                                    && encTickFrac < measure->ticks()) {
                                    cumTick[trackKey] = encTickFrac;
                                }
                            }
                        }
                    }
                    elemTick = measTick + cumTick[trackKey];
                    if (isNoteOrRest) {
                        lastChordPos[trackKey] = elemTick;
                    }
                    // Only notes establish a chord-extension anchor. A rest is a
                    // separator: a following note at the same MIDI tick is a fresh
                    // cluster, not an extension of the rest (the rest's segment
                    // would otherwise be silently replaced and the rest's cumTick
                    // contribution double-counted).
                    if (et == EncElemType::NOTE) {
                        prevMidiTick[trackKey] = e->tick;
                        prevEncVoice[trackKey] = voice;
                    }
                }
            }

            // -- Notes --
            auto handleNote = [&]() {
                const EncNote* en = static_cast<const EncNote*>(e);

                // Grace-note short-circuit. A grace chord must be parented under its
                // main Chord (not a Segment) or Chord::pagePos crashes via
                // toChord(explicitParent()) during beam layout. We build a detached
                // chord, queue it, and attach it later when the next non-grace chord
                // on this track is created.
                //
                // Grace classification: faceValue >= 4 (eighth or shorter) AND grace
                // bytes set. faceValue < 4 with grace bytes is a known Encore quirk
                // and must be treated as a normal note.
                {
                    // Grace requires eighth-or-shorter (low nibble >= 4) in addition
                    // to the standard face-value range check.
                    // v0xA6 inner-grace detection: after a leading grace
                    // (g1=0x20 = APPOGGIATURA) Encore places additional
                    // inner grace notes marked with g1=0x10. Inner graces
                    // are shorter (higher fv number) than the leading grace.
                    // Regular notes following the group are longer (lower fv)
                    // and must NOT be classified as graces (boda.enc: m57
                    // has a 64th inner grace after a 32nd leading grace, while
                    // m75 has regular 16ths after a 32nd leading grace).
                    const bool isV0xA6InnerGrace
                        = (en->size == 10)
                          && ((en->grace1 & 0x30) == 0x10)
                          && v0xA6LeadingGraceFv.count(trackKey)
                          && ((en->faceValue & 0x0F) > v0xA6LeadingGraceFv.at(trackKey));
                    if (isValidFaceValue(en->faceValue) && (en->faceValue & 0x0F) >= 4
                        && (en->graceType() != EncGraceType::NORMAL || isV0xA6InnerGrace)) {
                        // Roll back per-track tick state so the next note is not
                        // detected as a chord extension of this grace.
                        if (savedPrevMidiTick >= 0) {
                            prevMidiTick[trackKey] = savedPrevMidiTick;
                        } else {
                            prevMidiTick.erase(trackKey);
                        }
                        if (hadLastChordPos) {
                            lastChordPos[trackKey] = savedLastChordPos;
                        } else {
                            lastChordPos.erase(trackKey);
                        }

                        // Build the detached grace chord with its duration and pitch.
                        DurationType graceDt = realDuration2DurationType(en->realDuration, en->faceValue);
                        Chord* gc = Factory::createChord(score->dummy()->segment());
                        gc->setTrack(track);
                        TDuration gdur(graceDt);
                        gc->setDurationType(gdur);
                        gc->setTicks(gdur.fraction());
                        gc->setDots(0);
                        gc->setNoteType(en->graceType() == EncGraceType::ACCIACCATURA
                                        ? NoteType::ACCIACCATURA : NoteType::APPOGGIATURA);

                        Note* gnote = Factory::createNote(gc);
                        applyConcertPitch(gnote, en->semiTonePitch + ctx.staffPitchOffset[staffIdx]);
                        gc->add(gnote);

                        // Articulation on a grace is rare but Encore can encode it.
                        for (quint8 ab : { en->articulationUp, en->articulationDown }) {
                            for (SymId sid : encArticulation2SymIds(ab)) {
                                if (sid == SymId::noSym) {
                                    continue;
                                }
                                Articulation* art = Factory::createArticulation(gc);
                                art->setSymId(sid);
                                gc->add(art);
                            }
                        }

                        pendingGraces[trackKey].push_back(gc);
                        // Track the leading grace fv for inner-grace detection.
                        const quint8 curFv = en->faceValue & 0x0F;
                        auto it = v0xA6LeadingGraceFv.find(trackKey);
                        if (it == v0xA6LeadingGraceFv.end()) {
                            v0xA6LeadingGraceFv[trackKey] = curFv;
                        } else {
                            it->second = std::max(it->second, curFv);
                        }
                        // Accumulate stolen ticks for the post-flush snap guard.
                        v0xA6GraceStolenTicks[trackKey] += faceValue2ticks(curFv);
                        return;
                    }
                }

                if (!isValidFaceValue(en->faceValue)) {
                    return;
                }
                const quint8 safeFv = en->faceValue & 0x0F;
                // Skip MIDI tie-continuation artifacts (very short realDuration < 15 ticks).
                // Exceptions for 64th/128th notes (fvBase ≤ 15):
                //   (a) Tie-start notes (TIE element with 0xfe direction at this tick):
                //       real short notes that SEND ties — must be placed.
                //   (b) Chord-extension notes (within CHORD_MIDI_THRESHOLD of the previous
                //       note in the same voice): e.g. E@476 is a chord extension of C@473
                //       even though TIE@476 is arc-only (0x02).
                // Tie artifacts have neither: they don't have TIE-START elements and are
                // not chord extensions of a recently placed note.
                // NOTE: use isChordExt (computed from the OLD prevMidiTick, before the
                // update at line ~1889) rather than recomputing here.  After the update,
                // prevMidiTick equals e->tick, so a fresh computation always yields delta=0
                // and would incorrectly bypass the filter for non-chord-extension notes.
                if (en->realDuration > 0 && en->realDuration < 15) {
                    int fvBase = faceValue2ticks(safeFv);
                    if (fvBase <= 15) {
                        bool bypass = isTieStart(staffIdx, voice, (int)e->tick)
                                      || isChordExt;
                        if (!bypass) {
                            // Record tie-senders so their continuation note is also filtered.
                            if ((en->grace1 & 0x0F) == 1) {
                                filteredTieSenderPitches.insert(
                                    { staffIdx, voice, (int)en->semiTonePitch });
                            }
                            return;
                        }
                    } else {
                        // For 8th/16th/32nd+ notes: filter only if NOT at the chord-cluster
                        // boundary.  When realDuration == CHORD_CLUSTER_THRESHOLD,
                        // calculateRealDurations found the next non-clustered note exactly at
                        // the boundary (4 ticks away), which still indicates a live-recorded
                        // chord root whose cluster partner fell just outside the ±3-tick window.
                        // Keeping such notes ensures the chord root is placed so subsequent
                        // chord-extension notes (isChordExt) can join it correctly.
                        if (en->realDuration > CHORD_CLUSTER_THRESHOLD) {
                            return;
                        }
                    }
                }

                // Cascade-filter: if this note is a tie-receiver (grace1 low == 2) whose
                // sender was a filtered MIDI artifact, filter this note too.  Encore hides
                // both the artifact and its dotted-note continuation from display.
                if ((en->grace1 & 0x0F) == 2) {
                    auto cascKey = std::make_tuple(staffIdx, voice, (int)en->semiTonePitch);
                    if (filteredTieSenderPitches.count(cascKey)) {
                        filteredTieSenderPitches.erase(cascKey);
                        return;
                    }
                }

                // Determine if this note has an explicit standard tuplet byte.
                // This check is done before computing dt because explicit tuplet notes
                // use faceValue directly (rdur is MIDI timing and may be wrong when the
                // next MIDI event starts before the note's written duration ends).
                {
                    int preA = en->actualNotes(), preN = en->normalNotes();
                    bool stdE = (preA == 3 && preN == 2) || (preA == 5 && preN == 4) || (preA == 6 && preN == 4);
                    if (!stdE) {
                        preA = 0;
                        preN = 0;
                    }
                    if (preA == 0 && enc.header.isOldFormat() && (en->faceValue & 0x0F) >= 4
                        && (tuplets[trackKey].inTuplet() || impliedGroupMember.count(e))) {
                        preA = detectImpliedTuplet(en->realDuration, en->faceValue, preN);
                    }
                    // Store back for use below (only the stdE flag is needed here)
                    (void)preA;
                    (void)preN;
                }
                int preACheck = en->actualNotes(), preNCheck = en->normalNotes();
                bool isStandardExplicit = (preACheck == 3 && preNCheck == 2)
                                          || (preACheck == 5 && preNCheck == 4)
                                          || (preACheck == 6 && preNCheck == 4);

                // Duration type:
                // - Explicit tuplet notes: use faceValue directly (rdur = MIDI timing,
                //   can be truncated by a following event and give wrong dt).
                // - All other notes: use realDuration for whole-rest-in-partial-measure
                //   mapping and to handle MIDI timing drift gracefully.
                DurationType dt;
                int dots;
                if (isStandardExplicit) {
                    dt   = faceValue2DurationType(en->faceValue);
                    dots = 0;
                    // For partial measure-end groups only: reduce dt if the tuplet-scaled
                    // advance would overshoot the remaining measure space (e.g. last slot
                    // of a mixed-value bracket where the face value is one step too long).
                    // Not applied to complete groups that happen to start late — those use
                    // the existing capped-note-removed-from-tuplet path instead.
                    if (partialEndGroup.count(e)) {
                        const auto& ttX = tuplets[trackKey];
                        if (ttX.inTuplet() && dt != DurationType::V_INVALID) {
                            Fraction adv = TDuration(dt).fraction()
                                           * Fraction(ttX.normalN, ttX.actualN);
                            Fraction rem = measure->ticks() - cumTick[trackKey];
                            while (adv > rem && rem > Fraction(0, 1)
                                   && dt < DurationType::V_128TH) {
                                dt  = static_cast<DurationType>(static_cast<int>(dt) + 1);
                                adv = TDuration(dt).fraction()
                                      * Fraction(ttX.normalN, ttX.actualN);
                            }
                        }
                    }
                } else {
                    dt   = realDuration2DurationType(en->realDuration, en->faceValue);
                    // Use dotControl (actual sounding duration stored by Encore) for dot
                    // count.  dotControl stores the Encore sounding duration; realDuration
                    // comes from MIDI tick spacing and may have small timing drift (±2 ticks).
                    // Strategy: use dotControl when it identifies a dotted value (>0 dots);
                    // otherwise snap realDuration with tolerance for MIDI drift.
                    if (en->dotControl > 0) {
                        int dByCtrl = calcDots(static_cast<qint16>(en->dotControl),
                                               en->faceValue);
                        if (dByCtrl > 0) {
                            dots = dByCtrl; // dotControl gives a clear dotted value
                        } else {
                            // dotControl didn't identify dots; try snapping realDuration
                            dots = calcDotsSnap(en->realDuration, en->faceValue);
                        }
                    } else {
                        dots = calcDotsSnap(en->realDuration, en->faceValue);
                    }
                }
                // Save the face-value dt before any capping modifies it.
                // Used below to check whether an isolated explicit note exactly fills
                // the remaining measure space as a partial tuplet.
                const DurationType dtFace = dt;

                // For non-tuplet notes, cap the chord duration to remaining measure space.
                {
                    const auto& ttPre = tuplets[trackKey];
                    int preA = isStandardExplicit ? preACheck : 0;
                    int preN = isStandardExplicit ? preNCheck : 0;
                    if (!isStandardExplicit) {
                        if (enc.header.isOldFormat() && (en->faceValue & 0x0F) >= 4
                            && ((ttPre.inTuplet() && !ttPre.groupFull()) || impliedGroupMember.count(e))) {
                            preA = detectImpliedTuplet(en->realDuration, en->faceValue, preN);
                        }
                    }

                    // Partial implied-tuplet-group guard:
                    // If starting a NEW implied tuplet group whose full advance wouldn't fit
                    // in the current MuseScore voice's remaining space, SKIP this note
                    // entirely.  Placing only part of a triplet group (e.g. 2 out of 3)
                    // leaves a remainder (e.g. 1/3072) that is mathematically inexpressible
                    // as standard note durations and causes "Incomplete measure" on reload.
                    //
                    // By skipping and restoring prevMidiTick, the NEXT element no longer
                    // sees this note as a chord root, so it starts fresh in the same voice
                    // and fills the remaining space cleanly with standard (non-tuplet) notes
                    // from the other interleaved recording streams.
                    if (!isStandardExplicit && !ttPre.inTuplet()
                        && !isChordExt && preA > 0 && preN > 0) {
                        Fraction singleAdv = TDuration(faceValue2DurationType(en->faceValue & 0x0F)).fraction()
                                             * Fraction(preN, preA);
                        Fraction fullGroupAdv = singleAdv * Fraction(preA, 1);
                        Fraction mRemaining = measure->ticks() - cumTick[trackKey];
                        if (fullGroupAdv > mRemaining) {
                            // Restore prevMidiTick so the next element is not detected as a
                            // chord extension of this skipped note.
                            if (savedPrevMidiTick >= 0) {
                                prevMidiTick[trackKey] = savedPrevMidiTick;
                            } else {
                                prevMidiTick.erase(trackKey);
                            }
                            return; // Skip this note; don't place, don't advance cumTick
                        }
                    }

                    // willBeTuplet: true only when the note WILL actually be placed in a
                    // tuplet group. A groupFull tuplet will be closed before this note is
                    // added, so it does NOT count as "in tuplet" for capping purposes.
                    // For explicit-but-not-validated notes: they are treated as plain.
                    bool willBeExplicit = isStandardExplicit && validTupletGroupMember.count(e);
                    bool willBeTuplet = (preA > 0 && preN > 0 && (willBeExplicit || !isStandardExplicit))
                                        || (ttPre.inTuplet() && !ttPre.groupFull());
                    if (!willBeTuplet) {
                        Fraction remaining = measure->ticks() - cumTick[trackKey];
                        // Include dots in the comparison: TDuration(dt) alone gives the
                        // undotted fraction, missing 1/2 or 3/4 of the actual note length.
                        TDuration fullDur(dt);
                        fullDur.setDots(dots);
                        if (remaining > Fraction(0, 1) && fullDur.fraction() > remaining) {
                            TDuration capped(remaining, true);
                            // If remaining is so small that TDuration cannot represent any
                            // standard duration (e.g. residual 1/3072 left by an earlier
                            // tuplet/cap mismatch), placing the note would create a
                            // zero-tick chord that breaks sanityCheck and later layout.
                            // Skip the note instead.
                            if (capped.fraction().numerator() == 0) {
                                if (savedPrevMidiTick >= 0) {
                                    prevMidiTick[trackKey] = savedPrevMidiTick;
                                } else {
                                    prevMidiTick.erase(trackKey);
                                }
                                return;
                            }
                            dt   = capped.type();
                            dots = capped.dots();
                        }
                    }
                }

                Segment* seg = measure->getSegment(SegmentType::ChordRest, elemTick);
                Chord* chord = nullptr;
                if (seg->element(track) && seg->element(track)->isChord()) {
                    chord = toChord(seg->element(track));
                } else {
                    chord = Factory::createChord(seg);
                    chord->setTrack(track);
                    TDuration dur(dt);
                    dur.setDots(dots);
                    chord->setDurationType(dur);
                    chord->setTicks(dur.fraction());
                    chord->setDots(dots);
                    seg->add(chord);

                    // Tuplet handling.
                    auto& tt = tuplets[trackKey];
                    int actualN = isStandardExplicit ? preACheck : 0;
                    int normalN = isStandardExplicit ? preNCheck : 0;
                    // Implied tuplet detection (v0xC2 only, pre-validated groups).
                    // Use !tt.groupFull() so that a note arriving just as the previous
                    // implied group completes does NOT start a new unvalidated group via
                    // the tt.inTuplet() path (the full group will be closed immediately).
                    if (actualN == 0 && enc.header.isOldFormat() && (en->faceValue & 0x0F) >= 4
                        && ((tt.inTuplet() && !tt.groupFull()) || impliedGroupMember.count(e))) {
                        actualN = detectImpliedTuplet(en->realDuration, en->faceValue, normalN);
                    }

                    if (actualN > 0 && normalN > 0) {
                        // Close a completed group before starting a new one.
                        // For explicit tuplets: only start a new group when this element
                        // was pre-validated as part of a complete run.  Isolated notes
                        // that can't form a full group (e.g. note 4 after a completed
                        // 3:2 triplet) are treated as plain non-tuplet notes to avoid
                        // partial-tuplet checkMeasure overshoot.
                        if (tt.groupFull()) {
                            closeTupletWithFill(tt, trackKey);
                        }
                        if (!tt.inTuplet()) {
                            if (isStandardExplicit && !validTupletGroupMember.count(e)) {
                                // Isolated explicit note: not part of a pre-validated group.
                                // Special case: if its FACE-VALUE tuplet advance exactly fills
                                // the remaining measure space, create a partial tuplet so that
                                // checkMeasure sees the correct span and inserts no fills.
                                // Use dtFace (before any capping) for the advance check.
                                Fraction tupAdv = TDuration(dtFace).fraction()
                                                  * Fraction(normalN, actualN);
                                Fraction remaining = measure->ticks() - cumTick[trackKey];
                                if (tupAdv == remaining) {
                                    dt   = dtFace;// restore face value (undo capping)
                                    dots = 0;
                                    // Update the chord's duration to face value
                                    TDuration faceD(dtFace);
                                    chord->setDurationType(faceD);
                                    chord->setTicks(faceD.fraction());
                                    chord->setDots(0);
                                    tt.startTuplet(measure, elemTick, actualN, normalN, dt, track);
                                } else {
                                    actualN = 0;
                                    normalN = 0;           // treat as plain note
                                }
                            } else {
                                // For partial measure-end groups only: derive baseLen from
                                // remaining/normalN when the full normalN-unit span at face
                                // value would exceed the remaining measure space, so the bracket
                                // spans exactly what fits (e.g. 1/8 remaining / normalN=2 = 1/16).
                                // Not applied to complete groups — they use the existing baseLen.
                                DurationType baseLenDt = dt;
                                if (partialEndGroup.count(e)) {
                                    Fraction rem3 = measure->ticks() - cumTick[trackKey];
                                    Fraction fullAdv = TDuration(dt).fraction() * Fraction(normalN, 1);
                                    if (fullAdv > rem3 && rem3 > Fraction(0, 1)) {
                                        Fraction baseFrac = Fraction(rem3.numerator(),
                                                                     rem3.denominator() * normalN).reduced();
                                        TDuration baseLenDur(baseFrac, true /*truncate*/);
                                        if (baseLenDur.isValid() && baseLenDur.fraction() == baseFrac) {
                                            baseLenDt = baseLenDur.type();
                                        }
                                    }
                                }
                                tt.startTuplet(measure, elemTick, actualN, normalN, baseLenDt, track);
                            }
                        }
                    }
                    if (actualN > 0 && normalN > 0) {
                        chord->setTuplet(tt.currentTuplet);
                        tt.currentTuplet->add(chord);

                        tt.faceTicks += TDuration(dt).fraction();
                    } else {
                        if (tt.groupFull()) {
                            closeTupletWithFill(tt, trackKey);
                        }
                        if (tt.inTuplet()) {
                            closeTupletWithFill(tt, trackKey); // non-tuplet note exits group
                        }
                    }

                    // Advance cumulative position by the written duration.
                    // Grace notes are routed via the grace short-circuit above and
                    // never reach this branch, so the advance is unconditional here.
                    {
                        Fraction advance = tt.inTuplet()
                                           ? TDuration(dt).fraction() * Fraction(tt.normalN, tt.actualN)
                                           : dottedAdvance(dt, dots);
                        // Cap to remaining measure space to prevent overflow.
                        // For tuplet notes: if the advance must be capped, the note's
                        // face-value ticks exceed the capped advance, making actualTicks()
                        // > advance and causing sanityCheck overshoot. Remove the note from
                        // the tuplet and assign the capped duration as a plain note instead.
                        Fraction remaining = measure->ticks() - cumTick[trackKey];
                        if (advance > remaining && remaining > Fraction(0, 1)) {
                            advance = TDuration(remaining, true).fraction();
                            if (advance.numerator() == 0) {
                                // Remaining is smaller than any standard duration. The
                                // chord we just placed would become a zero-tick element
                                // (or, worse, get assigned garbage ticks by
                                // TDuration(0/0) further down). Remove it.
                                if (tt.inTuplet()) {
                                    chord->setTuplet(nullptr);
                                    tt.currentTuplet->remove(chord);
                                    tt.faceTicks -= TDuration(dtFace).fraction();
                                }
                                seg->remove(chord);
                                delete chord;
                                chord = nullptr;
                                if (savedPrevMidiTick >= 0) {
                                    prevMidiTick[trackKey] = savedPrevMidiTick;
                                } else {
                                    prevMidiTick.erase(trackKey);
                                }
                                return; // skip note add, ties, articulations
                            }
                            if (chord) {
                                if (tt.inTuplet()) {
                                    chord->setTuplet(nullptr);
                                    tt.currentTuplet->remove(chord);
                                    tt.faceTicks -= TDuration(dtFace).fraction(); // undo face contribution
                                }
                                // Always update the chord duration to match the capped
                                // advance: otherwise chord->actualTicks() (still the face
                                // value set at creation) exceeds the advance applied to
                                // cumTick, and the difference shows up as a sanityCheck
                                // overshoot (e.g. chord ticks=1/16, advance capped to 1/32
                                // produces a 1/32 voice overrun per such note).
                                TDuration cappedDur(advance);
                                chord->setDurationType(cappedDur);
                                chord->setTicks(cappedDur.fraction());
                                chord->setDots(0);
                            }
                        }
                        cumTick[trackKey] += advance;
                        if (tt.inTuplet()) {
                            tt.placedTicks += advance;
                        }
                    }
                }

                // Drain pending grace chords onto this main chord. Done for both the
                // new-chord and reused-chord paths so a grace queued just before this
                // note always gets attached at the first opportunity.
                {
                    auto& pg = pendingGraces[trackKey];
                    for (Chord* gc : pg) {
                        // Insert at the END of the grace-note list so graces
                        // appear in the same left-to-right order they were
                        // queued (= ascending tick order). The default
                        // graceIndex=0 would prepend each one, reversing the
                        // group when there are multiple grace notes.
                        gc->setGraceIndex(chord->graceNotes().size());
                        chord->add(gc);
                    }
                    // stolen ticks accumulated when graces were pushed.
                    pg.clear();
                    v0xA6LeadingGraceFv.erase(trackKey);
                    // DO NOT erase v0xA6GraceStolenTicks yet: the snap guard
                    // for the NEXT regular note needs to read it.
                }

                Note* note = Factory::createNote(chord);
                applyConcertPitch(note, en->semiTonePitch + ctx.staffPitchOffset[staffIdx]);
                chord->add(note);
                if (msVoice == 0) {
                    v0PitchesInMeasure[staffIdx].insert(note->pitch());
                }

                // faceValue high nibble = 3: square notehead (Encore's notation for bass drum
                // and similar instruments). Set HEAD_CUSTOM and register the pitch in the
                // drumset with noteheadSquareBlack so the custom lookup path is followed.
                if ((en->faceValue >> 4) == 3) {
                    note->setHeadGroup(NoteHeadGroup::HEAD_CUSTOM);
                    Drumset* ds = note->part()->instrument()->drumset();
                    if (ds && !ds->isValid(note->pitch())) {
                        DrumInstrument di;
                        di.name = u"drum";
                        di.notehead = NoteHeadGroup::HEAD_CUSTOM;
                        // Half/whole durations use the open (hollow) square;
                        // quarter and shorter use the filled square.
                        di.noteheads[int(NoteHeadType::HEAD_WHOLE)]   = SymId::noteheadSquareWhite;
                        di.noteheads[int(NoteHeadType::HEAD_HALF)]    = SymId::noteheadSquareWhite;
                        di.noteheads[int(NoteHeadType::HEAD_QUARTER)] = SymId::noteheadSquareBlack;
                        di.noteheads[int(NoteHeadType::HEAD_BREVIS)]  = SymId::noteheadSquareBlack;
                        di.line = 7;
                        di.stemDirection = DirectionV::DOWN;
                        ds->setDrum(note->pitch(), di);
                    }
                } else {
                    // For any note placed on a drumset instrument whose pitch is not defined
                    // in the drumset (e.g. a RHYTHM staff with arbitrary pitches), register
                    // it with a slash notehead on the staff line (line=0 for 1-line staves).
                    // Encore draws RHYTHM-staff notes as diagonal slashes on the single line.
                    Drumset* ds = note->part()->instrument()->drumset();
                    if (ds) {
                        if (!ds->isValid(note->pitch())) {
                            DrumInstrument di;
                            di.name = String::number(note->pitch());
                            di.notehead = NoteHeadGroup::HEAD_SLASH;
                            di.line = 0;
                            di.stemDirection = DirectionV::UP;
                            ds->setDrum(note->pitch(), di);
                        }
                        note->setHeadGroup(NoteHeadGroup::HEAD_SLASH);
                    }
                }

                // Fingering glyphs and the open-string marker travel in the
                // same artic byte slot. Fingerings 1..5 (bytes 0x0D..0x11)
                // become a Fingering text element; 0x46 becomes a
                // Fingering with STRING_NUMBER style and text "0" so the
                // MusicXML exporter emits <open-string/>.
                for (quint8 ab : { en->articulationUp, en->articulationDown }) {
                    int n = encArticByteToFingerNumber(ab);
                    if (n > 0) {
                        Fingering* fg = Factory::createFingering(note);
                        fg->setTrack(track);
                        fg->setXmlText(String::number(n));
                        note->add(fg);
                        break;
                    }
                    if (encArticByteIsOpenString(ab)) {
                        Fingering* fg = Factory::createFingering(
                            note, mu::engraving::TextStyleType::STRING_NUMBER);
                        fg->setTrack(track);
                        fg->setXmlText(u"0");
                        note->add(fg);
                        break;
                    }
                }

                // Complete pending tie: if a prior note of same (staffIdx, voice, pitch)
                // was a tie-start, create the Tie object from that note to this one.
                {
                    auto tieKey = std::make_tuple(staffIdx, voice, (int)en->semiTonePitch);
                    auto it = pendingTieNote.find(tieKey);
                    if (it != pendingTieNote.end()) {
                        Note* startNote = it->second;
                        Tie* tie = Factory::createTie(startNote);
                        tie->setStartNote(startNote);
                        tie->setEndNote(note);
                        tie->setTrack(startNote->track());
                        startNote->add(tie);
                        pendingTieNote.erase(it);
                    }
                }

                // Articulations (fermata, staccato, accent, marcato, tenuto,
                // staccatissimo, up/down bow) and ornaments (trill-mark,
                // mordent, inverted-mordent). Encore packs the glyph index
                // in articulationUp / articulationDown and may pair two
                // glyphs in a single byte. SymIds in the ornament family
                // must be wrapped in an Ornament element (Articulation
                // subclass) so MuseScore's MusicXML export emits them under
                // <ornaments> instead of <articulations>.
                auto isOrnamentSymId = [](SymId s) {
                    return s == SymId::ornamentTrill
                           || s == SymId::ornamentShortTrill
                           || s == SymId::ornamentTremblement
                           || s == SymId::ornamentMordent;
                };
                auto isFermataSymId = [](SymId s) {
                    return s == SymId::fermataAbove
                           || s == SymId::fermataBelow
                           || s == SymId::fermataShortAbove
                           || s == SymId::fermataShortBelow
                           || s == SymId::fermataLongAbove
                           || s == SymId::fermataLongBelow;
                };
                Segment* chordSeg = chord->segment();
                for (int slot = 0; slot < 2; ++slot) {
                    const quint8 ab = slot == 0 ? en->articulationUp
                                      : en->articulationDown;
                    const bool isAbove = slot == 0;
                    for (SymId sid : encArticulation2SymIds(ab)) {
                        if (sid == SymId::noSym) {
                            continue;
                        }
                        // Fermatas anchor on the ChordRest segment (not the
                        // chord) and produce MusicXML <fermata>. Slot drives
                        // the upright/inverted variant (articUp -> above,
                        // articDown -> below).
                        if (isFermataSymId(sid) && chordSeg) {
                            Fermata* ferm = Factory::createFermata(chordSeg);
                            ferm->setTrack(track);
                            SymId resolved = sid;
                            if (sid == SymId::fermataAbove || sid == SymId::fermataBelow) {
                                resolved = isAbove ? SymId::fermataAbove
                                           : SymId::fermataBelow;
                            } else if (sid == SymId::fermataShortAbove
                                       || sid == SymId::fermataShortBelow) {
                                resolved = isAbove ? SymId::fermataShortAbove
                                           : SymId::fermataShortBelow;
                            }
                            ferm->setSymId(resolved);
                            ferm->setPlacement(isAbove ? mu::engraving::PlacementV::ABOVE
                                               : mu::engraving::PlacementV::BELOW);
                            ferm->setPropertyFlags(mu::engraving::Pid::PLACEMENT,
                                                   mu::engraving::PropertyFlags::UNSTYLED);
                            chordSeg->add(ferm);
                            continue;
                        }
                        // Other ornaments (trill, mordent, ...) need to be
                        // wrapped in Ornament so MusicXML emits them under
                        // <ornaments>. Plain articulations stay as
                        // Articulation under <articulations>.
                        Articulation* art = isOrnamentSymId(sid)
                                            ? static_cast<Articulation*>(Factory::createOrnament(chord))
                                            : Factory::createArticulation(chord);
                        art->setSymId(sid);
                        chord->add(art);
                    }
                }
                // Single-note tremolos. Encore encodes the stroke count in
                // the low nibble of articulationUp / articulationDown for
                // bytes that the regular articulation table does not match.
                // Observed patterns in encore-symbols.enc:
                //   m1 tick=480: au=0x41 -> tremolo 1 stroke
                //   m2 tick=  0: ad=0x42 -> tremolo 2 strokes
                //   m2 tick=240: ad=0x03 -> tremolo 3 strokes
                //   m2 tick=480: ad=0x43 -> tremolo 3 strokes (REF: 4, off by 1)
                auto tremoloStrokeFromByte = [](quint8 b) -> int {
                    // Encore packs single-note tremolos into the artic byte
                    // with stroke count in the low nibble. The 0x40 flag
                    // appears for stroke counts 1..3 (0x41/0x42/0x43); a bare
                    // 0x03 also encodes "tremolo with 3 strokes" (m2 tick=240
                    // in encore-symbols.enc). Stop at 3 strokes: 0x44 and up
                    // are technical markings (fingering, thumb-position,
                    // harmonic, open-string), not tremolos. 4-stroke
                    // tremolos appear stored as 0x43 in the corpus and are
                    // rendered as 3 strokes for now.
                    if (b == 0x41 || b == 0x42 || b == 0x43) {
                        return b & 0x0F;
                    }
                    if (b == 0x03) {
                        return 3;
                    }
                    return 0;
                };
                int strokes = std::max(tremoloStrokeFromByte(en->articulationUp),
                                       tremoloStrokeFromByte(en->articulationDown));
                if (strokes > 0 && !chord->tremoloSingleChord()) {
                    TremoloSingleChord* trem = Factory::createTremoloSingleChord(chord);
                    TremoloType type = TremoloType::R8;
                    switch (strokes) {
                    case 1: type = TremoloType::R8;
                        break;
                    case 2: type = TremoloType::R16;
                        break;
                    case 3: type = TremoloType::R32;
                        break;
                    case 4: type = TremoloType::R64;
                        break;
                    default: break;
                    }
                    trem->setTremoloType(type);
                    chord->add(trem);
                }

                // Register tie-start if a TIE element exists at this note's position, OR
                // (v0xC2 only) if the note's grace1 lower nibble == 1, which marks it as a
                // tie-sender in Encore's live-recording encoding.  The TIE element at the
                // chord root (e.g. tick=141) is within isTieStart's ±3-tick window for notes
                // at ticks 141–144, but not for notes at ticks 145–146 (gap=4–5).  The g1low
                // indicator fills this gap for those outlying chord members.
                {
                    bool hasTieStart = isTieStart(staffIdx, voice, (int)e->tick)
                                       || (enc.header.isOldFormat()
                                           && (en->grace1 & 0x0F) == 1);
                    if (hasTieStart) {
                        pendingTieNote[{ staffIdx, voice, (int)en->semiTonePitch }] = note;
                    }
                }
            };

            auto handleRest = [&]() {
                const EncRest* er = static_cast<const EncRest*>(e);
                if (!isValidFaceValue(er->faceValue)) {
                    return;
                }
                if (er->realDuration > 0 && er->realDuration < 15) {
                    return;
                }
                DurationType dt = realDuration2DurationType(er->realDuration, er->faceValue);
                // Use dotControl (actual sounding duration stored by Encore) for dot count.
                // realDuration comes from MIDI tick spacing and may be wrong due to timing drift.
                qint16 durForDots = (er->dotControl > 0)
                                    ? static_cast<qint16>(er->dotControl)
                                    : er->realDuration;
                int dots = calcDots(durForDots, er->faceValue);
                // Cap rest duration to remaining measure space (rests are rarely in tuplets).
                // Cap when not currently in a tuplet, OR when the current group is full
                // (it will be closed before this rest is processed, so the rest is plain).
                {
                    const auto& ttPre = tuplets[trackKey];
                    if (!ttPre.inTuplet() || ttPre.groupFull()) {
                        Fraction remaining = measure->ticks() - cumTick[trackKey];
                        TDuration fullDur(dt);
                        fullDur.setDots(dots);
                        if (remaining > Fraction(0, 1) && fullDur.fraction() > remaining) {
                            TDuration capped(remaining, true);
                            dt   = capped.type();
                            dots = capped.dots();
                        }
                    }
                }

                Segment* seg = measure->getSegment(SegmentType::ChordRest, elemTick);
                if (!seg->element(track)) {
                    TDuration dur(dt);
                    dur.setDots(dots);
                    Rest* rest = Factory::createRest(seg, dur);
                    rest->setTrack(track);
                    rest->setTicks(dur.fraction());
                    rest->setDots(dots);
                    seg->add(rest);

                    auto& tt = tuplets[trackKey];
                    int actualNr = er->actualNotes();
                    int normalNr = er->normalNotes();
                    bool isStdExplicitR = (actualNr == 3 && normalNr == 2)
                                          || (actualNr == 5 && normalNr == 4)
                                          || (actualNr == 6 && normalNr == 4);
                    if (!isStdExplicitR) {
                        actualNr = 0;
                        normalNr = 0;
                    }
                    if (actualNr == 0 && enc.header.isOldFormat() && (er->faceValue & 0x0F) >= 4
                        && ((tt.inTuplet() && !tt.groupFull()) || impliedGroupMember.count(e))) {
                        actualNr = detectImpliedTuplet(er->realDuration, er->faceValue, normalNr);
                    }
                    if (actualNr > 0 && normalNr > 0) {
                        if (tt.groupFull()) {
                            closeTupletWithFill(tt, trackKey);
                        }
                        if (!tt.inTuplet()) {
                            if (isStdExplicitR && !validTupletGroupMember.count(e)) {
                                Fraction tupAdv = TDuration(dt).fraction()
                                                  * Fraction(normalNr, actualNr);
                                Fraction remaining = measure->ticks() - cumTick[trackKey];
                                if (tupAdv == remaining) {
                                    tt.startTuplet(measure, elemTick, actualNr, normalNr, dt, track);
                                } else {
                                    actualNr = 0;
                                    normalNr = 0;
                                }
                            } else {
                                tt.startTuplet(measure, elemTick, actualNr, normalNr, dt, track);
                            }
                        }
                    }
                    if (actualNr > 0 && normalNr > 0) {
                        rest->setTuplet(tt.currentTuplet);
                        tt.currentTuplet->add(rest);

                        tt.faceTicks += TDuration(dt).fraction();
                    } else {
                        if (tt.groupFull()) {
                            closeTupletWithFill(tt, trackKey);
                        }
                        if (tt.inTuplet()) {
                            closeTupletWithFill(tt, trackKey);
                        }
                    }

                    // Advance cumulative position. Mirror the chord path: when the cap
                    // shortens advance, also update the rest's ticks so that
                    // cr->actualTicks() matches the actual cumTick advance. Without this
                    // the rest claims its uncapped face value (e.g. 1/4) while cumTick
                    // moved by less, producing a sanityCheck overshoot of the difference.
                    Fraction advance = tt.inTuplet()
                                       ? TDuration(dt).fraction() * Fraction(tt.normalN, tt.actualN)
                                       : dottedAdvance(dt, dots);
                    Fraction remaining = measure->ticks() - cumTick[trackKey];
                    if (advance > remaining && remaining > Fraction(0, 1)) {
                        advance = TDuration(remaining, true).fraction();
                        if (advance.numerator() == 0) {
                            // Remaining too small to fit any standard duration: drop the
                            // rest we just placed rather than leave a zero-tick element.
                            if (tt.inTuplet()) {
                                rest->setTuplet(nullptr);
                                tt.currentTuplet->remove(rest);
                                tt.faceTicks -= TDuration(dt).fraction();
                            }
                            seg->remove(rest);
                            delete rest;
                            if (savedPrevMidiTick >= 0) {
                                prevMidiTick[trackKey] = savedPrevMidiTick;
                            } else {
                                prevMidiTick.erase(trackKey);
                            }
                            return;
                        }
                        if (tt.inTuplet()) {
                            rest->setTuplet(nullptr);
                            tt.currentTuplet->remove(rest);
                            tt.faceTicks -= TDuration(dt).fraction();
                        }
                        TDuration cappedDur(advance);
                        rest->setDurationType(cappedDur);
                        rest->setTicks(cappedDur.fraction());
                        rest->setDots(0);
                    }
                    cumTick[trackKey] += advance;
                    if (tt.inTuplet()) {
                        tt.placedTicks += advance;
                    }
                }
            };

            auto handleChordSym = [&]() {
                const EncChordSym* ec = static_cast<const EncChordSym*>(e);
                if (!ec->teksto.isEmpty()) {
                    Segment* seg = measure->getSegment(SegmentType::ChordRest, elemTick);
                    Harmony* h = Factory::createHarmony(score->dummy()->segment());
                    h->setTrack(track);
                    h->setHarmony(String(ec->teksto));
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
                    nextLyricHyphenBefore[track] = true;
                } else if (text.isEmpty()) {
                    nextLyricHyphenBefore[track] = false;
                } else {
                    PendingLyric pl;
                    pl.encTick = static_cast<int>(e->tick);
                    pl.text = text;
                    auto it = nextLyricHyphenBefore.find(track);
                    pl.hyphenBefore = (it != nextLyricHyphenBefore.end()) && it->second;
                    pl.hyphenAfter = false;
                    nextLyricHyphenBefore[track] = false;
                    queue.push_back(std::move(pl));
                }
            };

            auto handleOrnament = [&]() {
                const EncOrnament* eo = static_cast<const EncOrnament*>(e);

                // Snap an attached ornament's tick back to the chord-rest
                // whose layout xoffset matches the ornament's drawn position.
                // Encore stores the ornament at the CHORD-REST AT OR AFTER
                // its visible position but records the visible position in
                // `eo->xoffset`. When the ornament's xoff is less than the
                // stored chord-rest's xoff, the rendered position belongs to
                // the previous chord-rest (Encore visually pulls the glyph
                // back). The importer otherwise placed dynamics and hairpins
                // one chord later than the user saw in Encore. We only snap
                // when stored tick > 0 so measure-start dynamics (where
                // Encore's xoff is irrelevant and may be large for layout
                // padding) stay anchored on the bar line.
                auto snapTickByXoffset = [&](Fraction defaultTick) -> Fraction {
                    if (defaultTick <= measTick) {
                        return defaultTick;
                    }
                    const Fraction relTick = defaultTick - measTick;
                    if (encMeas.beatTicks == 0 || encMeas.timeSigDen == 0) {
                        return defaultTick;
                    }
                    const int wholeTicks = encMeas.beatTicks * encMeas.timeSigDen;
                    const int defaultEncTick = (relTick.numerator() * wholeTicks)
                                               / std::max(1, relTick.denominator());
                    const int ornXoff = static_cast<int>(eo->xoffset);
                    // Locate the chord-rest at the default tick and read its
                    // xoffset. If the ornament's xoffset is not strictly less
                    // than it, keep the default tick.
                    int defaultCrXoff = -1;
                    for (const auto& elem : encMeas.elements) {
                        const EncMeasureElem* em = elem.get();
                        if (em->type != static_cast<quint8>(EncElemType::NOTE)
                            && em->type != static_cast<quint8>(EncElemType::REST)) {
                            continue;
                        }
                        if (em->staffIdx != staffIdx || em->voice != voice) {
                            continue;
                        }
                        if (static_cast<int>(em->tick) != defaultEncTick) {
                            continue;
                        }
                        if (em->type == static_cast<quint8>(EncElemType::NOTE)) {
                            defaultCrXoff = static_cast<int>(static_cast<const EncNote*>(em)->xoffset);
                        } else {
                            defaultCrXoff = static_cast<int>(static_cast<const EncRest*>(em)->xoffset);
                        }
                        break;
                    }
                    if (defaultCrXoff >= 0 && ornXoff >= defaultCrXoff) {
                        return defaultTick;
                    }
                    // Find the latest NOTE/REST strictly before the default
                    // tick whose xoffset is <= the ornament's xoff. Also
                    // fires when defaultCrXoff < 0 (no chord-rest at the
                    // default tick, e.g. a WEDGESTART placed exactly at
                    // durTicks = the bar line). In that case the scan finds
                    // the last note in the measure and anchors the hairpin
                    // start there instead of spilling over the bar line.
                    int bestTick = -1;
                    for (const auto& elem : encMeas.elements) {
                        const EncMeasureElem* em = elem.get();
                        if (em->type != static_cast<quint8>(EncElemType::NOTE)
                            && em->type != static_cast<quint8>(EncElemType::REST)) {
                            continue;
                        }
                        if (em->staffIdx != staffIdx || em->voice != voice) {
                            continue;
                        }
                        if (static_cast<int>(em->tick) >= defaultEncTick) {
                            continue;
                        }
                        int xoff = 0;
                        if (em->type == static_cast<quint8>(EncElemType::NOTE)) {
                            xoff = static_cast<int>(static_cast<const EncNote*>(em)->xoffset);
                        } else {
                            xoff = static_cast<int>(static_cast<const EncRest*>(em)->xoffset);
                        }
                        if (xoff > ornXoff) {
                            continue;
                        }
                        if (static_cast<int>(em->tick) > bestTick) {
                            bestTick = static_cast<int>(em->tick);
                        }
                    }
                    if (bestTick < 0) {
                        return defaultTick;
                    }
                    return measTick + Fraction(bestTick, wholeTicks);
                };

                switch (eo->ornType()) {
                case EncOrnamentType::SLURSTART: {
                    // Encore .enc binaries do NOT emit SLURSTOP. alMezuro is the
                    // count of measures forward to the end measure. Endpoint is
                    // resolved in a post-pass once destination measures are
                    // populated; see the loop after the main measure loop.
                    //
                    // Overflow guard: if the slur is positioned within one 16th
                    // note of the measure boundary, its anchor note overflows the
                    // bar. Encore silently omits these slurs; skip them here.
                    if (encMeas.beatTicks > 0
                        && static_cast<int>(eo->tick) + static_cast<int>(encMeas.beatTicks) / 4
                        > static_cast<int>(encMeas.durTicks)) {
                        break;
                    }
                    int endIdx = measIdx + static_cast<int>(eo->alMezuro);
                    if (endIdx < 0 || endIdx >= static_cast<int>(ctx.measuresByIdx.size())) {
                        endIdx = measIdx;
                    }
                    PendingSlur ps;
                    // Use the raw Encore tick for the slur start position, not
                    // elemTick (cumTick-based). The ORN's encVoice is the voice
                    // where the arc is drawn, which may differ from the voice
                    // whose notes it connects. When voice 0 is empty, cumTick=0
                    // and elemTick = measure start; when voice 0 has a full-
                    // measure rest, elemTick = barline. Both are wrong. The raw
                    // eo->tick encodes the beat position directly.
                    {
                        // durTicks * timeSigDen / timeSigNum always yields 960
                        // ticks/whole regardless of whether Encore stores a simple
                        // or compound beat in beatTicks (e.g. 6/8 stores 360 for
                        // the dotted-quarter compound beat, giving beatTicks*den=2880
                        // instead of the correct 960).
                        const int wt = (encMeas.durTicks && encMeas.timeSigNum && encMeas.timeSigDen)
                                       ? (static_cast<int>(encMeas.durTicks) * encMeas.timeSigDen)
                                       / encMeas.timeSigNum : 960;
                        ps.startTick = measTick
                                       + Fraction(static_cast<int>(eo->tick), wt).reduced();
                    }
                    ps.track = track;
                    ps.startMeasIdx = measIdx;
                    ps.endMeasIdx = endIdx;
                    ps.alMezuro = static_cast<int>(eo->alMezuro);
                    ps.slurXoffset = static_cast<int>(eo->xoffset);
                    ps.slurXoffset2 = static_cast<int>(eo->xoffset2);
                    ps.staffIdx = staffIdx;
                    ps.encVoice = voice;
                    ctx.pendingSlurs.push_back(ps);
                    break;
                }
                case EncOrnamentType::SLURSTOP:
                    // Encore .enc files do not contain SLURSTOP markers; the
                    // endpoint is encoded inside the SLURSTART (see above).
                    break;
                case EncOrnamentType::WEDGESTART: {
                    // Encore .enc files do NOT emit a separate WEDGESTOP
                    // element. alMezuro carries the count of measures
                    // forward to the end measure (upper bound on the
                    // span). The actual visual endpoint is at the next
                    // Dynamic on the same track inside that span; we
                    // collect the hairpin here and resolve the precise
                    // tick2 in a post-pass once every Dynamic is placed.
                    int endIdx = measIdx + static_cast<int>(eo->alMezuro);
                    if (endIdx < 0 || endIdx >= static_cast<int>(ctx.measuresByIdx.size())) {
                        endIdx = measIdx;
                    }
                    Measure* endMeas = ctx.measuresByIdx[endIdx];
                    Fraction maxEnd = endMeas->tick() + endMeas->ticks();
                    // Snap the hairpin start to the chord-rest whose
                    // xoffset matches Encore's drawn position. Without it
                    // the hairpin attaches one chord later than the user
                    // saw in Encore (real-file case: a half note + eighth
                    // pair where Encore renders the crescendo under the
                    // half note but the binary tags the eighth).
                    const Fraction snappedStart = snapTickByXoffset(elemTick);
                    if (maxEnd <= snappedStart) {
                        break;
                    }
                    // speguleco direction: bit 0 selects diminuendo vs
                    // crescendo. Encore 5 also sets bit 1 on the same byte
                    // (so real files show 0x02 for crescendo and 0x03 for
                    // diminuendo, not the legacy 0x00 / 0x01 pair). The old
                    // `== 0` check treated every Encore 5 hairpin as
                    // diminuendo and flipped every cresc/dim pair on disk.
                    const HairpinType hpType
                        = ((eo->speguleco & 0x01) == 0)
                          ? HairpinType::CRESC_HAIRPIN
                          : HairpinType::DIM_HAIRPIN;
                    PendingHairpin ph;
                    ph.startTick = snappedStart;
                    ph.maxEndTick = maxEnd;
                    ph.track = track;
                    ph.type = hpType;
                    ph.endMeasIdx = endIdx;
                    ph.hairpinXoffset2 = static_cast<int>(eo->xoffset2);
                    ph.staffIdx = staffIdx;
                    ph.encVoice = voice;
                    ctx.pendingHairpins.push_back(ph);
                    break;
                }
                case EncOrnamentType::WEDGESTOP:
                    // Encore .enc files do not contain WEDGESTOP markers; the
                    // endpoint is encoded inside the WEDGESTART. Kept as a no-op
                    // in case a future Encore variant ever emits one.
                    break;
                case EncOrnamentType::TEMPO: {
                    if (eo->tempo > 0) {
                        Segment* seg = measure->getSegment(SegmentType::ChordRest, elemTick);
                        if (!seg) {
                            seg = measure->getSegment(SegmentType::ChordRest, measTick);
                        }
                        TempoText* tt2 = Factory::createTempoText(seg);
                        tt2->setTrack(track);
                        // eo->tempo is beat-unit BPM as displayed in Encore, not quarter-note BPM.
                        // For compound meters (6/8, 9/8, 12/8) the beat is a dotted quarter,
                        // so scale by 3/2 to recover quarter-note BPM for the tempo map.
                        // Use the MuseScore timesig (nominal, so pickup inherits the main sig).
                        const Fraction mts = measure->timesig();
                        const bool cmpd = mts.denominator() == 8
                                          && mts.numerator() % 3 == 0
                                          && mts.numerator() > 3;
                        const int quarterBpm = cmpd
                                               ? static_cast<int>(eo->tempo * 3.0 / 2.0 + 0.5)
                                               : static_cast<int>(eo->tempo);
                        const double bps = quarterBpm / 60.0;
                        tt2->setTempo(BeatsPerSecond(bps));
                        tt2->setXmlText(tempoXmlText(quarterBpm, measure->timesig()));
                        seg->add(tt2);
                        score->setTempo(elemTick, BeatsPerSecond(bps));
                    }
                    break;
                }
                case EncOrnamentType::ARPEGGIO: {
                    // Encore writes the ORN before the chord's notes in MEAS
                    // order, so the chord does not exist yet at this point.
                    // Queue the intent; a post-measure pass resolves it once
                    // the chord segment is populated.
                    ctx.pendingArpeggios.push_back({ elemTick, track });
                    break;
                }
                case EncOrnamentType::TREMOLO_32:
                case EncOrnamentType::TREMOLO_32B: {
                    // Single-chord tremolo ORN (3 slashes = 32nd-note speed).
                    // The ORN tick may equal the chord tick (most cases) or
                    // fall at durTicks (Encore stores it after the last note
                    // on long tied passages). Both are resolved in the post-
                    // pass which searches backwards when no chord is found at
                    // the exact tick.
                    PendingOrnTremolo pt;
                    pt.tick = elemTick;
                    pt.measTick = measTick;
                    pt.staffIdx = staffIdx;
                    pt.msVoice = msVoice;
                    pt.tremType = TremoloType::R32;
                    ctx.pendingOrnTremolos.push_back(pt);
                    break;
                }
                case EncOrnamentType::TRILL_START:
                case EncOrnamentType::TRILL_ALT: {
                    // Same deferred attachment as ARPEGGIO -- the chord is
                    // not built yet at this point in the element loop.
                    // TRILL_END (0x35) is dropped because it marks the end
                    // of a trill+wavy-line span; the start markers already
                    // place the visible trill-mark glyph.
                    ctx.pendingTrills.push_back({ elemTick, track });
                    break;
                }
                case EncOrnamentType::TRILL_END:
                    break;
                case EncOrnamentType::SEGNO:
                case EncOrnamentType::TO_CODA:
                case EncOrnamentType::CODA: {
                    // Section markers attach to a measure, not a chord, so
                    // they don't need the chord-deferred pattern. Queue the
                    // tick + marker type; the post-measure pass adds the
                    // Marker to the measure that contains the tick.
                    MarkerType mt = MarkerType::CODA;
                    if (eo->ornType() == EncOrnamentType::SEGNO) {
                        mt = MarkerType::SEGNO;
                    } else if (eo->ornType() == EncOrnamentType::TO_CODA) {
                        mt = MarkerType::TOCODA;
                    }
                    ctx.pendingMarkers.push_back({ elemTick, mt });
                    break;
                }
                case EncOrnamentType::STACCATO: {
                    // Encore stores chord-level staccato as a separate ORN
                    // tipo=0xC9 at the same tick as the chord. Its own
                    // MusicXML exporter drops 0xC9 entirely, but the dot
                    // is visible in Encore's display. Defer attachment
                    // like ARPEGGIO/TRILL because the chord segment is
                    // not built yet at this point in the element loop.
                    ctx.pendingStaccatos.push_back({ elemTick, track });
                    break;
                }
                case EncOrnamentType::DOWNBOW: {
                    const bool cm = !noteTicks.count(static_cast<int>(e->tick));
                    ctx.pendingBowings.push_back({ elemTick, track, SymId::stringsDownBow, measIdx, cm });
                    break;
                }
                case EncOrnamentType::UPBOW: {
                    const bool cm = !noteTicks.count(static_cast<int>(e->tick));
                    ctx.pendingBowings.push_back({ elemTick, track, SymId::stringsUpBow, measIdx, cm });
                    break;
                }
                case EncOrnamentType::FINGER_1:
                case EncOrnamentType::FINGER_2:
                case EncOrnamentType::FINGER_3:
                case EncOrnamentType::FINGER_4:
                case EncOrnamentType::FINGER_5: {
                    const int n = static_cast<int>(eo->ornType())
                                  - static_cast<int>(EncOrnamentType::FINGER_1) + 1;
                    const int orn_tick = static_cast<int>(e->tick);
                    // Pattern A: in a grand-staff measure, an ORN at the last voice=0
                    // tick with no voice=4 note at that tick was placed by Encore in the
                    // wrong measure. It belongs to the first chord of the NEXT measure on
                    // the sibling staff.
                    const bool cm = !voice4NoteTicks.empty()
                                    && !voice4NoteTicks.count(orn_tick)
                                    && orn_tick == maxVoice0Tick;
                    // Pattern B: in a grand-staff measure, when more FINGER ORNs appear
                    // at a tick than voice=0 notes, the excess ORNs target the voice=4
                    // (2nd staff) chord at the same tick.
                    const bool ps = !cm
                                    && voice4NoteTicks.count(orn_tick)
                                    && ornFingCountAtTick[orn_tick]
                                    > v0NoteCountAtTick[orn_tick];
                    ctx.pendingOrnFingerings.push_back({ elemTick, track, n, measIdx, cm, ps });
                    break;
                }
                case EncOrnamentType::DYN_PPP:
                case EncOrnamentType::DYN_PP:
                case EncOrnamentType::DYN_P:
                case EncOrnamentType::DYN_MP:
                case EncOrnamentType::DYN_MF:
                case EncOrnamentType::DYN_F:
                case EncOrnamentType::DYN_FF:
                case EncOrnamentType::DYN_FFF:
                case EncOrnamentType::DYN_SFZ:
                case EncOrnamentType::DYN_SFFZ:
                case EncOrnamentType::DYN_FP:
                case EncOrnamentType::DYN_FZ:
                case EncOrnamentType::DYN_SF: {
                    // Size-16 dynamic markings. The size-16 ORN payload only
                    // carries the tipo byte reliably (later fields run past
                    // the element boundary in the parser); the tipo alone
                    // selects the DynamicType.
                    //
                    // Staff displacement: Encore stores a dynamic on staff N
                    // but draws it above that staff (yoffset > 0) when the
                    // user has dragged it up onto the staff above. The binary
                    // staffByte points to staff N but the glyph visually
                    // belongs to staff N-1. Reroute to N-1 so MuseScore
                    // places it on the correct instrument.
                    if (eo->yoffset > 0 && staffIdx > 0) {
                        staffIdx -= 1;
                        track = static_cast<track_idx_t>(staffIdx * VOICES + msVoice);
                    }
                    DynamicType dt = DynamicType::OTHER;
                    switch (eo->ornType()) {
                    case EncOrnamentType::DYN_PPP:  dt = DynamicType::PPP;
                        break;
                    case EncOrnamentType::DYN_PP:   dt = DynamicType::PP;
                        break;
                    case EncOrnamentType::DYN_P:    dt = DynamicType::P;
                        break;
                    case EncOrnamentType::DYN_MP:   dt = DynamicType::MP;
                        break;
                    case EncOrnamentType::DYN_MF:   dt = DynamicType::MF;
                        break;
                    case EncOrnamentType::DYN_F:    dt = DynamicType::F;
                        break;
                    case EncOrnamentType::DYN_FF:   dt = DynamicType::FF;
                        break;
                    case EncOrnamentType::DYN_FFF:  dt = DynamicType::FFF;
                        break;
                    case EncOrnamentType::DYN_SFZ:  dt = DynamicType::SFZ;
                        break;
                    case EncOrnamentType::DYN_SFFZ: dt = DynamicType::SFFZ;
                        break;
                    case EncOrnamentType::DYN_FP:   dt = DynamicType::FP;
                        break;
                    case EncOrnamentType::DYN_FZ:   dt = DynamicType::FZ;
                        break;
                    case EncOrnamentType::DYN_SF:   dt = DynamicType::SF;
                        break;
                    default: break;
                    }
                    // Snap the dynamic's stored tick back to the chord-rest
                    // whose xoffset matches Encore's drawn position; Encore
                    // tags the chord at or after the visible glyph, but
                    // the rendered position belongs to the preceding chord
                    // when the dynamic's xoffset is smaller. Without this
                    // a dynamic placed under the 2nd eighth of a measure
                    // attaches one chord later in MuseScore.
                    Fraction placeTick = snapTickByXoffset(elemTick);
                    // Section-end dynamics (e.g. the "pp" that applies on
                    // the repeat of a 1st-volta measure) are stored at the
                    // measure's last tick (measureDurTicks). cumTick has
                    // already been advanced to fill the measure by the
                    // time we reach the ORN, so the snap result still
                    // lands at the next measure's first segment; clamp
                    // such overflow back to the last existing ChordRest
                    // segment inside the current measure.
                    if (placeTick >= measTick + measure->ticks()) {
                        Segment* last = measure->last(SegmentType::ChordRest);
                        placeTick = last ? last->tick() : measTick;
                    }
                    Segment* seg = measure->getSegment(SegmentType::ChordRest, placeTick);
                    if (!seg) {
                        seg = measure->getSegment(SegmentType::ChordRest, measTick);
                    }
                    // Encore can write the same dynamic twice on the same
                    // (staff, voice) at the same tick (real-file case: two
                    // MF ORNs at tick=120 with xoff 37/38). Encore renders
                    // only one; skip the duplicate so MuseScore does not
                    // stack two Dynamics on the same segment.
                    bool dupDyn = false;
                    for (EngravingItem* ann : seg->annotations()) {
                        if (!ann || !ann->isDynamic() || ann->track() != track) {
                            continue;
                        }
                        if (toDynamic(ann)->dynamicType() == dt) {
                            dupDyn = true;
                            break;
                        }
                    }
                    if (dupDyn) {
                        break;
                    }
                    Dynamic* dyn = Factory::createDynamic(seg);
                    dyn->setTrack(track);
                    dyn->setDynamicType(dt);
                    dyn->setXmlText(Dynamic::dynamicText(dt));
                    if (eo->yoffset < 0) {
                        dyn->setPlacement(mu::engraving::PlacementV::BELOW);
                        dyn->setPropertyFlags(mu::engraving::Pid::PLACEMENT, mu::engraving::PropertyFlags::UNSTYLED);
                    }
                    seg->add(dyn);
                    break;
                }
                case EncOrnamentType::STAFFTEXT: {
                    // STAFFTEXT 0x1E is a position-only marker; the text
                    // payload lives in the TEXT block and is referenced by
                    // the ornament's tind byte (+32).
                    const int textIdx = static_cast<int>(eo->tind);
                    if (textIdx < 0
                        || textIdx >= static_cast<int>(enc.textBlock.entries.size())) {
                        break;
                    }
                    QString text = enc.textBlock.entries[textIdx];
                    if (text.isEmpty()) {
                        break;
                    }
                    // Same clamp as the dynamic case above: STAFFTEXT
                    // ornaments stored at the measure's final tick must
                    // attach to the last existing segment in the current
                    // measure rather than spill over the bar line.
                    Fraction placeTick = elemTick;
                    if (placeTick >= measTick + measure->ticks()) {
                        Segment* last = measure->last(SegmentType::ChordRest);
                        placeTick = last ? last->tick() : measTick;
                    }
                    Segment* seg = measure->getSegment(SegmentType::ChordRest, placeTick);
                    if (!seg) {
                        seg = measure->getSegment(SegmentType::ChordRest, measTick);
                    }
                    // Encore stores text y-offset with Cartesian sign (positive =
                    // upward). When yoffset is below the staff baseline (negative
                    // in that convention) the text was placed under the system in
                    // Encore, e.g. "ten" markers in Beethoven Plectro m3. Mirror
                    // that into MuseScore's PlacementV.
                    const bool placeBelow = (eo->yoffset < 0);

                    // Promote standard Italian tempo terms (Allegro, Andante,
                    // "a tempo", ...) to a TempoText so MuseScore tracks them
                    // as tempo rather than free-form staff text.
                    const double tempoBps = encTextToTempoBps(text);
                    if (tempoBps >= 0.0) {
                        TempoText* tt2 = Factory::createTempoText(seg);
                        tt2->setTrack(track);
                        tt2->setXmlText(String(text));
                        if (tempoBps > 0.0) {
                            tt2->setTempo(BeatsPerSecond(tempoBps));
                            tt2->setFollowText(true);
                            score->setTempo(elemTick, BeatsPerSecond(tempoBps));
                        }
                        if (placeBelow) {
                            tt2->setPlacement(mu::engraving::PlacementV::BELOW);
                            tt2->setPropertyFlags(mu::engraving::Pid::PLACEMENT, mu::engraving::PropertyFlags::UNSTYLED);
                        }
                        seg->add(tt2);
                        break;
                    }

                    StaffText* st = Factory::createStaffText(seg);
                    st->setTrack(track);
                    st->setXmlText(String(text));
                    if (placeBelow) {
                        st->setPlacement(mu::engraving::PlacementV::BELOW);
                        st->setPropertyFlags(mu::engraving::Pid::PLACEMENT, mu::engraving::PropertyFlags::UNSTYLED);
                    }
                    seg->add(st);
                    break;
                }
                default:
                    break;
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
            case EncElemType::NOTE:      handleNote();
                break;
            case EncElemType::REST:      handleRest();
                break;
            case EncElemType::CHORD:     handleChordSym();
                break;
            case EncElemType::LYRIC:     handleLyric();
                break;
            case EncElemType::ORNAMENT:  handleOrnament();
                break;
            case EncElemType::KEYCHANGE: handleKeyChange();
                break;
            default: break;
            }
        }  // end element for-loop

        // Before checkMeasure: finalize any tuplets still open at measure end.
        // closeTupletWithFill fills partial groups whose placedTicks does not
        // fit a TDuration, then closes the tracker; TupletTracker::closeTuplet
        // handles the remaining cases (shrink to placedTicks when it fits, or
        // leave canonical for full / mixed-overshoot groups). Required before
        // checkMeasure -- otherwise the canonical tuplet ticks would skip
        // past the actual placed content and leave the measure incomplete.
        for (auto& [key, tt] : tuplets) {
            closeTupletWithFill(tt, key);
        }

        // Attach queued lyric syllables to ChordRest segments by tick. The
        // queue has already filtered out "-" and "" separator elements; each
        // remaining entry carries the raw Encore tick the syllable was
        // anchored to in the binary, and a hyphen flag pair that determines
        // its LyricsSyllabic when rendered. For every Chord segment in this
        // measure we pick the syllable whose encTick is closest to the
        // segment tick, within half a beat. Leftover lyrics (the binary
        // sometimes places end-of-measure syllables past the bar) persist
        // to the next measure's attach pass.
        // Encore PPQ derives from beatTicks (quarter-note tick count). For
        // a 240-PPQ quarter, 480 raw Encore ticks per whole note; one
        // MuseScore tick (DIVISION=480 per quarter) == 2 Encore ticks.
        const int encTicksPerQuarter = encMeas.beatTicks
                                       ? static_cast<int>(encMeas.beatTicks) : 240;
        const int matchThreshold = encTicksPerQuarter / 2;   // half-beat window
        for (auto& [lyTrack, entries] : ctx.pendingLyrics) {
            if (entries.empty()) {
                continue;
            }
            // Encore stores multi-verse lyrics on different voices of the
            // same staff: verse 1 = voice 0, verse 2 = voice 1, etc. Within
            // MuseScore every verse anchors to the same voice-0 chord and
            // uses Lyrics::setVerse() to disambiguate verse number.
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
            // Lyric ticks are encoded relative to the measure they live in,
            // so unmatched leftovers cannot anchor anywhere in a later
            // measure -- discard them rather than letting the queue grow.
            entries.clear();
        }
        // The hyphen-after flag on the trailing syllable is preserved as
        // part of the just-rendered Lyrics. The hyphen-before flag for the
        // next syllable, however, lives in nextLyricHyphenBefore and must
        // survive bar lines (e.g. Encore's "RO -" at the end of a measure
        // carrying the hyphen into the next bar's first syllable).

        // Fill any remaining gaps with invisible rests.
        // With faceValue-cumulative placement, notes always land at canonical positions
        // so gaps are only genuine rests not explicitly written in the Encore score.
        for (int si = 0; si < ctx.totalStaves; ++si) {
            measure->checkMeasure(static_cast<staff_idx_t>(si));
        }

        // Post-checkMeasure micro-correction: fix tiny over/undershoots (≤ 1/24)
        // that result from non-standard gaps that toRhythmicDurationList cannot
        // fill or match exactly.
        //
        // Overshoot (voiceSum > mLen ≤ 1/24): gap rests from cascade fills went
        //   1/64+1/256+1/1024 = 21/1024 when the actual gap was smaller (or zero).
        //   Remove gap rests smallest-first until voiceSum ≤ mLen.
        //
        // Undershoot (voiceSum < mLen ≤ 1/24): cascade left a sub-standard residual
        //   (e.g. 1/3072 = 1/48 - 21/1024). Add a V_MEASURE gap rest for the exact
        //   deficit (V_MEASURE accepts non-standard ticks without TDuration assertion).
        //
        // The 1/24 ceiling covers triplet 16th gaps left by mixed-value tuplets
        // (Q rest + eighth + eighth in a 3:2 quarter triplet contributes 1/3
        // instead of canonical 1/2, leaving the measure 1/24 short after the
        // following non-tuplet element forces an early close).
        {
            const Fraction mLen_fix = measure->ticks();
            const Fraction maxDelta(1, 24);
            for (int si = 0; si < ctx.totalStaves; ++si) {
                for (voice_idx_t v = 0; v < VOICES; ++v) {
                    track_idx_t tr = static_cast<track_idx_t>(si * VOICES + v);
                    // Compute voice sum
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

    // Apply per-measure tempo from the MEAS header BPM field. Encore stores
    // a BPM value in every measure's 54-byte header (offset 0, quarter-note
    // BPM regardless of the time signature); the importer previously read
    // the field into EncMeasure::bpm but never used it, so an imported
    // score always defaulted to MuseScore's 120 quarter-BPM.
    //
    // Emit a TempoText only on the first measure and on every measure
    // whose BPM differs from the previous applied value, so back-to-back
    // identical measures do not get redundant tempo markings. Skip the
    // tempo update entirely when a TempoText already lives at the target
    // segment (an ORN TEMPO subtype 0x32 or a STAFFTEXT promoted from an
    // Italian tempo term has already set both the visible mark and the
    // tempo map; trust that instance).
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
            for (EngravingItem* e : seg->annotations()) {
                if (e && e->isTempoText()) {
                    hasExisting = true;
                    break;
                }
            }
            if (!hasExisting) {
                const double bps = bpm / 60.0;
                TempoText* tt = Factory::createTempoText(seg);
                tt->setTrack(0);
                tt->setTempo(BeatsPerSecond(bps));
                tt->setXmlText(tempoXmlText(bpm, m->timesig()));
                seg->add(tt);
                score->setTempo(measTick, BeatsPerSecond(bps));
            }
            lastBpm = bpm;
        }
    }

    // Discard any grace chords still queued after the final measure (no main chord
    // followed them). They have no parent in the score tree, so delete them here.
    for (auto& [key, vec] : pendingGraces) {
        for (Chord* gc : vec) {
            LOGW() << "Encore import: discarding dangling grace chord at end of score"
                   << " (staff " << key.first << ", voice " << key.second << ")";
            delete gc;
        }
    }
    pendingGraces.clear();
}
} // namespace mu::iex::encore
