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

#include "../internal/parser/ticks.h"

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
    // Plain duration mappings: realDur == faceTicks, guard does not fire.
    EXPECT_EQ(realDuration2DurationType(960, 1), DurationType::V_WHOLE);
    EXPECT_EQ(realDuration2DurationType(480, 2), DurationType::V_HALF);
    EXPECT_EQ(realDuration2DurationType(240, 3), DurationType::V_QUARTER);
    EXPECT_EQ(realDuration2DurationType(120, 4), DurationType::V_EIGHTH);
    EXPECT_EQ(realDuration2DurationType(60, 5), DurationType::V_16TH);
    // Dotted mappings: realDur > faceTicks, guard does not fire.
    EXPECT_EQ(realDuration2DurationType(720, 2), DurationType::V_HALF);
    EXPECT_EQ(realDuration2DurationType(360, 3), DurationType::V_QUARTER);
    EXPECT_EQ(realDuration2DurationType(180, 4), DurationType::V_EIGHTH);
    // Multi-stream truncation: realDur < faceTicks → face value is authoritative.
    // Encore records 3 MIDI streams per instrument in the same voice; gaps between
    // overlapping streams are shorter than the written note value.
    EXPECT_EQ(realDuration2DurationType(120, 3), DurationType::V_QUARTER);
    EXPECT_EQ(realDuration2DurationType(240, 2), DurationType::V_HALF);
    EXPECT_EQ(realDuration2DurationType(480, 1), DurationType::V_WHOLE);
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

// computeDotCount: v0xC2 dotted-eighth scenario.
// In v0xC2, the dotted-eighth has dotControl=0x60 (bit 0 = 0) and
// realDuration=120 (plain-eighth gap).  Both paths return 0, so the dot is
// missed.  calculateRealDurations fixes this by setting dotControl|=1 on
// the eighth when the E→S@tick+120 pattern is detected.
TEST(Tst_EncoreRhythm, computeDotCount_v0c2_dotted_eighth)
{
    // Pre-fix state: dotControl=0x60, rdur=120 (plain gap), fv=4 (eighth).
    // calcDots(0x60=96, 4): base=120, 96≠180 → 0.
    // calcDotsSnap(120, 4): |120-120|=0 → 0 (exact plain match).
    // bit0 fallback: 0x60 & 1 = 0 → 0.
    EXPECT_EQ(computeDotCount(0x60, 120, 4, /*useBit0Fallback=*/ true), 0)
        << "v0xC2 dotted-eighth without fix: dotControl=0x60 yields 0 dots (bug)";

    // Post-fix state: calculateRealDurations sets dotControl|=1 → dotControl=0x61.
    // calcDots(97, 4): 97≠180 → 0.
    // calcDotsSnap(120, 4): exact match → 0.
    // bit0 fallback: 0x61 & 1 = 1 → 1.
    EXPECT_EQ(computeDotCount(0x61, 120, 4, /*useBit0Fallback=*/ true), 1)
        << "v0xC2 dotted-eighth after fix: dotControl=0x61 (bit 0 set) yields 1 dot";
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

// calcDots and calcDotsSnap must not return non-zero when the theoretical
// dotted value is non-integer (fractional ticks).  Integer division truncates
// (base * n) / d, making the truncated result accidentally equal to some real
// rdur even though no valid dotted duration of that face value exists.
//
// Concrete case from Salome_port_5.enc compás 19: a live-recorded 16th note
// (fv=5, base=60) has realDuration=112 from the tick-diff to the next event.
// 60 * 15 / 8 = 900 / 8 = 112 (C++ integer division; true value = 112.5).
// calcDotsSnap falsely returned 3, making the note a triple-dotted 16th.
TEST(Tst_EncoreRhythm, dotCalculation_noFalsePositiveForFractionalDottedValues)
{
    // 16th (base=60): triple-dotted = 60*15/8 = 112.5 → truncated to 112.
    // rdur=112 must NOT be 3 dots.
    EXPECT_EQ(calcDots(112, 5), 0);
    EXPECT_EQ(calcDotsSnap(112, 5), 0);
    EXPECT_EQ(calcDotsSnap(111, 5), 0);
    EXPECT_EQ(calcDotsSnap(113, 5), 0);

    // 32nd (base=30): double-dotted = 30*7/4 = 52.5 → truncated to 52.
    // rdur=52 must NOT be 2 dots.
    EXPECT_EQ(calcDots(52, 6), 0);
    EXPECT_EQ(calcDotsSnap(52, 6), 0);

    // 32nd (base=30): triple-dotted = 30*15/8 = 56.25 → truncated to 56.
    // rdur=56 must NOT be 3 dots.
    EXPECT_EQ(calcDots(56, 6), 0);
    EXPECT_EQ(calcDotsSnap(56, 6), 0);

    // Valid integer-valued dotted durations must still work.
    // 16th dotted (90) and double-dotted (105) are exact integers.
    EXPECT_EQ(calcDots(90, 5), 1);
    EXPECT_EQ(calcDotsSnap(90, 5), 1);
    EXPECT_EQ(calcDots(105, 5), 2);
    EXPECT_EQ(calcDotsSnap(105, 5), 2);
    // 8th triple-dotted (225 = 120*15/8 exact).
    EXPECT_EQ(calcDots(225, 4), 3);
    EXPECT_EQ(calcDotsSnap(225, 4), 3);
    EXPECT_EQ(calcDotsSnap(226, 4), 3);
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
