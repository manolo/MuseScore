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

#include "resolvers.h"
#include "../parser/elem.h"
#include "../parser/ticks.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/fingering.h"
#include "engraving/dom/note.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/systemlock.h"
#include "engraving/dom/layoutbreak.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/articulation.h"

#include <map>

using namespace mu::engraving;

namespace mu::iex::enc {
// Scan `m` for the first chord on preferTrack, falling back to fallbackTrack.
// Sets *outTrack to the track used; returns nullptr if neither is found.
static Chord* findFirstChordInMeasure(Measure* m, track_idx_t preferTrack,
                                      track_idx_t fallbackTrack,
                                      track_idx_t* outTrack)
{
    if (!m) {
        return nullptr;
    }
    // Tracks are derived from untrusted file data; ignore any that fall outside the score.
    const bool preferOk   = validTrack(m->score(), preferTrack);
    const bool fallbackOk = validTrack(m->score(), fallbackTrack);
    Chord* preferred = nullptr;
    Chord* fallback  = nullptr;
    for (Segment* s = m->first(SegmentType::ChordRest);
         s; s = s->next(SegmentType::ChordRest)) {
        if (!preferred && preferOk) {
            EngravingItem* el = s->element(preferTrack);
            if (el && el->isChord()) {
                preferred = toChord(el);
            }
        }
        if (!fallback && fallbackOk) {
            EngravingItem* el = s->element(fallbackTrack);
            if (el && el->isChord()) {
                fallback = toChord(el);
            }
        }
        if (preferred && fallback) {
            break;
        }
    }
    if (preferred) {
        *outTrack = preferTrack;
        return preferred;
    }
    if (fallback) {
        *outTrack = fallbackTrack;
        return fallback;
    }
    return nullptr;
}

static void correctBowingTickFromXoffset(
    PendingBowing& pb,
    const std::vector<PendingBowing>& allBowings,
    const BuildCtx& ctx)
{
    // When xoffset == 0 the ornament has no visual displacement data, it is
    // already tagged at its correct note tick, so no correction is needed.
    if (pb.ornXoffset == 0) {
        return;
    }
    // Pre-check: if there is a note on the ORN's own staff at the ORN's raw
    // Encore tick, that tick directly names the beat the mark sits on, trust it
    // and skip all correction.  The raw enc tick is an explicit per-ORN value and
    // is a far more reliable anchor than the xoffset heuristic: ornXoffset and
    // note xoffset use different horizontal origins (in real files the offset
    // between them is a per-file constant, not zero), so an xoffset-proximity
    // test misfires and snaps a first-beat mark onto a later note.  Correction is
    // only useful when the ORN's own staff has no note at its raw tick (the beat
    // is empty, so the stored tick cannot be taken literally).
    static constexpr int BOW_XOFF_CLUSTER = 6;
    {
        const int staffIdx2 = static_cast<int>(pb.track / VOICES);
        auto noteIt = ctx.noteXoffByMeasStaff.find({ pb.measIdx, staffIdx2 });
        if (noteIt != ctx.noteXoffByMeasStaff.end()) {
            for (const auto& np : noteIt->second) {
                if (np.first == pb.encTickRaw) {
                    return;  // a note exists at the ORN's own beat: trust the raw tick
                }
            }
        }
    }
    // Phase 1: anchor from same-measure ORN with matching xoffset.
    bool fixed = false;
    for (const PendingBowing& anchor : allBowings) {
        if (&anchor == &pb || anchor.measIdx != pb.measIdx || anchor.encTickRaw == 0) {
            continue;
        }
        if (std::abs(anchor.ornXoffset - pb.ornXoffset) <= BOW_XOFF_CLUSTER) {
            pb.tick = anchor.tick;
            fixed = true;
            break;
        }
    }
    if (fixed) {
        return;
    }
    // Phase 2: match via closest note xoffset on the same staff.
    const int staffIdx = static_cast<int>(pb.track / VOICES);
    auto it = ctx.noteXoffByMeasStaff.find({ pb.measIdx, staffIdx });
    if (it == ctx.noteXoffByMeasStaff.end()) {
        return;
    }
    int bestTick = -1;
    int bestDiff = INT_MAX;
    for (const auto& p : it->second) {   // p = { enc_tick, note.xoffset }
        const int diff = pb.ornXoffset - p.second;
        if (diff >= 0 && diff < bestDiff) {
            bestDiff = diff;
            bestTick = p.first;
        }
    }
    if (bestTick >= 0) {
        const int wholeTicks = kEncWholeTicks;  // bowing snaps to the same 960-tick note grid
        const Measure* m = (pb.measIdx >= 0 && pb.measIdx < static_cast<int>(ctx.measuresByIdx.size()))
                           ? ctx.measuresByIdx[pb.measIdx] : nullptr;
        if (m) {
            pb.tick = m->tick() + Fraction(bestTick, wholeTicks);
        }
    }
}

static void applySystemLocksFromLines(BuildCtx& ctx)
{
    const auto& lines  = ctx.enc.lines;
    const auto& enc2ms = ctx.encToMsIdx;
    const int totalMeas = static_cast<int>(ctx.measuresByIdx.size());

    for (size_t li = 0; li < lines.size(); ++li) {
        const auto& line = lines[li];
        const int firstBlock = static_cast<int>(line.start);
        // System span in MEAS blocks. Prefer the stored per-line measure count, but fall
        // back to the gap to the next line's start when it is absent (0). SCO5 (big-endian
        // Encore 5) does not surface measureCount, yet the line start indices are correct,
        // so the start delta recovers each system's length.
        int span = static_cast<int>(line.measureCount);
        if (span <= 0) {
            const int nextStart = (li + 1 < lines.size())
                                  ? static_cast<int>(lines[li + 1].start)
                                  : static_cast<int>(enc2ms.size());
            span = nextStart - firstBlock;
        }
        if (span <= 0) {
            continue;
        }
        const int lastBlock = firstBlock + span - 1;

        if (firstBlock < 0 || lastBlock < firstBlock
            || firstBlock >= static_cast<int>(enc2ms.size())
            || lastBlock >= static_cast<int>(enc2ms.size())) {
            continue;
        }

        const int firstMsIdx = static_cast<int>(enc2ms[static_cast<size_t>(firstBlock)]);
        // Last MuseScore measure = first of the last MEAS block's range, plus however
        // many MuseScore measures that block produces (gap to next block, or to end).
        // Last MS measure = first MS index of last block's range plus the block's span.
        const int nextBlockMs = (lastBlock + 1 < static_cast<int>(enc2ms.size()))
                                ? static_cast<int>(enc2ms[static_cast<size_t>(lastBlock + 1)])
                                : totalMeas;
        const int lastMsIdx = nextBlockMs - 1;

        if (firstMsIdx < 0 || lastMsIdx < firstMsIdx
            || firstMsIdx >= totalMeas || lastMsIdx >= totalMeas) {
            continue;
        }

        Measure* firstM = ctx.measuresByIdx[static_cast<size_t>(firstMsIdx)];
        Measure* lastM  = ctx.measuresByIdx[static_cast<size_t>(lastMsIdx)];
        if (!firstM || !lastM) {
            continue;
        }
        ctx.score->addSystemLock(new SystemLock(firstM, lastM));
    }
}

// pageIdx in EncLineStaffData is the row-on-page counter (0-based, resets each page).
// A page break is placed at the end of line[i] whenever line[i+1].pageIdx <= line[i].pageIdx
// (the counter did not increment, meaning a new page started).
static void applyPageBreaksFromLines(BuildCtx& ctx)
{
    const auto& lines  = ctx.enc.lines;
    const auto& enc2ms = ctx.encToMsIdx;
    const int totalMeas = static_cast<int>(ctx.measuresByIdx.size());

    for (size_t li = 1; li < lines.size(); ++li) {
        const EncLine& prev = lines[li - 1];
        const EncLine& curr = lines[li];

        if (prev.staffData.empty() || curr.staffData.empty()) {
            continue;
        }
        if (curr.staffData[0].pageIdx > prev.staffData[0].pageIdx) {
            continue;   // same page: row counter incremented normally
        }

        // Page break: add LayoutBreak to the last measure of line[li-1].
        const int firstBlock = static_cast<int>(prev.start);
        const int lastBlock  = firstBlock + static_cast<int>(prev.measureCount) - 1;
        if (firstBlock < 0 || lastBlock < firstBlock
            || lastBlock >= static_cast<int>(enc2ms.size())) {
            continue;
        }
        const int nextBlockMs = (lastBlock + 1 < static_cast<int>(enc2ms.size()))
                                ? static_cast<int>(enc2ms[static_cast<size_t>(lastBlock + 1)])
                                : totalMeas;
        const int lastMsIdx = nextBlockMs - 1;
        if (lastMsIdx < 0 || lastMsIdx >= totalMeas) {
            continue;
        }
        Measure* lastM = ctx.measuresByIdx[static_cast<size_t>(lastMsIdx)];
        if (!lastM) {
            continue;
        }
        bool alreadyHasPageBreak = false;
        for (EngravingItem* e : lastM->el()) {
            if (e && e->isLayoutBreak() && toLayoutBreak(e)->isPageBreak()) {
                alreadyHasPageBreak = true;
                break;
            }
        }
        if (!alreadyHasPageBreak) {
            LayoutBreak* lb = Factory::createLayoutBreak(lastM);
            lb->setLayoutBreakType(LayoutBreakType::PAGE);
            lb->setTrack(0);
            lastM->add(lb);
        }
    }
}

static void applyPendingBowings(BuildCtx& ctx, MasterScore* score)
{
    // Tick correction: Encore sometimes stores ORN enc tick=0 when the mark visually
    // falls on a later beat. Correct before attachment.
    for (PendingBowing& pb : ctx.pendingBowings) {
        if (pb.crossMeasure || pb.encTickRaw > 0) {
            continue;
        }
        correctBowingTickFromXoffset(pb, ctx.pendingBowings, ctx);
    }

    // Bowing marks: crossMeasure means Encore misplaced the ORN in the previous measure.
    for (const PendingBowing& pb : ctx.pendingBowings) {
        track_idx_t useTrack = pb.track;
        Chord* c = nullptr;

        if (pb.crossMeasure) {
            int nextIdx = pb.measIdx + 1;
            if (nextIdx >= 0 && nextIdx < static_cast<int>(ctx.measuresByIdx.size())) {
                c = findFirstChordInMeasure(ctx.measuresByIdx[nextIdx],
                                            pb.track + VOICES, pb.track, &useTrack);
            }
        } else {
            Measure* m = score->tick2measure(pb.tick);
            if (m) {
                Segment* seg = m->findSegment(SegmentType::ChordRest, pb.tick);
                if (seg) {
                    // ORN is always voice 0; scan all voices of own staff before sibling.
                    const track_idx_t staffBase = (pb.track / VOICES) * VOICES;
                    for (track_idx_t v = 0; v < VOICES && !c; ++v) {
                        if (!validTrack(score, staffBase + v)) {
                            break;
                        }
                        EngravingItem* el = seg->element(staffBase + v);
                        if (el && el->isChord()) {
                            c = toChord(el);
                            useTrack = staffBase + v;
                        }
                    }
                    if (!c) {
                        const track_idx_t sibBase = staffBase + VOICES;
                        for (track_idx_t v = 0; v < VOICES && !c; ++v) {
                            if (!validTrack(score, sibBase + v)) {
                                break;
                            }
                            EngravingItem* el = seg->element(sibBase + v);
                            if (el && el->isChord()) {
                                c = toChord(el);
                                useTrack = sibBase + v;
                            }
                        }
                    }
                }
            }
        }

        if (!c) {
            continue;
        }
        Articulation* art = Factory::createArticulation(c);
        art->setTrack(useTrack);
        art->setSymId(pb.symId);
        c->add(art);
    }
}

static void applyPendingFingeringOrns(BuildCtx& ctx, MasterScore* score)
{
    // Fingering ORNs (0xB9..0xBD): multiple ORNs at the same tick attach low-to-high.
    // crossMeasure: next-measure sibling chord. preferSibling: 2nd-staff chord at same tick.
    std::map<Chord*, int> fingeringCount;
    for (const PendingOrnFingering& pf : ctx.pendingOrnFingerings) {
        track_idx_t useTrack = pf.track;
        Chord* c = nullptr;

        if (pf.crossMeasure) {
            int nextIdx = pf.measIdx + 1;
            if (nextIdx >= 0 && nextIdx < static_cast<int>(ctx.measuresByIdx.size())) {
                c = findFirstChordInMeasure(ctx.measuresByIdx[nextIdx],
                                            pf.track + VOICES, pf.track, &useTrack);
            }
        } else {
            Measure* m = score->tick2measure(pf.tick);
            if (m) {
                Segment* seg = m->findSegment(SegmentType::ChordRest, pf.tick);
                if (seg) {
                    // Both tracks come from file data; only consult the ones in range.
                    track_idx_t sibTrack = pf.track + VOICES;
                    EngravingItem* sibEl = validTrack(score, sibTrack) ? seg->element(sibTrack) : nullptr;
                    EngravingItem* ownEl = validTrack(score, pf.track) ? seg->element(pf.track) : nullptr;
                    if (pf.preferSibling) {
                        if (sibEl && sibEl->isChord()) {
                            c = toChord(sibEl);
                            useTrack = sibTrack;
                        } else if (ownEl && ownEl->isChord()) {
                            c = toChord(ownEl);
                            useTrack = pf.track;
                        }
                    } else {
                        if (ownEl && ownEl->isChord()) {
                            c = toChord(ownEl);
                            useTrack = pf.track;
                        } else if (sibEl && sibEl->isChord()) {
                            c = toChord(sibEl);
                            useTrack = sibTrack;
                        }
                    }
                }
            }
        }

        if (!c) {
            continue;
        }
        const auto& notes = c->notes();
        if (notes.empty()) {
            continue;
        }
        if (pf.isStringNum) {
            // String number ORN (0xE6..0xEA): circled number above the top note.
            // The per-note artic "options bit 0 + hasScaleStringAnchors" path may have already
            // placed a STRING_NUMBER on this note. Skip if one already exists (dedup).
            Note* n = notes.back();
            bool alreadyHas = false;
            for (EngravingItem* el : n->el()) {
                if (el && el->isFingering()
                    && toFingering(el)->textStyleType() == TextStyleType::STRING_NUMBER) {
                    alreadyHas = true;
                    break;
                }
            }
            if (!alreadyHas) {
                Fingering* f = Factory::createFingering(n, TextStyleType::STRING_NUMBER);
                f->setTrack(useTrack);
                f->setXmlText(String::number(pf.fingerNum));
                n->add(f);
            }
        } else {
            int& idx = fingeringCount[c];
            Note* n = notes[std::min(idx, static_cast<int>(notes.size()) - 1)];
            ++idx;
            Fingering* f = Factory::createFingering(n);
            f->setTrack(useTrack);
            f->setXmlText(String::number(pf.fingerNum));
            n->add(f);
        }
    }
}

void resolveFingeringAndBowing(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;
    applyPendingBowings(ctx, score);
    applyPendingFingeringOrns(ctx, score);
    // SystemLocks enforce Encore's line layout as hard constraints so the engine compresses
    // spacing within the system rather than redistributing measures across lines.
    if (ctx.opts.importSystemLocks) {
        applySystemLocksFromLines(ctx);
    }
    if (ctx.opts.importPageBreaks) {
        applyPageBreaksFromLines(ctx);
    }
}
} // namespace mu::iex::enc
