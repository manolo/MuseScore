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

#include "builders.h"
#include "ctx.h"
#include "import.h"
#include "../parser/elements.h"
#include "mapping.h"
#include "../parser/ticks.h"
#include "tuplets.h"
#include <algorithm>
#include <memory>
#include <map>
#include <set>
#include <vector>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include "engraving/dom/arpeggio.h"
#include "engraving/dom/box.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/fermata.h"
#include "engraving/dom/fingering.h"
#include "engraving/dom/ornament.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/jump.h"
#include "engraving/dom/key.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/marker.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/part.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftext.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/text.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/volta.h"
#include "engraving/engravingerrors.h"
#include "log.h"

using namespace mu::engraving;

namespace mu::iex::encore {
static const InstrumentTemplate* applyBestInstrument(Part* part,
                                                     const EncInstrument& instr,
                                                     bool isPercByClef,
                                                     bool isRhythm)
{
    const int encMidi = instr.midiProgram > 0 ? instr.midiProgram - 1 : -1;
    const int encKey  = static_cast<int>(instr.keyTransposeSemitones);
    const bool nameTooShort = instr.name.trimmed().size() < 4;

    const InstrumentTemplate* tmpl = nullptr;
    int matchStep = 0;

    // Step 1: PERC clef → drumset
    if (isPercByClef) {
        tmpl = searchTemplate(String(u"drumset"));
        if (tmpl) {
            matchStep = 1;
        }
    }

    // Step 1b: GM Percussive range (113–128 1-indexed). Encore files often use performer
    // credits instead of instrument names, so match percussion from MIDI program alone.
    static constexpr int GM_PERC_FIRST = 113;
    if (!tmpl && instr.midiProgram >= GM_PERC_FIRST) {
        tmpl = searchTemplate(String(u"drumset"));
        if (tmpl) {
            matchStep = 1;
        }
    }

    // Step 2: name+MIDI score with transposition filter — rejects non-octave mismatches;
    // C/octave templates always qualify (octave handling via pickStaffClef).
    if (!tmpl) {
        tmpl = findEncoreInstrumentTemplate(instr.name, encMidi, encKey);
        if (tmpl) {
            matchStep = 2;
        } else if (!instr.name.trimmed().isEmpty()) {
            const InstrumentTemplate* rejected = findEncoreInstrumentTemplate(instr.name, encMidi);
            if (rejected) {
                LOGD() << "  instrument \"" << instr.name.toStdString()
                       << "\": name match \"" << rejected->trackName.toStdString()
                       << "\" rejected (template chromatic=" << rejected->transpose.chromatic
                       << " vs encKey=" << encKey << "), trying MIDI";
            }
        }
    }

    // Step 3: name scoring over drumset templates (handles localized names)
    if (!tmpl && !nameTooShort) {
        tmpl = findDrumsetTemplate(instr.name);
        if (tmpl) {
            matchStep = 3;
        }
    }

    // Step 4: generic percussion keywords ("Percusión", "Drums", "Batería"…)
    if (!tmpl && !nameTooShort) {
        const QString lname = instr.name.toLower();
        if (lname.contains(QStringLiteral("perc"))
            || lname.contains(QStringLiteral("drum"))
            || lname.contains(QStringLiteral("bater"))) {
            tmpl = searchTemplate(String(u"drumset"));
            if (tmpl) {
                matchStep = 4;
            }
        }
    }

    // Step 4a: RHYTHM staff → snare-drum; skip MIDI to avoid program-0 piano override.
    if (!tmpl && isRhythm) {
        tmpl = searchTemplate(String(u"snare-drum"));
        if (tmpl) {
            matchStep = 6;
        }
    }

    // Step 5: MIDI program lookup (skipped for RHYTHM staves).
    if (!tmpl && !isRhythm && instr.midiProgram > 0) {
        const InstrumentTemplate* midiTmpl = findTemplateByMidi(instr.midiProgram - 1);
        if (midiTmpl) {
            const int tmplChr = midiTmpl->transpose.chromatic;
            const bool transpMismatch = (tmplChr % 12 != 0)
                                        && (encKey % 12 != 0)
                                        && ((((encKey % 12) + 12) % 12) != (((tmplChr % 12) + 12) % 12));
            if (transpMismatch) {
                LOGD() << "  instrument \"" << instr.name.toStdString()
                       << "\": MIDI " << instr.midiProgram << " match \""
                       << midiTmpl->trackName.toStdString()
                       << "\" transposition differs (template chromatic=" << tmplChr
                       << " vs encKey=" << encKey << ")";
            }
            tmpl = midiTmpl;
            matchStep = 5;
        }
    }

    static const char* stepDesc[] = {
        "", "PERC clef", "name+MIDI score", "drumset name", "perc keyword", "MIDI program", "RHYTHM staff"
    };
    if (tmpl) {
        LOGD() << "  instrument \"" << instr.name.toStdString()
               << "\": step" << matchStep << "(" << stepDesc[matchStep]
               << (matchStep == 5 ? QString(" %1").arg(instr.midiProgram).toStdString() : std::string())
               << ")" << " -> " << tmpl->trackName.toStdString();
        Instrument instrument = Instrument::fromTemplate(tmpl);
        if (!instr.name.isEmpty()) {
            instrument.setLongName(String(instr.name));
        }
        instrument.setShortName(String());
        instrument.instrumentLabel().setAllowGroupName(false);
        part->setInstrument(instrument);
    } else {
        // Grand Piano fallback.
        const InstrumentTemplate* pianoTmpl = searchTemplateForMidiProgram(0, 0, false);
        if (pianoTmpl) {
            LOGD() << "  instrument \"" << instr.name.toStdString()
                   << "\": no match -> fallback: Grand Piano";
            Instrument instrument = Instrument::fromTemplate(pianoTmpl);
            if (!instr.name.isEmpty()) {
                instrument.setLongName(String(instr.name));
            }
            instrument.setShortName(String());
            instrument.instrumentLabel().setAllowGroupName(false);
            part->setInstrument(instrument);
        } else {
            LOGD() << "  instrument \"" << instr.name.toStdString()
                   << "\": no match, no piano template -> bare MIDI";
            part->setMidiProgram(0, 0);
            if (!instr.name.isEmpty()) {
                part->setPlainLongName(String(instr.name));
            }
            part->setPlainShortName(String());
        }
    }

    return tmpl;
}

void buildParts(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;
    const EncRoot& enc = ctx.enc;
    int cumStaffIdx = 0;  // running index into enc.lines[0].staffData
    for (const auto& instr : enc.instruments) {
        int ns = instr.nstaves > 0 ? instr.nstaves : 1;
        Part* part = new Part(score);

        const bool isPercByClef = !enc.lines.empty()
                                  && cumStaffIdx < static_cast<int>(enc.lines[0].staffData.size())
                                  && enc.lines[0].staffData[cumStaffIdx].clef == EncClefType::PERC;
        const bool isRhythm = !enc.lines.empty()
                              && cumStaffIdx < static_cast<int>(enc.lines[0].staffData.size())
                              && enc.lines[0].staffData[cumStaffIdx].staffType == EncStaffType::RHYTHM;
        const InstrumentTemplate* tmpl = applyBestInstrument(part, instr, isPercByClef, isRhythm);

        const bool showFromLine = enc.lines.empty()
                                  || cumStaffIdx >= static_cast<int>(enc.lines[0].staffData.size())
                                  || enc.lines[0].staffData[cumStaffIdx].showStaff;
        if (!showFromLine) {
            part->setShow(false);
        }

        const int pitchOffset = static_cast<int>(instr.keyTransposeSemitones);
        // Non-octave transposition: set on instrument so display shows written (Encore-stored) pitch.
        // Octave offsets are handled by pickStaffClef() and the template's own transposition.
        Instrument* instrument = part->instrument();
        if (instrument) {
            if (pitchOffset != 0 && std::abs(pitchOffset) % 12 != 0) {
                const Interval iv(pitchOffset);
                instrument->setTranspose(iv);
                static const char* const keyNames[] = {
                    "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"
                };
                const int keyIdx = ((pitchOffset % 12) + 12) % 12;
                LOGD() << "  instrument \"" << instr.name.toStdString()
                       << "\": transposition in " << keyNames[keyIdx]
                       << " (chromatic=" << iv.chromatic << " diatonic=" << iv.diatonic << ")";
            } else if (pitchOffset == 0) {
                // encKey=0 means "sounds as written" (ENCORE_FORMAT.md). Zero out any
                // transposition the template carries, including octave offsets (±12, ±24):
                // a double-bass template has chromatic=-12, but notes are already stored at
                // written pitch, so keeping it would shift the display up one octave.
                const Interval tmplT = instrument->transpose();
                if (!tmplT.isZero()) {
                    instrument->setTranspose(Interval(0, 0));
                    LOGD() << "  instrument \"" << instr.name.toStdString()
                           << "\": encKey=0 (sounds as written) → zeroing template transposition";
                }
            }
        }
        for (int s = 0; s < ns; ++s) {
            Staff* staff = Factory::createStaff(part);
            score->appendStaff(staff);
            if (tmpl) {
                staff->init(tmpl, nullptr, s);
            }
            ctx.staffPitchOffset.push_back(pitchOffset);
            ClefType cClef = ClefType::INVALID;
            ClefType tClef = ClefType::INVALID;
            if (tmpl) {
                const ClefTypeList ctl = tmpl->clefType(static_cast<staff_idx_t>(s));
                cClef = ctl.concertClef;
                tClef = ctl.transposingClef;
            }
            ctx.staffTemplateConcertClef.push_back(cClef);
            ctx.staffTemplateTransposingClef.push_back(tClef);
            ++ctx.totalStaves;
        }
        score->appendPart(part);
        cumStaffIdx += ns;
    }
    if (ctx.totalStaves == 0) {
        Part* part = new Part(score);
        part->setMidiProgram(0, 0);
        Staff* staff = Factory::createStaff(part);
        score->appendStaff(staff);
        score->appendPart(part);
        ctx.totalStaves = 1;
    }
}
} // namespace mu::iex::encore
