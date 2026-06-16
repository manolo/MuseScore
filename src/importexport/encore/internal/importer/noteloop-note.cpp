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

#include "noteloop-internal.h"
#include "mapping.h"
#include "../parser/ticks.h"
#include "engraving/dom/arpeggio.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/fermata.h"
#include "engraving/dom/fingering.h"
#include "engraving/dom/ornament.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/note.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/drumset.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/tie.h"
#include "log.h"
#include <set>
#include <tuple>

namespace mu::iex::encore {
using namespace mu::engraving;

void handleNote(BuildCtx& ctx, NoteLoopMeasCtx& mc, NoteElemCtx& ec)
{
    MasterScore* score = ctx.score;
    Measure* measure = mc.measure;
    const std::set<const EncMeasureElem*>& validTupletGroupMember = mc.validTupletGroupMember;
    const std::set<const EncMeasureElem*>& partialEndGroup = mc.partialEndGroup;
    const std::set<const EncMeasureElem*>& impliedGroupMember = mc.validTupletGroupMember;
    std::set<std::tuple<int, int, int> >& filteredTieSenderPitches = mc.filteredTieSenderPitches;
    const EncMeasureElem* e = ec.e;
    const bool isInnerFirst  = mc.nestedByInnerFirst.count(e) > 0;
    const bool isInnerLast   = mc.nestedByInnerLast.count(e) > 0;
    const bool isInnerMember = mc.innerGroupMembers.count(e) > 0;
    int staffIdx = ec.staffIdx;
    int voice = ec.voice;
    int msVoice = ec.msVoice;
    track_idx_t track = ec.track;
    auto trackKey = ec.trackKey;
    bool isChordExt = ec.isChordExt;
    Fraction elemTick = ec.elemTick;
    int savedPrevMidiTick = ec.savedPrevMidiTick;
    bool hadLastChordPos = ec.hadLastChordPos;
    Fraction savedLastChordPos = ec.savedLastChordPos;
    auto isTieStart = [&](int si, int v, int t, int pos = -1) { return mc.isTieStartAt(si, v, t, pos); };
    auto closeTupletWithFill = [&](TupletTracker& tt, std::pair<int, int> key) {
        mc.closeTupletWithFill(ctx, tt, key);
    };

    const EncNote* en = static_cast<const EncNote*>(e);

    if (tryHandleGraceNote(ctx, mc, ec, en)) {
        return;
    }

    if (!isValidFaceValue(en->faceValue)) {
        return;
    }
    const quint8 safeFv = en->faceValue & 0x0F;
    // Skip MIDI tie-continuation artifacts (realDuration < 15) unless tie-start or chord extension.
    // Use pre-computed isChordExt: after prevMidiTick update delta=0 and would falsely bypass.
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
            // For 8th+ notes: bypass tuplet members (measure-boundary truncation), chord extensions, and first note on staff (no artifact context).
            if (en->realDuration > CHORD_CLUSTER_THRESHOLD
                && !validTupletGroupMember.count(e)
                && !isChordExt
                && savedPrevMidiTick >= 0) {
                return;
            }
        }
    }

    // Cascade-filter: tie-receiver of a filtered artifact is also filtered.
    if ((en->grace1 & 0x0F) == 2) {
        auto cascKey = std::make_tuple(staffIdx, voice, (int)en->semiTonePitch);
        if (filteredTieSenderPitches.count(cascKey)) {
            filteredTieSenderPitches.erase(cascKey);
            return;
        }
    }

    // Explicit tuplet notes: faceValue drives dt (rdur may be truncated by the next MIDI event).
    {
        int preA = en->actualNotes(), preN = en->normalNotes();
        if (!isStandardExplicitTuplet(preA, preN)) {
            preA = 0;
            preN = 0;
        }
        if (preA == 0 && ctx.impliedTuplets && (en->faceValue & 0x0F) >= 4
            && (ctx.tuplets[trackKey].inTuplet() || impliedGroupMember.count(e))) {
            preA = detectImpliedTuplet(en->realDuration, en->faceValue, preN);
        }
        (void)preA;
        (void)preN;
    }
    int preACheck = en->actualNotes(), preNCheck = en->normalNotes();
    // Use uniform-fill override ratio when present.
    {
        auto orit = mc.overrideGroupRatios.find(e);
        if (orit != mc.overrideGroupRatios.end()) {
            preACheck = orit->second.first;
            preNCheck = orit->second.second;
        }
    }
    bool isStandardExplicit = isStandardExplicitTuplet(preACheck, preNCheck);

    DurationType dt;
    int dots;
    if (isStandardExplicit) {
        // In files where the face-value byte encodes "beats" rather than absolute note
        // values (e.g. 8/8 where fv=Q means one eighth beat), rdur equals exactly
        // beatTicks × (normalN/actualN). Use rdur in that case; otherwise trust fv.
        // This distinguishes beat-relative face values (rdur=beatTicks×ratio) from
        // truncated rdur (last note in a measure, rdur shortened by a following rest).
        dt = faceValue2DurationType(en->faceValue);
        if (en->realDuration > 0 && preACheck > 0 && preNCheck > 0
            && mc.encMeas && mc.encMeas->beatTicks > 0) {
            const int bt = static_cast<int>(mc.encMeas->beatTicks);
            const int expectedBeatAdv = (bt * preNCheck + preACheck / 2) / preACheck;
            if (static_cast<int>(en->realDuration) == expectedBeatAdv) {
                // Beat-relative fv (e.g. 8/8 where fv=Q means one beat): derive written note from rdur × ratio.
                const int faceTicks = (static_cast<int>(en->realDuration) * preACheck
                                       + preNCheck / 2) / preNCheck;
                // Choose fv to avoid the "realDur < faceValue2ticks(fv)" fallback in realDuration2DurationType.
                static constexpr int kFaceTicks[] = { 0, 960, 480, 240, 120, 60, 30, 15, 7 };
                quint8 computedFv = en->faceValue;
                for (int f = 1; f <= 8; ++f) {
                    if (kFaceTicks[f] <= faceTicks) {
                        computedFv = static_cast<quint8>(f);
                        break;
                    }
                }
                const DurationType dtRdur = realDuration2DurationType(
                    static_cast<qint16>(faceTicks), computedFv);
                if (dtRdur != DurationType::V_INVALID) {
                    dt = dtRdur;
                }
            }
        }
        dots = 0;
        // Partial measure-end groups: reduce dt when the tuplet advance overshoots remaining space.
        if (partialEndGroup.count(e)) {
            const auto& ttX = ctx.tuplets[trackKey];
            if (ttX.inTuplet() && dt != DurationType::V_INVALID) {
                Fraction adv = TDuration(dt).fraction()
                               * Fraction(ttX.normalN, ttX.actualN);
                Fraction rem = measure->ticks() - ctx.cumTick[trackKey];
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
        // dotControl bit 0 = dotted flag; computeDotCount tries tick-value interpretation first, falls back to bit 0 on MIDI drift.
        if (en->dotControl > 0) {
            dots = computeDotCount(en->dotControl, en->realDuration, en->faceValue,
                                   true /*useBit0Fallback*/);
        } else {
            dots = calcDotsSnap(en->realDuration, en->faceValue);
        }
    }
    const DurationType dtFace = dt;  // before capping; used for isolated-explicit fill check

    {
        const auto& ttPre = ctx.tuplets[trackKey];
        int preA = isStandardExplicit ? preACheck : 0;
        int preN = isStandardExplicit ? preNCheck : 0;
        if (!isStandardExplicit) {
            if (ctx.impliedTuplets && (en->faceValue & 0x0F) >= 4
                && ((ttPre.inTuplet() && !ttPre.groupFull()) || impliedGroupMember.count(e))) {
                preA = detectImpliedTuplet(en->realDuration, en->faceValue, preN);
            }
        }

        // Implied-tuplet guard: skip if full group advance doesn't fit (partial triplet leaves 1/3072 residual).
        if (!isStandardExplicit && !ttPre.inTuplet()
            && !isChordExt && preA > 0 && preN > 0) {
            Fraction singleAdv = TDuration(faceValue2DurationType(en->faceValue & 0x0F)).fraction()
                                 * Fraction(preN, preA);
            Fraction fullGroupAdv = singleAdv * Fraction(preA, 1);
            Fraction mRemaining = measure->ticks() - ctx.cumTick[trackKey];
            if (fullGroupAdv > mRemaining) {
                if (savedPrevMidiTick >= 0) {
                    ctx.prevMidiTick[trackKey] = savedPrevMidiTick;
                } else {
                    ctx.prevMidiTick.erase(trackKey);
                }
                return; // Skip this note; don't place, don't advance ctx.cumTick
            }
        }

        bool willBeExplicit = isStandardExplicit && validTupletGroupMember.count(e);
        bool willBeTuplet = (preA > 0 && preN > 0 && (willBeExplicit || !isStandardExplicit))
                            || (ttPre.inTuplet() && !ttPre.groupFull());
        if (!willBeTuplet) {
            Fraction remaining = measure->ticks() - ctx.cumTick[trackKey];
            TDuration fullDur(dt);  // must include dots; TDuration(dt) alone misses the dotted extension
            fullDur.setDots(dots);
            if (remaining > Fraction(0, 1) && fullDur.fraction() > remaining) {
                TDuration capped(remaining, true);
                // 1/3072-type residual: no valid TDuration; zero-tick chord breaks sanityCheck.
                if (capped.fraction().numerator() == 0) {
                    if (savedPrevMidiTick >= 0) {
                        ctx.prevMidiTick[trackKey] = savedPrevMidiTick;
                    } else {
                        ctx.prevMidiTick.erase(trackKey);
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

        auto& tt = ctx.tuplets[trackKey];
        int actualN = isStandardExplicit ? preACheck : 0;
        int normalN = isStandardExplicit ? preNCheck : 0;
        // Implied tuplet (v0xC2 only, pre-validated). !tt.groupFull() prevents a post-group note from opening a new unvalidated group.
        if (actualN == 0 && ctx.impliedTuplets && (en->faceValue & 0x0F) >= 4
            && ((tt.inTuplet() && !tt.groupFull()) || impliedGroupMember.count(e))) {
            actualN = detectImpliedTuplet(en->realDuration, en->faceValue, normalN);
        }
        // Sandwich orphan (tup=0 surrounded by tup=N:M notes): use active ratio to stay in bracket.
        if (actualN == 0 && tt.inTuplet() && !tt.groupFull() && validTupletGroupMember.count(e)) {
            actualN = tt.actualN;
            normalN = tt.normalN;
        }

        if (actualN > 0 && normalN > 0) {
            // Close full group before starting a new one.
            if (tt.groupFull()) {
                closeTupletWithFill(tt, trackKey);
            }
            if (!tt.inTuplet()) {
                if (isStandardExplicit && !validTupletGroupMember.count(e)) {
                    // Isolated explicit note: start partial tuplet only when it exactly fills remaining space.
                    Fraction tupAdv = TDuration(dtFace).fraction()
                                      * Fraction(normalN, actualN);
                    Fraction remaining = measure->ticks() - ctx.cumTick[trackKey];
                    if (tupAdv == remaining) {
                        dt   = dtFace;
                        dots = 0;
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
                    // Partial measure-end groups: derive baseLen from remaining/normalN (e.g. rem=1/8, normalN=2 -> baseLen=1/16).
                    DurationType baseLenDt = dt;
                    if (partialEndGroup.count(e)) {
                        Fraction rem3 = measure->ticks() - ctx.cumTick[trackKey];
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
            // Nested-tuplet: inner notes go into innerTt; outer advances via cumTick (doubly-nested block below).
            auto& innerTt = ctx.innerTuplets[trackKey];
            if (isInnerMember) {
                if (isInnerFirst) {
                    const NestedTupletInfo& ni = *mc.nestedByInnerFirst.at(e);
                    innerTt.closeTuplet();
                    innerTt.startTuplet(measure, elemTick, ni.innerActualN, ni.innerNormalN, dt, track);
                    if (tt.inTuplet() && tt.currentTuplet && innerTt.currentTuplet) {
                        innerTt.currentTuplet->setTuplet(tt.currentTuplet);
                        tt.currentTuplet->add(innerTt.currentTuplet);
                    }
                }
                if (innerTt.inTuplet()) {
                    chord->setTuplet(innerTt.currentTuplet);
                    innerTt.currentTuplet->add(chord);
                    innerTt.faceTicks += TDuration(dt).fraction();
                }
                if (isInnerLast && innerTt.inTuplet()) {
                    innerTt.closeTuplet();
                    // Credit outer TupletTracker with one outer-slot face value.
                    if (tt.inTuplet() && tt.currentTuplet) {
                        tt.faceTicks += TDuration(tt.currentTuplet->baseLen()).fraction();
                    }
                }
            } else {
                chord->setTuplet(tt.currentTuplet);
                tt.currentTuplet->add(chord);

                // No-downdate: only lower fullFaceSum when smaller fv arrives and current tally still fits the new threshold.
                // {Q,E}/3:2: before E, faceTicks=Q≤3E → update; {Q,Q,8,8}/3:2: faceTicks=2Q>3E → skip.
                if (tt.actualN > 0 && tt.fullFaceSum > Fraction(0, 1)) {
                    const Fraction thisFace = TDuration(dt).fraction();
                    const Fraction currentBaseLen = tt.fullFaceSum / tt.actualN;
                    if (thisFace > Fraction(0, 1) && thisFace < currentBaseLen) {
                        const Fraction newThreshold = thisFace * tt.actualN;
                        if (tt.faceTicks <= newThreshold) {
                            tt.fullFaceSum = newThreshold;
                        }
                    }
                }
                tt.faceTicks += TDuration(dt).fraction();
            }
        } else {
            auto& innerTt2 = ctx.innerTuplets[trackKey];
            if (innerTt2.inTuplet()) {
                innerTt2.closeTuplet();
            }
            if (tt.groupFull()) {
                closeTupletWithFill(tt, trackKey);
            }
            if (tt.inTuplet()) {
                closeTupletWithFill(tt, trackKey); // non-tuplet note exits group
            }
        }

        // Advance cumulative position.
        {
            // Doubly-nested advance: apply both inner and outer ratios so cumTick over the inner group equals one outer slot.
            // Without this, 3 inner 16ths at 1/24 each = 1/8 > 1/12 (one 3:2 outer slot).
            auto& innerTtAdv = ctx.innerTuplets[trackKey];
            Fraction advance;
            if (isInnerMember) {
                // Apply inner ratio AND outer ratio. Use saved NI ratios when innerLast has already closed the group.
                const NestedTupletInfo* niAdv = nullptr;
                if (mc.nestedByInnerFirst.count(e)) {
                    niAdv = mc.nestedByInnerFirst.at(e);
                } else if (mc.nestedByInnerLast.count(e)) {
                    niAdv = mc.nestedByInnerLast.at(e);
                } else if (!mc.nestedInfos.empty()) {
                    niAdv = &mc.nestedInfos.front();  // middle inner note; only one nested group per measure in known files
                }
                const int innerAN = niAdv ? niAdv->innerActualN : (innerTtAdv.inTuplet() ? innerTtAdv.actualN : preACheck);
                const int innerNN = niAdv ? niAdv->innerNormalN : (innerTtAdv.inTuplet() ? innerTtAdv.normalN : preNCheck);
                Fraction innerAdv = TDuration(dt).fraction()
                                    * Fraction(innerNN, innerAN);
                if (tt.inTuplet()) {
                    advance = innerAdv * Fraction(tt.normalN, tt.actualN);
                } else {
                    advance = innerAdv;
                }
            } else if (tt.inTuplet()) {
                advance = TDuration(dt).fraction() * Fraction(tt.normalN, tt.actualN);
            } else {
                advance = dottedAdvance(dt, dots);
            }

            // Tuplet-remaining cap: fv > baseLen (e.g. 8th inside 3:2 with baseLen=16th) would produce non-TDuration-aligned Tuplet.ticks and crash layout.
            if (tt.inTuplet() && chord) {
                const Fraction tupExpected = TDuration(tt.currentTuplet->baseLen()).fraction()
                                             * tt.normalN;
                const Fraction tupRemaining = tupExpected - tt.placedTicks;
                if (tupRemaining > Fraction(0, 1) && advance > tupRemaining) {
                    const Fraction neededFace = tupRemaining * Fraction(tt.actualN, tt.normalN);
                    TDuration cappedFace(neededFace, true /*truncate*/);
                    if (cappedFace.isValid() && cappedFace.fraction().numerator() > 0) {
                        advance = cappedFace.fraction() * Fraction(tt.normalN, tt.actualN);
                        chord->setDurationType(cappedFace);
                        chord->setTicks(cappedFace.fraction());
                        chord->setDots(0);
                        // Re-sync faceTicks: the original dt may have been larger.
                        tt.faceTicks -= TDuration(dt).fraction();
                        tt.faceTicks += cappedFace.fraction();
                    }
                }
            }

            // Cap advance to remaining space; remove tuplet membership to avoid sanityCheck overshoot.
            Fraction remaining = measure->ticks() - ctx.cumTick[trackKey];
            if (advance > remaining && remaining > Fraction(0, 1)) {
                advance = TDuration(remaining, true).fraction();
                if (advance.numerator() == 0) {
                    // Remaining smaller than any standard duration; chord would become zero-tick. Remove it.
                    if (chord->tuplet()) {
                        Tuplet* t = chord->tuplet();
                        chord->setTuplet(nullptr);
                        t->remove(chord);
                        tt.faceTicks -= chord->ticks();
                    }
                    seg->remove(chord);
                    delete chord;
                    chord = nullptr;
                    if (savedPrevMidiTick >= 0) {
                        ctx.prevMidiTick[trackKey] = savedPrevMidiTick;
                    } else {
                        ctx.prevMidiTick.erase(trackKey);
                    }
                    return;
                }
                if (chord) {
                    if (chord->tuplet()) {
                        Tuplet* t = chord->tuplet();
                        chord->setTuplet(nullptr);
                        t->remove(chord);
                        tt.faceTicks -= chord->ticks();
                    }
                    TDuration cappedDur(advance);
                    chord->setDurationType(cappedDur);
                    chord->setTicks(cappedDur.fraction());
                    chord->setDots(0);
                }
            }
            ctx.cumTick[trackKey] += advance;
            if (tt.inTuplet()) {
                tt.placedTicks += advance;
            }
            // Inner-group notes: advance innerTt.placedTicks by the singly-nested advance so closeTuplet() sees the correct inner span.
            auto& innerTtFin = ctx.innerTuplets[trackKey];
            if (isInnerMember && innerTtFin.inTuplet()) {
                const Fraction innerOnlyAdv = TDuration(dt).fraction()
                                              * Fraction(innerTtFin.normalN, innerTtFin.actualN);
                innerTtFin.placedTicks += innerOnlyAdv;
            }
        }
    }

    {
        auto& pg = ctx.pendingGraces[trackKey];
        for (Chord* gc : pg) {
            // Insert at END to preserve ascending-tick order (default graceIndex=0 prepends, reversing the group).
            gc->setGraceIndex(chord->graceNotes().size());
            chord->add(gc);
        }
        pg.clear();
        // DO NOT erase ctx.graceStolenTicks yet: the snap guard
        // for the NEXT regular note needs to read it.
    }

    const int concertPitch = en->semiTonePitch + ctx.staffPitchOffset[staffIdx];
    if (chord->findNote(concertPitch)) {
        // Suppress duplicate pitch: Encore encodes the same pitch twice in some v0xC2 files (two NOTE elements, grace1=0) and for chord-extension copies (grace1 bit 0x40).
        return;
    }
    Note* note = Factory::createNote(chord);
    applyConcertPitch(note, concertPitch);
    chord->add(note);

    // faceValue high nibble=3: square notehead (Encore bass drum notation).
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
        // 5-line PERC staff: line derived from Encore position byte (diatonic steps from C4; higher position = smaller MuseScore line).
        // faceValue high nibble 5 = cross notehead (cymbal/triangle).
        Drumset* ds = note->part()->instrument()->drumset();
        if (ds) {
            if (!ds->isValid(note->pitch())) {
                DrumInstrument di;
                di.name = String::number(note->pitch());
                di.line = std::max(-4, 10 - static_cast<int>(en->position));
                di.stemDirection = DirectionV::UP;
                ds->setDrum(note->pitch(), di);
            }
            // Override drumset default (e.g. MIDI Electric Snare defaults to HEAD_SLASH).
            const NoteHeadGroup nhg = ((en->faceValue >> 4) == 5)
                                      ? NoteHeadGroup::HEAD_CROSS
                                      : NoteHeadGroup::HEAD_NORMAL;
            ds->drum(note->pitch()).notehead = nhg;
            note->setHeadGroup(nhg);
        }
    }

    // Fingerings 1..5 (0x0D..0x11) and open-string 0x46 are packed in the artic byte.
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
            Fingering* fg = Factory::createFingering(note);  // "0" not circled STRING_NUMBER
            fg->setTrack(track);
            fg->setXmlText(u"0");
            note->add(fg);
            break;
        }
        const int sn = encArticByteToStringNumber(ab);
        if (sn > 0) {
            Fingering* fg = Factory::createFingering(
                note, mu::engraving::TextStyleType::STRING_NUMBER);
            fg->setTrack(track);
            fg->setXmlText(String::number(sn));
            note->add(fg);
            break;
        }
    }

    // Complete pending tie from same (staffIdx, voice, pitch).
    {
        auto tieKey = std::make_tuple(staffIdx, voice, (int)en->semiTonePitch);
        auto it = ctx.pendingTieNote.find(tieKey);
        if (it != ctx.pendingTieNote.end()) {
            Note* startNote = it->second;
            Tie* tie = Factory::createTie(startNote);
            tie->setStartNote(startNote);
            tie->setEndNote(note);
            tie->setTrack(startNote->track());
            startNote->add(tie);
            ctx.pendingTieNote.erase(it);
        }
    }

    applyNoteArticulations(note, chord, en, track, mc);

    // Register tie-start (TIE element or grace1 low==1 for chord members outside the ±3-tick window).
    {
        bool hasTieStart = isTieStart(staffIdx, voice, (int)e->tick, (int)en->position)
                           || (ctx.g1LowTieSender
                               && (en->grace1 & 0x0F) == 1);
        if (hasTieStart) {
            ctx.pendingTieNote[{ staffIdx, voice, (int)en->semiTonePitch }] = note;
        }
    }
}
} // namespace mu::iex::encore
