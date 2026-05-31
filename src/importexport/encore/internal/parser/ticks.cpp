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

namespace mu::iex::encore {
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
    // Dotted values
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
    if (realDur == (base * 3) / 2) {
        return 1;
    }
    if (realDur == (base * 7) / 4) {
        return 2;
    }
    if (realDur == (base * 15) / 8) {
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
    if (near(dur, (base * 3) / 2)) {
        return 1;
    }
    if (near(dur, (base * 7) / 4)) {
        return 2;
    }
    if (near(dur, (base * 15) / 8)) {
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
} // namespace mu::iex::encore
