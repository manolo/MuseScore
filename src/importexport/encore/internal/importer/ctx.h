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
#include "../parser/reader.h"
#include "tuplets.h"
#include "engraving/types/fraction.h"
#include "engraving/types/symid.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/arpeggio.h"
#include "engraving/dom/ornament.h"
#include "engraving/dom/marker.h"

using namespace mu::engraving;

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

// Arpeggio intents deferred: ORN comes before chord notes in MEAS order,
// so the chord does not exist yet at parse time.
struct PendingArpeggio {
    Fraction tick;
    track_idx_t track;
};

// Single-chord tremolo intents (tipo 0xAF/0xEF), deferred like ARPEGGIO.
// Post-pass falls back to latest chord before the stored tick.
struct PendingOrnTremolo {
    Fraction tick;
    Fraction measTick;
    int staffIdx;
    int msVoice;
    TremoloType tremType;
};

// Trill intents (tipo 0x35/0x36/0x37), deferred for the same reason as ARPEGGIO.
struct PendingTrill {
    Fraction tick;
    track_idx_t track;
    int alMezuro { 0 };           // measures forward to the trill end (0 = same measure)
    size_t measIdx  { 0 };        // index into ctx.measuresByIdx for the start measure
    int xoffset2 { 0 };           // end x-position hint (for same-measure endpoint detection)
    bool isAlt    { false };        // TRILL_ALT (0x37): secondary mark, always Ornament glyph
    bool isSimple { false };        // TRILL_SIMPLE/TRILL_TR: standalone glyph only, never a spanner
    mu::engraving::SymId simpleSymId { mu::engraving::SymId::ornamentTrill }; // symId when isSimple=true
};

// Staccato intents (tipo 0xC9), deferred for the same reason as ARPEGGIO.
struct PendingStaccato {
    Fraction tick;
    track_idx_t track;
};

// Fermata intents (tipo 0xCC/0xCD), deferred for the same reason as ARPEGGIO.
struct PendingFermata {
    Fraction tick;
    track_idx_t track;
    mu::engraving::SymId symId;
};

// Breath / caesura intents (tipo 0xA7/0xA8), attached to the chord segment.
struct PendingBreath {
    Fraction tick;
    track_idx_t track;
    mu::engraving::SymId symId;
};

// Measure repeat intents (tipo 0xA3): replace measure content with a "%" symbol.
struct PendingMeasureRepeat {
    Fraction measTick;
    int staffIdx;
};

// Bowing/stroke intents (tipo 0xC4, 0xC5), deferred like ARPEGGIO.
// v0xC4: 0xC4=stringsUpBow, 0xC5=stringsDownBow; v0xC2: 0xC4=articAccentAbove.
struct PendingBowing {
    Fraction tick;
    track_idx_t track;
    mu::engraving::SymId symId;
    int measIdx = -1;       // source measure index
    bool crossMeasure = false;  // no voice=0 note at same raw Encore tick; belongs to next measure
};

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

// Segno/Coda markers (tipo 0xA2/0xA6), attached to the measure, not a chord.
struct PendingMarker {
    Fraction tick;
    MarkerType type;
};

// Lyric syllables queued for attachment. Tick stored so we can find the
// closest chord at measure end (queue index shifts with ORN elements).
struct PendingLyric {
    int encTick;        // raw Encore tick within the current measure
    String text;        // the syllable text (separators are pre-filtered)
    bool hyphenBefore;  // a "-" LYRIC element preceded this syllable
    bool hyphenAfter;   // a "-" LYRIC element follows this syllable
};

struct BuildCtx
{
    mu::engraving::MasterScore* score;
    const EncFile& enc;
    const EncFormatReader* fmt { nullptr };    // set in buildScore(), non-owning

    // Format capability flags, set from fmt in buildScore() so that importer
    // phases do not need to query the reader directly.
    bool impliedTuplets      { false };  // v0xC2: tuplet membership by rdur/faceValue mismatch
    bool g1LowTieSender      { false };  // v0xC2: grace1 low nibble encodes tie-sender indicator
    bool alMezuroIsReliable  { true };   // v0xC2=false: alMezuro byte has no valid measure-count semantics

    // Populated by buildParts():
    int totalStaves = 0;
    std::vector<int> staffPitchOffset {};
    std::vector<ClefType> staffTemplateConcertClef {};
    std::vector<ClefType> staffTemplateTransposingClef {};

    // Populated by buildMeasures(): nominal time sig of the score (differs from
    // measures[0] when the first measure is a pickup / anacrusis).
    Fraction nominalTimeSig { 4, 4 };
    // Non-zero when measure 0 is a pickup: leading gap to insert before notes.
    Fraction measure0PickupOffset { 0, 1 };

    // Populated by buildMeasures(): encToMsIdx[i] = MuseScore measure index of the
    // first measure produced from enc.measures[i].  Accounts for single-block
    // multi-measure rest expansion (mrestCount > 1).
    std::vector<size_t> encToMsIdx {};

    // Populated by buildNoteLoop():
    std::vector<Measure*> measuresByIdx {};
    std::vector<PendingHairpin> pendingHairpins {};
    std::vector<PendingSlur> pendingSlurs {};
    std::vector<PendingArpeggio> pendingArpeggios {};
    std::vector<PendingOrnTremolo> pendingOrnTremolos {};
    std::vector<PendingTrill> pendingTrills {};
    // TRILL_END (0x35) ticks by track; consumed by resolveOrnaments() to compute span endpoints.
    std::map<track_idx_t, std::vector<mu::engraving::Fraction> > pendingTrillEnds {};
    std::vector<PendingStaccato> pendingStaccatos {};
    std::vector<PendingFermata> pendingFermatas {};
    std::vector<PendingBreath> pendingBreaths {};
    std::vector<PendingMeasureRepeat> pendingMeasureRepeats {};
    std::vector<PendingBowing> pendingBowings {};
    std::vector<PendingOrnFingering> pendingOrnFingerings {};
    std::vector<PendingMarker> pendingMarkers {};
    std::map<track_idx_t, std::vector<PendingLyric> > pendingLyrics {};

    // Per-measure note loop state (keyed by (staffIdx, msVoice) unless noted).
    // All start empty; BuildCtx is constructed fresh for every import.

    // Volta being coalesced: equal-bitmask runs collapse into one Volta.
    Volta* activeVolta { nullptr };
    quint8 activeVoltaBits { 0 };

    std::map<std::pair<int, int>, TupletTracker> tuplets {};

    // Pending tie-start notes: key=(staffIdx, voice, pitch), value=Note* to tie FROM.
    // Persists across measures for ties across barlines.
    std::map<std::tuple<int, int, int>, Note*> pendingTieNote {};

    // Accumulated written position per (staffIdx, msVoice).
    std::map<std::pair<int, int>, Fraction> cumTick {};
    // Last MIDI tick placed (chord detection — same MIDI tick = same chord).
    std::map<std::pair<int, int>, int> prevMidiTick {};
    // Encore voice of last note placed. Prevents chord-extension misdetection.
    std::map<std::pair<int, int>, int> prevEncVoice {};
    // MuseScore tick of last chord root.
    std::map<std::pair<int, int>, Fraction> lastChordPos {};

    // Grace chords held detached; attached to the next normal chord.
    std::map<std::pair<int, int>, std::vector<Chord*> > pendingGraces {};

    // Ticks borrowed by grace notes from the following note; used to suppress
    // spurious gap-snap rests after a grace group. Cleared each measure.
    std::map<std::pair<int, int>, int> graceStolenTicks {};

    // Inner (nested) TupletTrackers: active only when a note is inside a nested group.
    // Keyed by the same trackKey as ctx.tuplets.  Cleared each measure alongside tuplets.
    std::map<std::pair<int, int>, TupletTracker> innerTuplets {};

    // True when the next syllable follows a hyphen; reset at measure boundary.
    std::map<track_idx_t, bool> nextLyricHyphenBefore {};
};
} // namespace mu::iex::encore

#endif // MU_IMPORTEXPORT_ENC_IMPORT_CTX_H
