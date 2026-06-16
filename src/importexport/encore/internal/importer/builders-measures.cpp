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

#include "builders.h"
#include "ctx.h"
#include "import.h"
#include "../parser/elements.h"
#include "mapping.h"
#include "../parser/ticks.h"
#include "tuplets.h"
#include <algorithm>
#include <memory>
#include <map>
#include <set>
#include <vector>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include "engraving/dom/arpeggio.h"
#include "engraving/dom/box.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/fermata.h"
#include "engraving/dom/fingering.h"
#include "engraving/dom/ornament.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/jump.h"
#include "engraving/dom/key.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/marker.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/part.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftext.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/text.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/volta.h"
#include "engraving/engravingerrors.h"
#include "log.h"

using namespace mu::engraving;

namespace mu::iex::encore {
// True if every element in the measure is a REST element with mrestCount > 1.
// Multi-staff files emit one REST per staff, so the measure can have N > 1 elements.
static bool encMeasHasMultiRest(const EncMeasure& m)
{
    if (m.elements.empty()) {
        return false;
    }
    for (const auto& ep : m.elements) {
        if (static_cast<EncElemType>(ep->type) != EncElemType::REST) {
            return false;
        }
    }
    return static_cast<const EncRest*>(m.elements[0].get())->mrestCount > 1;
}

// Returns the number of MuseScore measures to create for a single EncMeasure.
// Normally 1:1, but Encore stores "N consecutive empty display measures" as a single MEAS
// block whose REST elements have mrestCount == N (byte +15 of the REST element data).
// Multi-staff files emit one REST element per staff inside the block; the count is read
// from the first element (all staves store the same count).
// Expansion is suppressed when the predecessor is itself a multi-measure REST block
// (prevents cascading in the rare case Encore writes consecutive mrest blocks).
static int encMeasDisplayCount(const EncMeasure& m, const EncMeasure* prev)
{
    if (m.elements.empty()) {
        return 1;
    }
    for (const auto& ep : m.elements) {
        if (static_cast<EncElemType>(ep->type) != EncElemType::REST) {
            return 1;
        }
    }
    const int cnt = static_cast<int>(static_cast<const EncRest*>(m.elements[0].get())->mrestCount);
    if (cnt <= 1) {
        return 1;
    }
    if (prev && encMeasHasMultiRest(*prev)) {
        return 1;
    }
    return cnt;
}

// Map an Encore time-signature glyph byte to the MuseScore TimeSigType.
// Encore encodes common time as 0x43 ('C') or 0x63 ('c') depending on version.
// Glyph 0x00 means normal numeric display.
static TimeSigType encGlyphToTimeSigType(quint8 glyph, Fraction ts)
{
    if ((glyph == 0x43 || glyph == 0x63) && ts == Fraction(4, 4)) {
        return TimeSigType::FOUR_FOUR;
    }
    return TimeSigType::NORMAL;
}

void buildMeasures(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;
    const EncFile& enc = ctx.enc;

    // Pickup measure detection: measure 0 holds the short duration, measure 1 the real sig.
    {
        int n0 = !enc.measures.empty() && enc.measures[0].timeSigNum > 0
                 ? enc.measures[0].timeSigNum : 4;
        int d0 = !enc.measures.empty() && enc.measures[0].timeSigDen > 0
                 ? enc.measures[0].timeSigDen : 4;
        ctx.nominalTimeSig = Fraction(n0, d0);
        if (enc.measures.size() >= 2) {
            int n1 = enc.measures[1].timeSigNum > 0 ? enc.measures[1].timeSigNum : 4;
            int d1 = enc.measures[1].timeSigDen > 0 ? enc.measures[1].timeSigDen : 4;
            Fraction ts1(n1, d1);
            if (ts1 != ctx.nominalTimeSig) {
                ctx.nominalTimeSig = ts1;
            }
        }
    }

    // Nominal time sig type (common time "C" or normal numeric display).
    {
        // The nominal measure is [1] when [0] is a pickup with a different fraction.
        const size_t nomIdx = (enc.measures.size() >= 2
                               && ctx.nominalTimeSig != Fraction(
                                   enc.measures[0].timeSigNum > 0 ? enc.measures[0].timeSigNum : 4,
                                   enc.measures[0].timeSigDen > 0 ? enc.measures[0].timeSigDen : 4))
                              ? 1 : 0;
        if (nomIdx < enc.measures.size()) {
            ctx.nominalTimeSigType = encGlyphToTimeSigType(enc.measures[nomIdx].timeSigGlyph,
                                                           ctx.nominalTimeSig);
        }
    }

    int currentTick = 0;
    bool firstMeasure = true;
    size_t msIdxCounter = 0;
    ctx.encToMsIdx.reserve(enc.measures.size());
    for (size_t mi = 0; mi < enc.measures.size(); ++mi) {
        const EncMeasure& encMeas = enc.measures[mi];
        int num = encMeas.timeSigNum > 0 ? encMeas.timeSigNum : 4;
        int den = encMeas.timeSigDen > 0 ? encMeas.timeSigDen : 4;
        Fraction ts(num, den);
        ctx.measTickToTimeSigType[currentTick] = encGlyphToTimeSigType(encMeas.timeSigGlyph, ts);

        const EncMeasure* prev = (mi > 0) ? &enc.measures[mi - 1] : nullptr;
        const int displayCount = encMeasDisplayCount(encMeas, prev);

        ctx.encToMsIdx.push_back(msIdxCounter);

        for (int di = 0; di < displayCount; ++di) {
            Measure* measure = Factory::createMeasure(score->dummy()->system());
            measure->setTick(Fraction::fromTicks(currentTick));

            // Case A: timeSig[0] != timeSig[1] — Encore stored a shorter time signature
            // for the pickup measure. Shorten the measure immediately.
            // Case B (same timesig, partial content): detected after the note loop in
            // noteloop.cpp using the actual cumTick across all staves.
            const bool isPickupA = firstMeasure && di == 0 && ts != ctx.nominalTimeSig;
            measure->setTimesig(isPickupA ? ctx.nominalTimeSig : ts);
            measure->setTicks(ts);

            if (di == 0) {
                if (encMeas.startBarline() == EncBarlineType::REPEATSTART) {
                    measure->setRepeatStart(true);
                }
                if (encMeas.endBarline() == EncBarlineType::REPEATEND) {
                    measure->setRepeatEnd(true);
                } else if (encMeas.endBarline() == EncBarlineType::FINAL
                           || encMeas.endBarline() == EncBarlineType::DOUBLEL
                           || encMeas.endBarline() == EncBarlineType::DOUBLER
                           || encMeas.endBarline() == EncBarlineType::DOTTED) {
                    BarLineType type = BarLineType::DOUBLE;
                    if (encMeas.endBarline() == EncBarlineType::FINAL) {
                        type = BarLineType::END;
                    } else if (encMeas.endBarline() == EncBarlineType::DOTTED) {
                        type = BarLineType::DOTTED;
                    }
                    for (int s = 0; s < ctx.totalStaves; ++s) {
                        measure->setEndBarLineType(type, static_cast<track_idx_t>(s) * VOICES);
                    }
                }
            }

            score->measures()->append(measure);
            currentTick += ts.ticks();
        }
        firstMeasure = false;
        msIdxCounter += static_cast<size_t>(displayCount);
    }
}

void buildInitialSignatures(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;
    const EncFile& enc = ctx.enc;
    if (!enc.measures.empty()) {
        addInitialTimeSig(score, ctx.totalStaves, ctx.nominalTimeSig, ctx.nominalTimeSigType);
    }
    if (!enc.lines.empty()) {
        const auto& firstLine = enc.lines[0];
        for (int si = 0; si < static_cast<int>(firstLine.staffData.size()) && si < ctx.totalStaves; ++si) {
            const auto& sd = firstLine.staffData[si];
            addInitialKeySig(score, si, sd.key);
            const ClefType cClef = si < static_cast<int>(ctx.staffTemplateConcertClef.size())
                                   ? ctx.staffTemplateConcertClef[si] : ClefType::INVALID;
            const ClefType tClef = si < static_cast<int>(ctx.staffTemplateTransposingClef.size())
                                   ? ctx.staffTemplateTransposingClef[si] : ClefType::INVALID;
            const int keyOffset = si < static_cast<int>(ctx.staffPitchOffset.size())
                                  ? ctx.staffPitchOffset[si] : 0;
            // If the staff's instrument carries a drumset (assigned via PERC clef or GM
            // percussion range), use PERC clef regardless of the LINE block's enc clef.
            // Without this, C3L/C4L/F clefs from the LINE block override the drumset clef.
            const Staff* st = score->staff(static_cast<staff_idx_t>(si));
            const bool hasDrumset = st && st->part() && st->part()->instrument()
                                    && st->part()->instrument()->drumset();
            const ClefType ct = hasDrumset ? ClefType::PERC
                                : pickStaffClef(sd.clef, cClef, tClef, keyOffset);
            addInitialClef(score, si, ct);
        }
    }

    // Emit TimeSig elements at change points (buildMeasures sets per-measure properties only).
    // Use identical() rather than operator== to distinguish time signatures whose fractions
    // are mathematically equal but musically distinct: Fraction(6,8) == Fraction(3,4) via
    // cross-multiplication (6×4 == 3×8 = 24), so operator== silently suppresses 6/8 → 3/4
    // and 3/4 → 6/8 changes.  identical() compares numerator and denominator directly.
    Fraction prevTs = ctx.nominalTimeSig;
    for (const Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        Fraction mTs = m->timesig();
        if (mTs.identical(prevTs)) {
            continue;
        }
        // Time sig changed — add a TimeSig element on every staff at this measure.
        Fraction mTick = m->tick();
        auto tsTypeIt = ctx.measTickToTimeSigType.find(mTick.ticks());
        TimeSigType tsType = (tsTypeIt != ctx.measTickToTimeSigType.end())
                             ? tsTypeIt->second : TimeSigType::NORMAL;
        for (int si = 0; si < ctx.totalStaves; ++si) {
            Segment* seg = const_cast<Measure*>(m)->getSegment(SegmentType::TimeSig, mTick);
            TimeSig* tsig = Factory::createTimeSig(seg);
            tsig->setTrack(static_cast<track_idx_t>(si) * VOICES);
            tsig->setSig(mTs, tsType);
            seg->add(tsig);
        }
        prevTs = mTs;
    }
}
} // namespace mu::iex::encore
