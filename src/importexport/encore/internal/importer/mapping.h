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

#ifndef MU_IMPORTEXPORT_ENC_IMPORT_MAPPING_H
#define MU_IMPORTEXPORT_ENC_IMPORT_MAPPING_H

#include <QString>
#include <vector>

#include "engraving/dom/clef.h"

#include "engraving/types/symid.h"
#include "../parser/elements.h"

namespace mu::engraving {
class MasterScore;
class Measure;
class Score;
class InstrumentTemplate;
}

namespace mu::iex::encore {

mu::engraving::ClefType encClef2MuseScore(EncClefType ct);

// Pick the staff clef. Encore stores plain G/F for octave-transposing instruments;
// the template carries G8_VB/F8_VB. Override applies when Key matches the octave:
//   keyOffset -12: G8_VB/F8_VB; -24: G15_MB/F15_MB; +12: G8_VA; +24: G15_MA.
// When template clefs differ, prefer the transposing clef (no octave glyph).
// When equal or no template match, Encore clef wins.
mu::engraving::ClefType pickStaffClef(EncClefType encClef,
                                      mu::engraving::ClefType concertClef,
                                      mu::engraving::ClefType transposingClef,
                                      int keyOffsetSemitones);

int encKeyToFifths(quint8 key);

void addTitleFrame(mu::engraving::MasterScore* score, const EncTitle& titleBlock);
void addInitialKeySig(mu::engraving::MasterScore* score, int staffIdx, quint8 encKey);
void addInitialTimeSig(mu::engraving::MasterScore* score, int nstaves, mu::engraving::Fraction ts);
void addInitialClef(mu::engraving::MasterScore* score, int staffIdx, EncClefType ct);
void addInitialClef(mu::engraving::MasterScore* score, int staffIdx, mu::engraving::ClefType clef);
void addRepeatMark(mu::engraving::Score* score, mu::engraving::Measure* measure, EncRepeatType rt);

QString normalizeEncoreInstrName(const QString& name);
// Find instrument template by name+MIDI score. midiProgram breaks ties (0-indexed; -1=none).
const mu::engraving::InstrumentTemplate* findEncoreInstrumentTemplate(
    const QString& encName, int encMidiProgram = -1);

// Same as findEncoreInstrumentTemplate but restricted to useDrumset templates.
const mu::engraving::InstrumentTemplate* findDrumsetTemplate(const QString& encName);

// Return BPS if text is a standard Italian tempo term (Allegro, Andante, ...).
// Return 0 for relative marks (a tempo, Tempo I). Return -1 if not a tempo mark.
double encTextToTempoBps(const QString& text);

// Map articulation byte to MuseScore SymIds. One byte can encode multiple glyphs
// (e.g. 0x24=tenuto+staccato). Empty vector means no known articulation mapping.
std::vector<mu::engraving::SymId> encArticulation2SymIds(quint8 articByte);

// Map articulation byte to fingering number 1..5, or 0 if not a fingering.
int encArticByteToFingerNumber(quint8 articByte);

// True when the articulation byte is an open-string marker (importer emits Fingering "0").
bool encArticByteIsOpenString(quint8 articByte);

} // namespace mu::iex::encore

#endif // MU_IMPORTEXPORT_ENC_IMPORT_MAPPING_H
