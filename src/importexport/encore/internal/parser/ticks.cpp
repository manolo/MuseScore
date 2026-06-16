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

#include "ticks.h"

#include <cstdlib>

using namespace mu::engraving;

namespace mu::iex::enc {
int faceValue2ticks(quint8 fv)
{
    switch (fv & 0x0F) {
    case 1: return 960;
    case 2: return 480;
    case 3: return 240;
    case 4: return 120;
    case 5: return 60;
    case 6: return 30;
    case 7: return 15;
    case 8: return 7;
    default: return 0;
    }
}

DurationType faceValue2DurationType(quint8 fv)
{
    switch (fv & 0x0F) {
    case 1: return DurationType::V_WHOLE;
    case 2: return DurationType::V_HALF;
    case 3: return DurationType::V_QUARTER;
    case 4: return DurationType::V_EIGHTH;
    case 5: return DurationType::V_16TH;
    case 6: return DurationType::V_32ND;
    case 7: return DurationType::V_64TH;
    case 8: return DurationType::V_128TH;
    default: return DurationType::V_QUARTER;
    }
}

// Reject rdur-based dot promotion when rdur is inflated by gap-to-next-event, not a real dotted value.
static bool inflatedDottedPromotion(qint16 realDur, quint8 fv)
{
    int faceTicks = faceValue2ticks(fv);
    return faceTicks > 0 && realDur > faceTicks && calcDots(realDur, fv) == 0;
}

// Maps the actual MIDI duration to the best MuseScore DurationType.
DurationType realDuration2DurationType(qint16 realDur, quint8 fv)
{
    if (realDur <= 0) {
        return faceValue2DurationType(fv);
    }
    // Multi-stream MIDI overlap can shorten rdur below the written value; trust face value when rdur < faceTicks.
    const int faceTicks = faceValue2ticks(fv);
    if (faceTicks > 0 && realDur < faceTicks) {
        return faceValue2DurationType(fv);
    }
    switch (realDur) {
    case 960: return DurationType::V_WHOLE;
    case 480: return DurationType::V_HALF;
    case 240: return DurationType::V_QUARTER;
    case 120: return DurationType::V_EIGHTH;
    case  60: return DurationType::V_16TH;
    case  30: return DurationType::V_32ND;
    case  15: return DurationType::V_64TH;
    case 720: return inflatedDottedPromotion(720, fv) ? faceValue2DurationType(fv) : DurationType::V_HALF;
    case 360: return inflatedDottedPromotion(360, fv) ? faceValue2DurationType(fv) : DurationType::V_QUARTER;
    case 180: return inflatedDottedPromotion(180, fv) ? faceValue2DurationType(fv) : DurationType::V_EIGHTH;
    case  90: return inflatedDottedPromotion(90,  fv) ? faceValue2DurationType(fv) : DurationType::V_16TH;
    case  45: return inflatedDottedPromotion(45,  fv) ? faceValue2DurationType(fv) : DurationType::V_32ND;
    // Triplet rdur (160/80/40...) falls through; detectImpliedTuplet handles them.
    // Mapping to longer type misrepresents notes not in a tuplet context.
    default:  return faceValue2DurationType(fv);
    }
}

int calcDots(qint16 realDur, quint8 fv)
{
    int base = faceValue2ticks(fv);
    if (base <= 0 || realDur <= 0 || realDur == base) {
        return 0;
    }
    // Guard: skip if the dotted value is non-integer (base not divisible by
    // the denominator).  Integer division would truncate e.g. 112.5 → 112,
    // creating a false match for a note whose rdur happens to equal that
    // truncated value (seen with 16th notes where 60*15/8=112 ≠ 112.5).
    if ((base * 3) % 2 == 0 && realDur == (base * 3) / 2) {
        return 1;
    }
    if ((base * 7) % 4 == 0 && realDur == (base * 7) / 4) {
        return 2;
    }
    if ((base * 15) % 8 == 0 && realDur == (base * 15) / 8) {
        return 3;
    }
    return 0;
}

int calcDotsSnap(qint16 dur, quint8 fv)
{
    static constexpr int DOTS_SNAP_TOL = 1;

    int base = faceValue2ticks(fv);
    if (base <= 0 || dur <= 0) {
        return 0;
    }
    auto near = [](int a, int b) { return std::abs(a - b) <= DOTS_SNAP_TOL; };
    if (near(dur, base)) {
        return 0;
    }
    if ((base * 3) % 2 == 0 && near(dur, (base * 3) / 2)) {
        return 1;
    }
    if ((base * 7) % 4 == 0 && near(dur, (base * 7) / 4)) {
        return 2;
    }
    if ((base * 15) % 8 == 0 && near(dur, (base * 15) / 8)) {
        return 3;
    }
    return 0;
}

int detectImpliedTuplet(qint16 realDur, quint8 fv, int& normalNotes)
{
    int base = faceValue2ticks(fv);
    if (base <= 0 || realDur <= 0) {
        normalNotes = 0;
        return 0;
    }
    // Triplet (3:2): realDuration = base * 2/3
    if (realDur * 3 == base * 2) {
        normalNotes = 2;
        return 3;
    }
    // Quintuplet (5:4): realDuration = base * 4/5
    if (realDur * 5 == base * 4) {
        normalNotes = 4;
        return 5;
    }
    normalNotes = 0;
    return 0;
}

Fraction dottedAdvance(DurationType durationType, int dots)
{
    Fraction multiplier = Fraction(1, 1);
    if (dots == 1) {
        multiplier = Fraction(3, 2);
    } else if (dots == 2) {
        multiplier = Fraction(7, 4);
    } else if (dots >= 3) {
        multiplier = Fraction(15, 8);
    }
    return TDuration(durationType).fraction() * multiplier;
}

int computeDotCount(quint8 dotControl, qint16 realDuration, quint8 faceValue, bool useBit0Fallback)
{
    if (dotControl > 0) {
        const int dByCtrl = calcDots(static_cast<qint16>(dotControl), faceValue);
        if (dByCtrl > 0) {
            return dByCtrl;
        }
        const int dBySnap = calcDotsSnap(realDuration, faceValue);
        if (dBySnap > 0) {
            return dBySnap;
        }
        if (useBit0Fallback && (dotControl & 1)) {
            return 1;   // bit 0 = Encore's dotted flag; force dot when rdur drift is too large
        }
        return 0;
    }
    return calcDotsSnap(realDuration, faceValue);
}

bool isStandardExplicitTuplet(int actualN, int normalN)
{
    if (actualN < 2 || normalN < 1) {
        return false;
    }
    // Common tuplets (the "standard 4" always supported)
    if ((actualN == 3 && normalN == 2)
        || (actualN == 4 && normalN == 3)
        || (actualN == 5 && normalN == 4)
        || (actualN == 6 && normalN == 4)) {
        return true;
    }
    // normalN must yield a TDuration-aligned fraction (normalN × baseLen).
    // normalN=5,10,15,20 give 5/(2^k), which is not representable; 7 is safe (double-dotted).
    if (normalN == 10 || normalN == 15 || normalN == 20) {
        return false;
    }

    // Duplets: 2 in the time of 1 (dosillo), or 2 in time of 3 (compound-meter duplet)
    if (actualN == 2 && (normalN == 1 || normalN == 3)) {
        return true;
    }
    // 2:4 (dosillo spanning a full measure's face value)
    if (actualN == 2 && normalN == 4) {
        return true;
    }
    // Quintuplets with safe normalN: 5:2, 5:3, 5:6, 5:8
    if (actualN == 5 && (normalN == 2 || normalN == 3 || normalN == 6 || normalN == 8)) {
        return true;
    }
    // Sextuplets: 6:5 excluded (normalN=5 unsafe); 6:7, 6:8 safe
    if (actualN == 6 && (normalN == 7 || normalN == 8)) {
        return true;
    }
    // Septuplets: 7:4, 7:6, 7:8
    if (actualN == 7 && (normalN == 4 || normalN == 6 || normalN == 8)) {
        return true;
    }
    // Octuplets: 8:4, 8:6
    if (actualN == 8 && (normalN == 4 || normalN == 6)) {
        return true;
    }
    // Segment-override groups: normalN in {4,6,8} always yields a standard TDuration.
    if ((normalN == 4 || normalN == 6 || normalN == 8)
        && actualN >= 2 && actualN <= 64) {
        return true;
    }
    // 9:5 is non-TDuration-aligned but closeTuplet sets ticks after placement (same as sanitizeTuplet).
    if (actualN == 9 && (normalN == 4 || normalN == 5 || normalN == 6 || normalN == 8)) {
        return true;
    }
    // Decuplets with safe normalN: 10:6, 10:8
    if (actualN == 10 && (normalN == 6 || normalN == 8)) {
        return true;
    }
    // Unusual observed: 4:1, 4:2
    if (actualN == 4 && (normalN == 1 || normalN == 2)) {
        return true;
    }
    return false;
}
} // namespace mu::iex::enc
