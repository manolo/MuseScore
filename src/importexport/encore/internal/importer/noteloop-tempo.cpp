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

#include "engraving/dom/factory.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/tempotext.h"

namespace mu::iex::encore {

// Render tempo text. displayBpm is the beat-unit BPM that Encore shows the user.
// beatTicks=360 means the beat unit is a dotted quarter; beatTicks=240 is a quarter.
String tempoXmlText(int displayBpm, int beatTicks)
{
    if (beatTicks == 360) {
        return String(u"<sym>metNoteQuarterUp</sym><sym>space</sym><sym>metAugmentationDot</sym> = %1").arg(displayBpm);
    }
    return String(u"<sym>metNoteQuarterUp</sym> = %1").arg(displayBpm);
}

// Apply per-measure BPM from MEAS headers as TempoText elements.
// Emits a TempoText only when BPM changes, and skips measures that already
// have an ORN TEMPO or STAFFTEXT tempo mark to avoid duplicates.
void applyMeasureBpmMarks(BuildCtx& ctx)
{
    const EncRoot& enc = ctx.enc;
    MasterScore* score = ctx.score;

    quint16 lastBpm = 0;
    for (size_t mi = 0; mi < enc.measures.size(); ++mi) {
        const quint16 bpm = enc.measures[mi].bpm;
        if (bpm == 0) {
            continue;
        }
        if (mi > 0 && bpm == lastBpm) {
            continue;
        }
        const size_t msI = (mi < ctx.encToMsIdx.size()) ? ctx.encToMsIdx[mi] : mi;
        if (msI >= ctx.measuresByIdx.size()) {
            continue;
        }
        Measure* m = ctx.measuresByIdx[msI];
        const Fraction measTick = m->tick();
        Segment* seg = m->getSegment(SegmentType::ChordRest, measTick);
        if (!seg) {
            continue;
        }
        bool hasExisting = false;
        for (Segment* s = m->first(SegmentType::ChordRest); s && !hasExisting;
             s = s->next(SegmentType::ChordRest)) {
            for (EngravingItem* e : s->annotations()) {
                if (e && e->isTempoText()) {
                    hasExisting = true;
                    break;
                }
            }
        }
        if (!hasExisting) {
            // Detect dotted-quarter beat: MEAS header beatTicks=360, OR compound time sig
            // (6/8, 9/8, 12/8). Old fixtures store beatTicks=240 even for 6/8, so keep
            // the timesig fallback for backward compatibility.
            const quint16 rawBeatTicks = enc.measures[mi].beatTicks;
            const Fraction mts = m->timesig();
            const bool cmpd = (rawBeatTicks == 360)
                              || (mts.denominator() == 8
                                  && mts.numerator() % 3 == 0
                                  && mts.numerator() > 3);
            const double bps = bpm / 60.0;
            const int displayBpm = cmpd ? (bpm * 2 + 1) / 3 : static_cast<int>(bpm);
            TempoText* tt = Factory::createTempoText(seg);
            tt->setTrack(0);
            tt->setTempo(BeatsPerSecond(bps));
            tt->setXmlText(tempoXmlText(displayBpm, cmpd ? 360 : 240));
            tt->setFollowText(true);
            seg->add(tt);
            score->setTempo(measTick, BeatsPerSecond(bps));
        }
        lastBpm = bpm;
    }
}

} // namespace mu::iex::encore
