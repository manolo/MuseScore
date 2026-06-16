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

#ifndef MU_IEX_ENCORE_NOTELOOP_INTERNAL_H
#define MU_IEX_ENCORE_NOTELOOP_INTERNAL_H

#include "ctx.h"
#include "tuplets.h"
#include "../parser/elements.h"
#include "engraving/types/types.h"
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace mu::engraving {
class Measure;
class Note;
}

namespace mu::iex::enc {
// faceValue low nibble: 1=whole, 2=half ... 8=256th; 0 and 9..15 are invalid.
bool isValidFaceValue(quint8 faceValue);
void applyConcertPitch(mu::engraving::Note* n, int semitone);

struct NoteLoopMeasCtx {
    mu::engraving::Measure* measure = nullptr;
    const EncMeasure* encMeas = nullptr;
    mu::engraving::Fraction measTick;
    int measIdx = 0;
    int nLineStaves = 0;
    const std::vector<int>* lineStaffInstrIdx = nullptr;
    const std::vector<int>* lineStaffWithin = nullptr;

    std::set<const EncMeasureElem*> validTupletGroupMember;
    std::set<const EncMeasureElem*> partialEndGroup;
    std::vector<NestedTupletInfo>   nestedInfos;
    // All notes that belong to an INNER group (the notes inside the nested sub-tuplet).
    std::set<const EncMeasureElem*> innerGroupMembers;
    // Override (actualN, normalN) for notes detected as uniform-fill groups (e.g. 15→[15:8]).
    std::map<const EncMeasureElem*, std::pair<int,int>> overrideGroupRatios;
    // Lookup: first elem of inner group → NestedTupletInfo*
    std::map<const EncMeasureElem*, const NestedTupletInfo*> nestedByInnerFirst;
    // Lookup: last elem of inner group → NestedTupletInfo*
    std::map<const EncMeasureElem*, const NestedTupletInfo*> nestedByInnerLast;
    // key={si,v,tick}, value=sourcePosition from EncTie (+14); -1 means all notes at that tick
    std::multimap<std::tuple<int, int, int>, int8_t> tieStartSet;
    std::set<int> noteTicks;
    std::set<int> voice4NoteTicks;
    std::map<int, int> v0NoteCountAtTick;
    std::map<int, int> ornFingCountAtTick;
    int maxVoice0Tick = -1;
    std::set<std::tuple<int, int, int> > filteredTieSenderPitches;
    // True when at least one note in the measure has au in 0x39..0x40 (scale string anchors).
    // When set, notes with options bit 0 and no other artic byte also show string numbers.
    bool hasScaleStringAnchors { false };

    // notePosition=-1 matches any note (used for bypass checks); otherwise only matches the given position.
    bool isTieStartAt(int si, int v, int tick, int notePosition = -1) const;
    void closeTupletWithFill(BuildCtx& ctx, TupletTracker& tt, std::pair<int, int> trackKey);
};

// Per-element context: computed in the main element loop before dispatch.
struct NoteElemCtx {
    const EncMeasureElem* e = nullptr;
    EncElemType et {};
    int staffIdx = 0;
    int voice = 0;
    int msVoice = 0;
    mu::engraving::track_idx_t track = 0;
    std::pair<int, int> trackKey;
    std::pair<int, int> encVoiceKey;
    bool isChordExt = false;
    bool isNoteOrRest = false;
    mu::engraving::Fraction elemTick;
    int savedPrevMidiTick = -1;
    bool hadLastChordPos = false;
    mu::engraving::Fraction savedLastChordPos;
};

void handleNote(BuildCtx& ctx, NoteLoopMeasCtx& mc, NoteElemCtx& ec);
// Returns true if note was a grace note (caller must return). (noteloop-note-grace.cpp)
bool tryHandleGraceNote(BuildCtx& ctx, NoteLoopMeasCtx& mc, NoteElemCtx& ec,
                        const EncNote* en);
// Apply articulations, ornaments, tremolos, string numbers to a note/chord. (noteloop-note-artic.cpp)
void applyNoteArticulations(mu::engraving::Note* note, mu::engraving::Chord* chord,
                             const EncNote* en, mu::engraving::track_idx_t track,
                             const NoteLoopMeasCtx& mc);
void handleRest(BuildCtx& ctx, NoteLoopMeasCtx& mc, NoteElemCtx& ec);
void handleOrnament(BuildCtx& ctx, NoteLoopMeasCtx& mc, NoteElemCtx& ec);
void handleChordSym(BuildCtx& ctx, const NoteLoopMeasCtx& mc, const NoteElemCtx& ec);

// Queue one LYRIC element into ctx.pendingLyrics. (noteloop-lyrics.cpp)
void enqueueLyric(BuildCtx& ctx, const EncLyric* el, mu::engraving::track_idx_t track);
// Attach queued lyrics to the nearest chords in the measure. (noteloop-lyrics.cpp)
void attachPendingLyrics(BuildCtx& ctx, mu::engraving::Measure* measure,
                         const EncMeasure& encMeas, mu::engraving::Fraction measTick);

// Case-B pickup adjustment: shorten measure 0 if loop placed less than its nominal length. (noteloop-fill.cpp)
void adjustPickupMeasure(BuildCtx& ctx, mu::engraving::Measure* measure, int measIdx);
// Pre-fill trailing silence with invisible gap rests. (noteloop-fill.cpp)
void fillTrailingGaps(BuildCtx& ctx, mu::engraving::Measure* measure, mu::engraving::Fraction measTick);
// Fix over/undershoots up to 1/24. (noteloop-fill.cpp)
void correctMeasureLength(mu::engraving::Measure* measure, int totalStaves);
// Nuclear hard-cap: remove trailing elements and fill deficit. (noteloop-fill.cpp)
void capMeasureLength(mu::engraving::Measure* measure, int totalStaves);

// Apply per-measure BPM marks as TempoText elements. (noteloop-tempo.cpp)
void applyMeasureBpmMarks(BuildCtx& ctx);

// Render tempo text (beatTicks: 360=dotted-quarter, 240=quarter). (noteloop-tempo.cpp)
mu::engraving::String tempoXmlText(int displayBpm, int beatTicks);
} // namespace mu::iex::enc

#endif // MU_IEX_ENCORE_NOTELOOP_INTERNAL_H
