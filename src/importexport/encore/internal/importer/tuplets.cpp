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

#include "tuplets.h"

#include <map>
#include <vector>

#include "engraving/dom/factory.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/mscore.h"
#include "../parser/ticks.h"

#include "log.h"

using namespace mu::engraving;

namespace mu::iex::encore {
bool TupletTracker::groupFull() const
{
    // Close when accumulated face values reach actualN × baseLen.
    // >= handles both standard groups ({8,8,8}: exact) and mixed-duration groups ({16,16,Q}: exceeds threshold).
    return inTuplet() && fullFaceSum > Fraction(0, 1) && faceTicks >= fullFaceSum;
}

// True when a Fraction fits exactly in a TDuration (power-of-two, up to 4 dots).
// Guards tuplet setTicks: beam layout calls TDuration(ticks, truncate=false) and asserts on non-fitting fractions.
static bool fitsTDuration(const Fraction& f)
{
    if (f.numerator() <= 0) {
        return false;
    }
    TDuration snap(f, true /*truncate*/);
    return snap.isValid() && snap.fraction() == f;
}

void TupletTracker::closeTuplet()
{
    // Correct tuplet ticks to actual placed span when it differs from baseLen*normalN (wrong value causes fill-rest gaps or overshoots).
    // Skip when placedTicks doesn't fit a TDuration (e.g. 1/3 from a partial triplet): beam layout asserts on non-fitting fractions.
    if (currentTuplet && placedTicks > Fraction(0, 1)) {
        const Fraction expected = TDuration(currentTuplet->baseLen()).fraction()
                                  * currentTuplet->ratio().denominator();
        // If ticks were not set in startTuplet (non-TDuration-aligned span, e.g. 5/8 for 9:5),
        // set them now from placedTicks. This mirrors MuseScore's sanitizeTuplet() which also
        // sets ticks = baseLen.fraction() * normalN after notes are placed. checkMeasure reads
        // Tuplet::ticks() at line "expectedPos += de->ticks()" to advance the scan position;
        // with ticks=0 the scan stalls and every following note triggers a spurious gap fill.
        if (currentTuplet->ticks() == Fraction(0, 1)) {
            currentTuplet->setTicks(placedTicks);
        } else {
            const bool mixedValueOvershoot = (placedTicks > expected)
                                             && (faceTicks > fullFaceSum);
            const bool willSet = (placedTicks < expected || mixedValueOvershoot)
                                 && fitsTDuration(placedTicks);
            if (willSet) {
                currentTuplet->setTicks(placedTicks);
            }
        }
    }
    currentTuplet = nullptr;
    actualN = 0;
    normalN = 0;
    placedTicks = Fraction(0, 1);
    faceTicks   = Fraction(0, 1);
    fullFaceSum = Fraction(0, 1);
}

Tuplet* TupletTracker::startTuplet(Measure* measure, Fraction tick,
                                   int aN, int normalN_, DurationType baseType, track_idx_t track_)
{
    closeTuplet();
    currentTuplet = Factory::createTuplet(measure);
    currentTuplet->setRatio(Fraction(aN, normalN_));
    currentTuplet->setBaseLen(TDuration(baseType));
    currentTuplet->setTick(tick);
    currentTuplet->setTrack(track_);
    // setTicks requires a TDuration-aligned fraction. For ratios like 9:5 the
    // computed span (normalN × baseLen = 5/8) is not representable as a standard
    // note value; calling setTicks with such a fraction triggers a MuseScore
    // internal assertion. Skip setTicks in that case and let MuseScore compute
    // the Tuplet span from its elements at layout time.
    // setTicks requires a TDuration-aligned fraction. Ratios like 9:5 yield
    // normalN × baseLen = 5/8, which is non-standard and triggers an assertion in
    // MuseScore's checkMeasure when passed as Tuplet.ticks. Skip setTicks in that
    // case; keep ticks=0 so MuseScore refrains from creating fill rests inside the
    // group. The nuclear cap in noteloop.cpp handles any measure overflow.
    Fraction tupletDuration = TDuration(baseType).fraction() * normalN_;
    if (fitsTDuration(tupletDuration)) {
        currentTuplet->setTicks(tupletDuration);
    }
    measure->add(currentTuplet);
    actualN = aN;
    normalN = normalN_;
    placedTicks = Fraction(0, 1);
    faceTicks   = Fraction(0, 1);
    // Face-value target: actualN × baseLen (e.g. 3×(1/8) = 3/8 for a 3:2 8th triplet).
    // A mixed-duration group (8th+8th+16th+16th) also sums to 3/8 and should close here.
    fullFaceSum = TDuration(baseType).fraction() * aN;
    return currentTuplet;
}

Fraction TupletTracker::noteAdvance(DurationType baseType) const
{
    if (!inTuplet()) {
        return TDuration(baseType).fraction();
    }
    return TDuration(baseType).fraction() * Fraction(normalN, actualN);
}

std::set<const EncMeasureElem*> computeImpliedTupletMembers(
    const MeasureElemRefVec& sortedElems,
    const EncMeasure& encMeas,
    int totalStaves,
    std::set<const EncMeasureElem*>* partialEndGroup,
    std::vector<NestedTupletInfo>* nestedInfos,
    std::map<const EncMeasureElem*, std::pair<int, int> >* overrideRatios)
{
    std::set<const EncMeasureElem*> result;

    // Group elements by (staffIdx, voice) in tick order, collapsing same-tick notes
    // into chord groups (each chord group = one "beat" in the tuplet run).
    std::map<std::pair<int, int>, std::vector<std::vector<const EncMeasureElem*> > > voiceChords;
    for (const EncMeasureElem* e : sortedElems) {
        EncElemType et = static_cast<EncElemType>(e->type);
        if (et != EncElemType::NOTE && et != EncElemType::REST) {
            continue;
        }
        if (e->tick >= encMeas.durTicks) {
            continue;
        }
        int si = static_cast<int>(e->staffIdx);
        int v  = static_cast<int>(e->voice);
        if (si >= totalStaves || v >= static_cast<int>(VOICES)) {
            continue;
        }
        auto key = std::make_pair(si, v);
        auto& chords = voiceChords[key];
        if (!chords.empty() && chords.back()[0]->tick == e->tick) {
            chords.back().push_back(e);  // same chord group
        } else {
            chords.push_back({ e });     // new chord group
        }
    }

    // Helper: get the explicit tuplet ratio from the tup byte of a chord group's first elem.
    auto getExplicit = [](const std::vector<const EncMeasureElem*>& grp,
                          int& outActual, int& outNormal) {
        outActual = 0;
        outNormal = 0;
        if (grp.empty()) {
            return;
        }
        const EncMeasureElem* e = grp[0];
        EncElemType et = static_cast<EncElemType>(e->type);
        quint8 tup = 0;
        if (et == EncElemType::NOTE) {
            tup = static_cast<const EncNote*>(e)->tuplet;
        } else if (et == EncElemType::REST) {
            tup = static_cast<const EncRest*>(e)->tuplet;
        }
        int a = tup >> 4, n = tup & 0x0F;
        if (isStandardExplicitTuplet(a, n)) {
            outActual = a;
            outNormal = n;
        }
    };

    // Helper: get implied ratio for the first element of a chord group.
    auto getImplied = [](const std::vector<const EncMeasureElem*>& grp,
                         int& outActual, int& outNormal) {
        outActual = 0;
        outNormal = 0;
        if (grp.empty()) {
            return;
        }
        const EncMeasureElem* e = grp[0];
        EncElemType et = static_cast<EncElemType>(e->type);
        quint8 fv = 0;
        qint16 rdur = 0;
        if (et == EncElemType::NOTE) {
            fv   = static_cast<const EncNote*>(e)->faceValue & 0x0F;
            rdur = e->realDuration;
        } else if (et == EncElemType::REST) {
            fv   = static_cast<const EncRest*>(e)->faceValue & 0x0F;
            rdur = e->realDuration;
        }
        if (fv >= 4) {
            outActual = detectImpliedTuplet(rdur, fv, outNormal);
        }
    };

    // Helper: face value (as Fraction) for first element of a chord group.
    auto getFaceValue = [](const std::vector<const EncMeasureElem*>& grp) -> Fraction {
        if (grp.empty()) {
            return Fraction(0, 1);
        }
        const EncMeasureElem* e = grp[0];
        EncElemType et = static_cast<EncElemType>(e->type);
        quint8 fv = 0;
        if (et == EncElemType::NOTE) {
            fv = static_cast<const EncNote*>(e)->faceValue & 0x0F;
        } else if (et == EncElemType::REST) {
            fv = static_cast<const EncRest*>(e)->faceValue & 0x0F;
        }
        return faceValue2DurationType(fv) == DurationType::V_INVALID ? Fraction(0, 1)
               : TDuration(faceValue2DurationType(fv)).fraction();
    };

    for (auto& [key, chords] : voiceChords) {
        int n = static_cast<int>(chords.size());

        // Segment-override pre-pass: find contiguous runs of notes with the same
        // explicit tup=an:nn byte where N is not a clean multiple of an (so the run
        // doesn't form a whole number of standard groups). Compute m from the space
        // available after accounting for any notes before or after the segment, then
        // reinterpret as [N:m].  Works at any position in the measure.
        //
        // Formula: available = durTicks - leading_dur - trailing_dur
        //          m = round(available / fv_ticks)
        //
        // Examples:
        //   M7 (15 tup=9:5, no tail):   available=960, m=960/120=8  → [15:8] ✓
        //   M7 (12 tup=9:5 + 2 plain):  available=720, m=720/120=6  → [12:6] ✓
        //   Mid-measure (2 plain + 12):  available=720, m=6          → [12:6] ✓
        if (overrideRatios && n >= 2 && encMeas.durTicks > 0) {
            // Helper: actual Encore-tick duration of chord at index k.
            // = fv_ticks × (nn/an) for explicit tup, = fv_ticks for plain note.
            auto actualDurEnc = [&](int k) -> int {
                if (k < 0 || k >= n || chords[k].empty()) {
                    return 0;
                }
                const EncMeasureElem* ch = chords[k][0];
                EncElemType et = static_cast<EncElemType>(ch->type);
                quint8 fv = 0;
                quint8 tupByte = 0;
                if (et == EncElemType::NOTE) {
                    fv      = static_cast<const EncNote*>(ch)->faceValue & 0x0F;
                    tupByte = static_cast<const EncNote*>(ch)->tuplet;
                } else if (et == EncElemType::REST) {
                    fv      = static_cast<const EncRest*>(ch)->faceValue & 0x0F;
                    tupByte = static_cast<const EncRest*>(ch)->tuplet;
                }
                const int fvt = faceValue2ticks(fv);
                const int an2 = tupByte >> 4, nn2 = tupByte & 0x0F;
                if (an2 > 0 && nn2 > 0) {
                    return (fvt * nn2 + an2 / 2) / an2;  // tuplet-scaled, rounded
                }
                return fvt;
            };

            int j = 0;
            while (j < n) {
                int a0 = 0, n0 = 0;
                getExplicit(chords[j], a0, n0);
                if (a0 <= 0) {
                    ++j;
                    continue;
                }

                // Find end of same-tup run.
                int segStart = j;
                while (j < n) {
                    int aj = 0, nj = 0;
                    getExplicit(chords[j], aj, nj);
                    if (aj != a0 || nj != n0) {
                        break;
                    }
                    ++j;
                }
                int N = j - segStart;

                // Must exceed one complete group AND not be a clean multiple.
                if (N <= a0 || (N % a0) == 0) {
                    continue;
                }

                // All notes in segment must share the same face value and be NOTEs.
                const Fraction fv0 = getFaceValue(chords[segStart]);
                if (fv0 <= Fraction(0, 1)) {
                    continue;
                }
                bool allOk = !chords[segStart].empty()
                             && (static_cast<EncElemType>(chords[segStart][0]->type) == EncElemType::NOTE);
                for (int k = segStart + 1; k < j && allOk; ++k) {
                    allOk = !chords[k].empty()
                            && (static_cast<EncElemType>(chords[k][0]->type) == EncElemType::NOTE)
                            && (getFaceValue(chords[k]) == fv0);
                }
                if (!allOk) {
                    continue;
                }

                // Compute leading and trailing actual durations.
                int leadingDur = 0;
                for (int k = 0; k < segStart; ++k) {
                    leadingDur += actualDurEnc(k);
                }
                int trailingDur = 0;
                for (int k = j; k < n; ++k) {
                    trailingDur += actualDurEnc(k);
                }

                const int available = static_cast<int>(encMeas.durTicks) - leadingDur - trailingDur;
                if (available <= 0) {
                    continue;
                }

                // fv_ticks in Encore: face value as a fraction of durTicks.
                // fv0 is in "whole note = 1" units; durTicks covers one measure.
                // fv_enc = fv0 × durTicks (= baseLen in Encore ticks).
                const int fvEnc = static_cast<int>(
                    static_cast<long long>(fv0.numerator()) * encMeas.durTicks / fv0.denominator());
                if (fvEnc <= 0) {
                    continue;
                }

                // m = round(available / fvEnc).
                const int m = (available + fvEnc / 2) / fvEnc;
                if (m <= 0) {
                    continue;
                }

                // Tolerance: |m × fvEnc - available| < 10% of available.
                if (std::abs(m * fvEnc - available) * 10 > available) {
                    continue;
                }

                const Fraction tupTicks = fv0 * m;
                if (!fitsTDuration(tupTicks) || !isStandardExplicitTuplet(N, m)) {
                    continue;
                }

                // Only override when the NON-override interpretation overflows the measure.
                // Non-override total for the segment: complete groups + orphan plain notes.
                //   = floor(N/a0) × (fvEnc × n0) + (N%a0) × fvEnc
                // If this fits within available space (no overflow without override), the
                // regular explicit-group logic handles it correctly and we must not override.
                // Example: 4 notes tup=3:2 + 1 plain Q: non-override = 480+240=720, trailing=240,
                //   total=960 ≤ 960 → leave alone (note 4 is a legit isolated tup note).
                {
                    const int completeTicks = (N / a0) * (fvEnc * n0);
                    const int orphanTicks   = (N % a0) * fvEnc;
                    const int noOverrideTotal = leadingDur + completeTicks + orphanTicks + trailingDur;
                    if (noOverrideTotal <= static_cast<int>(encMeas.durTicks)) {
                        continue;  // non-override interpretation fits: skip override
                    }
                }

                // Valid segment override: mark notes and record override ratio.
                for (int k = segStart; k < j; ++k) {
                    for (const EncMeasureElem* e2 : chords[k]) {
                        result.insert(e2);
                        (*overrideRatios)[e2] = { N, m };
                    }
                }
            }
        }

        int i = 0;
        while (i < n) {
            // Skip chords already handled by the segment-override pre-pass.
            if (overrideRatios && !chords[i].empty()
                && overrideRatios->count(chords[i][0])) {
                ++i;
                continue;
            }

            // Try explicit tuplet first, then implied.
            int actualN = 0, normalN = 0;
            bool isExplicit = false;
            getExplicit(chords[i], actualN, normalN);
            if (actualN > 0) {
                isExplicit = true;
            } else {
                getImplied(chords[i], actualN, normalN);
            }
            if (actualN < 2 || normalN < 1) {
                ++i;
                continue;
            }

            if (isExplicit) {
                // Explicit: group by face-value sum using a no-downdate baseLen rule.
                // baseLen starts as the first note's face value and only updates to a
                // smaller value when the current faceSum still fits within the new
                // (lower) threshold. This correctly handles mixed-duration brackets:
                //   {Q,E}/3:2: baseLen=Q, threshold=3Q. Before E: faceSum=Q<=3E → update
                //              baseLen=E, threshold=3E. faceSum(Q+E)=3E → close. ✓
                //   {Q,Q,8,8}/3:2: before 8th: faceSum=2Q>3/8 → no update. threshold stays
                //              3Q. faceSum(Q+Q+8+8)=3Q → close after 4. ✓
                //   {Q,Q,Q,Q}/4:3: uniform Q, threshold=4Q. Closes after 4. ✓
                //   {Q,Q}/4:3 partial: faceSum(2Q)<4Q → continues. ✓
                Fraction baseLen = getFaceValue(chords[i]);
                if (baseLen <= Fraction(0, 1)) {
                    ++i;
                    continue;
                }
                Fraction threshold = baseLen * actualN;
                Fraction faceSum(0, 1);
                int groupStart = i;
                Fraction originalBaseLen = baseLen;  // before any no-downdate
                int innerGroupStartIdx = -1;          // index of inner group's first chord
                Fraction innerBaseLen(0, 1);
                bool seenCompleteGroup = false;        // at least one full group closed
                while (i < n) {
                    int a2 = 0, n2 = 0;
                    getExplicit(chords[i], a2, n2);
                    if (a2 != actualN || n2 != normalN) {
                        // Sandwich heuristic: include a note with a missing tup byte when
                        // it is sandwiched between two notes with the same explicit ratio
                        // and lies at the expected triplet-advance tick position.
                        // Real-world trigger: live-recorded v0xC4 files occasionally omit
                        // the tup byte on one note in the middle of a triplet run.
                        bool includeOrphan = false;
                        if (faceSum > Fraction(0, 1) && i + 1 < n
                            && !chords[i].empty() && !chords[i - 1].empty()) {
                            int a3 = 0, n3 = 0;
                            getExplicit(chords[i + 1], a3, n3);
                            const Fraction fvOrphan = getFaceValue(chords[i]);
                            if (a3 == actualN && n3 == normalN && fvOrphan == baseLen) {
                                // Check that the orphan is at the expected advance tick.
                                const int advNum = baseLen.numerator() * normalN;
                                const int advDen = baseLen.denominator() * actualN;
                                const int advTicks = (advNum * 960 + advDen / 2) / advDen;
                                const int tol = std::max(4, advTicks / 4);
                                const int lastTick   = static_cast<int>(chords[i - 1][0]->tick);
                                const int orphanTick = static_cast<int>(chords[i][0]->tick);
                                if (std::abs(orphanTick - lastTick - advTicks) <= tol) {
                                    includeOrphan = true;
                                    a2 = actualN;
                                    n2 = normalN;
                                }
                            }
                        }
                        if (!includeOrphan) {
                            break;
                        }
                    }
                    // No-downdate: update baseLen only when the NEW smaller face value
                    // still fits within the new threshold given the CURRENT faceSum.
                    const Fraction fv_i = getFaceValue(chords[i]);
                    if (fv_i > Fraction(0, 1) && fv_i < baseLen) {
                        const Fraction newThreshold = fv_i * actualN;
                        if (faceSum <= newThreshold) {
                            // Record where the inner group starts (= the downdating note).
                            innerGroupStartIdx = i;
                            innerBaseLen       = fv_i;
                            baseLen    = fv_i;
                            threshold  = newThreshold;
                        }
                    }
                    faceSum += fv_i;
                    ++i;
                    if (faceSum < threshold) {
                        continue;
                    }
                    // Complete group: mark all in [groupStart, i-1]
                    for (int j = groupStart; j < i; ++j) {
                        for (const EncMeasureElem* e : chords[j]) {
                            result.insert(e);
                        }
                    }

                    // Nested-tuplet detection: when the group closed via a no-downdate
                    // baseLen reduction AND the downdating note + the next (actualN-1)
                    // notes in the following group all share the same smaller face value,
                    // they form a complete inner triplet. Record a NestedTupletInfo for
                    // the noteloop to use when building nested Tuplet objects.
                    if (nestedInfos && innerGroupStartIdx >= 0
                        && innerBaseLen > Fraction(0, 1)
                        && innerBaseLen < originalBaseLen) {
                        // Peek ahead: do the next (actualN-1) notes of the next group
                        // share innerBaseLen and the same ratio, completing an inner group?
                        int peekAhead = actualN - (i - innerGroupStartIdx);  // remaining inner notes needed
                        bool innerOk = (peekAhead >= 0);
                        int innerEndIdx = i - 1;  // last note of inner group so far (= end of current group)
                        if (innerOk && peekAhead > 0) {
                            innerOk = false;
                            if (i + peekAhead <= n) {
                                innerOk = true;
                                for (int p = 0; p < peekAhead && innerOk; ++p) {
                                    int a3 = 0, n3 = 0;
                                    getExplicit(chords[i + p], a3, n3);
                                    if (a3 != actualN || n3 != normalN) {
                                        innerOk = false;
                                    } else {
                                        const Fraction fvp = getFaceValue(chords[i + p]);
                                        if (fvp != innerBaseLen) {
                                            innerOk = false;
                                        }
                                    }
                                }
                                if (innerOk) {
                                    innerEndIdx = i + peekAhead - 1;
                                }
                            }
                        }
                        if (innerOk && innerEndIdx >= innerGroupStartIdx) {
                            NestedTupletInfo ni;
                            ni.outerActualN = actualN;
                            ni.outerNormalN = normalN;
                            ni.innerActualN = actualN;
                            ni.innerNormalN = normalN;
                            if (!chords[innerGroupStartIdx].empty()) {
                                ni.innerFirst = chords[innerGroupStartIdx][0];
                            }
                            if (!chords[innerEndIdx].empty()) {
                                ni.innerLast = chords[innerEndIdx][0];
                            }
                            if (ni.innerFirst && ni.innerLast) {
                                nestedInfos->push_back(ni);
                                // Mark the peeked-ahead notes as group members too.
                                for (int p = 0; p < peekAhead; ++p) {
                                    for (const EncMeasureElem* e2 : chords[i + p]) {
                                        result.insert(e2);
                                    }
                                }
                            }
                        }
                    }

                    faceSum   = Fraction(0, 1);
                    groupStart = i;
                    originalBaseLen    = Fraction(0, 1);
                    innerGroupStartIdx = -1;
                    innerBaseLen       = Fraction(0, 1);
                    seenCompleteGroup  = true;
                    // Reset baseLen for the next group (may start with a different face value)
                    if (i < n) {
                        baseLen         = getFaceValue(chords[i]);
                        threshold       = baseLen * actualN;
                        originalBaseLen = baseLen;
                    }
                }
                // Mark a partial end group only when rdur fills to exact measure end AND face-value sum would overflow without tuplet scaling.
                // Condition (b) prevents false-positives where plain advances already fill the measure (e.g. {Q,8,8} triplet at tick=0).
                if (i > groupStart) {
                    int startTick = 0;
                    if (!chords[groupStart].empty()) {
                        startTick = static_cast<int>(chords[groupStart][0]->tick);
                    }
                    int rdurSum = 0;
                    int faceTickSum = 0;
                    for (int j = groupStart; j < i; ++j) {
                        if (!chords[j].empty()) {
                            const EncMeasureElem* ch = chords[j][0];
                            rdurSum += std::max(0, static_cast<int>(ch->realDuration));
                            EncElemType cht = static_cast<EncElemType>(ch->type);
                            quint8 fv = 0;
                            if (cht == EncElemType::NOTE) {
                                fv = static_cast<const EncNote*>(ch)->faceValue & 0x0F;
                            } else if (cht == EncElemType::REST) {
                                fv = static_cast<const EncRest*>(ch)->faceValue & 0x0F;
                            }
                            faceTickSum += faceValue2ticks(fv);
                        }
                    }
                    const bool rdurFillsMeasure = (rdurSum > 0)
                                                  && (startTick + rdurSum == static_cast<int>(encMeas.durTicks));
                    const bool faceWouldOverflow = (startTick + faceTickSum
                                                    > static_cast<int>(encMeas.durTicks));
                    // Extra guard: verify the TUPLET ADVANCE for these n notes approximately
                    // fills the remaining measure. Without this, notes whose rdur sums happen
                    // to reach durTicks (e.g. tail MIDI notes after a complete group) would be
                    // incorrectly treated as a partial group.
                    // tupletAdvance = sum(faceValue_ticks × normalN) / actualN
                    // expectedRemaining = durTicks - startTick
                    // Only mark a partial group when no complete group of this ratio was
                    // already found in this measure. Without this guard, tail MIDI notes after
                    // a complete group (e.g. notes 10-15 after a full [9:5] group) can have
                    // rdur values that happen to sum to measure end, falsely triggering a
                    // second partial group.
                    if (rdurFillsMeasure && faceWouldOverflow && !seenCompleteGroup) {
                        for (int j = groupStart; j < i; ++j) {
                            for (const EncMeasureElem* e2 : chords[j]) {
                                result.insert(e2);
                                if (partialEndGroup) {
                                    partialEndGroup->insert(e2);
                                }
                            }
                        }
                    }
                }
            } else {
                // Implied tuplets (v0xC2): check for exactly actualN consecutive groups.
                if (i + actualN > n) {
                    ++i;
                    continue;
                }
                bool allMatch = true;
                for (int j = 1; j < actualN && allMatch; ++j) {
                    int a2 = 0, n2 = 0;
                    getImplied(chords[i + j], a2, n2);
                    allMatch = (a2 == actualN && n2 == normalN);
                }
                if (allMatch) {
                    for (int j = 0; j < actualN; ++j) {
                        for (const EncMeasureElem* e : chords[i + j]) {
                            result.insert(e);
                        }
                    }
                    i += actualN;
                } else {
                    ++i;
                }
            }
        }
    }
    return result;
}
} // namespace mu::iex::encore
