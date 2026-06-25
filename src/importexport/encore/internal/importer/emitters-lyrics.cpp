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

#include "emitters-internal.h"

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
    auto& queue = ctx.pendingLyrics[track];
    if (text == u"-") {
        if (!queue.empty()) {
            queue.back().hyphenAfter = true;
        }
        ctx.nextLyricHyphenBefore[track] = true;
    } else if (text.isEmpty()) {
        ctx.nextLyricHyphenBefore[track] = false;
    } else {
        PendingLyric pl;
        pl.encTick = static_cast<int>(el->tick);
        pl.text = text;
        auto it = ctx.nextLyricHyphenBefore.find(track);
        pl.hyphenBefore = (it != ctx.nextLyricHyphenBefore.end()) && it->second;
        pl.hyphenAfter = false;
        ctx.nextLyricHyphenBefore[track] = false;
        queue.push_back(std::move(pl));
    }
}

// Attach queued lyrics from ctx.pendingLyrics to the nearest chord in the measure.
// Uses a "lyrics-first" greedy assignment: for each syllable in tick order, claim
// the nearest available note within the threshold, so later syllables cannot steal
// the note from an earlier one.
void attachPendingLyrics(BuildCtx& ctx, const MeasEmitCtx& mc)
{
    Measure* measure = mc.measure;
    const EncMeasure& encMeas = *mc.encMeas;
    const Fraction measTick = mc.measTick;

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
    std::map<int, std::vector<int> > encNoteTicksByStaff;
    for (const auto& elem : encMeas.elements) {
        const EncMeasureElem* e = elem.get();
        if (e->type != static_cast<quint8>(EncElemType::NOTE)) {
            continue;
        }
        int rStaff = 0, rVoice = 0, rMsVoice = 0;
        track_idx_t rTrack = 0;
        std::pair<int, int> rTrackKey, rEncVoiceKey;
        if (!mc.lineSlotByRawByte
            || !routeElementStaffVoice(e, /*isNoteOrRest*/ true, *mc.lineSlotByRawByte, mc, ctx,
                                       rStaff, rVoice, rMsVoice, rTrack, rTrackKey, rEncVoiceKey)) {
            continue;
        }
        if (rMsVoice != 0) {
            continue;   // lyrics anchor to voice-0 chords
        }
        auto& tickList = encNoteTicksByStaff[rStaff];
        const int t = static_cast<int>(e->tick);
        if (tickList.empty() || tickList.back() != t) {
            tickList.push_back(t);
        }
    }

    // matchThreshold: half a beat in Encore ticks.
    const int beatTicksVal = encMeas.beatTicks ? static_cast<int>(encMeas.beatTicks) : 240;
    const int matchThreshold = beatTicksVal / 2;

    for (auto& [lyTrack, entries] : ctx.pendingLyrics) {
        if (entries.empty()) {
            continue;
        }
        // Encore multi-verse: verse 1=voice 0, verse 2=voice 1.
        // MuseScore anchors all verses to voice-0 chord via setVerse().
        const int lyStaffIdx = static_cast<int>(lyTrack) / VOICES;
        const int lyVerseNo = static_cast<int>(lyTrack) % VOICES;
        const track_idx_t chordTrack = static_cast<track_idx_t>(lyStaffIdx) * VOICES;

        // Build a list of all ChordRest elements (chords and rests) with their enc ticks.
        // segEncTick is taken directly from the Encore NOTE elements in order (positional
        // assignment): the kth MuseScore chord corresponds to the kth Encore note tick.
        // This is more accurate than converting MuseScore cumTick to Encore ticks because
        // the note loop accumulates durations (not encTicks), so the relationship is not
        // proportional when note durations don't align with the Encore tick grid.
        const std::vector<int>* noteTickList = nullptr;
        {
            auto it = encNoteTicksByStaff.find(lyStaffIdx);
            if (it != encNoteTicksByStaff.end() && !it->second.empty()) {
                noteTickList = &it->second;
            }
        }
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
                const int durTicks = encMeas.durTicks ? static_cast<int>(encMeas.durTicks) : 960;
                segEncTick = (relTick.numerator() * durTicks)
                             / std::max(1, relTick.denominator());
            }
            crTickPairs.emplace_back(segEncTick, cr);
        }

        std::vector<bool> crConsumed(crTickPairs.size(), false);
        for (const auto& pl : entries) {
            // First pass: find nearest chord within the threshold, with two-tier preference:
            // Tier 1: prefer notes where note_tick <= lyric_tick (lyric comes after note start).
            // Tier 2: within tier, prefer the closest note by absolute distance.
            // This prevents lyrics from matching a later note simply because it is
            // absolutely closer (e.g., when lyric and note are not perfectly aligned).
            int bestIdx = -1;
            int bestDelta = matchThreshold + 1;
            bool bestIsAfter = false;  // true if best match has note_tick > lyric_tick
            for (size_t ni = 0; ni < crTickPairs.size(); ++ni) {
                if (crConsumed[ni] || !crTickPairs[ni].second->isChord()) {
                    continue;
                }
                const int delta = std::abs(crTickPairs[ni].first - pl.encTick);
                if (delta > matchThreshold) {
                    continue;
                }
                const bool isAfter = (crTickPairs[ni].first > pl.encTick);
                // Prefer not-after over after; within same tier, prefer smaller delta.
                if (bestIdx < 0
                    || (!isAfter && bestIsAfter)
                    || (isAfter == bestIsAfter && delta < bestDelta)) {
                    bestDelta = delta;
                    bestIdx = static_cast<int>(ni);
                    bestIsAfter = isAfter;
                }
            }
            // Fallback: attach to the nearest rest in the measure.
            if (bestIdx < 0) {
                int bestRestDelta = INT_MAX;
                for (size_t ni = 0; ni < crTickPairs.size(); ++ni) {
                    if (crConsumed[ni] || !crTickPairs[ni].second->isRest()) {
                        continue;
                    }
                    const int delta = std::abs(crTickPairs[ni].first - pl.encTick);
                    if (delta < bestRestDelta) {
                        bestRestDelta = delta;
                        bestIdx = static_cast<int>(ni);
                    }
                }
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
        }
        // Lyric ticks are measure-relative; unmatched leftovers cannot anchor in a
        // later measure, so discard them.
        entries.clear();
    }
    // ctx.nextLyricHyphenBefore survives barlines so a trailing hyphen (e.g. "RO -")
    // carries into the next measure's first syllable.
}
} // namespace mu::iex::enc
