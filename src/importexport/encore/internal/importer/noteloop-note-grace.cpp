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
#include "../parser/ticks.h"

#include "engraving/dom/chord.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/note.h"
#include "engraving/dom/segment.h"

namespace mu::iex::enc {
using namespace mu::engraving;

// Create a detached grace Chord for the given EncNote and queue or attach it.
// Returns true if the note was handled as a grace (caller must return immediately).
//
// Grace chords must be parented under a Chord, not a Segment, or pagePos crashes.
// In v0xC4 files Encore serializes the main note BEFORE the grace at the same beat
// (isChordExt=TRUE). In that case we attach retroactively to the existing chord
// instead of queuing for the next one.
bool tryHandleGraceNote(BuildCtx& ctx, NoteLoopMeasCtx& mc, NoteElemCtx& ec,
                        const EncNote* en)
{
    // en->isInnerGrace is set by calculateRealDurations() for v0xA6:
    // a note with grace1 high nibble=0x10 that is shorter than the leading grace.
    if (!isValidFaceValue(en->faceValue)) {
        return false;
    }
    if ((en->faceValue & 0x0F) < 4) {
        return false;
    }
    if (en->graceType() == EncGraceType::NORMAL && !en->isInnerGrace) {
        return false;
    }

    const auto trackKey = ec.trackKey;

    // Roll back per-track tick state so the next note is not detected as a
    // chord extension of this grace.
    if (ec.savedPrevMidiTick >= 0) {
        ctx.prevMidiTick[trackKey] = ec.savedPrevMidiTick;
    } else {
        ctx.prevMidiTick.erase(trackKey);
    }
    if (ec.hadLastChordPos) {
        ctx.lastChordPos[trackKey] = ec.savedLastChordPos;
    } else {
        ctx.lastChordPos.erase(trackKey);
    }

    DurationType graceDt = realDuration2DurationType(en->realDuration, en->faceValue);
    Chord* gc = Factory::createChord(ctx.score->dummy()->segment());
    gc->setTrack(ec.track);
    TDuration gdur(graceDt);
    gc->setDurationType(gdur);
    gc->setTicks(gdur.fraction());
    gc->setDots(0);
    gc->setNoteType(en->graceType() == EncGraceType::ACCIACCATURA
                    ? NoteType::ACCIACCATURA : NoteType::APPOGGIATURA);

    Note* gnote = Factory::createNote(gc);
    applyConcertPitch(gnote, en->semiTonePitch + ctx.staffPitchOffset[ec.staffIdx]);
    gc->add(gnote);

    for (quint8 ab : { en->articulationUp, en->articulationDown }) {
        for (SymId sid : encArticulation2SymIds(ab)) {
            if (sid == SymId::noSym) {
                continue;
            }
            Articulation* art = Factory::createArticulation(gc);
            art->setSymId(sid);
            gc->add(art);
        }
    }

    // Retroactive attachment: main note already placed at elemTick (isChordExt=TRUE).
    if (ec.isChordExt) {
        Segment* existingSeg = mc.measure->getSegment(SegmentType::ChordRest, ec.elemTick);
        if (existingSeg) {
            EngravingItem* existingEl = existingSeg->element(ec.track);
            if (existingEl && existingEl->isChord()) {
                gc->setGraceIndex(0);
                toChord(existingEl)->add(gc);
                ctx.graceStolenTicks[trackKey] += faceValue2ticks(en->faceValue & 0x0F);
                return true;
            }
        }
    }

    ctx.pendingGraces[trackKey].push_back(gc);
    ctx.graceStolenTicks[trackKey] += faceValue2ticks(en->faceValue & 0x0F);
    return true;
}

} // namespace mu::iex::enc
