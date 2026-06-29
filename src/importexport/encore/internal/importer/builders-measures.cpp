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
#include "../parser/elem.h"
#include "mappers.h"
#include "../parser/ticks.h"
#include "emitters-tuplets.h"
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

namespace mu::iex::enc {
// True if every element in the measure is a REST with mrestCount > 1.
// Multi-staff files emit one REST per staff, so there can be N > 1 elements.
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
// Encore stores N consecutive empty measures as one MEAS block with mrestCount==N
// (byte +15 of REST element data). Expansion is suppressed when the predecessor is
// already an mrest block, preventing cascades from consecutive mrest blocks.
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

void buildMeasures(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;
    const EncRoot& enc = ctx.enc;

    // Pickup measure: measure 0 holds the short duration, measure 1 the real sig.
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

    // Nominal time sig type; use measure [1] when [0] is a pickup.
    {
        const size_t nomIdx = (enc.measures.size() >= 2
                               && ctx.nominalTimeSig != Fraction(
                                   enc.measures[0].timeSigNum > 0 ? enc.measures[0].timeSigNum : 4,
                                   enc.measures[0].timeSigDen > 0 ? enc.measures[0].timeSigDen : 4))
                              ? 1 : 0;
        if (nomIdx < enc.measures.size()) {
            ctx.nominalTimeSigType = encTimeSigGlyph2Type(enc.measures[nomIdx].timeSigGlyph,
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
        ctx.measTickToTimeSigType[currentTick] = encTimeSigGlyph2Type(encMeas.timeSigGlyph, ts);

        const EncMeasure* prev = (mi > 0) ? &enc.measures[mi - 1] : nullptr;
        const int displayCount = encMeasDisplayCount(encMeas, prev);

        ctx.encToMsIdx.push_back(msIdxCounter);

        for (int di = 0; di < displayCount; ++di) {
            Measure* measure = Factory::createMeasure(score->dummy()->system());
            measure->setTick(Fraction::fromTicks(currentTick));

            // Case A: timeSig[0] != timeSig[1], pickup with explicit shorter sig; shorten now.
            // Case B (same sig, partial content): detected post-emitters via actual cumTick.
            // When firstMeasureIsPickup=false, bypass pickup detection and use the nominal sig.
            const bool pickupEnabled = ctx.opts.firstMeasureIsPickup;
            const bool isPickupA = pickupEnabled && firstMeasure && di == 0
                                   && ts != ctx.nominalTimeSig;
            if (!pickupEnabled && firstMeasure && di == 0) {
                measure->setTimesig(ctx.nominalTimeSig);
                measure->setTicks(ctx.nominalTimeSig);
            } else {
                measure->setTimesig(isPickupA ? ctx.nominalTimeSig : ts);
                measure->setTicks(ts);
            }

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
            // Must match setTicks() above: both use nominalTimeSig in the no-pickup first-measure
            // branch so subsequent measure positions are consistent with that measure's duration.
            const bool usedNominal = !pickupEnabled && firstMeasure && di == 0;
            currentTick += usedNominal ? ctx.nominalTimeSig.ticks() : ts.ticks();
        }
        firstMeasure = false;
        msIdxCounter += static_cast<size_t>(displayCount);
    }
}

void buildInitialSignatures(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;
    const EncRoot& enc = ctx.enc;
    if (!enc.measures.empty()) {
        addInitialTimeSig(score, ctx.totalStaves, ctx.nominalTimeSig, ctx.nominalTimeSigType);
    }
    if (!enc.lines.empty()) {
        const auto& firstLine = enc.lines[0];
        for (int si = 0; si < static_cast<int>(firstLine.staffData.size()) && si < ctx.totalStaves; ++si) {
            const auto& sd = firstLine.staffData[si];
            addInitialKeySig(score, si, sd.key);
            const int keyOffset = si < static_cast<int>(ctx.staffPitchOffset.size())
                                  ? ctx.staffPitchOffset[si] : 0;
            // Drumset instruments always use PERC clef; LINE block clefs must not override it.
            const Staff* st = score->staff(static_cast<staff_idx_t>(si));
            const bool hasDrumset = st && st->part() && st->part()->instrument()
                                    && st->part()->instrument()->drumset();
            const ClefType ct = hasDrumset ? ClefType::PERC
                                : pickStaffClef(sd.clef, keyOffset);
            addInitialClef(score, si, ct);
        }

        // v0xA6: staffData is empty (its header staffPerSystem reads 0 and the staff entry
        // layout differs), so the loop above adds no key signature. The per-staff written
        // key was parsed separately into staffKeys; apply it here. Clefs still come from the
        // instrument template, handled by the !haveLineClefs block below.
        if (firstLine.staffData.empty() && !firstLine.staffKeys.empty()) {
            for (int si = 0; si < ctx.totalStaves; ++si) {
                const size_t ki = std::min(static_cast<size_t>(si), firstLine.staffKeys.size() - 1);
                addInitialKeySig(score, si, firstLine.staffKeys[ki]);
            }
        }
    }

    // Files without per-staff LINE clef data (v0xA6): the initial clef comes from the
    // instrument template, which does not reflect an octave Key. The note pitches are already
    // octave-shifted by the Key, so apply the matching octave-decorated clef to bring the
    // display back to the written octave, mirroring what pickStaffClef does for v0xC4.
    const bool haveLineClefs = !enc.lines.empty() && !enc.lines[0].staffData.empty();
    if (!haveLineClefs) {
        for (int si = 0; si < ctx.totalStaves; ++si) {
            const int keyOffset = si < static_cast<int>(ctx.staffPitchOffset.size())
                                  ? ctx.staffPitchOffset[si] : 0;
            if (keyOffset == 0 || keyOffset % 12 != 0) {
                continue;   // only pure-octave Keys need a compensating clef
            }
            const Staff* st = score->staff(static_cast<staff_idx_t>(si));
            const bool hasDrumset = st && st->part() && st->part()->instrument()
                                    && st->part()->instrument()->drumset();
            if (hasDrumset) {
                continue;   // percussion has no octave clef
            }
            const ClefType base = si < static_cast<int>(ctx.staffTemplateConcertClef.size())
                                  ? ctx.staffTemplateConcertClef[si] : ClefType::INVALID;
            if (base == ClefType::INVALID) {
                continue;
            }
            const ClefType oct = applyOctaveToClef(base, keyOffset);
            if (oct != base) {
                addInitialClef(score, si, oct);
            }
        }
    }

    // Emit TimeSig elements at change points. Use identical() not operator==: Fraction(6,8)
    // == Fraction(3,4) via cross-multiplication, so operator== would miss 6/8 ↔ 3/4 changes.
    Fraction prevTs = ctx.nominalTimeSig;
    for (const Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        Fraction mTs = m->timesig();
        if (mTs.identical(prevTs)) {
            continue;
        }
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
} // namespace mu::iex::enc
