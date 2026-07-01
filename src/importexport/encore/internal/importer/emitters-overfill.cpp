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

// Overfull-measure resolution. When a voice's content sums to more than the measure
// length, each overfill strategy resolves it here, in the post-pass, so a tuplet is
// always handled atomically (never left as an invalid partial tuplet):
//   - Truncate ("Remove extra notes"): dissolve any cut tuplet, drop trailing notes,
//     dot the last survivor, fill the remainder with an exact rest. Destructive.
//   - StretchLastNote ("Stretch last notes"): preserve the notes by robbing value from
//     earlier notes / compressing the tuplet bracket; fall back to irregular.
//   - IrregularMeasure: extend the measure to fit (handled by capMeasureLength).

#include "emitters-internal.h"

#include "engraving/dom/chord.h"
#include "engraving/dom/durationtype.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/tuplet.h"

#include <vector>

using namespace mu::engraving;

namespace mu::iex::enc {
static void addFillRest(Measure* measure, track_idx_t tr, const Fraction& startTick, const Fraction& totalLen);

// Largest representable note value (with up to maxDots dots) whose fraction is <= f.
// Returns 0/1 if nothing fits.
static Fraction largestDottedLE(const Fraction& f, int maxDots)
{
    if (f.numerator() <= 0) {
        return Fraction(0, 1);
    }
    TDuration d(f, true /*truncate*/, maxDots);
    if (!d.isValid()) {
        return Fraction(0, 1);
    }
    return d.fraction();
}

// Dissolve a tuplet: detach every member so it reverts to its plain face value, then
// remove the (now empty) tuplet from its parent and delete it. A tuplet is atomic, so it
// is dissolved whole rather than leaving a partial tuplet behind.
void dissolveTuplet(Tuplet* t)
{
    if (!t) {
        return;
    }
    std::vector<DurationElement*> members(t->elements().begin(), t->elements().end());
    for (DurationElement* de : members) {
        if (de->isTuplet()) {
            dissolveTuplet(toTuplet(de));
        } else {
            de->setTuplet(nullptr);
        }
    }
    if (EngravingItem* parent = t->parentItem()) {
        parent->remove(t);
    }
    delete t;
}

// Remove any spanner (slur, hairpin, ottava, ...) anchored to this ChordRest before the CR
// is removed or moved. Segment::remove() would otherwise call score()->undo() to null the
// spanner's start/end, leaving a dangling spanner that crashes layout. Done up front here so
// no such undo fires. (Ties live on notes, not in the spanner map, so they are unaffected.)
static void detachSpannersAt(ChordRest* cr)
{
    Score* score = cr->score();
    std::vector<Spanner*> toRemove;
    for (const auto& [tick, s] : score->spanner()) {
        if (s->startElement() == cr || s->endElement() == cr) {
            toRemove.push_back(s);
        }
    }
    for (Spanner* s : toRemove) {
        score->removeSpanner(s);
        delete s;
    }
}

// Collect the ordered ChordRests of one track in a measure and their total actual ticks.
Fraction collectVoice(Measure* measure, track_idx_t tr, std::vector<ChordRest*>& out)
{
    out.clear();
    Fraction sum(0, 1);
    for (Segment* seg = measure->first(SegmentType::ChordRest); seg; seg = seg->next(SegmentType::ChordRest)) {
        EngravingItem* el = seg->element(tr);
        if (!el || !el->isChordRest()) {
            continue;
        }
        ChordRest* cr = toChordRest(el);
        out.push_back(cr);
        sum += cr->actualTicks();
    }
    return sum;
}

// "Remove extra notes" (Truncate). When the overflow is a trailing tuplet, the tuplet is
// dissolved (members revert to plain face value) and the resulting notes are SHRUNK from the
// right to keep as many of them as possible: each trailing note is halved (down to a quarter
// of its value) and only removed if even a quarter still overflows; the process stops as soon
// as the content fits. The last survivor is then dotted (up to 3 dots) to reach the barline
// and any remainder is filled with an exact rest. Plain trailing overflow is just removed
// from the right.
static void removeExtraNotes(BuildCtx& ctx, Measure* measure, track_idx_t tr)
{
    const Fraction mLen = measure->ticks();
    const Fraction measTick = measure->tick();

    std::vector<ChordRest*> crs;

    // 1. Remove trailing PLAIN notes from the right until the voice fits or a tuplet surfaces.
    while (true) {
        Fraction sum = collectVoice(measure, tr, crs);
        if (crs.empty() || sum <= mLen || crs.back()->tuplet()) {
            break;
        }
        detachSpannersAt(crs.back());
        Segment* s = crs.back()->segment();
        ChordRest* last = crs.back();
        s->remove(last);
        delete last;
    }

    ChordRest* lastSurvivor = nullptr;
    Fraction contentEnd(0, 1);   // offset from measTick where surviving content ends

    Fraction sum = collectVoice(measure, tr, crs);
    if (!crs.empty() && sum > mLen && crs.back()->tuplet()) {
        // 2. Trailing tuplet: dissolve, then shrink its members from the right.
        Tuplet* t = crs.back()->tuplet();
        std::vector<ChordRest*> members;
        Fraction preContent(0, 1);
        for (ChordRest* cr : crs) {
            if (cr->tuplet() == t) {
                members.push_back(cr);
            } else if (members.empty()) {
                preContent += cr->actualTicks();
            }
        }
        dissolveTuplet(t);   // members now plain at face value (segment positions are stale)

        const Fraction available = mLen - preContent;
        std::vector<Fraction> durs;
        std::vector<int> halvings(members.size(), 0);
        for (ChordRest* m : members) {
            durs.push_back(m->actualTicks());
        }
        auto durSum = [&]() {
            Fraction s(0, 1);
            for (const Fraction& d : durs) {
                s += d;
            }
            return s;
        };
        // Halve each trailing note (max twice -> down to 1/4 of its value); if even a quarter
        // of it still overflows, remove it and move left. Stop as soon as the content fits.
        int idx = static_cast<int>(members.size()) - 1;
        while (durSum() > available && idx >= 0) {
            if (durs[idx] <= Fraction(0, 1)) {
                --idx;
                continue;
            }
            const Fraction halved = durs[idx] * Fraction(1, 2);
            if (halvings[idx] < 2 && fitsTDuration(halved)) {
                durs[idx] = halved;
                ++halvings[idx];
            } else {
                durs[idx] = Fraction(0, 1);
                --idx;
            }
        }

        // Detach every member from its stale segment, then re-place the survivors
        // sequentially from preContent (deleting the removed ones).
        for (ChordRest* m : members) {
            detachSpannersAt(m);
            m->segment()->remove(m);
        }
        Fraction pos = preContent;
        for (size_t j = 0; j < members.size(); ++j) {
            if (durs[j] <= Fraction(0, 1)) {
                delete members[j];
                continue;
            }
            const TDuration td(durs[j]);
            members[j]->setDurationType(td);
            members[j]->setTicks(td.fraction());
            members[j]->setDots(td.dots());
            members[j]->setTrack(tr);
            Segment* seg = measure->getSegment(SegmentType::ChordRest, measTick + pos);
            seg->add(members[j]);
            pos += durs[j];
            lastSurvivor = members[j];
        }
        contentEnd = pos;
    } else {
        contentEnd = sum;
        lastSurvivor = (!crs.empty() && crs.back()->isChord() && !crs.back()->tuplet()) ? crs.back() : nullptr;
    }

    // 3. Dot the last surviving note (up to 3 dots) to absorb the remaining gap, then fill the
    //    residual with an exact rest.
    Fraction gap = mLen - contentEnd;
    if (gap <= Fraction(0, 1)) {
        return;
    }
    if (lastSurvivor && lastSurvivor->isChord()) {
        const Fraction cur = lastSurvivor->actualTicks();
        const Fraction target = largestDottedLE(cur + gap, 3 /*maxDots*/);
        if (target > cur) {
            const TDuration td(target);
            lastSurvivor->setDurationType(td);
            lastSurvivor->setTicks(td.fraction());
            lastSurvivor->setDots(td.dots());
            contentEnd += (target - cur);
            gap = mLen - contentEnd;
        }
    }
    addFillRest(measure, tr, measTick + contentEnd, gap);
}

// Fill `totalLen` from `startTick` with visible rests, split into individually notatable
// figures (up to 3 dots) via toDurationList so layout keeps them as-is. A single rest with an
// odd total (e.g. a dotted 16th) is fine; larger residues split into a tied-rest sequence.
static void addFillRest(Measure* measure, track_idx_t tr, const Fraction& startTick, const Fraction& totalLen)
{
    if (totalLen <= Fraction(0, 1)) {
        return;
    }
    Fraction pos = startTick;
    for (const TDuration& d : toDurationList(totalLen, true /*useDots*/, 3 /*maxDots*/, false)) {
        if (!d.isValid()) {
            break;
        }
        Segment* fillSeg = measure->getSegment(SegmentType::ChordRest, pos);
        if (!fillSeg->element(tr)) {
            Rest* r = Factory::createRest(fillSeg, d);
            r->setTicks(d.fraction());
            r->setTrack(tr);
            r->setGap(false);
            fillSeg->add(r);
        }
        pos += d.fraction();
    }
}

// "Stretch last notes" for one overfull voice. Preserves all notes by either compressing
// the trailing tuplet's bracket (tier 2) or, for a lone trailing note, reducing it with
// dots; fills the remainder with an exact rest. Returns false (declining) when the result
// would be too small to be musical (tuplet bracket < half its natural span, or no space):
// the caller then falls back to extending the measure (tier 3, IrregularMeasure).
// Note: tier 1 (robbing value from earlier notes) is intentionally not implemented yet.
static bool stretchOverfullVoice(BuildCtx& ctx, Measure* measure, track_idx_t tr)
{
    const Fraction mLen = measure->ticks();
    const Fraction measTick = measure->tick();

    std::vector<ChordRest*> crs;
    const Fraction voiceSum = collectVoice(measure, tr, crs);
    if (crs.empty() || voiceSum <= mLen) {
        return true;
    }

    ChordRest* last = crs.back();
    if (last->tuplet()) {
        Tuplet* t = last->tuplet();
        // Position where the tuplet starts = total actual ticks of the notes before its first member.
        Fraction preContent(0, 1);
        std::vector<ChordRest*> members;
        for (ChordRest* cr : crs) {
            if (cr->tuplet() == t) {
                members.push_back(cr);
            } else if (members.empty()) {
                preContent += cr->actualTicks();
            }
        }
        const Fraction available = mLen - preContent;
        if (available <= Fraction(0, 1) || members.empty()) {
            return false;
        }
        const int aN = t->ratio().numerator();
        const int nN = t->ratio().denominator();
        if (aN <= 0 || nN <= 0) {
            return false;
        }
        const Fraction naturalBracket = TDuration(t->baseLen()).fraction() * nN;
        // Compress the bracket to the largest base that fits (up to 3 dots); the exact
        // remainder is filled with a rest below.
        const Fraction newBaseLen = largestDottedLE(available / nN, 3 /*maxDots*/);
        if (newBaseLen <= Fraction(0, 1)) {
            return false;
        }
        const Fraction newBracket = newBaseLen * nN;
        // Too small to be musical: the largest bracket that fits is < half the natural span.
        if (newBracket * Fraction(2, 1) < naturalBracket) {
            return false;
        }
        // Resize and reposition each member within the compressed bracket.
        const TDuration baseTd(newBaseLen);
        const Fraction memberActual = newBaseLen * Fraction(nN, aN);
        Fraction pos = preContent;
        for (ChordRest* m : members) {
            m->setDurationType(baseTd);
            m->setTicks(baseTd.fraction());
            m->setDots(baseTd.dots());
            const Fraction newTick = measTick + pos;
            if (m->segment()->tick() != newTick) {
                detachSpannersAt(m);
                Segment* oldSeg = m->segment();
                oldSeg->remove(m);
                Segment* ns = measure->getSegment(SegmentType::ChordRest, newTick);
                ns->add(m);
            }
            pos += memberActual;
        }
        t->setBaseLen(baseTd);
        t->setTicks(newBracket);
        t->setTick(measTick + preContent);
        addFillRest(measure, tr, measTick + preContent + newBracket, mLen - (preContent + newBracket));
        return true;
    }

    // Lone trailing note (no tuplet): reduce it to the largest dotted figure that fits.
    const Fraction preContent = voiceSum - last->actualTicks();
    const Fraction available = mLen - preContent;
    if (available <= Fraction(0, 1)) {
        return false;
    }
    const Fraction newDur = largestDottedLE(available, 3 /*maxDots*/);
    if (newDur <= Fraction(0, 1)) {
        return false;
    }
    const TDuration td(newDur);
    last->setDurationType(td);
    last->setTicks(td.fraction());
    last->setDots(td.dots());
    addFillRest(measure, tr, measTick + preContent + newDur, mLen - (preContent + newDur));
    return true;
}

// Post-pass entry point: resolve any overfull voice according to the overfill strategy.
void fitOverfullMeasure(BuildCtx& ctx, Measure* measure)
{
    // IrregularMeasure extends the measure to hold all content (existing logic).
    if (ctx.opts.overfillMeasureStrategy == OverfillStrategy::IrregularMeasure) {
        capMeasureLength(ctx, measure);
        return;
    }

    const Fraction mLen = measure->ticks();
    std::vector<ChordRest*> crs;
    bool needIrregularFallback = false;
    for (int si = 0; si < ctx.totalStaves; ++si) {
        for (voice_idx_t v = 0; v < VOICES; ++v) {
            const track_idx_t tr = static_cast<track_idx_t>(si * VOICES + v);
            const Fraction sum = collectVoice(measure, tr, crs);
            if (crs.empty() || sum <= mLen) {
                continue;
            }
            switch (ctx.opts.overfillMeasureStrategy) {
            case OverfillStrategy::Truncate:
                removeExtraNotes(ctx, measure, tr);
                break;
            case OverfillStrategy::StretchLastNote:
                // Tier 2 (compress bracket) / lone-note reduce; decline -> tier 3 (irregular).
                // Documented limitation: tier 1 (rob value from earlier notes in the bar) is not
                // implemented, so a voice stretch cannot resolve degrades to IrregularMeasure
                // output rather than a standard-length bar. See ENCORE_IMPORTER.md §Overfull measures.
                if (!stretchOverfullVoice(ctx, measure, tr)) {
                    needIrregularFallback = true;
                }
                break;
            default:
                break;
            }
        }
    }
    if (needIrregularFallback) {
        extendMeasureIrregular(ctx, measure);
    }
}
} // namespace mu::iex::enc
