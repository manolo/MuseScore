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

// Dispatch Encore ORNAMENT elements to dynamics, wedges, tempo, slurs, trills, markers and more.

#include "emitters-internal.h"
#include "coords.h"
#include "../parser/ticks.h"
#include "mappers.h"
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

static void handleDynamicOrnament(BuildCtx& /*ctx*/, MeasEmitCtx& mc,
                                  NoteElemCtx& ec, const EncOrnament* eo,
                                  Measure* measure, MasterScore* /*score*/)
{
    const EncMeasure& encMeas = *mc.encMeas;
    const Fraction measTick = mc.measTick;
    const EncMeasureElem* e = ec.e;
    int& staffIdx = ec.staffIdx;
    int msVoice = ec.msVoice;
    track_idx_t& track = ec.track;

    // Size-16 ORN: only the tipo byte is reliable.
    // yoffset > 0 means the user dragged it onto the staff above; reroute to staffIdx-1.
    if (eo->yoffset > 0 && staffIdx > 0) {
        staffIdx -= 1;
        track = static_cast<track_idx_t>(staffIdx * VOICES + msVoice);
    }
    const DynamicType dt = encOrnType2DynamicType(eo->ornType());
    // Use enc tick as the base: cumTick for voice=0 may be 0 when notes are in other voices.
    const Fraction dynBase = measTick + Fraction(static_cast<int>(e->tick), kEncWholeTicks);
    Fraction placeTick = snapStartTickByXoffset(dynBase, encMeas, staffIdx,
                                                static_cast<int>(eo->xoffset), measTick);
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
        return;
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
}

static void handleStaffTextOrnament(BuildCtx& ctx, const MeasEmitCtx& mc,
                                    const NoteElemCtx& ec, const EncOrnament* eo,
                                    Measure* measure, MasterScore* score)
{
    const EncRoot& enc = ctx.enc;
    const Fraction measTick = mc.measTick;
    const Fraction elemTick = ec.elemTick;
    const track_idx_t track = ec.track;

    // Text content lives in enc.textBlock.entries[eo->tind].
    const int textIdx = static_cast<int>(eo->tind);
    if (textIdx < 0
        || textIdx >= static_cast<int>(enc.textBlock.entries.size())) {
        return;
    }
    QString text = enc.textBlock.entries[textIdx];
    if (text.isEmpty()) {
        return;
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
    // Skipped when importTempoTextSemantic is off: the text stays as StaffText.
    const double tempoBps = ctx.opts.importTempoTextSemantic ? encTextToTempoBps(text) : -1.0;
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
        return;
    }

    StaffText* st = Factory::createStaffText(seg);
    st->setTrack(track);
    st->setXmlText(String(text));
    if (placeBelow) {
        st->setPlacement(mu::engraving::PlacementV::BELOW);
        st->setPropertyFlags(mu::engraving::Pid::PLACEMENT, mu::engraving::PropertyFlags::UNSTYLED);
    }
    seg->add(st);
}

static void handleTempoOrnament(BuildCtx& ctx, const MeasEmitCtx& mc,
                                const NoteElemCtx& ec, const EncOrnament* eo,
                                MasterScore* score)
{
    const EncRoot& enc      = ctx.enc;
    const EncMeasure& encMeas = *mc.encMeas;
    const Fraction measTick = mc.measTick;
    const Fraction elemTick = ec.elemTick;
    const track_idx_t track = ec.track;
    Measure* measure = mc.measure;

    if (eo->tempo > 0) {
        // When importTempoTextSemantic is off, visual tempo marks (ORN TEMPO) are suppressed
        // just like Italian text terms; only the MEAS header BPM creates a TempoText.
        if (!ctx.opts.importTempoTextSemantic) {
            return;
        }
        // Detect beat unit from beatTicks so we can compare units correctly and set BPS/display.
        const quint16 rawBeatTicks = encMeas.beatTicks;
        // Use nominal timesig so a pickup measure inherits the main sig's classification.
        const Fraction mts = measure->timesig();
        // Dotted-quarter beat: beatTicks=360 (explicit) or compound time sig (6/8, 9/8, 12/8)
        // with legacy beatTicks=240.
        const bool cmpd = (rawBeatTicks == 360)
                          || (mts.denominator() == 8
                              && mts.numerator() % 3 == 0
                              && mts.numerator() > 3);
        // The MEAS header BPM is the authoritative tempo position: applyMeasureBpmMarks places a
        // TempoText at the measure START (a real ChordRest segment that registers in the tempo map).
        // The ORN TEMPO is only a visual mark whose stored tick is often off (Encore puts it at the
        // end of a measure, or a full system before the actual change). Suppress the ORN and let the
        // header place the tempo whenever a header BPM equals the ORN tempo:
        //   - same measure (eo->tempo == encMeas.bpm): the ORN is redundant with this measure's
        //     header, e.g. an initial "♩=230" Encore stores at the end of measure 1 instead of its
        //     start. Without this, the ORN's end-of-measure segment fails to set the playback tempo
        //     (stays at the default) and also blocks the header from placing it at the start.
        //   - a later measure: a misplaced ornament stored before the measure that actually changes.
        // Keep the ORN only when NO header BPM matches it (a genuine standalone mark).
        if (static_cast<quint16>(eo->tempo) == encMeas.bpm) {
            return;  // Redundant: this measure's header BPM places it at the measure start
        }
        {
            for (size_t mi = mc.measIdx + 1; mi < enc.measures.size(); ++mi) {
                if (enc.measures[mi].bpm == static_cast<quint16>(eo->tempo)) {
                    return;  // Misplaced ornament: the header BPM at mi will place it
                }
            }
            // Not misplaced: the ORN is the genuine score marking; fall through to use it.
        }

        // Encore anchors a tempo mark to a note's tick but draws the glyph at an xoffset that
        // may sit to the LEFT of that note, over an earlier downbeat rest. Snap to the chord-rest
        // whose xoffset matches the drawn position (same logic as dynamics), so the tempo lands on
        // the rest it visually governs rather than the later note.
        Fraction placeTick = snapStartTickByXoffset(elemTick, encMeas, ec.staffIdx,
                                                    static_cast<int>(eo->xoffset), measTick);
        Segment* seg = measure->getSegment(SegmentType::ChordRest, placeTick);
        if (!seg) {
            seg = measure->getSegment(SegmentType::ChordRest, measTick);
        }
        TempoText* tt2 = Factory::createTempoText(seg);
        tt2->setTrack(track);

        // The tempo value is expressed in the mark's beat unit. Prefer the unit Encore stored
        // explicitly on the mark (`noto`); a compound meter is often beaten in dotted quarters,
        // but the composer may pick a plain quarter (e.g. quarter=198 in 6/8), and only `noto`
        // records that choice. Fall back to the meter heuristic when `noto` is unset.
        const int notoTicks = notoToBeatTicks(eo->noto);
        const int displayBeatTicks = notoTicks ? notoTicks : (cmpd ? 360 : 240);
        const double beatInQuarters = displayBeatTicks / 240.0;
        const double bps = eo->tempo * beatInQuarters / 60.0;
        tt2->setTempo(BeatsPerSecond(bps));
        tt2->setXmlText(tempoXmlText(static_cast<int>(eo->tempo), displayBeatTicks));
        tt2->setFollowText(true);
        seg->add(tt2);
        score->setTempo(seg->tick(), BeatsPerSecond(bps));
    }
}

static void handleWedgeStart(BuildCtx& ctx, const MeasEmitCtx& mc,
                             const NoteElemCtx& ec, const EncOrnament* eo)
{
    const EncMeasure& encMeas = *mc.encMeas;
    const Fraction measTick = mc.measTick;
    const int staffIdx = ec.staffIdx;
    const track_idx_t track = ec.track;
    const int voice = ec.voice;
    const int measIdx = mc.measIdx;
    const EncMeasureElem* e = ec.e;

    // On grand-staff instruments (staffWithin > 0), ec.elemTick is cumTick-based and may be
    // wrong because the WEDGESTART ORN (always voice=0) uses a different trackKey than the
    // actual notes (which may be in voice=1+).  Compute the tick directly from the raw Encore
    // element tick so hairpins in the second half of a grand-staff measure get the right start.
    // For single-staff instruments (staffWithin == 0) cumTick is correct; use ec.elemTick.
    const int wholeTicks2 = (e->staffWithin > 0) ? encWholeNoteTicks(encMeas) : 0;
    const Fraction rawElemTick = (wholeTicks2 > 0)
                                 ? measTick + Fraction(static_cast<int>(e->tick), wholeTicks2).reduced()
                                 : ec.elemTick;

    // No WEDGESTOP in .enc; alMezuro = forward measure count (upper bound). Precise tick2 resolved in post-pass.
    int endIdx = measIdx + static_cast<int>(eo->alMezuro);
    if (endIdx < 0 || endIdx >= static_cast<int>(ctx.measuresByIdx.size())) {
        endIdx = measIdx;
    }
    Measure* endMeas = ctx.measuresByIdx[endIdx];
    Fraction maxEnd = endMeas->tick() + endMeas->ticks();
    const Fraction snappedStart = snapStartTickByXoffset(rawElemTick, encMeas, staffIdx,
                                                         static_cast<int>(eo->xoffset), measTick);
    if (maxEnd <= snappedStart) {
        return;
    }
    // Encore 5 sets bit 1 too (0x02=cresc, 0x03=dim); test bit 0 only.
    const HairpinType hpType
        = ((eo->speguleco & 0x01) == 0)
          ? HairpinType::CRESC_HAIRPIN
          : HairpinType::DIM_HAIRPIN;
    // On grand-staff instruments (staffWithin > 0), WEDGESTART ORNs use Encore voice=0 but the
    // actual notes may be in a different Encore voice (e.g. voice=1).  Find the voice used by
    // the first note on the same sub-staff so the hairpin ends up on the same MuseScore track
    // as the notes it spans, otherwise it lands on the measure-rest-only voice 0 and cannot
    // be positioned at its true start tick.
    track_idx_t resolvedTrack = track;
    int resolvedEncVoice = voice;
    if (e->staffWithin > 0) {
        for (const auto& elem : encMeas.elements) {
            const EncMeasureElem* em = elem.get();
            if (em->type != static_cast<quint8>(EncElemType::NOTE)
                && em->type != static_cast<quint8>(EncElemType::REST)) {
                continue;
            }
            if (static_cast<int>(em->staffIdx) != static_cast<int>(e->staffIdx)
                || static_cast<int>(em->staffWithin) != static_cast<int>(e->staffWithin)) {
                continue;
            }
            const int emEncVoice = static_cast<int>(em->voice);
            const int vBase = static_cast<int>(e->staffWithin) * (static_cast<int>(VOICES) / 2);
            const int emMsVoice = (emEncVoice >= vBase) ? emEncVoice - vBase : emEncVoice;
            resolvedTrack    = static_cast<track_idx_t>(staffIdx * VOICES + emMsVoice);
            resolvedEncVoice = emEncVoice;
            break;
        }
    }

    PendingHairpin ph;
    ph.startTick = snappedStart;
    ph.maxEndTick = maxEnd;
    ph.track = resolvedTrack;
    ph.type = hpType;
    ph.endMeasIdx = endIdx;
    ph.hairpinXoffset2 = static_cast<int>(eo->xoffset2);
    // Raw Encore staffIdx (rawStaff & 0x3F), not the MuseScore-mapped slot: resolveHairpinEndByXoffset
    // compares against em->staffIdx on note elements which also use rawStaff & 0x3F.
    ph.staffIdx = static_cast<int>(e->staffIdx);
    ph.encVoice = resolvedEncVoice;
    ctx.pendingHairpins.push_back(ph);
}

static void handleTrillOrnament(BuildCtx& ctx, const MeasEmitCtx& mc,
                                const NoteElemCtx& ec, const EncOrnament* eo)
{
    const EncMeasure& encMeas = *mc.encMeas;
    const Fraction measTick = mc.measTick;
    const Fraction elemTick = ec.elemTick;
    const int staffIdx = ec.staffIdx;
    const int voice = ec.voice;
    const track_idx_t track = ec.track;
    const int measIdx = mc.measIdx;
    const EncMeasureElem* e = ec.e;

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
            pt.tick = snapStartTickByXoffset(elemTick, encMeas, staffIdx,
                                             static_cast<int>(eo->xoffset), measTick);
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
}

static void handleStringNumberOrnament(BuildCtx& ctx, const MeasEmitCtx& mc,
                                       const NoteElemCtx& ec, const EncOrnament* eo)
{
    const Fraction elemTick = ec.elemTick;
    const track_idx_t track = ec.track;
    const int measIdx = mc.measIdx;

    const int sn = static_cast<int>(eo->ornType())
                   - static_cast<int>(EncOrnamentType::STRING_NUMBER_2) + 2;
    ctx.pendingOrnFingerings.push_back({ elemTick, track, sn, measIdx, false, false, true });
}

static void handleFingerOrnament(BuildCtx& ctx, const MeasEmitCtx& mc,
                                 const NoteElemCtx& ec, const EncOrnament* eo)
{
    const EncMeasureElem* e = ec.e;
    const Fraction elemTick = ec.elemTick;
    const track_idx_t track = ec.track;
    const int measIdx = mc.measIdx;
    const std::set<int>& voice4NoteTicks = mc.voice4NoteTicks;
    const std::map<int, int>& v0NoteCountAtTick = mc.v0NoteCountAtTick;
    const std::map<int, int>& ornFingCountAtTick = mc.ornFingCountAtTick;
    const int maxVoice0Tick = mc.maxVoice0Tick;

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
}

void handleOrnament(BuildCtx& ctx, MeasEmitCtx& mc, NoteElemCtx& ec)
{
    MasterScore* score = ctx.score;
    Measure* measure = mc.measure;
    const EncMeasure& encMeas = *mc.encMeas;
    const Fraction measTick = mc.measTick;
    const int measIdx = mc.measIdx;
    const std::set<int>& noteTicks = mc.noteTicks;
    const EncMeasureElem* e = ec.e;
    int& staffIdx = ec.staffIdx;          // mutable ref (dynamic rerouting)
    int voice = ec.voice;
    int msVoice = ec.msVoice;
    track_idx_t& track = ec.track;        // mutable ref (dynamic rerouting)
    Fraction elemTick = ec.elemTick;

    const EncOrnament* eo = static_cast<const EncOrnament*>(e);

    // Helper: register a bowing/articulation ORN in pendingBowings.
    // The three repeated lines (cm, bowTick, push_back) are identical across 8 cases.
    auto pushBowing = [&](SymId sid) {
        const bool cm = !noteTicks.count(static_cast<int>(e->tick));
        const Fraction bt = measTick + Fraction(static_cast<int>(e->tick), kEncWholeTicks);
        ctx.pendingBowings.push_back({ bt, track, sid, measIdx, cm,
                                       static_cast<int>(eo->xoffset), static_cast<int>(e->tick) });
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
        {
            const int wt = encWholeNoteTicks(encMeas);
            ps.startTick = measTick
                           + Fraction(static_cast<int>(eo->tick), wt).reduced();
        }
        ps.track = track;
        ps.startMeasIdx = measIdx;
        ps.endMeasIdx = endIdx;
        ps.alMezuro = static_cast<int>(eo->alMezuro);
        ps.alMezuroValid = eo->alMezuroValid;
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
    case EncOrnamentType::WEDGESTART:
        handleWedgeStart(ctx, mc, ec, eo);
        break;
    case EncOrnamentType::WEDGESTOP:
        break;
    case EncOrnamentType::TEMPO:
        handleTempoOrnament(ctx, mc, ec, eo, score);
        break;
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
    case EncOrnamentType::TRILL_SHORT:
        handleTrillOrnament(ctx, mc, ec, eo);
        break;
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
    case EncOrnamentType::FERMATA_ABOVE:
    case EncOrnamentType::FERMATA_BELOW:
        ctx.pendingFermatas.push_back({ elemTick, track,
                                        eo->ornType() == EncOrnamentType::FERMATA_ABOVE
                                        ? SymId::fermataAbove : SymId::fermataBelow });
        break;
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
    case EncOrnamentType::CAESURA:
    case EncOrnamentType::BREATH_COMMA:
        ctx.pendingBreaths.push_back({ elemTick, track,
                                       eo->ornType() == EncOrnamentType::CAESURA
                                       ? SymId::caesura : SymId::breathMarkComma });
        break;
    case EncOrnamentType::ACCENT:               pushBowing(SymId::articAccentAbove);
        break;
    case EncOrnamentType::DOWNBOW:              pushBowing(SymId::stringsDownBow);
        break;
    case EncOrnamentType::UPBOW:  pushBowing(SymId::stringsUpBow);
        break;
    case EncOrnamentType::MARCATO:              pushBowing(SymId::articMarcatoAbove);
        break;
    case EncOrnamentType::MARCATO_BELOW:        pushBowing(SymId::articMarcatoBelow);
        break;
    case EncOrnamentType::MARCATO_STACCATO_BELOW: pushBowing(SymId::articMarcatoStaccatoBelow);
        break;
    case EncOrnamentType::TENUTO:               pushBowing(SymId::articTenutoAbove);
        break;
    case EncOrnamentType::GUITAR_BEND:
    case EncOrnamentType::GUITAR_BEND_2:
    case EncOrnamentType::GUITAR_PREBEND:
    case EncOrnamentType::GUITAR_PREBEND_RELEASE:
    case EncOrnamentType::GUITAR_BEND_V:
        LOGW() << QString("Encore: guitar bend 0x%1 not yet imported (measure %2 staff %3 tick %4)")
            .arg(eo->tipo, 2, 16, QChar('0'))
            .arg(measIdx)
            .arg(staffIdx)
            .arg(static_cast<int>(e->tick));
        break;
    case EncOrnamentType::DOUBLE_MORDENT:       pushBowing(SymId::ornamentPrallMordent);
        break;
    case EncOrnamentType::TREMOLO_16: {
        PendingOrnTremolo pt;
        pt.tick = elemTick;
        pt.measTick = measTick;
        pt.staffIdx = staffIdx;
        pt.msVoice = msVoice;
        pt.tremType = TremoloType::R16;
        ctx.pendingOrnTremolos.push_back(pt);
        break;
    }
    case EncOrnamentType::STRING_NUMBER_2:
    case EncOrnamentType::STRING_NUMBER_3:
    case EncOrnamentType::STRING_NUMBER_4:
    case EncOrnamentType::STRING_NUMBER_5:
    case EncOrnamentType::STRING_NUMBER_6:
        handleStringNumberOrnament(ctx, mc, ec, eo);
        break;
    case EncOrnamentType::OTTAVA_ALTA:
    case EncOrnamentType::OTTAVA_BASSA: {
        const OttavaType ot = (eo->ornType() == EncOrnamentType::OTTAVA_ALTA)
                              ? OttavaType::OTTAVA_8VA
                              : OttavaType::OTTAVA_8VB;
        ctx.pendingOttavas.push_back({ elemTick, track, staffIdx, ot });
        break;
    }
    case EncOrnamentType::GRAPHIC_LINE:
        break;
    case EncOrnamentType::FINGER_1:
    case EncOrnamentType::FINGER_2:
    case EncOrnamentType::FINGER_3:
    case EncOrnamentType::FINGER_4:
    case EncOrnamentType::FINGER_5:
        handleFingerOrnament(ctx, mc, ec, eo);
        break;
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
    case EncOrnamentType::DYN_SF:
        handleDynamicOrnament(ctx, mc, ec, eo, measure, score);
        break;
    case EncOrnamentType::STAFFTEXT:
        handleStaffTextOrnament(ctx, mc, ec, eo, measure, score);
        break;
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
