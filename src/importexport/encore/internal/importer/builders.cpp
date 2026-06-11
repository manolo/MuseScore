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

    // Step 2: name + MIDI scoring with transposition filter — rejects templates whose non-octave transposition
    // conflicts with encKey; C/octave templates always qualify (octave handling via pickStaffClef).
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

    // Step 4a: RHYTHM staff — fall back to snare-drum (neutral 1-line perc); skip MIDI so program 0 (piano) doesn't override.
    if (!tmpl && isRhythm) {
        tmpl = searchTemplate(String(u"snare-drum"));
        if (tmpl) {
            matchStep = 6;
        }
    }

    // Step 5: MIDI program lookup (skip for RHYTHM staves — program 0 would select Grand Piano).
    // Accept any MIDI match as better than Grand Piano fallback; log when transposition differs.
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
    // staffPitchOffset: Encore stores written pitch; add Key field (chromatic semitones) to each note pitch for correct playback.
    // staffTemplateConcertClef/TransposingClef: template clefs for octave instruments (e.g. bass guitar). INVALID if no template.
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
        // Non-octave transposition: set on the instrument so display shows written pitch (Encore's stored pitch).
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
                // encKey=0: Encore's Key field is not set, meaning "sounds as written"
                // (ENCORE_FORMAT.md: 0 = sounds as written). Zero out any non-octave
                // transposition that the selected template may carry (e.g. Bb clarinet
                // selected via MIDI fallback has transposeChromatic=-2). Without this,
                // written notes would be shifted by the template's interval, displaying
                // the wrong pitch (Bb4 → C5 for Bb clarinet template).
                const Interval tmplT = instrument->transpose();
                if (!tmplT.isZero() && tmplT.chromatic % 12 != 0) {
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

static bool encMeasHasPitchedNotes(const EncMeasure& m)
{
    for (const auto& elem : m.elements) {
        if (static_cast<EncElemType>(elem->type) == EncElemType::NOTE) {
            return true;
        }
    }
    return false;
}

static bool encMeasHasSingleRest(const EncMeasure& m)
{
    return m.elements.size() == 1
           && static_cast<EncElemType>(m.elements[0]->type) == EncElemType::REST;
}

// Returns the number of MuseScore measures to create for a single EncMeasure.
// Normally 1:1, but Encore stores "N consecutive empty display measures" as a single MEAS
// block whose lone REST element has mrestCount == N (byte +15 of the REST element data).
// Expansion only applies to an ISOLATED single-block rest:
//   - exactly one REST element with mrestCount > 1
//   - predecessor is NOT also a single-REST block (otherwise we are in a consecutive run)
//   - successor HAS pitched notes (ensures the next block is the real content)
static int encMeasDisplayCount(const EncMeasure& m, const EncMeasure* prev, const EncMeasure* next)
{
    if (m.elements.size() != 1) {
        return 1;
    }
    const EncMeasureElem* e = m.elements[0].get();
    if (static_cast<EncElemType>(e->type) != EncElemType::REST) {
        return 1;
    }
    const int cnt = static_cast<int>(static_cast<const EncRest*>(e)->mrestCount);
    if (cnt <= 1) {
        return 1;
    }
    if (prev && encMeasHasSingleRest(*prev)) {
        return 1;
    }
    if (!next || !encMeasHasPitchedNotes(*next)) {
        return 1;
    }
    return cnt;
}

void buildMeasures(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;
    const EncFile& enc = ctx.enc;

    // Pickup measure detection: measure 0 holds the short duration, measure 1 the real sig.
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
                ctx.nominalTimeSig = ts1;
            }
        }
    }

    int currentTick = 0;
    bool firstMeasure = true;
    size_t msIdxCounter = 0;
    ctx.encToMsIdx.reserve(enc.measures.size());
    for (size_t mi = 0; mi < enc.measures.size(); ++mi) {
        const EncMeasure& encMeas = enc.measures[mi];
        int num = encMeas.timeSigNum > 0 ? encMeas.timeSigNum : 4;
        int den = encMeas.timeSigDen > 0 ? encMeas.timeSigDen : 4;
        Fraction ts(num, den);

        const EncMeasure* prev = (mi > 0) ? &enc.measures[mi - 1] : nullptr;
        const EncMeasure* next = (mi + 1 < enc.measures.size()) ? &enc.measures[mi + 1] : nullptr;
        const int displayCount = encMeasDisplayCount(encMeas, prev, next);

        ctx.encToMsIdx.push_back(msIdxCounter);

        for (int di = 0; di < displayCount; ++di) {
            Measure* measure = Factory::createMeasure(score->dummy()->system());
            measure->setTick(Fraction::fromTicks(currentTick));

            if (firstMeasure && ts != ctx.nominalTimeSig && di == 0) {
                measure->setTimesig(ctx.nominalTimeSig);
                measure->setTicks(ts);
            } else {
                measure->setTimesig(ts);
                measure->setTicks(ts);
            }

            if (di == 0) {
                if (encMeas.startBarline() == EncBarlineType::REPEATSTART) {
                    measure->setRepeatStart(true);
                }
                if (encMeas.endBarline() == EncBarlineType::REPEATEND) {
                    measure->setRepeatEnd(true);
                } else if (encMeas.endBarline() == EncBarlineType::FINAL
                           || encMeas.endBarline() == EncBarlineType::DOUBLEL
                           || encMeas.endBarline() == EncBarlineType::DOUBLER
                           || encMeas.endBarline() == EncBarlineType::DOTTED) {
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
            }

            score->measures()->append(measure);
            currentTick += ts.ticks();
        }
        firstMeasure = false;
        msIdxCounter += static_cast<size_t>(displayCount);
    }
}

void buildInitialSignatures(BuildCtx& ctx)
{
    MasterScore* score = ctx.score;
    const EncFile& enc = ctx.enc;
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

    // Emit TimeSig elements at change points (buildMeasures sets per-measure properties only).
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
