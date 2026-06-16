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
#include "engraving/dom/dynamic.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/jump.h"
#include "engraving/dom/marker.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/stafftext.h"
#include "engraving/dom/tempotext.h"

namespace mu::iex::enc {
using namespace mu::engraving;

void handleOrnament(BuildCtx& ctx, NoteLoopMeasCtx& mc, NoteElemCtx& ec)
{
    MasterScore* score = ctx.score;
    const EncRoot& enc = ctx.enc;
    Measure* measure = mc.measure;
    const EncMeasure& encMeas = *mc.encMeas;
    const Fraction measTick = mc.measTick;
    const int measIdx = mc.measIdx;
    const std::set<int>& noteTicks = mc.noteTicks;
    const std::set<int>& voice4NoteTicks = mc.voice4NoteTicks;
    const std::map<int, int>& v0NoteCountAtTick = mc.v0NoteCountAtTick;
    const std::map<int, int>& ornFingCountAtTick = mc.ornFingCountAtTick;
    int maxVoice0Tick = mc.maxVoice0Tick;
    const EncMeasureElem* e = ec.e;
    int& staffIdx = ec.staffIdx;          // mutable ref (dynamic rerouting)
    int voice = ec.voice;
    int msVoice = ec.msVoice;
    track_idx_t& track = ec.track;        // mutable ref (dynamic rerouting)
    Fraction elemTick = ec.elemTick;

    const EncOrnament* eo = static_cast<const EncOrnament*>(e);

    static constexpr int kWholeTicks = 960;

    // Snap a dynamic/ORN tick to the chord-rest whose xoffset matches its drawn position.
    // ORN xoffset < chord xoffset means the glyph belongs to the preceding chord.
    // Scans all voices so ORNs on staves whose notes are in voice=1+ are placed correctly.
    auto snapTickByXoffset = [&](Fraction defaultTick, int dynEncTick) -> Fraction {
        if (encMeas.beatTicks == 0 || encMeas.timeSigDen == 0) {
            return defaultTick;
        }
        const int wholeTicks2 = encMeas.beatTicks * encMeas.timeSigDen;
        const Fraction relTick = defaultTick - measTick;
        const int defaultEncTick = (relTick.numerator() * wholeTicks2)
                                   / std::max(1, relTick.denominator());
        const int ornXoff = static_cast<int>(eo->xoffset);

        // Find the note/rest at defaultEncTick on the same staff (any voice).
        int defaultCrXoff = -1;
        for (const auto& elem : encMeas.elements) {
            const EncMeasureElem* em = elem.get();
            if (em->type != static_cast<quint8>(EncElemType::NOTE)
                && em->type != static_cast<quint8>(EncElemType::REST)) {
                continue;
            }
            if (static_cast<int>(em->staffIdx) != staffIdx) {
                continue;
            }
            if (static_cast<int>(em->tick) != defaultEncTick) {
                continue;
            }
            if (em->type == static_cast<quint8>(EncElemType::NOTE)) {
                defaultCrXoff = static_cast<int>(static_cast<const EncNote*>(em)->xoffset);
            } else {
                defaultCrXoff = static_cast<int>(static_cast<const EncRest*>(em)->xoffset);
            }
            break;
        }
        if (defaultCrXoff >= 0 && ornXoff >= defaultCrXoff) {
            return defaultTick;
        }
        // Find the latest note/rest before defaultEncTick on the same staff (any voice)
        // whose xoffset <= ornXoff. Also handles WEDGESTART at durTicks (no CR at default tick).
        int bestTick = -1;
        for (const auto& elem : encMeas.elements) {
            const EncMeasureElem* em = elem.get();
            if (em->type != static_cast<quint8>(EncElemType::NOTE)
                && em->type != static_cast<quint8>(EncElemType::REST)) {
                continue;
            }
            if (static_cast<int>(em->staffIdx) != staffIdx) {
                continue;
            }
            if (static_cast<int>(em->tick) >= defaultEncTick) {
                continue;
            }
            int xoff = 0;
            if (em->type == static_cast<quint8>(EncElemType::NOTE)) {
                xoff = static_cast<int>(static_cast<const EncNote*>(em)->xoffset);
            } else {
                xoff = static_cast<int>(static_cast<const EncRest*>(em)->xoffset);
            }
            if (xoff > ornXoff) {
                continue;
            }
            if (static_cast<int>(em->tick) > bestTick) {
                bestTick = static_cast<int>(em->tick);
            }
        }
        if (bestTick < 0) {
            return defaultTick;
        }
        return measTick + Fraction(bestTick, wholeTicks2);
    };

    switch (eo->ornType()) {
    case EncOrnamentType::SLURSTART: {
        // No SLURSTOP in .enc; alMezuro = forward measure count. Endpoint resolved in post-pass.
        // Skip slurs within one 16th of the bar boundary; Encore silently omits these.
        if (encMeas.beatTicks > 0
            && static_cast<int>(eo->tick) + static_cast<int>(encMeas.beatTicks) / 4
            > static_cast<int>(encMeas.durTicks)) {
            break;
        }
        int endIdx = measIdx + static_cast<int>(eo->alMezuro);
        if (endIdx < 0 || endIdx >= static_cast<int>(ctx.measuresByIdx.size())) {
            endIdx = measIdx;
        }
        PendingSlur ps;
        // Use raw eo->tick (not elemTick): elemTick is cumTick-based and wrong when voice 0 is empty.
        // durTicks*timeSigDen/timeSigNum = 960 ticks/whole regardless of compound beat storage.
        {
            const int wt = (encMeas.durTicks && encMeas.timeSigNum && encMeas.timeSigDen)
                           ? (static_cast<int>(encMeas.durTicks) * encMeas.timeSigDen)
                           / encMeas.timeSigNum : 960;
            ps.startTick = measTick
                           + Fraction(static_cast<int>(eo->tick), wt).reduced();
        }
        ps.track = track;
        ps.startMeasIdx = measIdx;
        ps.endMeasIdx = endIdx;
        ps.alMezuro = static_cast<int>(eo->alMezuro);
        // xoffset is a pixel position that wraps at 256; cast to quint8 to get the true positive value.
        ps.slurXoffset  = static_cast<int>(static_cast<quint8>(eo->xoffset));
        ps.slurXoffset2 = static_cast<int>(eo->xoffset2);  // already quint8
        ps.staffIdx = staffIdx;
        ps.encVoice = voice;
        ctx.pendingSlurs.push_back(ps);
        break;
    }
    case EncOrnamentType::SLURSTOP:
        break;
    case EncOrnamentType::WEDGESTART: {
        // No WEDGESTOP in .enc; alMezuro = forward measure count (upper bound). Precise tick2 resolved in post-pass.
        int endIdx = measIdx + static_cast<int>(eo->alMezuro);
        if (endIdx < 0 || endIdx >= static_cast<int>(ctx.measuresByIdx.size())) {
            endIdx = measIdx;
        }
        Measure* endMeas = ctx.measuresByIdx[endIdx];
        Fraction maxEnd = endMeas->tick() + endMeas->ticks();
        const Fraction snappedStart = snapTickByXoffset(elemTick, static_cast<int>(e->tick));
        if (maxEnd <= snappedStart) {
            break;
        }
        // Encore 5 sets bit 1 too (0x02=cresc, 0x03=dim); test bit 0 only.
        const HairpinType hpType
            = ((eo->speguleco & 0x01) == 0)
              ? HairpinType::CRESC_HAIRPIN
              : HairpinType::DIM_HAIRPIN;
        PendingHairpin ph;
        ph.startTick = snappedStart;
        ph.maxEndTick = maxEnd;
        ph.track = track;
        ph.type = hpType;
        ph.endMeasIdx = endIdx;
        ph.hairpinXoffset2 = static_cast<int>(eo->xoffset2);
        ph.staffIdx = staffIdx;
        ph.encVoice = voice;
        ctx.pendingHairpins.push_back(ph);
        break;
    }
    case EncOrnamentType::WEDGESTOP:
        break;
    case EncOrnamentType::TEMPO: {
        if (eo->tempo > 0) {
            // Skip ORN TEMPO when it conflicts with the measure's header BPM. Encore stores tempo marks
            // in the last measure of a system when the mark is visually at the next system start;
            // the MEAS header BPM is authoritative in that case.
            if (encMeas.bpm != 0 && static_cast<quint16>(eo->tempo) != encMeas.bpm) {
                break;
            }
            Segment* seg = measure->getSegment(SegmentType::ChordRest, elemTick);
            if (!seg) {
                seg = measure->getSegment(SegmentType::ChordRest, measTick);
            }
            TempoText* tt2 = Factory::createTempoText(seg);
            tt2->setTrack(track);
            // Detect dotted-quarter beat: beatTicks=360 covers 3/8 feel; the compound sig check
            // (den=8, num%3==0, num>3) catches 6/8 when old fixtures stored beatTicks=240.
            const quint16 rawBeatTicks = encMeas.beatTicks;
            // Use nominal timesig so a pickup measure inherits the main sig's classification.
            const Fraction mts = measure->timesig();
            const bool cmpd = (rawBeatTicks == 360)
                              || (mts.denominator() == 8
                                  && mts.numerator() % 3 == 0
                                  && mts.numerator() > 3);
            const double bps = cmpd ? eo->tempo * 1.5 / 60.0 : eo->tempo / 60.0;
            tt2->setTempo(BeatsPerSecond(bps));
            tt2->setXmlText(tempoXmlText(static_cast<int>(eo->tempo), cmpd ? 360 : 240));
            tt2->setFollowText(true);
            seg->add(tt2);
            score->setTempo(elemTick, BeatsPerSecond(bps));
        }
        break;
    }
    case EncOrnamentType::ARPEGGIO: {
        ctx.pendingArpeggios.push_back({ elemTick, track });
        break;
    }
    case EncOrnamentType::TREMOLO_32:
    case EncOrnamentType::TREMOLO_32B: {
        // Post-pass searches backwards when no chord is found at the exact tick (tied passages).
        PendingOrnTremolo pt;
        pt.tick = elemTick;
        pt.measTick = measTick;
        pt.staffIdx = staffIdx;
        pt.msVoice = msVoice;
        pt.tremType = TremoloType::R32;
        ctx.pendingOrnTremolos.push_back(pt);
        break;
    }
    case EncOrnamentType::TRILL_START:
    case EncOrnamentType::TRILL_ALT:
    case EncOrnamentType::TRILL_TR:
    case EncOrnamentType::TRILL_SHORT: {
        // TRILL_START: spanner when TRILL_END or alMezuro marks the end; TRILL_ALT: always Ornament glyph;
        // TRILL_TR/TRILL_SHORT: standalone glyph only, never a spanner. Chord deferred to post-pass.
        PendingTrill pt;
        pt.isAlt    = (eo->ornType() != EncOrnamentType::TRILL_START);
        pt.isSimple = (eo->ornType() == EncOrnamentType::TRILL_TR
                       || eo->ornType() == EncOrnamentType::TRILL_SHORT);
        if (pt.isSimple) {
            // Snap to visual position; 20px threshold ignores small alignment nudges.
            const int ornXoff = static_cast<int>(eo->xoffset);
            int crXoffAtTick = -1;
            for (const auto& elem : encMeas.elements) {
                const EncMeasureElem* em = elem.get();
                if (static_cast<int>(em->tick) != static_cast<int>(e->tick)) {
                    continue;
                }
                if (em->staffIdx != staffIdx || em->voice != voice) {
                    continue;
                }
                if (em->type == static_cast<quint8>(EncElemType::NOTE)) {
                    crXoffAtTick = static_cast<int>(static_cast<const EncNote*>(em)->xoffset);
                    break;
                } else if (em->type == static_cast<quint8>(EncElemType::REST)) {
                    crXoffAtTick = static_cast<int>(static_cast<const EncRest*>(em)->xoffset);
                    break;
                }
            }
            constexpr int TRILL_SNAP_THRESHOLD = 20;
            if (crXoffAtTick >= 0 && ornXoff < crXoffAtTick - TRILL_SNAP_THRESHOLD) {
                pt.tick = snapTickByXoffset(elemTick, static_cast<int>(e->tick));
            } else {
                pt.tick = elemTick;
            }
            pt.simpleSymId = (eo->ornType() == EncOrnamentType::TRILL_SHORT)
                             ? SymId::ornamentShortTrill
                             : SymId::ornamentTrill;
        } else {
            pt.tick = elemTick;
        }
        pt.track   = track;
        if (!pt.isAlt) {
            pt.alMezuro = static_cast<int>(eo->alMezuro);
            pt.measIdx  = static_cast<size_t>(measIdx);
            pt.xoffset2 = static_cast<int>(eo->xoffset2);
        }
        ctx.pendingTrills.push_back(pt);
        break;
    }
    case EncOrnamentType::TRILL_END:
        ctx.pendingTrillEnds[track].push_back(elemTick);
        break;
    case EncOrnamentType::SEGNO:
    case EncOrnamentType::TO_CODA:
    case EncOrnamentType::CODA: {
        MarkerType mt = MarkerType::CODA;
        if (eo->ornType() == EncOrnamentType::SEGNO) {
            mt = MarkerType::SEGNO;
        } else if (eo->ornType() == EncOrnamentType::TO_CODA) {
            mt = MarkerType::TOCODA;
        }
        ctx.pendingMarkers.push_back({ elemTick, mt });
        break;
    }
    case EncOrnamentType::STACCATO: {
        ctx.pendingStaccatos.push_back({ elemTick, track });
        break;
    }
    case EncOrnamentType::FERMATA_ABOVE: {
        ctx.pendingFermatas.push_back({ elemTick, track, SymId::fermataAbove });
        break;
    }
    case EncOrnamentType::FERMATA_BELOW: {
        ctx.pendingFermatas.push_back({ elemTick, track, SymId::fermataBelow });
        break;
    }
    case EncOrnamentType::REPEAT_MEASURE: {
        bool already = false;
        for (const auto& pmr : ctx.pendingMeasureRepeats) {
            if (pmr.staffIdx == ec.staffIdx && pmr.measTick == measTick) {
                already = true;
                break;
            }
        }
        if (!already) {
            ctx.pendingMeasureRepeats.push_back({ measTick, ec.staffIdx });
        }
        break;
    }
    case EncOrnamentType::CAESURA: {
        ctx.pendingBreaths.push_back({ elemTick, track, SymId::caesura });
        break;
    }
    case EncOrnamentType::BREATH_COMMA: {
        ctx.pendingBreaths.push_back({ elemTick, track, SymId::breathMarkComma });
        break;
    }
    case EncOrnamentType::ACCENT: {
        const bool cm = !noteTicks.count(static_cast<int>(e->tick));
        const Fraction bowTick = measTick + Fraction(static_cast<int>(e->tick), 960);
        ctx.pendingBowings.push_back({ bowTick, track, SymId::articAccentAbove, measIdx, cm,
                                       static_cast<int>(eo->xoffset), static_cast<int>(e->tick) });
        break;
    }
    case EncOrnamentType::DOWNBOW: {
        const bool cm = !noteTicks.count(static_cast<int>(e->tick));
        const Fraction bowTick = measTick + Fraction(static_cast<int>(e->tick), 960);
        ctx.pendingBowings.push_back({ bowTick, track, SymId::stringsDownBow, measIdx, cm,
                                       static_cast<int>(eo->xoffset), static_cast<int>(e->tick) });
        break;
    }
    case EncOrnamentType::UPBOW: {
        const bool cm = !noteTicks.count(static_cast<int>(e->tick));
        const SymId sid = enc.fmt->ornC4IsAccent() ? SymId::articAccentAbove : SymId::stringsUpBow;
        const Fraction bowTick = measTick + Fraction(static_cast<int>(e->tick), 960);
        ctx.pendingBowings.push_back({ bowTick, track, sid, measIdx, cm,
                                       static_cast<int>(eo->xoffset), static_cast<int>(e->tick) });
        break;
    }
    case EncOrnamentType::FINGER_1:
    case EncOrnamentType::FINGER_2:
    case EncOrnamentType::FINGER_3:
    case EncOrnamentType::FINGER_4:
    case EncOrnamentType::FINGER_5: {
        const int n = static_cast<int>(eo->ornType())
                      - static_cast<int>(EncOrnamentType::FINGER_1) + 1;
        const int orn_tick = static_cast<int>(e->tick);
        // cm: ORN at last voice=0 tick with no voice=4 note there; belongs to first chord of next measure.
        const bool cm = !voice4NoteTicks.empty()
                        && !voice4NoteTicks.count(orn_tick)
                        && orn_tick == maxVoice0Tick;
        // ps: excess FINGER ORNs beyond voice=0 note count target the voice=4 (2nd staff) chord.
        const bool ps = !cm
                        && voice4NoteTicks.count(orn_tick)
                        && ornFingCountAtTick.count(orn_tick)
                        && v0NoteCountAtTick.count(orn_tick)
                        && ornFingCountAtTick.at(orn_tick)
                        > v0NoteCountAtTick.at(orn_tick);
        ctx.pendingOrnFingerings.push_back({ elemTick, track, n, measIdx, cm, ps });
        break;
    }
    case EncOrnamentType::DYN_PPP:
    case EncOrnamentType::DYN_PP:
    case EncOrnamentType::DYN_P:
    case EncOrnamentType::DYN_MP:
    case EncOrnamentType::DYN_MF:
    case EncOrnamentType::DYN_F:
    case EncOrnamentType::DYN_FF:
    case EncOrnamentType::DYN_FFF:
    case EncOrnamentType::DYN_SFZ:
    case EncOrnamentType::DYN_SFFZ:
    case EncOrnamentType::DYN_FP:
    case EncOrnamentType::DYN_FZ:
    case EncOrnamentType::DYN_SF: {
        // Size-16 ORN: only the tipo byte is reliable.
        // yoffset > 0 means the user dragged it onto the staff above; reroute to staffIdx-1.
        if (eo->yoffset > 0 && staffIdx > 0) {
            staffIdx -= 1;
            track = static_cast<track_idx_t>(staffIdx * VOICES + msVoice);
        }
        DynamicType dt = DynamicType::OTHER;
        switch (eo->ornType()) {
        case EncOrnamentType::DYN_PPP:  dt = DynamicType::PPP;
            break;
        case EncOrnamentType::DYN_PP:   dt = DynamicType::PP;
            break;
        case EncOrnamentType::DYN_P:    dt = DynamicType::P;
            break;
        case EncOrnamentType::DYN_MP:   dt = DynamicType::MP;
            break;
        case EncOrnamentType::DYN_MF:   dt = DynamicType::MF;
            break;
        case EncOrnamentType::DYN_F:    dt = DynamicType::F;
            break;
        case EncOrnamentType::DYN_FF:   dt = DynamicType::FF;
            break;
        case EncOrnamentType::DYN_FFF:  dt = DynamicType::FFF;
            break;
        case EncOrnamentType::DYN_SFZ:  dt = DynamicType::SFZ;
            break;
        case EncOrnamentType::DYN_SFFZ: dt = DynamicType::SFFZ;
            break;
        case EncOrnamentType::DYN_FP:   dt = DynamicType::FP;
            break;
        case EncOrnamentType::DYN_FZ:   dt = DynamicType::FZ;
            break;
        case EncOrnamentType::DYN_SF:   dt = DynamicType::SF;
            break;
        default: break;
        }
        // Use enc tick as the base: cumTick for voice=0 may be 0 when notes are in other voices.
        const Fraction dynBase = measTick + Fraction(static_cast<int>(e->tick), kWholeTicks);
        Fraction placeTick = snapTickByXoffset(dynBase, static_cast<int>(e->tick));
        // Section-end dynamics are stored at measureDurTicks; clamp back to the last ChordRest.
        if (placeTick >= measTick + measure->ticks()) {
            Segment* last = measure->last(SegmentType::ChordRest);
            placeTick = last ? last->tick() : measTick;
        }
        Segment* seg = measure->getSegment(SegmentType::ChordRest, placeTick);
        if (!seg) {
            seg = measure->getSegment(SegmentType::ChordRest, measTick);
        }
        // Skip duplicate dynamics at the same tick (Encore can emit e.g. two MF ORNs).
        bool dupDyn = false;
        for (EngravingItem* ann : seg->annotations()) {
            if (!ann || !ann->isDynamic() || ann->track() != track) {
                continue;
            }
            if (toDynamic(ann)->dynamicType() == dt) {
                dupDyn = true;
                break;
            }
        }
        if (dupDyn) {
            break;
        }
        Dynamic* dyn = Factory::createDynamic(seg);
        dyn->setTrack(track);
        dyn->setDynamicType(dt);
        dyn->setXmlText(Dynamic::dynamicText(dt));
        if (eo->yoffset < 0) {
            dyn->setPlacement(mu::engraving::PlacementV::BELOW);
            dyn->setPropertyFlags(mu::engraving::Pid::PLACEMENT, mu::engraving::PropertyFlags::UNSTYLED);
        }
        seg->add(dyn);
        break;
    }
    case EncOrnamentType::STAFFTEXT: {
        // Text content lives in enc.textBlock.entries[eo->tind].
        const int textIdx = static_cast<int>(eo->tind);
        if (textIdx < 0
            || textIdx >= static_cast<int>(enc.textBlock.entries.size())) {
            break;
        }
        QString text = enc.textBlock.entries[textIdx];
        if (text.isEmpty()) {
            break;
        }
        Fraction placeTick = elemTick;
        if (placeTick >= measTick + measure->ticks()) {
            Segment* last = measure->last(SegmentType::ChordRest);
            placeTick = last ? last->tick() : measTick;
        }
        Segment* seg = measure->getSegment(SegmentType::ChordRest, placeTick);
        if (!seg) {
            seg = measure->getSegment(SegmentType::ChordRest, measTick);
        }
        // Encore y-offset is Cartesian (positive = up); negative maps to PlacementV::BELOW.
        const bool placeBelow = (eo->yoffset < 0);

        // Promote Italian tempo terms to TempoText so MuseScore tracks them in the tempo map.
        const double tempoBps = encTextToTempoBps(text);
        if (tempoBps >= 0.0) {
            TempoText* tt2 = Factory::createTempoText(seg);
            tt2->setTrack(track);
            tt2->setXmlText(String(text));
            if (tempoBps > 0.0) {
                tt2->setTempo(BeatsPerSecond(tempoBps));
                tt2->setFollowText(true);
                score->setTempo(elemTick, BeatsPerSecond(tempoBps));
            }
            if (placeBelow) {
                tt2->setPlacement(mu::engraving::PlacementV::BELOW);
                tt2->setPropertyFlags(mu::engraving::Pid::PLACEMENT, mu::engraving::PropertyFlags::UNSTYLED);
            }
            seg->add(tt2);
            break;
        }

        StaffText* st = Factory::createStaffText(seg);
        st->setTrack(track);
        st->setXmlText(String(text));
        if (placeBelow) {
            st->setPlacement(mu::engraving::PlacementV::BELOW);
            st->setPropertyFlags(mu::engraving::Pid::PLACEMENT, mu::engraving::PropertyFlags::UNSTYLED);
        }
        seg->add(st);
        break;
    }
    default:
        LOGW() << QString("Encore: unknown ornament type 0x%1 at measure %2 staff %3 tick %4")
                      .arg(eo->tipo, 2, 16, QChar('0'))
                      .arg(measIdx)
                      .arg(staffIdx)
                      .arg(static_cast<int>(e->tick));
        break;
    }
}
} // namespace mu::iex::enc
