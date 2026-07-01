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

// Queue Encore LYRIC elements and attach syllables to the nearest chords in a measure.

#include "emitters-internal.h"

#include "../parser/ticks.h"

#include "engraving/dom/chord.h"
#include "engraving/dom/chordrest.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/segment.h"

namespace mu::iex::enc {
// Queue a LYRIC element. Hyphen separators ("-") update the hyphen flags on the
// preceding syllable and set a carry-forward flag for the next one.
void enqueueLyric(BuildCtx& ctx, const EncLyric* el, track_idx_t track)
{
    const String text(el->text);
    auto& queue = ctx.scratch.pendingLyrics[track];
    if (text == u"-") {
        if (!queue.empty()) {
            queue.back().hyphenAfter = true;
        } else {
            // The preceding syllable was attached in an earlier measure (its queue was
            // cleared at the barline). Promote it so the hyphen renders across the bar:
            // a standalone word becomes the start of a hyphenated one (SINGLE -> BEGIN),
            // and an ending syllable becomes a middle one (END -> MIDDLE).
            auto it = ctx.scratch.lastAttachedLyric.find(track);
            if (it != ctx.scratch.lastAttachedLyric.end() && it->second) {
                mu::engraving::Lyrics* prev = it->second;
                if (prev->syllabic() == mu::engraving::LyricsSyllabic::SINGLE) {
                    prev->setSyllabic(mu::engraving::LyricsSyllabic::BEGIN);
                } else if (prev->syllabic() == mu::engraving::LyricsSyllabic::END) {
                    prev->setSyllabic(mu::engraving::LyricsSyllabic::MIDDLE);
                }
            }
        }
        ctx.scratch.nextLyricHyphenBefore[track] = true;
    } else if (text.isEmpty()) {
        ctx.scratch.nextLyricHyphenBefore[track] = false;
    } else {
        PendingLyric pl;
        pl.encTick = static_cast<int>(el->tick);
        pl.xoffset = static_cast<int>(el->kie);   // horizontal anchor; see attach pass
        pl.text = text;
        auto it = ctx.scratch.nextLyricHyphenBefore.find(track);
        pl.hyphenBefore = (it != ctx.scratch.nextLyricHyphenBefore.end()) && it->second;
        pl.hyphenAfter = false;
        ctx.scratch.nextLyricHyphenBefore[track] = false;
        queue.push_back(std::move(pl));
    }
}

// Build a sorted list of Encore NOTE encTicks for each MuseScore staff, so the
// segEncTick lookup uses the actual Encore tick of each note rather than a
// conversion of the MuseScore cumTick.  The cumTick-to-encTick conversion is
// unreliable because the note loop accumulates durations (not Encore ticks),
// and the relationship is not proportional when durations don't align with
// the Encore tick grid (e.g. v0xC4 6/8 with beatTicks=240, lyric offset=51
// while the first note is at encTick=0).
//
// The note is routed to its MuseScore (staff, voice) with the SAME logic as the
// note loop (routeElementStaffVoice).  This matters for grand staves: a note can
// reach a lower staff either via staffWithin (raw-byte slot) or via the voice>=VOICES
// case, and a lyric can reach that staff by a different mechanism.  Keying the tick
// list by raw encStaff (instead of the routed staff) put the lyrics' notes on the
// wrong staff and reversed the syllables.  encNoteTicksByStaff[msStaff] holds the
// Encore ticks of the voice-0 notes that the note loop routed to that MuseScore staff.
static std::map<int, std::vector<int> > buildEncNoteTicksByStaff(
    BuildCtx& ctx, const MeasEmitCtx& mc, const EncMeasure& encMeas)
{
    std::map<int, std::vector<int> > encNoteTicksByStaff;
    for (const auto& elem : encMeas.elements) {
        const EncMeasureElem* e = elem.get();
        if (e->type != static_cast<quint8>(EncElemType::NOTE)) {
            continue;
        }
        if (!mc.lineSlotByRawByte) {
            continue;
        }
        std::optional<RoutedTrack> routed
            = routeElementStaffVoice(e, /*isNoteOrRest*/ true, *mc.lineSlotByRawByte, mc, ctx);
        if (!routed || routed->msVoice != 0) {
            continue;   // skip unroutable; lyrics anchor to voice-0 chords
        }
        auto& tickList = encNoteTicksByStaff[routed->staffIdx];
        const int t = static_cast<int>(e->tick);
        if (tickList.empty() || tickList.back() != t) {
            tickList.push_back(t);
        }
    }
    return encNoteTicksByStaff;
}

// Build a list of all ChordRest elements (chords and rests) on chordTrack with their enc ticks.
// segEncTick is taken directly from the Encore NOTE elements in order (positional assignment):
// the kth MuseScore chord corresponds to the kth Encore note tick. This is more accurate than
// converting MuseScore cumTick to Encore ticks because the note loop accumulates durations
// (not encTicks), so the relationship is not proportional when note durations don't align with
// the Encore tick grid.
static std::vector<std::pair<int, ChordRest*> > buildCrTickPairs(
    Measure* measure, const Fraction& measTick, const EncMeasure& encMeas,
    track_idx_t chordTrack, const std::vector<int>* noteTickList)
{
    std::vector<std::pair<int, ChordRest*> > crTickPairs;
    size_t noteTickIdx = 0;
    for (Segment* s = measure->first(SegmentType::ChordRest);
         s; s = s->next(SegmentType::ChordRest)) {
        EngravingItem* el = s->element(chordTrack);
        if (!el || !el->isChordRest()) {
            continue;
        }
        int segEncTick;
        ChordRest* cr = toChordRest(el);
        // Only advance noteTickIdx for chords, not for rests. Otherwise rests
        // consume encTick entries intended for notes, causing all subsequent notes
        // to have incorrect encTick values and lyrics to mismatch.
        if (cr->isChord() && noteTickList && noteTickIdx < noteTickList->size()) {
            segEncTick = (*noteTickList)[noteTickIdx++];
        } else {
            // Fallback for rests or when Encore note list is exhausted: estimate
            // from the measure's beat grid.
            const Fraction relTick = s->tick() - measTick;
            const int durTicks = encMeas.durTicks ? static_cast<int>(encMeas.durTicks) : kEncWholeTicks;
            segEncTick = (relTick.numerator() * durTicks)
                         / std::max(1, relTick.denominator());
        }
        crTickPairs.emplace_back(segEncTick, cr);
    }
    return crTickPairs;
}

// Find the index of the best unconsumed ChordRest for a lyric at encTick.
// wantChord selects chords (true) or rests (false); maxDelta caps the distance.
// When preferNotAfter is set, notes at/before encTick win over later notes regardless of
// distance, and ties break to the closest (the threshold pass); otherwise the closest by
// absolute distance wins (the rest/last-resort fallback passes).
static int findBestCr(const std::vector<std::pair<int, ChordRest*> >& pairs,
                      const std::vector<bool>& consumed, int encTick,
                      bool wantChord, int maxDelta, bool preferNotAfter)
{
    int bestIdx = -1;
    int bestDelta = INT_MAX;
    bool bestIsAfter = false;  // true if best match has note_tick > encTick
    for (size_t ni = 0; ni < pairs.size(); ++ni) {
        if (consumed[ni]) {
            continue;
        }
        const bool ok = wantChord ? pairs[ni].second->isChord() : pairs[ni].second->isRest();
        if (!ok) {
            continue;
        }
        const int delta = std::abs(pairs[ni].first - encTick);
        if (delta > maxDelta) {
            continue;
        }
        const bool isAfter = (pairs[ni].first > encTick);
        if (bestIdx < 0
            || (preferNotAfter && !isAfter && bestIsAfter)
            || ((!preferNotAfter || isAfter == bestIsAfter) && delta < bestDelta)) {
            bestDelta = delta;
            bestIdx = static_cast<int>(ni);
            bestIsAfter = isAfter;
        }
    }
    return bestIdx;
}

// Attach queued lyrics from ctx.scratch.pendingLyrics to the nearest chord in the measure.
// Uses a "lyrics-first" greedy assignment: for each syllable in tick order, claim
// the nearest available note within the threshold, so later syllables cannot steal
// the note from an earlier one.
void attachPendingLyrics(BuildCtx& ctx, const MeasEmitCtx& mc)
{
    Measure* measure = mc.measure;
    const EncMeasure& encMeas = *mc.encMeas;
    const Fraction measTick = mc.measTick;

    std::map<int, std::vector<int> > encNoteTicksByStaff = buildEncNoteTicksByStaff(ctx, mc, encMeas);

    // matchThreshold: half a beat in Encore ticks.
    const int beatTicksVal = encMeas.beatTicks ? static_cast<int>(encMeas.beatTicks) : 240;
    const int matchThreshold = beatTicksVal / 2;

    // Encore stores the second and later verses with tick=0 on every syllable; the real horizontal
    // position lives only in the xoffset (kie), identical to the matching first-verse syllable.
    // Build a per-staff xoffset->tick reference from the verses whose ticks are reliable (they span
    // more than one value); a collapsed verse is then remapped by nearest xoffset below so all verses
    // align on the same notes. Note xoffsets are not usable for this (absent/zero in v0xA6).
    std::map<int, std::vector<std::pair<int, int> > > xoffTickRefByStaff;   // staff -> [(xoffset, encTick)]
    for (const auto& [refTrack, refEntries] : ctx.scratch.pendingLyrics) {
        if (refEntries.size() < 2) {
            continue;
        }
        bool spans = false;
        for (const auto& e : refEntries) {
            if (e.encTick != refEntries.front().encTick) {
                spans = true;
                break;
            }
        }
        if (!spans) {
            continue;
        }
        auto& ref = xoffTickRefByStaff[static_cast<int>(refTrack) / VOICES];
        for (const auto& e : refEntries) {
            ref.emplace_back(e.xoffset, e.encTick);
        }
    }

    for (auto& [lyTrack, entries] : ctx.scratch.pendingLyrics) {
        if (entries.empty()) {
            continue;
        }
        // Encore multi-verse: verse 1=voice 0, verse 2=voice 1.
        // MuseScore anchors all verses to voice-0 chord via setVerse().
        const int lyStaffIdx = static_cast<int>(lyTrack) / VOICES;
        const int lyVerseNo = static_cast<int>(lyTrack) % VOICES;
        const track_idx_t chordTrack = static_cast<track_idx_t>(lyStaffIdx) * VOICES;

        // A collapsed verse (every syllable at the same tick) but with distinct xoffsets is the
        // tick=0 verse-2 case: remap each syllable's tick from its xoffset via the staff reference.
        {
            bool collapsed = entries.size() > 1;
            bool xoffDistinct = false;
            for (const auto& e : entries) {
                if (e.encTick != entries.front().encTick) {
                    collapsed = false;
                }
                if (e.xoffset != entries.front().xoffset) {
                    xoffDistinct = true;
                }
            }
            auto rit = xoffTickRefByStaff.find(lyStaffIdx);
            if (collapsed && xoffDistinct && rit != xoffTickRefByStaff.end()) {
                for (auto& e : entries) {
                    int bestTick = -1;
                    int bestDelta = INT_MAX;
                    for (const auto& [rx, rt] : rit->second) {
                        const int d = std::abs(rx - e.xoffset);
                        if (d < bestDelta) {
                            bestDelta = d;
                            bestTick = rt;
                        }
                    }
                    if (bestTick >= 0) {
                        e.encTick = bestTick;
                    }
                }
            }
        }

        const std::vector<int>* noteTickList = nullptr;
        {
            auto it = encNoteTicksByStaff.find(lyStaffIdx);
            if (it != encNoteTicksByStaff.end() && !it->second.empty()) {
                noteTickList = &it->second;
            }
        }
        std::vector<std::pair<int, ChordRest*> > crTickPairs
            = buildCrTickPairs(measure, measTick, encMeas, chordTrack, noteTickList);

        std::vector<bool> crConsumed(crTickPairs.size(), false);
        for (const auto& pl : entries) {
            // First pass: nearest chord within the threshold, with two-tier preference:
            // prefer notes at/before the lyric tick, then the closest by absolute distance.
            // This prevents lyrics from matching a later note simply because it is
            // absolutely closer (e.g., when lyric and note are not perfectly aligned).
            int bestIdx = findBestCr(crTickPairs, crConsumed, pl.encTick,
                                     /*wantChord*/ true, matchThreshold, /*preferNotAfter*/ true);
            // Fallback: attach to the nearest rest in the measure.
            if (bestIdx < 0) {
                bestIdx = findBestCr(crTickPairs, crConsumed, pl.encTick,
                                     /*wantChord*/ false, INT_MAX, /*preferNotAfter*/ false);
            }
            // Last resort: a sung syllable always belongs to a note. If nothing matched
            // within the threshold and no rest was available, attach it to the nearest
            // unconsumed chord at any distance rather than dropping it. This recovers
            // continuation syllables (e.g. "fin-ger", "soft-ly") whose stored tick sits
            // between notes, further than half a beat from the note they belong to.
            if (bestIdx < 0) {
                bestIdx = findBestCr(crTickPairs, crConsumed, pl.encTick,
                                     /*wantChord*/ true, INT_MAX, /*preferNotAfter*/ false);
            }
            if (bestIdx < 0) {
                continue;
            }
            crConsumed[bestIdx] = true;
            ChordRest* c = crTickPairs[bestIdx].second;
            Lyrics* ly = Factory::createLyrics(c);
            ly->setTrack(chordTrack);
            ly->setVerse(lyVerseNo);
            ly->setXmlText(pl.text);
            LyricsSyllabic syll = LyricsSyllabic::SINGLE;
            if (pl.hyphenBefore && pl.hyphenAfter) {
                syll = LyricsSyllabic::MIDDLE;
            } else if (pl.hyphenBefore) {
                syll = LyricsSyllabic::END;
            } else if (pl.hyphenAfter) {
                syll = LyricsSyllabic::BEGIN;
            }
            ly->setSyllabic(syll);
            c->add(ly);
            ctx.scratch.lastAttachedLyric[lyTrack] = ly;
        }
        // Lyric ticks are measure-relative; unmatched leftovers cannot anchor in a
        // later measure, so discard them.
        entries.clear();
    }
    // ctx.scratch.nextLyricHyphenBefore survives barlines so a trailing hyphen (e.g. "RO -")
    // carries into the next measure's first syllable.
}
} // namespace mu::iex::enc
