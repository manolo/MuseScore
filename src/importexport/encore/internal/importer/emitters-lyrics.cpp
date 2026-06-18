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
void attachPendingLyrics(BuildCtx& ctx, Measure* measure,
                         const EncMeasure& encMeas, Fraction measTick)
{
    // In compound meters (6/8, 9/8, 12/8) beatTicks is ticks per dotted-quarter beat.
    // Scale to ticks-per-quarter for the segEncTick formula.
    const bool isCompoundMeter = (encMeas.timeSigDen == 8 || encMeas.timeSigDen == 16)
                                 && encMeas.timeSigNum >= 6
                                 && (encMeas.timeSigNum % 3) == 0;
    const int beatTicksVal = encMeas.beatTicks ? static_cast<int>(encMeas.beatTicks) : 240;
    const int encTicksPerQuarter = isCompoundMeter ? beatTicksVal * 2 / 3 : beatTicksVal;
    const int matchThreshold = isCompoundMeter ? beatTicksVal * 2 / 3 : beatTicksVal / 2;

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
        // Chords are preferred for lyric attachment; rests act as fallback anchors.
        std::vector<std::pair<int, ChordRest*> > crTickPairs;
        for (Segment* s = measure->first(SegmentType::ChordRest);
             s; s = s->next(SegmentType::ChordRest)) {
            EngravingItem* el = s->element(chordTrack);
            if (!el || !el->isChordRest()) {
                continue;
            }
            const Fraction relTick = s->tick() - measTick;
            const int segEncTick = (relTick.numerator() * encTicksPerQuarter * 4)
                                   / std::max(1, relTick.denominator());
            crTickPairs.emplace_back(segEncTick, toChordRest(el));
        }

        std::vector<bool> crConsumed(crTickPairs.size(), false);
        for (const auto& pl : entries) {
            // First pass: find nearest chord within the threshold.
            int bestIdx = -1;
            int bestDelta = matchThreshold + 1;
            for (size_t ni = 0; ni < crTickPairs.size(); ++ni) {
                if (crConsumed[ni] || !crTickPairs[ni].second->isChord()) {
                    continue;
                }
                const int delta = std::abs(crTickPairs[ni].first - pl.encTick);
                if (delta < bestDelta) {
                    bestDelta = delta;
                    bestIdx = static_cast<int>(ni);
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
