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
                                                     bool isPercByClef,
                                                     bool isRhythm)
{
    const int encMidi = instr.midiProgram > 0 ? instr.midiProgram - 1 : -1;
    const int encKey  = static_cast<int>(instr.keyTransposeSemitones);
    const bool nameTooShort = instr.name.trimmed().size() < 4;

    const InstrumentTemplate* tmpl = nullptr;
    int matchStep = 0;

    // Step 1: PERC clef → drumset (language-agnostic binary signal)
    if (isPercByClef) {
        tmpl = searchTemplate(String(u"drumset"));
        if (tmpl) {
            matchStep = 1;
        }
    }

    // Step 2: name + MIDI scoring with transposition compatibility filter.
    // If the best name-scored template has an incompatible non-octave transposition,
    // the function returns nullptr so we fall through to the MIDI step.
    // Templates with C or octave-only transpositions always qualify (octave handling
    // is done via pickStaffClef; C templates get overridden by buildParts if needed).
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

    // Step 3: name scoring over drumset templates (localized names)
    if (!tmpl && !nameTooShort) {
        tmpl = findDrumsetTemplate(instr.name);
        if (tmpl) {
            matchStep = 3;
        }
    }

    // Step 4: generic percussion keywords (last resort for labels too generic
    // to match a specific template name: "Percusión", "Drums", "Batería"…)
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

    // Step 4a: RHYTHM staff type detected (single-line percussion via Encore's EncStaffType).
    // Falls through here when name and MIDI matched nothing; snare-drum is a neutral 1-line
    // perc template that gives the correct staff and drumset scaffold.  MIDI step is skipped
    // so a pitched MIDI program (e.g. piano=0) does not override the perc detection.
    if (!tmpl && isRhythm) {
        tmpl = searchTemplate(String(u"snare-drum"));
        if (tmpl) {
            matchStep = 6;
        }
    }

    // Step 5: MIDI program lookup (always active: when name is missing it is the only signal).
    // Uses findTemplateByMidi rather than searchTemplateForMidiProgram so the "common" genre
    // tiebreaker applies (e.g. Oboe wins over Castilian Dulzaina for program 68).
    // Discard the MIDI result if its non-octave transposition conflicts with encKey.
    // Skip this step for rhythm staves: MIDI program 0 would wrongly select Grand Piano.
    if (!tmpl && !isRhythm && instr.midiProgram > 0) {
        const InstrumentTemplate* midiTmpl = findTemplateByMidi(instr.midiProgram - 1);
        if (midiTmpl) {
            const int tmplChr = midiTmpl->transpose.chromatic;
            // C/octave templates always OK; non-octave must match encKey mod 12.
            const bool ok = (tmplChr % 12 == 0)
                            || (encKey % 12 != 0
                                && (((encKey % 12) + 12) % 12 == ((tmplChr % 12) + 12) % 12));
            if (ok) {
                tmpl = midiTmpl;
                matchStep = 5;
            } else {
                LOGD() << "  instrument \"" << instr.name.toStdString()
                       << "\": MIDI match \"" << midiTmpl->trackName.toStdString()
                       << "\" rejected (template chromatic=" << tmplChr
                       << " vs encKey=" << encKey << "), using fallback";
            }
        }
    }

    static const char* stepDesc[] = {
        "", "PERC clef", "name+MIDI score", "drumset name", "perc keyword", "MIDI program", "RHYTHM staff"
    };
    if (tmpl) {
        LOGD() << "  instrument \"" << instr.name.toStdString()
               << "\": step" << matchStep << "(" << stepDesc[matchStep] << ")"
               << " -> " << tmpl->trackName.toStdString();
        Instrument instrument = Instrument::fromTemplate(tmpl);
        if (!instr.name.isEmpty()) {
            instrument.setLongName(String(instr.name));
        }
        instrument.setShortName(String());
        instrument.instrumentLabel().setAllowGroupName(false);
        part->setInstrument(instrument);
    } else {
        // Grand Piano fallback — user can reassign from the instrument browser.
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
        const bool isRhythm = !enc.lines.empty()
                              && cumStaffIdx < static_cast<int>(enc.lines[0].staffData.size())
                              && enc.lines[0].staffData[cumStaffIdx].staffType == EncStaffType::RHYTHM;
        const InstrumentTemplate* tmpl = applyBestInstrument(part, instr, isPercByClef, isRhythm);

        // Apply Encore's staff visibility flag (showByte at +19 in EncLineStaffData).
        const bool showFromLine = enc.lines.empty()
                                  || cumStaffIdx >= static_cast<int>(enc.lines[0].staffData.size())
                                  || enc.lines[0].staffData[cumStaffIdx].showStaff;
        if (!showFromLine) {
            part->setShow(false);
        }

        const int pitchOffset = static_cast<int>(instr.keyTransposeSemitones);
        // Non-octave key transposition: set on the instrument so MuseScore displays
        // written pitch (= Encore's pitch). Notes are already stored as concert pitch
        // (semiTonePitch + pitchOffset); instrument.transpose() provides the inverse
        // so the display formula (concert - chromatic = written) works correctly.
        // Octave offsets (±12, ±24) are handled via octave-decorated clefs in
        // pickStaffClef() and via the matched template's own transposition.
        if (pitchOffset != 0 && std::abs(pitchOffset) % 12 != 0) {
            Instrument* instrument = part->instrument();
            if (instrument) {
                instrument->setTranspose(Interval(pitchOffset));
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

void buildMeasures(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;
    const EncFile& enc = ctx.enc;
    // --------------- Measures ---------------

    // Determine the nominal (displayed) time signature. When the first measure is
    // shorter than the rest (anacrusis / pickup), Encore stores its actual duration
    // as the time sig. The nominal sig comes from the second measure.
    {
        int n0 = !enc.measures.empty() && enc.measures[0].timeSigNum > 0
                 ? enc.measures[0].timeSigNum : 4;
        int d0 = !enc.measures.empty() && enc.measures[0].timeSigDen > 0
                 ? enc.measures[0].timeSigDen : 4;
        ctx.nominalTimeSig = Fraction(n0, d0);
        if (enc.measures.size() >= 2) {
            int n1 = enc.measures[1].timeSigNum > 0 ? enc.measures[1].timeSigNum : 4;
            int d1 = enc.measures[1].timeSigDen > 0 ? enc.measures[1].timeSigDen : 4;
            Fraction ts1(n1, d1);
            if (ts1 != ctx.nominalTimeSig) {
                ctx.nominalTimeSig = ts1;   // m0 is a pickup; m1 carries the real sig
            }
        }
    }

    int currentTick = 0;
    bool firstMeasure = true;
    for (const auto& encMeas : enc.measures) {
        int num = encMeas.timeSigNum > 0 ? encMeas.timeSigNum : 4;
        int den = encMeas.timeSigDen > 0 ? encMeas.timeSigDen : 4;
        Fraction ts(num, den);

        Measure* measure = Factory::createMeasure(score->dummy()->system());
        measure->setTick(Fraction::fromTicks(currentTick));

        if (firstMeasure && ts != ctx.nominalTimeSig) {
            // Pickup measure: display nominal sig, use actual (shorter) duration.
            // isIrregular() returns true automatically when timesig != ticks.
            measure->setTimesig(ctx.nominalTimeSig);
            measure->setTicks(ts);
        } else {
            measure->setTimesig(ts);
            measure->setTicks(ts);
        }
        firstMeasure = false;

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
        addInitialTimeSig(score, ctx.totalStaves, ctx.nominalTimeSig);
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

    // --------------- Intermediate time signature changes ---------------
    // buildMeasures() sets measure->setTimesig() per measure but never adds
    // TimeSig engraving elements at change points. Walk all measures and emit
    // a TimeSig element whenever the nominal sig changes from the previous one.
    Fraction prevTs = ctx.nominalTimeSig;
    for (const Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        Fraction mTs = m->timesig();
        if (mTs == prevTs) {
            continue;
        }
        // Time sig changed — add a TimeSig element on every staff at this measure.
        Fraction mTick = m->tick();
        for (int si = 0; si < ctx.totalStaves; ++si) {
            Segment* seg = const_cast<Measure*>(m)->getSegment(SegmentType::TimeSig, mTick);
            TimeSig* tsig = Factory::createTimeSig(seg);
            tsig->setTrack(static_cast<track_idx_t>(si) * VOICES);
            tsig->setSig(mTs);
            seg->add(tsig);
        }
        prevTs = mTs;
    }
}
} // namespace mu::iex::encore
