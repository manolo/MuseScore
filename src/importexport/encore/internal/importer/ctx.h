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

#ifndef MU_IMPORTEXPORT_ENC_IMPORT_CTX_H
#define MU_IMPORTEXPORT_ENC_IMPORT_CTX_H

#include <map>
#include <set>
#include <memory>
#include <vector>

#include <QtGlobal>

#include "../parser/elements.h"
#include "engraving/types/fraction.h"
#include "engraving/types/symid.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/arpeggio.h"
#include "engraving/dom/ornament.h"
#include "engraving/dom/marker.h"

namespace mu::engraving { class MasterScore; }

namespace mu::iex::encore {

// Pending intents collected during note-import, resolved in resolveAll().
struct PendingSlur {
    Fraction startTick;
    track_idx_t track;
    int startMeasIdx;
    int endMeasIdx;
    int alMezuro;
    int slurXoffset;
    int slurXoffset2;
    int staffIdx;
    int encVoice;
};

// Hairpin intents resolved after the measure pass so each hairpin ends
// at the next Dynamic on the same track, not at the barline.
// (Encore chains like `mf<f>mf` terminate at the next dynamic glyph.)
struct PendingHairpin {
    Fraction startTick;
    Fraction maxEndTick;     // end of alMezuro target measure (upper bound)
    track_idx_t track;
    HairpinType type;
    int endMeasIdx;          // alMezuro target measure index
    int hairpinXoffset2;     // xoffset2 in target measure layout
    int staffIdx;
    int encVoice;
};
std::vector<PendingHairpin> pendingHairpins;
std::vector<PendingSlur> pendingSlurs;

// Arpeggio intents deferred: ORN comes before chord notes in MEAS order,
// so the chord does not exist yet at parse time.
struct PendingArpeggio {
    Fraction tick;
    track_idx_t track;
};
std::vector<PendingArpeggio> pendingArpeggios;

// Single-chord tremolo intents (tipo 0xAF/0xEF), deferred like ARPEGGIO.
// Post-pass falls back to latest chord before the stored tick.
struct PendingOrnTremolo {
    Fraction tick;
    Fraction measTick;
    int staffIdx;
    int msVoice;
    TremoloType tremType;
};
std::vector<PendingOrnTremolo> pendingOrnTremolos;

// Trill intents (tipo 0x35/0x36/0x37), deferred for the same reason as ARPEGGIO.
struct PendingTrill {
    Fraction tick;
    track_idx_t track;
};
std::vector<PendingTrill> pendingTrills;

// Staccato intents (tipo 0xC9), deferred for the same reason as ARPEGGIO.
struct PendingStaccato {
    Fraction tick;
    track_idx_t track;
};
std::vector<PendingStaccato> pendingStaccatos;

// Bowing/stroke intents (tipo 0xC4=upbow, 0xC5=downbow), deferred like ARPEGGIO.
struct PendingBowing {
    Fraction tick;
    track_idx_t track;
    mu::engraving::SymId symId;
    int measIdx = -1;       // source measure index
    bool crossMeasure = false;  // no voice=0 note at same raw Encore tick; belongs to next measure
};
std::vector<PendingBowing> pendingBowings;

// Fingering number intents from stand-alone ORN elements (tipo 0xB9..0xBD),
// deferred because the chord segment is not built yet at ORN parse time.
struct PendingOrnFingering {
    Fraction tick;
    track_idx_t track;
    int fingerNum;
    int measIdx = -1;
    bool crossMeasure = false;   // ORN at last v0 tick, no v4 note there: belongs to next measure
    bool preferSibling = false;  // more ORNs than v0 notes at tick: belongs to 2nd-staff chord
};
std::vector<PendingOrnFingering> pendingOrnFingerings;

// Segno/Coda markers (tipo 0xA2/0xA6), attached to the measure, not a chord.
struct PendingMarker {
    Fraction tick;
    MarkerType type;
};
std::vector<PendingMarker> pendingMarkers;

// Volta being coalesced: Encore tags every measure in the ending with the
// same bitmask; equal-bitmask runs collapse into one Volta.
Volta* activeVolta = nullptr;
quint8 activeVoltaBits = 0;

// Tuplet state per (staffIdx, msVoice).
std::map<std::pair<int, int>, TupletTracker> tuplets;

// Pending tie-start notes: key=(staffIdx, voice, pitch), value=Note* to tie FROM.
// Persists across measures for ties across barlines.
std::map<std::tuple<int, int, int>, Note*> pendingTieNote;

// Lyric syllables queued for attachment. Tick stored so we can find the
// closest chord at measure end (queue index shifts with ORN elements).
struct PendingLyric {
    int encTick;        // raw Encore tick within the current measure
    String text;        // the syllable text (separators are pre-filtered)
    bool hyphenBefore;  // a "-" LYRIC element preceded this syllable
    bool hyphenAfter;   // a "-" LYRIC element follows this syllable
};
std::map<track_idx_t, std::vector<PendingLyric> > pendingLyrics;
// True when the next syllable follows a hyphen. Reset at measure boundary.
std::map<track_idx_t, bool> nextLyricHyphenBefore;

// All maps keyed by (staffIdx, msVoice). Notes placed at cumTick, not MIDI tick.
// MIDI ticks used only for chord grouping (same MIDI tick = same chord).
std::map<std::pair<int, int>, Fraction> cumTick;      // accumulated written position
std::map<std::pair<int, int>, int> prevMidiTick;       // last MIDI tick placed (chord detection)
// Encore voice of last note placed. Prevents misdetecting a chord extension
// when two Encore voices share the same MuseScore voice via streamOffset.
std::map<std::pair<int, int>, int> prevEncVoice;
std::map<std::pair<int, int>, Fraction> lastChordPos;  // MuseScore tick of last chord root

// Grace chords held detached; attached via Chord::add() to the next normal chord.
// NOT added to a Segment: that crashes beam layout (Chord::pagePos asserts non-Segment parent).
// Cleared per measure.
std::map<std::pair<int, int>, std::vector<Chord*> > pendingGraces;

// Stream-overflow voice assignment: when cumTick fills and a new note arrives,
// it belongs to another recording stream; assign it to the next MuseScore voice.
// Reset each measure; overflow from one measure must not affect the next.
std::map<std::pair<int, int>, int> streamOffset;  // key=(staffIdx,encVoice), val=extra offset

// Pitches placed in voice 0 for the current measure, keyed by staffIdx.
// Used to suppress stream-duplicate notes overflowing to voice 1+: any overflow
// note whose pitch already exists in voice 0 is a recording-stream artifact.
// Cleared per measure.
std::map<int, std::set<int>> v0PitchesInMeasure;

// v0xA6 inner-grace tracking: leading grace fv stored here so inner graces
// (g1=0x10) with higher fv are also classified as graces. Cleared on flush.
std::map<std::pair<int, int>, quint8> v0xA6LeadingGraceFv;

// Face ticks of the last flushed grace group. Used to suppress spurious
// gap rests when the next note's tick is shifted by exactly that amount.
std::map<std::pair<int, int>, int> v0xA6GraceStolenTicks;


// Shared state for all build phases: setup, measures, notes, resolvers.
struct BuildCtx
{
    mu::engraving::MasterScore* score;
    const EncFile&           enc;

    // Populated by buildParts():
    int                      totalStaves = 0;
    std::vector<int>         staffPitchOffset;
    std::vector<ClefType>    staffTemplateConcertClef;
    std::vector<ClefType>    staffTemplateTransposingClef;

    // Populated by buildMeasures(): nominal time sig of the score (differs from
    // measures[0] when the first measure is a pickup / anacrusis).
    Fraction                 nominalTimeSig { 4, 4 };

    // Populated by buildNoteLoop():
    std::vector<Measure*>    measuresByIdx;
    std::vector<PendingHairpin>    pendingHairpins;
    std::vector<PendingSlur>       pendingSlurs;
    std::vector<PendingArpeggio>   pendingArpeggios;
    std::vector<PendingOrnTremolo> pendingOrnTremolos;
    std::vector<PendingTrill>         pendingTrills;
    std::vector<PendingStaccato>      pendingStaccatos;
    std::vector<PendingBowing>        pendingBowings;
    std::vector<PendingOrnFingering>  pendingOrnFingerings;
    std::vector<PendingMarker>        pendingMarkers;
    std::map<track_idx_t, std::vector<PendingLyric> > pendingLyrics;
};


} // namespace mu::iex::encore

#endif // MU_IMPORTEXPORT_ENC_IMPORT_CTX_H
