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
        const bool mixedValueOvershoot = (placedTicks > expected)
                                         && (faceTicks > fullFaceSum);
        const bool willSet = (placedTicks < expected || mixedValueOvershoot)
                             && fitsTDuration(placedTicks);
        if (willSet) {
            currentTuplet->setTicks(placedTicks);
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
    Fraction tupletDuration = TDuration(baseType).fraction() * normalN_;
    currentTuplet->setTicks(tupletDuration);
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
    std::set<const EncMeasureElem*>* partialEndGroup)
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
        if ((a == 3 && n == 2) || (a == 5 && n == 4) || (a == 6 && n == 4)) {
            outActual=a;
            outNormal=n;
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
        int i = 0;
        while (i < n) {
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
                // Explicit: group by face-value sum (actualN × baseLen). A mixed group ({8,8,16,16} in 3:2) closes at 3/8, not after 3 notes.
                // Only complete groups are marked; partial remainders handled by isolated-fill-remaining in the main element loop.
                Fraction baseLen = getFaceValue(chords[i]);
                if (baseLen <= Fraction(0, 1)) {
                    ++i;
                    continue;
                }
                Fraction threshold = baseLen * actualN;
                Fraction faceSum(0, 1);
                int groupStart = i;
                while (i < n) {
                    int a2 = 0, n2 = 0;
                    getExplicit(chords[i], a2, n2);
                    if (a2 != actualN || n2 != normalN) {
                        break;
                    }
                    faceSum += getFaceValue(chords[i]);
                    ++i;
                    if (faceSum >= threshold) {
                        // Complete group: mark all in [groupStart, i-1]
                        for (int j = groupStart; j < i; ++j) {
                            for (const EncMeasureElem* e : chords[j]) {
                                result.insert(e);
                            }
                        }
                        faceSum = Fraction(0, 1);
                        groupStart = i;
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
                    if (rdurFillsMeasure && faceWouldOverflow) {
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
