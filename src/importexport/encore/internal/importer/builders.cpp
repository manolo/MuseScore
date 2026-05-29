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

namespace mu::iex::encore {

// Apply the best matching MuseScore template to `part`; return it for per-staff clef info.
static const InstrumentTemplate* applyBestInstrument(Part* part,
                                                      const EncInstrument& instr,
                                                      bool isPercByClef)
{
    const int encMidi = instr.midiProgram > 0 ? instr.midiProgram - 1 : -1;
    const bool nameTooShort = instr.name.trimmed().size() < 4;

    const InstrumentTemplate* tmpl = nullptr;

    // Step 1: PERC clef → drumset (language-agnostic binary signal)
    if (isPercByClef) {
        tmpl = searchTemplate(String(u"drumset"));
    }

    // Step 2: name + MIDI scoring across non-drumset templates
    if (!tmpl) {
        tmpl = findEncoreInstrumentTemplate(instr.name, encMidi);
    }

    // Step 3: name scoring over drumset templates (localized names)
    if (!tmpl && !nameTooShort) {
        tmpl = findDrumsetTemplate(instr.name);
    }

    // Step 4: generic percussion keywords (last resort for labels too generic
    // to match a specific template name: "Percusión", "Drums", "Batería"…)
    if (!tmpl && !nameTooShort) {
        const QString lname = instr.name.toLower();
        if (lname.contains(QStringLiteral("perc"))
            || lname.contains(QStringLiteral("drum"))
            || lname.contains(QStringLiteral("bater"))) {
            tmpl = searchTemplate(String(u"drumset"));
        }
    }

    // Step 5: MIDI program lookup
    if (!tmpl && !nameTooShort && instr.midiProgram > 0) {
        tmpl = searchTemplateForMidiProgram(0, instr.midiProgram - 1, false);
    }

    if (tmpl) {
        Instrument instrument = Instrument::fromTemplate(tmpl);
        if (!instr.name.isEmpty()) {
            instrument.setLongName(String(instr.name));
        }
        part->setInstrument(instrument);
    } else {
        // Grand Piano fallback — user can reassign from the instrument browser.
        const InstrumentTemplate* pianoTmpl = searchTemplateForMidiProgram(0, 0, false);
        if (pianoTmpl) {
            Instrument instrument = Instrument::fromTemplate(pianoTmpl);
            if (!instr.name.isEmpty()) {
                instrument.setLongName(String(instr.name));
            }
            part->setInstrument(instrument);
        } else {
            part->setMidiProgram(0, 0);
            if (!instr.name.isEmpty()) {
                part->setPlainLongName(String(instr.name));
            }
        }
    }

    return tmpl;
}

void buildParts(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;
    const EncFile& enc = ctx.enc;
    // staffPitchOffset: Encore stores written pitch; Key field gives chromatic shift.
    // Add it to every note's setPitch() call so playback matches the source.
    // staffTemplateConcertClef/TransposingClef: template clefs for octave instruments
    // (e.g. bass guitar: F8_VB concert / F transposing). INVALID if no template matched.
    int cumStaffIdx = 0;  // running index into enc.lines[0].staffData
    for (const auto& instr : enc.instruments) {
        int ns = instr.nstaves > 0 ? instr.nstaves : 1;
        Part* part = new Part(score);

        const bool isPercByClef = !enc.lines.empty()
            && cumStaffIdx < static_cast<int>(enc.lines[0].staffData.size())
            && enc.lines[0].staffData[cumStaffIdx].clef == EncClefType::PERC;

        const InstrumentTemplate* tmpl = applyBestInstrument(part, instr, isPercByClef);

        // Apply Encore's staff visibility flag (showByte at +19 in EncLineStaffData).
        const bool showFromLine = enc.lines.empty()
            || cumStaffIdx >= static_cast<int>(enc.lines[0].staffData.size())
            || enc.lines[0].staffData[cumStaffIdx].showStaff;
        if (!showFromLine) {
            part->setShow(false);
        }

        const int pitchOffset = static_cast<int>(instr.keyTransposeSemitones);
        for (int s = 0; s < ns; ++s) {
            Staff* staff = Factory::createStaff(part);
            score->appendStaff(staff);
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

void buildMeasures(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;
    const EncFile& enc = ctx.enc;
    // --------------- Measures ---------------
    int currentTick = 0;
    for (const auto& encMeas : enc.measures) {
        int num = encMeas.timeSigNum > 0 ? encMeas.timeSigNum : 4;
        int den = encMeas.timeSigDen > 0 ? encMeas.timeSigDen : 4;
        Fraction ts(num, den);

        Measure* measure = Factory::createMeasure(score->dummy()->system());
        measure->setTick(Fraction::fromTicks(currentTick));
        measure->setTimesig(ts);
        measure->setTicks(ts);

        if (encMeas.startBarline() == EncBarlineType::REPEATSTART) {
            measure->setRepeatStart(true);
        }
        if (encMeas.endBarline() == EncBarlineType::REPEATEND) {
            measure->setRepeatEnd(true);
        } else if (encMeas.endBarline() == EncBarlineType::FINAL
                   || encMeas.endBarline() == EncBarlineType::DOUBLEL
                   || encMeas.endBarline() == EncBarlineType::DOUBLER
                   || encMeas.endBarline() == EncBarlineType::DOTTED) {
            // Encore renders barline graphics across every instrument on the
            // system. MuseScore stores barlines per staff, so set the bar
            // line on every track. Passing only track 0 leaves the bar line
            // visible on the first instrument only (Beethoven Plectro m26
            // double bar reproduced this).
            BarLineType type = BarLineType::DOUBLE;
            if (encMeas.endBarline() == EncBarlineType::FINAL) {
                type = BarLineType::END;
            } else if (encMeas.endBarline() == EncBarlineType::DOTTED) {
                type = BarLineType::DOTTED;
            }
            for (int s = 0; s < ctx.totalStaves; ++s) {
                measure->setEndBarLineType(type, static_cast<track_idx_t>(s) * VOICES);
            }
        }

        score->measures()->append(measure);
        currentTick += ts.ticks();
    }

}

void buildInitialSignatures(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;
    const EncFile& enc = ctx.enc;
    // --------------- Initial key/time/clef signatures ---------------
    if (!enc.measures.empty()) {
        addInitialTimeSig(score, ctx.totalStaves, enc.measures[0]);
    }
    if (!enc.lines.empty()) {
        const auto& firstLine = enc.lines[0];
        for (int si = 0; si < static_cast<int>(firstLine.staffData.size()) && si < ctx.totalStaves; ++si) {
            const auto& sd = firstLine.staffData[si];
            addInitialKeySig(score, si, sd.key);
            const ClefType cClef = si < static_cast<int>(ctx.staffTemplateConcertClef.size())
                                   ? ctx.staffTemplateConcertClef[si] : ClefType::INVALID;
            const ClefType tClef = si < static_cast<int>(ctx.staffTemplateTransposingClef.size())
                                   ? ctx.staffTemplateTransposingClef[si] : ClefType::INVALID;
            const int keyOffset = si < static_cast<int>(ctx.staffPitchOffset.size())
                                  ? ctx.staffPitchOffset[si] : 0;
            addInitialClef(score, si, pickStaffClef(sd.clef, cClef, tClef, keyOffset));
        }
    }

}


} // namespace mu::iex::encore
