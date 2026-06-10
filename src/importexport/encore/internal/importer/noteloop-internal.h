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

namespace mu::iex::encore {
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
    std::set<std::tuple<int, int, int> > tieStartSet;
    std::set<int> noteTicks;
    std::set<int> voice4NoteTicks;
    std::map<int, int> v0NoteCountAtTick;
    std::map<int, int> ornFingCountAtTick;
    int maxVoice0Tick = -1;
    std::set<std::tuple<int, int, int> > filteredTieSenderPitches;

    bool isTieStartAt(int si, int v, int tick) const;
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
void handleRest(BuildCtx& ctx, NoteLoopMeasCtx& mc, NoteElemCtx& ec);
void handleOrnament(BuildCtx& ctx, NoteLoopMeasCtx& mc, NoteElemCtx& ec);

// Render tempo text (beatTicks: 360=dotted-quarter, 240=quarter). Defined in noteloop.cpp.
mu::engraving::String tempoXmlText(int displayBpm, int beatTicks);
} // namespace mu::iex::encore

#endif // MU_IEX_ENCORE_NOTELOOP_INTERNAL_H
