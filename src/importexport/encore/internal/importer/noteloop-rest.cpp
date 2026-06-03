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
#include "engraving/dom/factory.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/tuplet.h"

namespace mu::iex::encore {
using namespace mu::engraving;

void handleRest(BuildCtx& ctx, NoteLoopMeasCtx& mc, NoteElemCtx& ec)
{
    Measure* measure = mc.measure;
    const EncMeasureElem* e = ec.e;
    track_idx_t track = ec.track;
    auto trackKey = ec.trackKey;
    Fraction elemTick = ec.elemTick;
    int savedPrevMidiTick = ec.savedPrevMidiTick;
    const std::set<const EncMeasureElem*>& validTupletGroupMember = mc.validTupletGroupMember;
    const std::set<const EncMeasureElem*>& impliedGroupMember = mc.validTupletGroupMember;
    auto closeTupletWithFill = [&](TupletTracker& tt, std::pair<int, int> key) {
        mc.closeTupletWithFill(ctx, tt, key);
    };

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
    // Cap rest duration to remaining space. Also cap when the current tuplet group is full (it's closed before this rest, making it plain).
    {
        const auto& ttPre = ctx.tuplets[trackKey];
        if (!ttPre.inTuplet() || ttPre.groupFull()) {
            Fraction remaining = measure->ticks() - ctx.cumTick[trackKey];
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

        auto& tt = ctx.tuplets[trackKey];
        int actualNr = er->actualNotes();
        int normalNr = er->normalNotes();
        bool isStdExplicitR = (actualNr == 3 && normalNr == 2)
                              || (actualNr == 5 && normalNr == 4)
                              || (actualNr == 6 && normalNr == 4);
        if (!isStdExplicitR) {
            actualNr = 0;
            normalNr = 0;
        }
        if (actualNr == 0 && ctx.impliedTuplets && (er->faceValue & 0x0F) >= 4
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
                    Fraction remaining = measure->ticks() - ctx.cumTick[trackKey];
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

        // Advance cumulative position; when capped, also update rest's ticks so actualTicks() matches ctx.cumTick advance (avoids sanityCheck overshoot).
        Fraction advance = tt.inTuplet()
                           ? TDuration(dt).fraction() * Fraction(tt.normalN, tt.actualN)
                           : dottedAdvance(dt, dots);
        Fraction remaining = measure->ticks() - ctx.cumTick[trackKey];
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
                    ctx.prevMidiTick[trackKey] = savedPrevMidiTick;
                } else {
                    ctx.prevMidiTick.erase(trackKey);
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
        ctx.cumTick[trackKey] += advance;
        if (tt.inTuplet()) {
            tt.placedTicks += advance;
        }
    }
}
} // namespace mu::iex::encore
