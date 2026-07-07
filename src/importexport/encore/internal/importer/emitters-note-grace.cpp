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

// Emit grace-note chords and attach them to their principal chord.

#include "emitters-internal.h"
#include "mappers.h"
#include "../parser/ticks.h"
#include "durations.h"

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
bool tryHandleGraceNote(BuildCtx& ctx, MeasEmitCtx& mc, NoteElemCtx& ec,
                        const EncNote* en)
{
    // en->isInnerGrace is set by calculateRealDurations() for v0xA6:
    // a note with grace1 high nibble=0x10 that is shorter than the leading grace.
    if (!isValidFaceValue(en->faceValue)) {
        return false;
    }
    if (en->graceType() == EncGraceType::NORMAL && !en->isInnerGrace) {
        return false;
    }
    // An explicitly-flagged grace (acciaccatura/appoggiatura) may carry any written value: Encore
    // lets you turn a quarter or half note into a grace. Only the v0xA6 inferred-grace path
    // (isInnerGrace, graceType NORMAL) keeps the historical "shorter than a quarter" guard.
    if (en->graceType() == EncGraceType::NORMAL && (en->faceValue & 0x0F) < 4) {
        return false;
    }

    const auto trackKey = ec.trackKey;

    // Roll back per-track tick state so the next note is not detected as a
    // chord extension of this grace.
    if (ec.savedPrevMidiTick >= 0) {
        ctx.scratch.prevMidiTick[trackKey] = ec.savedPrevMidiTick;
    } else {
        ctx.scratch.prevMidiTick.erase(trackKey);
    }
    if (ec.hadLastChordPos) {
        ctx.scratch.lastChordPos[trackKey] = ec.savedLastChordPos;
    } else {
        ctx.scratch.lastChordPos.erase(trackKey);
    }

    // Same-tick chord member of the previous grace: Encore serializes each grace chord member as its
    // own note at the same tick. Because the grace path rolls prevMidiTick back, the main-loop chord
    // detection (isChordExt) does not fire for the second member, so merge it into the last grace
    // chord for this track. Otherwise a 2-note grace chord would split into two single-note graces
    // (e.g. a percussion ruff whose members are chords). Covers both before- and after-graces.
    {
        auto gcIt = ctx.scratch.lastGraceChord.find(trackKey);
        auto tkIt = ctx.scratch.lastGraceTick.find(trackKey);
        if (gcIt != ctx.scratch.lastGraceChord.end() && gcIt->second && !gcIt->second->notes().empty()
            && tkIt != ctx.scratch.lastGraceTick.end() && tkIt->second == static_cast<int>(en->tick)) {
            Note* member = Factory::createNote(gcIt->second);
            applyConcertPitch(member, en->semiTonePitch + ctx.staffPitchOffset[ec.staffIdx]);
            if (en->isMuted()) {
                member->setPlay(false);
            }
            gcIt->second->add(member);
            return true;
        }
    }

    const bool appoggiatura = (en->graceType() == EncGraceType::APPOGGIATURA);
    const bool beamedGroup = (en->grace1 & 0x10);

    // Classify the grace against the principal notes of its own voice/measure (from the raw enc):
    //  - principalAtOrAfter: a principal note co-located with or after the grace -> the grace
    //    ornaments THAT note (grace-before), including the co-located grace+main case.
    //  - contiguousNoteBefore: a principal note whose written (face-value) span reaches the grace
    //    tick, with no silence between -> the grace belongs to that preceding note (grace-after).
    // A grace preceded by SILENCE with nothing at/after it in the bar (the Himno floreo) is neither:
    // it falls through to the grace-before path and, via the cross-barline pending carry, ornaments
    // the next bar's downbeat. This matches Encore's rest-attached graces without displacing them
    // onto the nearest left-hand note.
    bool principalAtOrAfter = false;
    bool contiguousNoteBefore = false;
    if (mc.encMeas) {
        const int graceTick = static_cast<int>(en->tick);
        for (const auto& elp : mc.encMeas->elements) {
            if (static_cast<EncElemType>(elp->type) != EncElemType::NOTE
                || elp->staffIdx != ec.staffIdx || elp->voice != ec.voice) {
                continue;
            }
            const EncNote* n = static_cast<const EncNote*>(elp.get());
            if (n->graceType() != EncGraceType::NORMAL) {
                continue;   // only principal (non-grace) notes bound the decision
            }
            if (static_cast<int>(n->tick) >= graceTick) {
                principalAtOrAfter = true;
            } else if (static_cast<int>(n->tick) + faceValue2ticks(n->faceValue & 0x0F) >= graceTick) {
                contiguousNoteBefore = true;
            }
        }
    }

    // A no-slash small note (reported APPOGGIATURA) that stands alone, with no principal note to
    // ornament, is a CUE note, not a grace: it keeps its full rhythmic value. Hand it back to the
    // normal note path (which draws it small). Only a no-slash small note ADJACENT to a principal
    // (co-located, following, or contiguous before) is a real appoggiatura. Acciaccaturas (slash) are
    // always graces, even standalone (a percussion ruff), so this only applies to appoggiaturas.
    if (appoggiatura && !principalAtOrAfter && !contiguousNoteBefore) {
        return false;
    }

    // grace-after only when a contiguous principal note precedes and nothing sits at/after the grace.
    Chord* precedingChord = nullptr;
    if (!appoggiatura && !ec.isChordExt && !principalAtOrAfter && contiguousNoteBefore) {
        for (Segment* s = mc.measure->last(SegmentType::ChordRest); s; s = s->prev(SegmentType::ChordRest)) {
            if (s->tick() >= ec.elemTick) {
                continue;
            }
            EngravingItem* el = s->element(ec.track);
            if (el && el->isChord() && !toChord(el)->isGrace()) {
                precedingChord = toChord(el);
                break;
            }
        }
    }
    const bool afterMode = (precedingChord != nullptr);

    // Grace figure. Appoggiatura keeps its written type on the beat; a lone acciaccatura is the classic
    // slashed eighth; a beamed group keeps its written figure (GRACE16 for sixteenths, no slash, to
    // match Encore, which does not slash beamed grace groups). Grace-after uses the *_AFTER variants.
    NoteType graceNoteType;
    if (appoggiatura) {
        graceNoteType = NoteType::APPOGGIATURA;
    } else if (afterMode) {
        switch (en->faceValue & 0x0F) {
        case 6:  graceNoteType = NoteType::GRACE32_AFTER;
            break;
        case 5:  graceNoteType = NoteType::GRACE16_AFTER;
            break;
        default: graceNoteType = beamedGroup ? NoteType::GRACE16_AFTER : NoteType::GRACE8_AFTER;
            break;
        }
    } else if (beamedGroup) {
        switch (en->faceValue & 0x0F) {
        case 3:  graceNoteType = NoteType::GRACE4;
            break;
        case 6:  graceNoteType = NoteType::GRACE32;
            break;
        default: graceNoteType = NoteType::GRACE16;
            break;
        }
    } else {
        graceNoteType = NoteType::ACCIACCATURA;
    }

    // A lone slashed acciaccatura (and its after-analogue GRACE8_AFTER) is drawn as an eighth
    // regardless of the stored value; the small stored value (e.g. a 128th) would otherwise render as
    // a many-flagged glyph. Beamed groups and appoggiaturas keep their written figure.
    const bool eighthGlyph = (graceNoteType == NoteType::ACCIACCATURA
                              || graceNoteType == NoteType::GRACE8_AFTER);
    DurationType graceDt = eighthGlyph ? DurationType::V_EIGHTH
                           : realDuration2DurationType(en->realDuration, en->faceValue);
    Chord* gc = Factory::createChord(ctx.score->dummy()->segment());
    gc->setTrack(ec.track);
    TDuration gdur(graceDt);
    gc->setDurationType(gdur);
    gc->setTicks(gdur.fraction());
    gc->setDots(0);
    gc->setNoteType(graceNoteType);

    Note* gnote = Factory::createNote(gc);
    applyConcertPitch(gnote, en->semiTonePitch + ctx.staffPitchOffset[ec.staffIdx]);
    if (en->isMuted()) {
        gnote->setPlay(false);   // Encore per-note mute flag
    }
    gc->add(gnote);

    ctx.scratch.lastGraceChord[trackKey] = gc;
    ctx.scratch.lastGraceTick[trackKey] = static_cast<int>(en->tick);

    // Articulations are applied after attachment (a detached grace chord only sees the dummy segment,
    // where fermatas cannot anchor).

    // Grace-after: attach immediately to the preceding principal chord (stays in this bar).
    if (afterMode) {
        gc->setGraceIndex(precedingChord->graceNotes().size());
        precedingChord->add(gc);
        applyNoteArticulations(ctx, gnote, gc, en, ec.track, mc);
        return true;
    }

    // Retroactive attachment: main note already placed at elemTick (isChordExt=TRUE).
    if (ec.isChordExt) {
        Segment* existingSeg = mc.measure->getSegment(SegmentType::ChordRest, ec.elemTick);
        if (existingSeg) {
            EngravingItem* existingEl = existingSeg->element(ec.track);
            if (existingEl && existingEl->isChord()) {
                gc->setGraceIndex(0);
                toChord(existingEl)->add(gc);
                applyNoteArticulations(ctx, gnote, gc, en, ec.track, mc);
                ctx.scratch.graceStolenTicks[trackKey] += faceValue2ticks(en->faceValue & 0x0F);
                return true;
            }
        }
    }

    // Grace-before: queue for the next principal chord.
    ctx.scratch.pendingGraces[trackKey].push_back({ gc, en, mc.measure });
    ctx.scratch.graceStolenTicks[trackKey] += faceValue2ticks(en->faceValue & 0x0F);
    return true;
}
} // namespace mu::iex::enc
