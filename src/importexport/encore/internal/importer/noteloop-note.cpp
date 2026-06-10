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
    // Nested-tuplet membership for this note (requires e).
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
    auto isTieStart = [&](int si, int v, int t) { return mc.isTieStartAt(si, v, t); };
    auto closeTupletWithFill = [&](TupletTracker& tt, std::pair<int, int> key) {
        mc.closeTupletWithFill(ctx, tt, key);
    };

    const EncNote* en = static_cast<const EncNote*>(e);

    // Grace chords must be parented under a Chord (not a Segment) or pagePos crashes. Build detached, queue, attach to the next non-grace chord.
    {
        // en->isInnerGrace is set by calculateRealDurations() for v0xA6:
        // a note with grace1 high nibble=0x10 that is shorter than the leading grace.
        if (isValidFaceValue(en->faceValue) && (en->faceValue & 0x0F) >= 4
            && (en->graceType() != EncGraceType::NORMAL || en->isInnerGrace)) {
            // Roll back per-track tick state so the next note is not
            // detected as a chord extension of this grace.
            if (savedPrevMidiTick >= 0) {
                ctx.prevMidiTick[trackKey] = savedPrevMidiTick;
            } else {
                ctx.prevMidiTick.erase(trackKey);
            }
            if (hadLastChordPos) {
                ctx.lastChordPos[trackKey] = savedLastChordPos;
            } else {
                ctx.lastChordPos.erase(trackKey);
            }

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

            ctx.pendingGraces[trackKey].push_back(gc);
            // Accumulate stolen ticks for the post-grace snap guard (noteloop.cpp).
            ctx.graceStolenTicks[trackKey] += faceValue2ticks(en->faceValue & 0x0F);
            return;
        }
    }

    if (!isValidFaceValue(en->faceValue)) {
        return;
    }
    const quint8 safeFv = en->faceValue & 0x0F;
    // Skip MIDI tie-continuation artifacts (realDuration < 15) unless note is a tie-start or chord extension.
    // Use pre-computed isChordExt; after prevMidiTick update, fresh computation yields delta=0 and falsely bypasses.
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
            // For 8th+ notes: filter only when realDuration > CHORD_CLUSTER_THRESHOLD; at exactly the threshold the note is a live-recorded chord root.
            // Bypass for notes in a validated tuplet group: the last note of a measure-spanning
            // tuplet legitimately has a short rdur because it's cut off at the measure boundary.
            if (en->realDuration > CHORD_CLUSTER_THRESHOLD
                && !validTupletGroupMember.count(e)) {
                return;
            }
        }
    }

    // Cascade-filter: tie-receiver (grace1 low==2) of a filtered artifact is also filtered (Encore hides both the artifact and its continuation).
    if ((en->grace1 & 0x0F) == 2) {
        auto cascKey = std::make_tuple(staffIdx, voice, (int)en->semiTonePitch);
        if (filteredTieSenderPitches.count(cascKey)) {
            filteredTieSenderPitches.erase(cascKey);
            return;
        }
    }

    // Explicit tuplet notes use faceValue for dt (rdur may be truncated by the next MIDI event and give wrong duration).
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
        // Store back for use below (only the stdE flag is needed here)
        (void)preA;
        (void)preN;
    }
    int preACheck = en->actualNotes(), preNCheck = en->normalNotes();
    // If a uniform-fill override was detected, use it instead of the tup byte.
    {
        auto orit = mc.overrideGroupRatios.find(e);
        if (orit != mc.overrideGroupRatios.end()) {
            preACheck = orit->second.first;
            preNCheck = orit->second.second;
        }
    }
    bool isStandardExplicit = isStandardExplicitTuplet(preACheck, preNCheck);

    // Explicit tuplet: written note value from rdur × (actualN/normalN).
    // In files where fv encodes "beats" rather than absolute note values (e.g. 8/8 where
    // fv=Q means one eighth beat), scaling rdur by the ratio gives the correct MuseScore
    // duration. In 4/4 files this is identical to faceValue2DurationType (rdur×ratio=fv_ticks).
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
                // rdur == one beat per tuplet slot: beat-relative face value.
                // Derive the true written note from rdur × ratio.
                const int faceTicks = (static_cast<int>(en->realDuration) * preACheck
                                       + preNCheck / 2) / preNCheck;
                // Choose fv so realDuration2DurationType doesn't hit the
                // "realDur < faceValue2ticks(fv)" fallback.
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
        // Partial measure-end groups only: reduce dt when the tuplet advance overshoots remaining space.
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
        // dotControl is a bitmask (bit 0 = dotted flag, not a tick count).
        // Try calcDots(dotControl) first (works when dotControl happens to be a tick value).
        // Fallback 1: calcDotsSnap(realDuration) handles exact or near-exact rdur.
        // Fallback 2: when rdur has MIDI timing drift (>±1 tick), trust bit 0 of dotControl.
        if (en->dotControl > 0) {
            dots = computeDotCount(en->dotControl, en->realDuration, en->faceValue,
                                   true /*useBit0Fallback*/);
        } else {
            dots = calcDotsSnap(en->realDuration, en->faceValue);
        }
    }
    // dtFace: face-value dt before capping; used to check whether an isolated explicit note fills remaining measure space.
    const DurationType dtFace = dt;

    // For non-tuplet notes, cap the chord duration to remaining measure space.
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

        // Implied-tuplet guard: skip if the full group advance doesn't fit in remaining space (partial triplet leaves 1/3072 residual inexpressible by standard durations).
        if (!isStandardExplicit && !ttPre.inTuplet()
            && !isChordExt && preA > 0 && preN > 0) {
            Fraction singleAdv = TDuration(faceValue2DurationType(en->faceValue & 0x0F)).fraction()
                                 * Fraction(preN, preA);
            Fraction fullGroupAdv = singleAdv * Fraction(preA, 1);
            Fraction mRemaining = measure->ticks() - ctx.cumTick[trackKey];
            if (fullGroupAdv > mRemaining) {
                // Restore ctx.prevMidiTick so the next element is not detected as a
                // chord extension of this skipped note.
                if (savedPrevMidiTick >= 0) {
                    ctx.prevMidiTick[trackKey] = savedPrevMidiTick;
                } else {
                    ctx.prevMidiTick.erase(trackKey);
                }
                return; // Skip this note; don't place, don't advance ctx.cumTick
            }
        }

        // willBeTuplet: true only when the note will be placed in an active group (a full group is closed first, so it doesn't count).
        bool willBeExplicit = isStandardExplicit && validTupletGroupMember.count(e);
        bool willBeTuplet = (preA > 0 && preN > 0 && (willBeExplicit || !isStandardExplicit))
                            || (ttPre.inTuplet() && !ttPre.groupFull());
        if (!willBeTuplet) {
            Fraction remaining = measure->ticks() - ctx.cumTick[trackKey];
            // Include dots in the comparison: TDuration(dt) alone gives the
            // undotted fraction, missing 1/2 or 3/4 of the actual note length.
            TDuration fullDur(dt);
            fullDur.setDots(dots);
            if (remaining > Fraction(0, 1) && fullDur.fraction() > remaining) {
                TDuration capped(remaining, true);
                // Remaining too small for any standard TDuration (e.g. 1/3072 residual); zero-tick chord breaks sanityCheck, so skip.
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
        // Implied tuplet (v0xC2 only, pre-validated): !tt.groupFull() prevents a post-group note from starting a new unvalidated group.
        if (actualN == 0 && ctx.impliedTuplets && (en->faceValue & 0x0F) >= 4
            && ((tt.inTuplet() && !tt.groupFull()) || impliedGroupMember.count(e))) {
            actualN = detectImpliedTuplet(en->realDuration, en->faceValue, normalN);
        }

        if (actualN > 0 && normalN > 0) {
            // Close full group before starting a new one. Isolated explicit notes (not pre-validated) are treated as plain to avoid partial-group checkMeasure overshoot.
            if (tt.groupFull()) {
                closeTupletWithFill(tt, trackKey);
            }
            if (!tt.inTuplet()) {
                if (isStandardExplicit && !validTupletGroupMember.count(e)) {
                    // Isolated explicit note: if its face-value tuplet advance exactly fills remaining space, create a partial tuplet so checkMeasure sees the correct span.
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
                    // Partial measure-end groups: derive baseLen from remaining/normalN when full span exceeds remaining space (e.g. 1/8 / normalN=2 = 1/16).
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
            // ---------------------------------------------------------------------------
            // Nested-tuplet handling: inner group notes go into the inner TupletTracker.
            // Outer TupletTracker advances via cumTick (doubly-nested advance in advance
            // block below), not via direct faceTicks here.
            // ---------------------------------------------------------------------------
            auto& innerTt = ctx.innerTuplets[trackKey];
            if (isInnerMember) {
                // Start inner TupletTracker on the first note of the inner group.
                if (isInnerFirst) {
                    const NestedTupletInfo& ni = *mc.nestedByInnerFirst.at(e);
                    innerTt.closeTuplet();
                    innerTt.startTuplet(measure, elemTick, ni.innerActualN, ni.innerNormalN, dt, track);
                    // Add inner Tuplet as element of outer Tuplet so MuseScore sees nesting.
                    if (tt.inTuplet() && tt.currentTuplet && innerTt.currentTuplet) {
                        innerTt.currentTuplet->setTuplet(tt.currentTuplet);
                        tt.currentTuplet->add(innerTt.currentTuplet);
                    }
                }
                // Add this chord to the inner TupletTracker.
                if (innerTt.inTuplet()) {
                    chord->setTuplet(innerTt.currentTuplet);
                    innerTt.currentTuplet->add(chord);
                    innerTt.faceTicks += TDuration(dt).fraction();
                }
                // On the last inner note: close inner group and credit one outer slot.
                if (isInnerLast && innerTt.inTuplet()) {
                    innerTt.closeTuplet();
                    // Credit outer TupletTracker with one outer-slot face value.
                    if (tt.inTuplet() && tt.currentTuplet) {
                        tt.faceTicks += TDuration(tt.currentTuplet->baseLen()).fraction();
                    }
                }
            } else {
                // Normal outer-group note: add to outer TupletTracker.
                chord->setTuplet(tt.currentTuplet);
                tt.currentTuplet->add(chord);

                // No-downdate: update fullFaceSum only when a smaller face value arrives AND
                // the current (pre-add) tally still fits in the new lower threshold.
                // {Q,E}/3:2: before E, faceTicks=Q=1/4, new threshold=3E=3/8: 1/4≤3/8 → update. ✓
                // {Q,Q,8,8}/3:2: before 8th, faceTicks=2Q=1/2 > 3/8 → no update (stays 3Q). ✓
                // {Q,Q,Q,Q}/4:3: Q is not < currentBaseLen(Q) → no update; fullFaceSum stays 4Q. ✓
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

        // Advance cumulative position (graces are short-circuited above and never reach here).
        {
            // Doubly-nested advance: when an inner Tuplet is active inside an outer Tuplet,
            // apply both ratios so the total cumTick advance over the inner group equals one
            // outer slot. Without this, 3 inner 16ths at 1/24 each = 1/8 > 1/12 (one 3:2
            // outer slot), causing a 1/24 overshoot and spurious gap rests.
            auto& innerTtAdv = ctx.innerTuplets[trackKey];
            // Use doubly-nested advance for ALL inner group notes (including innerLast,
            // which closes the inner Tuplet before the advance block runs).
            Fraction advance;
            if (isInnerMember) {
                // Inner note: apply inner ratio AND outer ratio.
                // If inner was already closed (isInnerLast), use saved ratios from NI.
                const NestedTupletInfo* niAdv = nullptr;
                if (mc.nestedByInnerFirst.count(e)) {
                    niAdv = mc.nestedByInnerFirst.at(e);
                } else if (mc.nestedByInnerLast.count(e)) {
                    niAdv = mc.nestedByInnerLast.at(e);
                } else if (!mc.nestedInfos.empty()) {
                    // Middle inner note: use the first available NestedTupletInfo (only
                    // one nested group per measure in all known Encore files).
                    niAdv = &mc.nestedInfos.front();
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

            // Tuplet-remaining cap: when a note's face value is LARGER than the tuplet's
            // baseLen (e.g. an 8th inside a 3:2 group with baseLen=16th), the advance can
            // exceed the remaining tuplet span, producing a non-TDuration-aligned Tuplet.ticks
            // that crashes MuseScore's layout. Cap both the advance and the chord duration to
            // fit within the remaining expected tuplet span.
            if (tt.inTuplet() && chord) {
                const Fraction tupExpected = TDuration(tt.currentTuplet->baseLen()).fraction()
                                             * tt.normalN;
                const Fraction tupRemaining = tupExpected - tt.placedTicks;
                if (tupRemaining > Fraction(0, 1) && advance > tupRemaining) {
                    // Compute what face value the chord needs so that face*ratio = tupRemaining.
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

            // Cap advance to remaining space. For tuplet notes: remove from tuplet and assign capped duration to avoid sanityCheck overshoot.
            Fraction remaining = measure->ticks() - ctx.cumTick[trackKey];
            if (advance > remaining && remaining > Fraction(0, 1)) {
                advance = TDuration(remaining, true).fraction();
                if (advance.numerator() == 0) {
                    // Remaining smaller than any standard duration; chord would become zero-tick. Remove it.
                    if (tt.inTuplet()) {
                        chord->setTuplet(nullptr);
                        tt.currentTuplet->remove(chord);
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
                    if (tt.inTuplet()) {
                        chord->setTuplet(nullptr);
                        tt.currentTuplet->remove(chord);
                        tt.faceTicks -= chord->ticks();
                    }
                    // Update chord duration to match capped advance; otherwise actualTicks() exceeds ctx.cumTick advance and causes sanityCheck overshoot.
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
            // For inner-group notes: also advance the inner TupletTracker's placedTicks
            // by the SINGLY-nested inner advance (not the doubly-nested cumTick advance)
            // so closeTuplet() sees the correct inner span.
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
        // Suppress duplicate pitch in the same chord. Encore can encode the same
        // pitch twice at identical tick/staff/voice in two ways:
        //   (a) chord-extension copy (grace1 bit 0x40 set on the second element), or
        //   (b) two identical NOTE elements with grace1=0 (seen in some v0xC2 files).
        // In both cases the second note would produce a redundant notehead on the same
        // stem position. Standard notation never places two identical pitches in one
        // chord, so suppressing here is always safe.
        return;
    }
    Note* note = Factory::createNote(chord);
    applyConcertPitch(note, concertPitch);
    chord->add(note);

    // faceValue high nibble=3: square notehead (Encore's notation for bass drum). Register pitch in drumset as HEAD_CUSTOM.
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
        // RHYTHM staff: register undefined pitches as slash noteheads on line 0 (Encore draws them as diagonal slashes on the single line).
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

    // Fingerings 1..5 (0x0D..0x11) and open-string 0x46 are packed in the artic byte. Open-string emits Fingering "0" (STRING_NUMBER style) for MusicXML <open-string/>.
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
            // Open string shown as plain fingering "0" (not circled STRING_NUMBER).
            Fingering* fg = Factory::createFingering(note);
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

    // Complete pending tie: if a prior note of same (staffIdx, voice, pitch)
    // was a tie-start, create the Tie object from that note to this one.
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

    // Articulations and ornaments from articulationUp/Down bytes; ornament SymIds must be wrapped in Ornament for MusicXML <ornaments>.
    auto isOrnamentSymId = [](SymId s) {
        return s == SymId::ornamentTrill
               || s == SymId::ornamentShortTrill
               || s == SymId::ornamentTremblement
               || s == SymId::ornamentMordent
               || s == SymId::ornamentTurn;
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
            // Fermatas anchor on the segment (not the chord) for MusicXML <fermata>. Slot 0 = above, slot 1 = below.
            if (isFermataSymId(sid) && chordSeg) {
                // 0x20/0x21 doubles as a "tuplet bracket placement" flag on the last note
                // of a tuplet group; Encore never exports it as a <fermata> in that context.
                if ((ab == 0x20 || ab == 0x21) && en->tuplet != 0) {
                    continue;
                }
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
            // Ornaments need Ornament wrapper for MusicXML <ornaments>; plain articulations use Articulation.
            // Dedup: when multiple notes in the same chord carry the same artic byte (common for
            // trills, mordents, etc. that apply to the whole chord), each note would independently
            // add the same SymId to the chord. Skip if the chord already has this symbol.
            if (isOrnamentSymId(sid)) {
                // Ornament extends Articulation and is stored in chord->articulations().
                bool alreadyHas = false;
                for (Articulation* a : chord->articulations()) {
                    if (a->isOrnament() && toOrnament(a)->symId() == sid) {
                        alreadyHas = true;
                        break;
                    }
                }
                if (!alreadyHas) {
                    Ornament* orn = Factory::createOrnament(chord);
                    orn->setSymId(sid);
                    if (sid == SymId::ornamentTrill) {
                        const auto interval = encArticByteToTrillInterval(ab);
                        if (interval.type != mu::engraving::IntervalType::AUTO) {
                            orn->setIntervalAbove(interval);
                        }
                    }
                    chord->add(orn);
                }
            } else {
                bool alreadyHas = false;
                for (Articulation* a : chord->articulations()) {
                    if (a->symId() == sid) {
                        alreadyHas = true;
                        break;
                    }
                }
                if (!alreadyHas) {
                    Articulation* art = Factory::createArticulation(chord);
                    art->setSymId(sid);
                    chord->add(art);
                }
            }
        }
    }
    // Single-note tremolos: stroke count in artic byte low nibble. 0x41/0x42/0x43 = 1/2/3 strokes; 0x03 also = 3 strokes.
    auto tremoloStrokeFromByte = [](quint8 b) -> int {
        // 0x41/0x42/0x43 = 1/2/3 strokes; 0x44+ are technical markings (not tremolos); 4-stroke stored as 0x43.
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

    // Circled string number from note options bit 0.
    // When bit 0 of the options byte is set, the position field encodes the
    // string number - 1 (0=①..7=⑧). Only applied when:
    //  - no artic byte already provides a number (prevents doubling)
    //  - the staff has standard G clef (EncClefType::G = 0) — excludes piano/keyboard
    //    staves that use clef 0x09 and do not represent string exercises
    //  - v0xA6 excluded (its options byte has different semantics)
    // Scale string numbers (0x39..0x40 = string 1..8).
    // Two cases handled together:
    //  (A) Explicit: au in 0x39..0x40 → string number = au - 0x38
    //  (B) Fallback: au == 0, opt bit 0 set, pos in 0-7, and the measure contains
    //      at least one explicit anchor (mc.hasScaleStringAnchors) → string number = pos + 1
    {
        const int scaleSn = encArticByteToScaleStringNumber(en->articulationUp);
        if (scaleSn > 0) {
            Fingering* fg = Factory::createFingering(note, mu::engraving::TextStyleType::STRING_NUMBER);
            fg->setTrack(track);
            fg->setXmlText(String::number(scaleSn));
            note->add(fg);
        } else if (mc.hasScaleStringAnchors
                   && (en->options & 0x01)
                   && en->position >= 0 && en->position <= 7
                   && en->articulationUp == 0
                   && en->articulationDown == 0) {
            Fingering* fg = Factory::createFingering(note, mu::engraving::TextStyleType::STRING_NUMBER);
            fg->setTrack(track);
            fg->setXmlText(String::number(static_cast<int>(en->position) + 1));
            note->add(fg);
        }
    }

    // Register tie-start if TIE element exists at this tick, or (v0xC2) if grace1 low==1. The g1low indicator covers chord members just outside the ±3-tick window.
    {
        bool hasTieStart = isTieStart(staffIdx, voice, (int)e->tick)
                           || (ctx.g1LowTieSender
                               && (en->grace1 & 0x0F) == 1);
        if (hasTieStart) {
            ctx.pendingTieNote[{ staffIdx, voice, (int)en->semiTonePitch }] = note;
        }
    }
}
} // namespace mu::iex::encore
