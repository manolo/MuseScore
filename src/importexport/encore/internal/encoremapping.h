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

#ifndef MU_IMPORTEXPORT_ENCOREMAPPING_H
#define MU_IMPORTEXPORT_ENCOREMAPPING_H

#include <QString>
#include <vector>

#include "engraving/dom/clef.h"

#include "engraving/types/symid.h"
#include "encoreelements.h"

namespace mu::engraving {
class MasterScore;
class Measure;
class Score;
class InstrumentTemplate;
}

namespace mu::iex::encore {

mu::engraving::ClefType encClef2MuseScore(EncClefType ct);

// Pick the staff clef. Encore stores a plain G / F clef for many octave-
// transposing instruments (laud, classical guitar, electric bass, etc.)
// while the MuseScore instrument template carries the octave-bassa
// variant (G8_VB / F8_VB) on its concert clef. The override only kicks
// in when the staff's "Key" transposition (signed semitones from the
// Encore Staff Sheet) matches the concert clef's octave decoration:
//   * G8_VB / F8_VB  require keyOffsetSemitones == -12
//   * G8_VA / F_8VA  require keyOffsetSemitones == +12
//   * G15_MB / F15_MB require keyOffsetSemitones == -24
//   * G15_MA / F_15MA require keyOffsetSemitones == +24
// When the override applies, prefer the template's transposing clef over
// its concert clef WHEN the two are distinct and the transposing clef
// has no octave decoration. In that case the instrument's
// transposeChromatic moves the noteheads to the same staff position as
// the octave-decorated concert clef would, but the clef GLYPH stays
// identical to what Encore stored (bass guitar, double bass, etc.).
// When the two template clefs are equal (laud, classical guitar with a
// single G8vb), the concert clef wins.
// In every other case (no template, exact match between Encore and
// template clef, mismatched key offset, or different glyph family) the
// Encore clef wins.
mu::engraving::ClefType pickStaffClef(EncClefType encClef,
                                      mu::engraving::ClefType concertClef,
                                      mu::engraving::ClefType transposingClef,
                                      int keyOffsetSemitones);

int encKeyToFifths(quint8 key);

void addTitleFrame(mu::engraving::MasterScore* score, const EncTitle& titleBlock);
void addInitialKeySig(mu::engraving::MasterScore* score, int staffIdx, quint8 encKey);
void addInitialTimeSig(mu::engraving::MasterScore* score, int nstaves, const EncMeasure& firstMeas);
void addInitialClef(mu::engraving::MasterScore* score, int staffIdx, EncClefType ct);
void addInitialClef(mu::engraving::MasterScore* score, int staffIdx, mu::engraving::ClefType clef);
void addRepeatMark(mu::engraving::Score* score, mu::engraving::Measure* measure, EncRepeatType rt);

QString normalizeEncoreInstrName(const QString& name);
// When two templates match the Encore name equally well (Spanish "Bajo"
// matches both the choral Bass voice and the Acoustic Bass via shortName),
// the .enc midiProgram (0-indexed; -1 = not provided) breaks the tie.
const mu::engraving::InstrumentTemplate* findEncoreInstrumentTemplate(
    const QString& encName, int encMidiProgram = -1);

// Same scoring but restricted to useDrumset templates, so localized names
// ("Batería", "Batterie", "Drumset") drive the match without hardcoded keywords.
const mu::engraving::InstrumentTemplate* findDrumsetTemplate(const QString& encName);

// Returns beats-per-second if `text` matches a standard Italian tempo
// marking (Allegro, Andante, ...). Returns 0 for relative markings
// (a tempo, Tempo I) that defer to previous context. Returns -1 when
// the text is not a tempo marking.
double encTextToTempoBps(const QString& text);

// Map an Encore articulation byte (the value of EncNote::articulationUp /
// articulationDown) to MuseScore SymIds. Encore frequently packs more than
// one glyph into a single byte (e.g. 0x24 = tenuto + staccato). The returned
// vector lists every articulation the byte represents; an empty vector means
// the byte has no known articulation mapping (e.g. an accidental indicator
// that the importer renders elsewhere).
std::vector<mu::engraving::SymId> encArticulation2SymIds(quint8 articByte);

// Map an Encore articulation byte to a fingering number (1..5), or 0
// when the byte is not a fingering. Encore stores per-note fingering
// glyphs in the same articulation slot as articulations and ornaments;
// the importer creates a `Fingering` element with the number as text.
int encArticByteToFingerNumber(quint8 articByte);

// True when the articulation byte indicates a per-note open-string
// marker. MuseScore lacks a dedicated SymId for it, so the importer
// emits a `Fingering` with `TextStyleType::STRING_NUMBER` and text "0"
// (the MusicXML exporter then writes `<open-string/>`).
bool encArticByteIsOpenString(quint8 articByte);

} // namespace mu::iex::encore

#endif // MU_IMPORTEXPORT_ENCOREMAPPING_H
