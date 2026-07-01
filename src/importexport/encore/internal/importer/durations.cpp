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

// Rendering decisions derived from raw Encore ticks (see durations.h).

#include "durations.h"

#include <cstdlib>
#include <utility>

#include "../parser/ticks.h"

using namespace mu::engraving;

namespace mu::iex::enc {
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
    // When rdur exceeds faceTicks but is NOT a dotted augmentation of faceValue, the excess is a
    // trailing gap to the next event (rest or next note held at its original position after a
    // duration change). Trust the written face value. calcDots == 0 identifies this case.
    if (inflatedDottedPromotion(realDur, fv)) {
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
    case 720: return DurationType::V_HALF;
    case 360: return DurationType::V_QUARTER;
    case 180: return DurationType::V_EIGHTH;
    case  90: return DurationType::V_16TH;
    case  45: return DurationType::V_32ND;
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
            // Guard: only force a dot when rdur > faceTicks.
            // A dotted note has a longer MIDI duration than the plain face value, so
            // rdur must exceed faceTicks for the bit-0 flag to be a plausible dotted
            // indicator.  When rdur <= faceTicks the note is plain (exact match) or
            // shorter than the face value (multi-stream overlap / timing slop); bit 0
            // in dotControl may then be a spurious layout flag rather than a dotted
            // indicator (observed in v0xC2 files such as tapada.enc).
            // The dotted-eighth anomaly in v0xC2 is handled via EncNote::forceDotted.
            const int faceTicks = faceValue2ticks(faceValue);
            if (faceTicks > 0 && realDuration > faceTicks) {
                return 1;
            }
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
    // normalN must yield a TDuration-aligned fraction (normalN x baseLen). normalN=5,10,15,20 give
    // 5/(2^k), not representable; 10/15/20 are rejected outright (the few valid 5:x and 9:5 ratios
    // are whitelisted below), while 7 is safe (double-dotted).
    if (normalN == 10 || normalN == 15 || normalN == 20) {
        return false;
    }
    // Whitelisted ratios whose normalN is NOT in {4,6,8} (those are covered by the blanket rule
    // below). With normalN in {1,2,3,5,7} only these specific pairs are TDuration-aligned:
    //   3:2, 4:3 (common); 2:1/2:3 (duplets); 5:2/5:3; 6:7; 9:5; 4:1/4:2 (observed).
    static const std::pair<int, int> kStandardRatios[] = {
        { 3, 2 }, { 4, 3 }, { 2, 1 }, { 2, 3 }, { 5, 2 }, { 5, 3 }, { 6, 7 }, { 9, 5 }, { 4, 1 }, { 4, 2 },
    };
    for (const auto& r : kStandardRatios) {
        if (actualN == r.first && normalN == r.second) {
            return true;
        }
    }
    // Blanket rule: normalN in {4,6,8} always yields a standard TDuration for 2..64-tuplets. This
    // does NOT subsume the whitelist above, which uses normalN in {1,2,3,5,7}. Covers e.g. 5:4/6:4,
    // 2:4, 5:6/5:8, 6:8, 7:4/7:6/7:8, 8:4/8:6, 9:4/9:6/9:8, 10:6/10:8.
    if ((normalN == 4 || normalN == 6 || normalN == 8)
        && actualN >= 2 && actualN <= 64) {
        return true;
    }
    return false;
}

bool isCompoundBeat(quint16 rawBeatTicks, Fraction timesig)
{
    // Explicit dotted-quarter beat, or a compound x/8 meter (6/8, 9/8, 12/8, ...) whose legacy
    // files still store beatTicks=240.
    return rawBeatTicks == 360
           || (timesig.denominator() == 8
               && timesig.numerator() % 3 == 0
               && timesig.numerator() > 3);
}
} // namespace mu::iex::enc
