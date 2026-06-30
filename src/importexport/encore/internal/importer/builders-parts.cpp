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

// Build score parts and staves, matching each Encore instrument to the best MuseScore instrument template.

#include "builders.h"
#include "ctx.h"
#include "import.h"
#include "../parser/elem.h"
#include "mappers.h"
#include "../parser/ticks.h"
#include "emitters-tuplets.h"
#include <algorithm>
#include <memory>
#include <map>
#include <set>
#include <vector>
#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include "engraving/dom/clef.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/part.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftype.h"
#include "engraving/engravingerrors.h"
#include "log.h"

using namespace mu::engraving;

namespace mu::iex::enc {
// Build a human-readable instrument name from a template id slug
// ("bass-clarinet" -> "Bass Clarinet"). Used only when a matched template
// carries no track name of its own (see applyInstrumentOrFallback).
static String humanizeTemplateId(const String& id)
{
    QString out;
    bool startWord = true;
    for (const QChar& ch : id.toQString()) {
        if (ch == u'-' || ch == u'_') {
            out.append(u' ');
            startWord = true;
        } else {
            out.append(startWord ? ch.toUpper() : ch);
            startWord = false;
        }
    }
    return String(out);
}

// Which matching rule selected the instrument template, in source-evaluation order.
// Used only for the debug log; an enum (not a bare int + parallel label array) keeps the
// label switch compiler-checked so reordering or adding a step cannot index out of range.
enum class MatchStep {
    None,
    PercClef,       // PERC clef or GM percussive range -> drumset
    NameMidiScore,  // name+MIDI score match
    DrumsetName,    // name scoring over drumset templates
    PercKeyword,    // generic percussion keyword in the name -> drumset
    RhythmStaff,    // RHYTHM staff -> snare-drum
    MidiProgram,    // MIDI program lookup
};

static const char* matchStepLabel(MatchStep step)
{
    switch (step) {
    case MatchStep::None:          return "";
    case MatchStep::PercClef:      return "PERC clef";
    case MatchStep::NameMidiScore: return "name+MIDI score";
    case MatchStep::DrumsetName:   return "drumset name";
    case MatchStep::PercKeyword:   return "perc keyword";
    case MatchStep::RhythmStaff:   return "RHYTHM staff";
    case MatchStep::MidiProgram:   return "MIDI program";
    }
    return "";
}

// Apply a found template (or fallback to Grand Piano) and set the instrument's long name.
static void applyInstrumentOrFallback(Part* part, const InstrumentTemplate* tmpl,
                                      const EncInstrument& instr, MatchStep matchStep)
{
    auto setInstrName = [&](Instrument& ins) {
        if (!instr.name.isEmpty()) {
            ins.setLongName(String(instr.name));
        }
        ins.setShortName(String());
        ins.instrumentLabel().setAllowGroupName(false);
    };
    if (tmpl) {
        Instrument ins = Instrument::fromTemplate(tmpl);
        setInstrName(ins);
        // A few generic templates (recorder, clarinet, trumpet, ...) carry no
        // track name in instruments.xml because their UI name is supplied by the
        // muse instruments, which are unavailable here. That would leave the
        // mixer and Instruments panel blank, so derive the sounding instrument's
        // name from the template id rather than from the Encore part label.
        if (ins.trackName().isEmpty()) {
            ins.setTrackName(humanizeTemplateId(tmpl->id));
        }
        LOGD() << "  instrument \"" << instr.name.toStdString()
               << "\": step(" << matchStepLabel(matchStep)
               << (matchStep == MatchStep::MidiProgram ? QString(" %1").arg(instr.midiProgram).toStdString() : std::string())
               << ")" << " -> " << ins.trackName().toStdString();
        part->setInstrument(ins);
        return;
    }
    // Grand Piano fallback.
    const InstrumentTemplate* pianoTmpl = searchTemplateForMidiProgram(0, 0, false);
    if (pianoTmpl) {
        LOGD() << "  instrument \"" << instr.name.toStdString() << "\": no match -> fallback: Grand Piano";
        Instrument ins = Instrument::fromTemplate(pianoTmpl);
        setInstrName(ins);
        part->setInstrument(ins);
    } else {
        LOGD() << "  instrument \"" << instr.name.toStdString() << "\": no match, no piano template -> bare MIDI";
        part->setMidiProgram(0, 0);
        if (!instr.name.isEmpty()) {
            part->setPlainLongName(String(instr.name));
        }
        part->setPlainShortName(String());
    }
}

// Step 2: name+MIDI score with transposition filter. A contains-only (substring) name match can
// outrank the GM instrument via the MIDI bonus: e.g. "Bajo" hits "Contrabajo"/"Clarín contrabajo"
// (a treble bugle sharing the tuba's program) instead of the bass-clef Tuba. When the name match
// is not exact, prefer the MIDI-program instrument if it differs. Exact matches are kept as-is, so
// the deliberate "Bass" -> acoustic-bass MIDI tiebreak (findTemplateByMidi returns the same
// template) is unaffected. A unique name match (a needle no other template's name contains, e.g.
// "Dulzaina") is as trustworthy as an exact one, so it is kept too.
static const InstrumentTemplate* tryNameMidiScore(const EncInstrument& instr, int encMidi,
                                                  int encKey, bool isRhythm)
{
    bool nameExact = false;
    bool nameUnique = false;
    const InstrumentTemplate* nameTmpl = findEncoreInstrumentTemplate(instr.name, encMidi, encKey, &nameExact, &nameUnique);
    if (nameTmpl && !nameExact && !nameUnique && !isRhythm && instr.midiProgram > 0) {
        const InstrumentTemplate* midiTmpl = findTemplateByMidi(instr.midiProgram - 1);
        if (midiTmpl && midiTmpl != nameTmpl) {
            LOGD() << "  instrument \"" << instr.name.toStdString()
                   << "\": weak name match \"" << nameTmpl->trackName.toStdString()
                   << "\" overridden by MIDI " << instr.midiProgram << " -> "
                   << midiTmpl->trackName.toStdString();
            nameTmpl = midiTmpl;
        }
    }
    if (!nameTmpl && !instr.name.trimmed().isEmpty()) {
        const InstrumentTemplate* rejected = findEncoreInstrumentTemplate(instr.name, encMidi);
        if (rejected) {
            LOGD() << "  instrument \"" << instr.name.toStdString()
                   << "\": MIDI " << instr.midiProgram << " match \""
                   << rejected->trackName.toStdString()
                   << "\" rejected (template chromatic=" << rejected->transpose.chromatic
                   << " vs encKey=" << encKey << "), trying MIDI";
        }
    }
    return nameTmpl;
}

// Step 4: generic percussion keyword in the instrument name.
static bool nameHasPercKeyword(const EncInstrument& instr)
{
    const QString lname = instr.name.toLower();
    return lname.contains(QStringLiteral("perc"))
           || lname.contains(QStringLiteral("drum"))
           || lname.contains(QStringLiteral("bater"));
}

// Step 6: MIDI program lookup (logs a transposition mismatch against the Encore key).
static const InstrumentTemplate* tryMidiProgram(const EncInstrument& instr, int encKey)
{
    if (instr.midiProgram <= 0) {
        return nullptr;
    }
    const InstrumentTemplate* midiTmpl = findTemplateByMidi(instr.midiProgram - 1);
    if (midiTmpl) {
        const int tmplChr = midiTmpl->transpose.chromatic;
        const bool transpMismatch = (tmplChr % 12 != 0) && (encKey % 12 != 0)
                                    && ((((encKey % 12) + 12) % 12) != (((tmplChr % 12) + 12) % 12));
        if (transpMismatch) {
            LOGD() << "  instrument \"" << instr.name.toStdString()
                   << "\": MIDI " << instr.midiProgram << " match \""
                   << midiTmpl->trackName.toStdString()
                   << "\" transposition differs (template chromatic=" << tmplChr
                   << " vs encKey=" << encKey << ")";
        }
    }
    return midiTmpl;
}

static const InstrumentTemplate* applyBestInstrument(Part* part,
                                                     const EncInstrument& instr,
                                                     bool isPercByClef,
                                                     bool isRhythm,
                                                     bool encWantsTab,
                                                     InstrumentSearchMode searchMode)
{
    // Piano mode: skip all matching and go straight to fallback.
    if (searchMode == InstrumentSearchMode::Piano) {
        applyInstrumentOrFallback(part, nullptr, instr, MatchStep::None);
        return nullptr;
    }

    const int encMidi = instr.midiProgram > 0 ? instr.midiProgram - 1 : -1;
    const int encKey  = static_cast<int>(instr.keyTransposeSemitones);
    const bool nameTooShort = instr.name.trimmed().size() < 4;
    const bool useNameSearch = (searchMode == InstrumentSearchMode::NameAndMidi);

    const InstrumentTemplate* tmpl = nullptr;
    MatchStep matchStep = MatchStep::None;
    auto tryStep = [&](MatchStep step, const InstrumentTemplate* candidate) {
        if (!tmpl && candidate) {
            tmpl = candidate;
            matchStep = step;
        }
    };

    // Step 1: PERC clef or GM Percussive range (113 to 128 1-indexed) → drumset.
    static constexpr int GM_PERC_FIRST = 113;
    if (isPercByClef || instr.midiProgram >= GM_PERC_FIRST) {
        tryStep(MatchStep::PercClef, searchTemplate(String(u"drumset")));
    }

    // Steps 2-4: name-based matching (skipped in MidiOnly mode).
    if (useNameSearch) {
        if (!tmpl) {
            tryStep(MatchStep::NameMidiScore, tryNameMidiScore(instr, encMidi, encKey, isRhythm));
        }
        // Step 3: name scoring over drumset templates.
        if (!nameTooShort) {
            tryStep(MatchStep::DrumsetName, findDrumsetTemplate(instr.name));
        }
        // Step 4: generic percussion keywords.
        if (!tmpl && !nameTooShort && nameHasPercKeyword(instr)) {
            tryStep(MatchStep::PercKeyword, searchTemplate(String(u"drumset")));
        }
    }

    // Step 5 (RHYTHM): snare-drum; skip MIDI to avoid program-0 piano override.
    if (isRhythm) {
        tryStep(MatchStep::RhythmStaff, searchTemplate(String(u"snare-drum")));
    }

    // Step 6: MIDI program lookup (skipped for RHYTHM staves).
    if (!tmpl && !isRhythm) {
        tryStep(MatchStep::MidiProgram, tryMidiProgram(instr, encKey));
    }

    // Encore stores the staff's notation/tablature choice in the clef; a name/MIDI match may
    // land on the wrong variant (e.g. "Classical Guitar (tablature)" when Encore uses a normal
    // clef). Swap to the standard or tablature sibling to match Encore's clef.
    if (tmpl && !isRhythm && (tmpl->staffGroup == StaffGroup::TAB) != encWantsTab) {
        if (const InstrumentTemplate* variant = findInstrumentVariant(tmpl, encWantsTab)) {
            LOGD() << "  instrument \"" << instr.name.toStdString() << "\": clef-driven variant "
                   << (encWantsTab ? "tablature" : "standard") << " -> " << variant->trackName.toStdString();
            tmpl = variant;
        }
    }

    applyInstrumentOrFallback(part, tmpl, instr, matchStep);
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

        const EncLineStaffData* lsd = lineStaffDataAt(enc, cumStaffIdx);
        const bool isPercByClef = lsd && lsd->clef == EncClefType::PERC;
        const bool isRhythm = lsd && lsd->staffType == EncStaffType::RHYTHM;
        // Tablature only when Encore explicitly marks it (clef==TAB or staffType==TAB). v0xA6
        // has no per-staff clef in the LINE block, so it defaults to standard notation.
        const bool encWantsTab = lsd && (lsd->clef == EncClefType::TAB
                                         || lsd->staffType == EncStaffType::TAB);
        const InstrumentTemplate* tmpl = applyBestInstrument(part, instr, isPercByClef, isRhythm,
                                                             encWantsTab, ctx.opts.instrumentSearchMode);

        const bool showFromLine = !lsd || lsd->showStaff;
        if (!showFromLine) {
            part->setShow(false);
        }

        const int pitchOffset = static_cast<int>(instr.keyTransposeSemitones);
        // Transposition handling depends on the offset:
        //  - non-octave (Bb, Eb, F instruments): set on instrument so the display shows the
        //    written (Encore-stored) pitch.
        //  - positive octave (instrument sounds higher): also set on instrument, so the staff
        //    keeps a plain clef and the notes stay at their written height (the octave is a
        //    playback transposition). applyOctaveToClef() deliberately produces no 8va clef.
        //  - negative octave (instrument sounds lower): left to the octave-down clef applied by
        //    pickStaffClef()/applyOctaveToClef() plus the template's own transposition.
        Instrument* instrument = part->instrument();
        if (instrument) {
            if (pitchOffset != 0 && (std::abs(pitchOffset) % 12 != 0 || pitchOffset > 0)) {
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
                // Encore does not store bracket/brace grouping data, so remove any
                // bracket the template may have set to avoid spurious cross-part brackets.
                staff->setBracketType(0, BracketType::NO_BRACKET);
                staff->setBracketSpan(0, 0);
                // Safety net: if no standard sibling template was found above and the staff is
                // still tablature though Encore stores standard notation, force a standard staff.
                if (!encWantsTab && staff->staffType(Fraction(0, 1))->group() == StaffGroup::TAB) {
                    if (const StaffType* stdType = StaffType::getDefaultPreset(StaffGroup::STANDARD)) {
                        staff->setStaffType(Fraction(0, 1), *stdType);
                    }
                }
            }
            ctx.staffPitchOffset.push_back(pitchOffset);
            ClefType cClef = ClefType::INVALID;
            if (tmpl) {
                cClef = tmpl->clefType(static_cast<staff_idx_t>(s)).concertClef;
            }
            ctx.staffTemplateConcertClef.push_back(cClef);
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
} // namespace mu::iex::enc
