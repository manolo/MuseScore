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

#include <gtest/gtest.h>

#include "../internal/encorerhythm.h"

using namespace mu::engraving;
using namespace mu::iex::encore;

TEST(Tst_EncoreRhythm, faceValueToTicks)
{
    EXPECT_EQ(faceValue2ticks(1), 960);
    EXPECT_EQ(faceValue2ticks(2), 480);
    EXPECT_EQ(faceValue2ticks(3), 240);
    EXPECT_EQ(faceValue2ticks(4), 120);
    EXPECT_EQ(faceValue2ticks(5), 60);
    EXPECT_EQ(faceValue2ticks(6), 30);
    EXPECT_EQ(faceValue2ticks(7), 15);
    EXPECT_EQ(faceValue2ticks(8), 7);
    EXPECT_EQ(faceValue2ticks(0), 0);
    EXPECT_EQ(faceValue2ticks(9), 0);
    EXPECT_EQ(faceValue2ticks(0xFF), 0);
    // Upper nibble must be ignored (fv & 0x0F).
    EXPECT_EQ(faceValue2ticks(0x14), 120);
    EXPECT_EQ(faceValue2ticks(0xF8), 7);
}

TEST(Tst_EncoreRhythm, faceValueToDurationType)
{
    EXPECT_EQ(faceValue2DurationType(1), DurationType::V_WHOLE);
    EXPECT_EQ(faceValue2DurationType(2), DurationType::V_HALF);
    EXPECT_EQ(faceValue2DurationType(3), DurationType::V_QUARTER);
    EXPECT_EQ(faceValue2DurationType(4), DurationType::V_EIGHTH);
    EXPECT_EQ(faceValue2DurationType(5), DurationType::V_16TH);
    EXPECT_EQ(faceValue2DurationType(6), DurationType::V_32ND);
    EXPECT_EQ(faceValue2DurationType(7), DurationType::V_64TH);
    EXPECT_EQ(faceValue2DurationType(8), DurationType::V_128TH);
    // Invalid face values fall back to V_QUARTER.
    EXPECT_EQ(faceValue2DurationType(0), DurationType::V_QUARTER);
    EXPECT_EQ(faceValue2DurationType(9), DurationType::V_QUARTER);
    // Upper nibble ignored.
    EXPECT_EQ(faceValue2DurationType(0x14), DurationType::V_EIGHTH);
}

TEST(Tst_EncoreRhythm, realDurationToDurationType)
{
    // Plain duration mappings.
    EXPECT_EQ(realDuration2DurationType(960, 1), DurationType::V_WHOLE);
    EXPECT_EQ(realDuration2DurationType(480, 1), DurationType::V_HALF);
    EXPECT_EQ(realDuration2DurationType(240, 3), DurationType::V_QUARTER);
    EXPECT_EQ(realDuration2DurationType(120, 4), DurationType::V_EIGHTH);
    EXPECT_EQ(realDuration2DurationType(60, 5), DurationType::V_16TH);
    // Dotted mappings.
    EXPECT_EQ(realDuration2DurationType(720, 1), DurationType::V_HALF);
    EXPECT_EQ(realDuration2DurationType(360, 3), DurationType::V_QUARTER);
    EXPECT_EQ(realDuration2DurationType(180, 4), DurationType::V_EIGHTH);
    // Triplet rdur values fall back to faceValue: a triplet 8th (rdur=80
    // for fv=eighth=120) is still notated as an eighth, with the 3:2 ratio
    // expressed via the tuplet wrapper rather than by upgrading the type.
    EXPECT_EQ(realDuration2DurationType(160, 4), DurationType::V_EIGHTH);
    EXPECT_EQ(realDuration2DurationType(80, 4), DurationType::V_EIGHTH);
    EXPECT_EQ(realDuration2DurationType(80, 5), DurationType::V_16TH);
    EXPECT_EQ(realDuration2DurationType(40, 5), DurationType::V_16TH);
    // realDur <= 0 falls back to faceValue2DurationType.
    EXPECT_EQ(realDuration2DurationType(0, 5), DurationType::V_16TH);
    EXPECT_EQ(realDuration2DurationType(-1, 4), DurationType::V_EIGHTH);
    // Unknown realDur also falls back to faceValue2DurationType.
    EXPECT_EQ(realDuration2DurationType(99, 4), DurationType::V_EIGHTH);
    EXPECT_EQ(realDuration2DurationType(31000, 3), DurationType::V_QUARTER);
    // Inflated dotted ratios: when calculateRealDurations sets rdur to the
    // gap-to-next-event spacing for a note that has no following event in
    // its voice, rdur can land on a dotted-multiple of a LONGER face value.
    // The face is authoritative in that case (rdur > faceTicks AND not a
    // real dotted multiple of the face): a face=quarter note with rdur=720
    // (chord-only voice with implicit trailing silence) stays a quarter,
    // not a dotted half.
    EXPECT_EQ(realDuration2DurationType(720, 3), DurationType::V_QUARTER);
    EXPECT_EQ(realDuration2DurationType(360, 4), DurationType::V_EIGHTH);
    EXPECT_EQ(realDuration2DurationType(180, 5), DurationType::V_16TH);
    // But a face=quarter + rdur=360 IS a real dotted quarter (calcDots>0),
    // so the dotted mapping still applies.
    EXPECT_EQ(realDuration2DurationType(360, 3), DurationType::V_QUARTER);
}

TEST(Tst_EncoreRhythm, dotCalculation)
{
    // Strict mode.
    EXPECT_EQ(calcDots(180, 4), 1);    // 120 * 3/2
    EXPECT_EQ(calcDots(210, 4), 2);    // 120 * 7/4
    EXPECT_EQ(calcDots(225, 4), 3);    // 120 * 15/8
    EXPECT_EQ(calcDots(120, 4), 0);    // == base
    EXPECT_EQ(calcDots(181, 4), 0);    // strict mode: no snap
    EXPECT_EQ(calcDots(0, 4), 0);      // dur <= 0
    EXPECT_EQ(calcDots(-1, 4), 0);
    EXPECT_EQ(calcDots(180, 0), 0);    // base <= 0
    EXPECT_EQ(calcDots(180, 9), 0);

    // Snap mode (±1 tick tolerance).
    EXPECT_EQ(calcDotsSnap(181, 4), 1);
    EXPECT_EQ(calcDotsSnap(179, 4), 1);
    EXPECT_EQ(calcDotsSnap(178, 4), 0);    // 2 ticks off → outside tolerance
    EXPECT_EQ(calcDotsSnap(211, 4), 2);
    EXPECT_EQ(calcDotsSnap(226, 4), 3);
    EXPECT_EQ(calcDotsSnap(121, 4), 0);    // base ± 1 → 0 dots
    EXPECT_EQ(calcDotsSnap(0, 4), 0);
    EXPECT_EQ(calcDotsSnap(180, 0), 0);
}

TEST(Tst_EncoreRhythm, impliedTuplets)
{
    int normalNotes = 0;

    // Triplet (3:2): 120 * 2/3 = 80.
    EXPECT_EQ(detectImpliedTuplet(80, 4, normalNotes), 3);
    EXPECT_EQ(normalNotes, 2);

    // Quintuplet (5:4): 120 * 4/5 = 96.
    normalNotes = 0;
    EXPECT_EQ(detectImpliedTuplet(96, 4, normalNotes), 5);
    EXPECT_EQ(normalNotes, 4);

    // Exact match is not a tuplet.
    normalNotes = 7;
    EXPECT_EQ(detectImpliedTuplet(120, 4, normalNotes), 0);
    EXPECT_EQ(normalNotes, 0);

    // Invalid base resets normalNotes to 0.
    normalNotes = 7;
    EXPECT_EQ(detectImpliedTuplet(120, 0, normalNotes), 0);
    EXPECT_EQ(normalNotes, 0);

    // Non-positive realDur resets normalNotes to 0.
    normalNotes = 7;
    EXPECT_EQ(detectImpliedTuplet(0, 4, normalNotes), 0);
    EXPECT_EQ(normalNotes, 0);
}

TEST(Tst_EncoreRhythm, dottedAdvance)
{
    EXPECT_EQ(dottedAdvance(DurationType::V_EIGHTH, 0), Fraction(1, 8));
    EXPECT_EQ(dottedAdvance(DurationType::V_EIGHTH, 1), Fraction(3, 16));
    EXPECT_EQ(dottedAdvance(DurationType::V_EIGHTH, 2), Fraction(7, 32));
    EXPECT_EQ(dottedAdvance(DurationType::V_EIGHTH, 3), Fraction(15, 64));
    // dots >= 3 clamps to 15/8 multiplier.
    EXPECT_EQ(dottedAdvance(DurationType::V_EIGHTH, 4), Fraction(15, 64));
    EXPECT_EQ(dottedAdvance(DurationType::V_EIGHTH, 99), Fraction(15, 64));
    // Quarter base.
    EXPECT_EQ(dottedAdvance(DurationType::V_QUARTER, 0), Fraction(1, 4));
    EXPECT_EQ(dottedAdvance(DurationType::V_QUARTER, 1), Fraction(3, 8));
}
