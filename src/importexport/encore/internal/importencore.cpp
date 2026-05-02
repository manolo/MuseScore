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

// Encore (.enc) file importer for MuseScore.
// The binary format was reverse-engineered by Leon Vinken (Enc2MusicXML project,
// https://github.com/lvinken/Enc2MusicXML, GPL v3+) building on enc2ly by Felipe Castro.
// This importer is based on that work.

#include "importencore.h"

#include "encoreelements.h"
#include "encoremapping.h"
#include "encorerhythm.h"
#include "encoretuplets.h"

#include <algorithm>
#include <memory>
#include <map>
#include <set>
#include <vector>

#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include "engraving/dom/box.h"
#include "engraving/dom/chord.h"
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

static void buildScore(MasterScore* score, const EncFile& enc)
{
    // Initialize chord list for harmony parsing
    score->style().set(Sid::chordsXmlFile, true);
    score->chordList()->read(u"chords.xml");

    // --------------- Setup: parts and staves ---------------
    int totalStaves = 0;
    for (const auto& instr : enc.instruments) {
        int ns = instr.nstaves > 0 ? instr.nstaves : 1;
        Part* part = new Part(score);

        // Find the best matching instrument template.
        // Strategy: name matching first, then MIDI program (v0xC4 only).
        const InstrumentTemplate* tmpl = findEncoreInstrumentTemplate(instr.name);
        if (!tmpl && instr.midiProgram > 0) {
            // MIDI program is stored 1-indexed; searchTemplateForMidiProgram
            // expects 0-indexed (GM bank 0).
            tmpl = searchTemplateForMidiProgram(0, instr.midiProgram - 1, false);
        }
        if (tmpl) {
            Instrument instrument = Instrument::fromTemplate(tmpl);
            // Override the template's generic long name with the original
            // Encore instrument name so the Layout tab shows e.g. "Bandurria 1"
            // rather than the bare template name "Bandurria".
            if (!instr.name.isEmpty()) {
                instrument.setLongName(String(instr.name));
            }
            part->setInstrument(instrument);
        } else {
            // No template found by name or MIDI program: fall back to Grand Piano.
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

        // Apply Encore's staff visibility flag (showByte at +19 in EncLineStaffData).
        if (!instr.showStaff) {
            part->setShow(false);
        }

        for (int s = 0; s < ns; ++s) {
            Staff* staff = Factory::createStaff(part);
            score->appendStaff(staff);
            ++totalStaves;
        }
        score->appendPart(part);
    }
    if (totalStaves == 0) {
        Part* part = new Part(score);
        part->setMidiProgram(0, 0);
        Staff* staff = Factory::createStaff(part);
        score->appendStaff(staff);
        score->appendPart(part);
        totalStaves = 1;
    }

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
        } else if (encMeas.endBarline() == EncBarlineType::FINAL) {
            measure->setEndBarLineType(BarLineType::END, false);
        } else if (encMeas.endBarline() == EncBarlineType::DOUBLEL
                   || encMeas.endBarline() == EncBarlineType::DOUBLER) {
            measure->setEndBarLineType(BarLineType::DOUBLE, false);
        }

        score->measures()->append(measure);
        currentTick += ts.ticks();
    }

    // --------------- Initial key/time/clef signatures ---------------
    if (!enc.measures.empty()) {
        addInitialTimeSig(score, totalStaves, enc.measures[0]);
    }
    if (!enc.lines.empty()) {
        const auto& firstLine = enc.lines[0];
        for (int si = 0; si < static_cast<int>(firstLine.staffData.size()) && si < totalStaves; ++si) {
            const auto& sd = firstLine.staffData[si];
            addInitialKeySig(score, si, sd.key);
            addInitialClef(score, si, sd.clef);
        }
    }

    // --------------- Notes, rests, ornaments, chord symbols ---------------
    // Track pending slurs/hairpins: key = (staffIdx, xoffset)
    std::map<std::pair<int, int>, Slur*> pendingSlurs;
    std::map<std::pair<int, int>, Hairpin*> pendingHairpins;

    // Tuplet state per (staffIdx, msVoice) — keyed by MuseScore voice/track
    std::map<std::pair<int, int>, TupletTracker> tuplets;

    // Pending tie-start notes: key = (staffIdx, voice, pitch), value = Note* to tie FROM.
    // Populated when a TIE element exists at a note's position; cleared when the
    // tie-end note (same staffIdx, voice, pitch) is placed.  Persists across measures
    // to handle ties across barlines.
    std::map<std::tuple<int, int, int>, Note*> pendingTieNote;

    // faceValue-cumulative placement: accumulate written note durations per (staffIdx, msVoice).
    // Notes are placed at cumTick, not at MIDI tick positions.  MIDI ticks are used only
    // to determine note order and chord grouping (same MIDI tick = same chord).
    // All maps below are keyed by (staffIdx, msVoice) — the MuseScore voice actually used.
    std::map<std::pair<int, int>, Fraction> cumTick;      // accumulated written position
    std::map<std::pair<int, int>, int> prevMidiTick;       // last MIDI tick placed (chord detection)
    // Encore voice of the last note placed at trackKey.  When two different
    // Encore voices land at the same MuseScore voice via streamOffset (a
    // multi-stream split of one Encore voice spilled into the slot of another),
    // their prevMidiTick entries would otherwise collide and a note from the
    // second Encore voice could be misdetected as a chord extension of the first.
    std::map<std::pair<int, int>, int> prevEncVoice;
    std::map<std::pair<int, int>, Fraction> lastChordPos;  // MuseScore tick of last chord root

    // Grace chords queued for attachment to the next normal chord on the same track.
    // Grace chords are NOT added to a Segment (they would crash beam layout: Chord::pagePos
    // does toChord(explicitParent()) which asserts when the parent is a Segment instead of
    // the main Chord). They are held detached, then attached via Chord::add() which inserts
    // them into the main chord's m_graceNotes list. Cleared per measure.
    std::map<std::pair<int, int>, std::vector<Chord*> > pendingGraces;

    // Multi-stream voice assignment (Option B):
    // When Encore encodes multiple simultaneous recording streams in the same (staffIdx, voice)
    // slot, the importer normally merges them into one MuseScore voice.  When cumTick for the
    // current MuseScore voice reaches measure->ticks() and a new non-chord note arrives, it
    // belongs to a DIFFERENT recording stream.  Assign it to the next MuseScore voice instead
    // of skipping it, so each stream gets its own voice (voice 0, 1, 2 … up to VOICES-1).
    // This map persists across measures: once stream 2 starts on msVoice=1, it continues
    // on msVoice=1 in all subsequent measures.
    std::map<std::pair<int, int>, int> streamOffset;  // key=(staffIdx,encVoice), val=extra offset

    int measIdx = 0;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        if (measIdx >= static_cast<int>(enc.measures.size())) {
            break;
        }
        Measure* measure = toMeasure(mb);
        const EncMeasure& encMeas = enc.measures[measIdx];
        const Fraction measTick = measure->tick();

        // Reset per-measure state: tuplets, cumulative positions, chord tracking.
        for (auto& [key, tt] : tuplets) {
            if (tt.inTuplet()) {
                tt.closeTuplet();
            }
        }
        tuplets.clear();
        cumTick.clear();
        prevMidiTick.clear();
        prevEncVoice.clear();
        lastChordPos.clear();

        // Drop any grace chords left unattached from the previous measure (no main
        // chord followed them within the measure). They are not in the score tree,
        // so we must delete them explicitly to avoid a leak.
        for (auto& [key, vec] : pendingGraces) {
            for (Chord* gc : vec) {
                LOGW() << "Encore import: discarding dangling grace chord at measure " << measIdx
                       << " (staff " << key.first << ", voice " << key.second << ")";
                delete gc;
            }
        }
        pendingGraces.clear();

        // Repeat navigation marks
        EncRepeatType rt = encMeas.repeatMark();
        if (rt != EncRepeatType::NONE) {
            addRepeatMark(score, measure, rt);
        }

        // Volta (first/second endings) from repeatAlternative bitmask
        if (encMeas.repeatAlternative != 0) {
            Volta* volta = Factory::createVolta(score->dummy());
            volta->setVoltaType(Volta::Type::CLOSED);
            volta->setTrack(0);
            volta->setTrack2(0);
            volta->setTick(measTick);
            volta->setTick2(measTick + measure->ticks());
            std::vector<int> endings;
            for (int b = 0; b < 8; ++b) {
                if (encMeas.repeatAlternative & (1 << b)) {
                    endings.push_back(b + 1);
                }
            }
            volta->setEndings(endings);
            score->addElement(volta);
        }

        // Sort elements by tick, then by type (ornaments/non-notes before notes),
        // then among notes at the same tick: tuplet notes before non-tuplet notes.
        // The last rule ensures that when Encore stores both a tuplet note (tup!=0)
        // and a tie-artifact note (tup=0) at the same tick, the tuplet note creates
        // the chord with the correct duration (e.g. V_EIGHTH, not V_QUARTER).
        MeasureElemRefVec sortedElems;
        sortedElems.reserve(encMeas.elements.size());
        for (const auto& elem : encMeas.elements) {
            sortedElems.push_back(elem.get());
        }
        std::stable_sort(sortedElems.begin(), sortedElems.end(),
                         [](const EncMeasureElem* a, const EncMeasureElem* b) {
            if (a->tick != b->tick) {
                return a->tick < b->tick;
            }
            bool aIsNote = (static_cast<EncElemType>(a->type) == EncElemType::NOTE
                            || static_cast<EncElemType>(a->type) == EncElemType::REST);
            bool bIsNote = (static_cast<EncElemType>(b->type) == EncElemType::NOTE
                            || static_cast<EncElemType>(b->type) == EncElemType::REST);
            if (aIsNote != bIsNote) {
                return !aIsNote;  // non-notes before notes
            }
            if (!aIsNote) {
                return false;     // both non-notes: preserve stable order
            }
            // Among notes at same tick: tuplet notes before non-tuplet notes
            bool aTuplet = a->tupletByte() != 0;
            bool bTuplet = b->tupletByte() != 0;
            if (aTuplet != bTuplet) {
                return aTuplet;   // tuplet note first
            }
            return false;         // stable for equal keys
        });

        // Pre-scan: collect TIE element positions for this measure.
        // Only TIE-STARTs (elemStart+2 == 0xfe, outgoing tie) are added.
        // Arc-only markers (0x02 etc.) indicate an incoming tie endpoint and are NOT
        // added — adding them would falsely mark nearby notes as tie-senders.
        std::set<std::tuple<int, int, int> > tieStartSet;
        for (const EncMeasureElem* e : sortedElems) {
            if (static_cast<EncElemType>(e->type) == EncElemType::TIE) {
                const EncTie* et = static_cast<const EncTie*>(e);
                if (et->isTieStart) {
                    tieStartSet.insert({ (int)e->staffIdx, (int)e->voice, (int)e->tick });
                }
            }
        }
        // Helper: check if a note at (si, v, tick) is a tie-start, tolerating
        // near-simultaneous chord clustering (notes within CHORD_CLUSTER_THRESHOLD ticks).
        // Use strict less-than (< not <=) so TIE elements at exactly
        // CHORD_CLUSTER_THRESHOLD ticks away do not match a later chord
        // (e.g., TIE@476 must not match SC chord at tick=480 when gap=4=threshold).
        auto isTieStart = [&](int si, int v, int tick) -> bool {
            for (int dt = 0; dt < CHORD_CLUSTER_THRESHOLD; ++dt) {
                if (tieStartSet.count({ si, v, tick - dt })) {
                    return true;
                }
                if (dt > 0 && tieStartSet.count({ si, v, tick + dt })) {
                    return true;
                }
            }
            return false;
        };

        // Pre-compute which elements belong to COMPLETE tuplet groups.
        // - Explicit tuplets (all formats): groups of exactly actualN consecutive notes
        //   with the same standard tup byte.  Isolated tail notes (e.g. note 4 after a
        //   3:2 triplet group) are excluded to avoid partial-tuplet checkMeasure issues.
        // - Implied tuplets (v0xC2 only): groups of exactly actualN notes with matching
        //   detectImpliedTuplet ratio; isolated swing-timing notes are excluded.
        // Both sets are merged into validTupletGroupMember.
        std::set<const EncMeasureElem*> validTupletGroupMember
            = computeImpliedTupletMembers(sortedElems, encMeas, totalStaves);
        // Alias for implied-tuplet checks (v0xC2 guard applied in the element loop).
        const auto& impliedGroupMember = validTupletGroupMember;

        // Tracks filtered MIDI artifact notes that are tie-senders (grace1 low nibble == 1).
        // When such a note is filtered by the rdur<15 check, its continuation note (same
        // pitch, same voice, grace1 low == 2) should also be filtered — Encore hides both.
        std::set<std::tuple<int, int, int> > filteredTieSenderPitches;

        for (const EncMeasureElem* e : sortedElems) {
            EncElemType et = static_cast<EncElemType>(e->type);

            // Skip notes/rests/ornaments at or beyond measure end.
            if ((et == EncElemType::NOTE || et == EncElemType::REST
                 || et == EncElemType::ORNAMENT)
                && e->tick >= encMeas.durTicks) {
                continue;
            }

            int staffIdx = static_cast<int>(e->staffIdx);
            int voice    = static_cast<int>(e->voice);

            if (staffIdx >= totalStaves) {
                continue;
            }
            if (voice >= static_cast<int>(VOICES)) {
                continue;
            }

            // Determine which MuseScore voice to use for this Encore (staffIdx, voice) slot.
            // streamOffset starts at 0 and increments when a recording stream overflows
            // the current MuseScore voice (see "multi-stream voice assignment" below).
            auto encVoiceKey = std::make_pair(staffIdx, voice);
            int msVoice = voice + streamOffset[encVoiceKey];
            if (msVoice >= static_cast<int>(VOICES)) {
                continue;  // all MuseScore voices used up
            }
            track_idx_t track = static_cast<track_idx_t>(staffIdx * VOICES + msVoice);
            // trackKey keys all per-voice state (cumTick, prevMidiTick, lastChordPos, tuplets).
            auto trackKey = std::make_pair(staffIdx, msVoice);

            // faceValue-cumulative placement: compute MuseScore position from accumulated
            // written durations, not from the MIDI tick position.
            // Same MIDI tick (or near-simultaneous within CHORD_MIDI_THRESHOLD) in the
            // same MuseScore voice = chord extension (same cumulative position).
            constexpr int CHORD_MIDI_THRESHOLD = 2 * CHORD_CLUSTER_THRESHOLD;  // = 8
            bool isNoteOrRest = (et == EncElemType::NOTE || et == EncElemType::REST);
            // Only treat as a chord extension if the previous note at this trackKey
            // came from the SAME Encore voice. Otherwise a multi-stream spill from
            // another encVoice can falsely chord-extend this note's encVoice.
            bool isChordExt   = isNoteOrRest && prevMidiTick.count(trackKey)
                                && prevEncVoice.count(trackKey)
                                && prevEncVoice.at(trackKey) == voice
                                && (int)e->tick - (int)prevMidiTick.at(trackKey) >= 0
                                && (int)e->tick - (int)prevMidiTick.at(trackKey)
                                < CHORD_MIDI_THRESHOLD;

            // Multi-stream voice assignment (Option B):
            // When a non-chord note arrives and the current MuseScore voice is already full,
            // this note belongs to a second (or third…) simultaneous recording stream encoded
            // in the same Encore voice slot.  Assign it to the next MuseScore voice.
            // Multi-stream voice assignment loop: keep switching voices until we find
            // one with remaining space or exhaust all VOICES. A single switch is not
            // enough because the target voice may also be full (e.g. a prior rest filled
            // it). Without the loop, a note can be placed in a full voice and overflow
            // the measure.
            bool dropNote = false;
            while (isNoteOrRest && !isChordExt && cumTick[trackKey] >= measure->ticks()) {
                int newOffset = streamOffset[encVoiceKey] + 1;
                if (voice + newOffset >= static_cast<int>(VOICES)) {
                    dropNote = true;  // all MuseScore voices full, skip
                    break;
                }
                streamOffset[encVoiceKey] = newOffset;
                msVoice  = voice + newOffset;
                track    = static_cast<track_idx_t>(staffIdx * VOICES + msVoice);
                trackKey = std::make_pair(staffIdx, msVoice);
                // isChordExt for the fresh voice: no prevMidiTick yet → always false
                isChordExt = false;
            }
            if (dropNote) {
                continue;
            }

            // Save prevMidiTick/lastChordPos so we can restore them if we later decide
            // to skip the note (implied tuplet group that doesn't fit in remaining
            // measure space) or route it to the grace queue (graces must not look like
            // a chord root to the next note's chord-extension check).
            const int savedPrevMidiTick = prevMidiTick.count(trackKey)
                                          ? prevMidiTick.at(trackKey) : -1;
            const bool hadLastChordPos = lastChordPos.count(trackKey);
            const Fraction savedLastChordPos = hadLastChordPos
                                               ? lastChordPos.at(trackKey) : Fraction(-1, 1);

            Fraction elemTick;
            {
                if (isChordExt) {
                    elemTick = lastChordPos.count(trackKey) ? lastChordPos.at(trackKey)
                               : measTick;
                } else {
                    elemTick = measTick + cumTick[trackKey];
                    if (isNoteOrRest) {
                        lastChordPos[trackKey] = elemTick;
                    }
                    // Only notes establish a chord-extension anchor. A rest is a
                    // separator: a following note at the same MIDI tick is a fresh
                    // cluster, not an extension of the rest (the rest's segment
                    // would otherwise be silently replaced and the rest's cumTick
                    // contribution double-counted).
                    if (et == EncElemType::NOTE) {
                        prevMidiTick[trackKey] = e->tick;
                        prevEncVoice[trackKey] = voice;
                    }
                }
            }

            // -- Notes --
            if (et == EncElemType::NOTE) {
                const EncNote* en = static_cast<const EncNote*>(e);

                // Grace-note short-circuit. A grace chord must be parented under its
                // main Chord (not a Segment) or Chord::pagePos crashes via
                // toChord(explicitParent()) during beam layout. We build a detached
                // chord, queue it, and attach it later when the next non-grace chord
                // on this track is created.
                //
                // Grace classification: faceValue >= 4 (eighth or shorter) AND grace
                // bytes set. faceValue < 4 with grace bytes is a known Encore quirk
                // and must be treated as a normal note.
                {
                    quint8 safeFvGrace = en->faceValue & 0x0F;
                    if (safeFvGrace > 0 && safeFvGrace <= 8 && safeFvGrace >= 4
                        && en->graceType() != EncGraceType::NORMAL) {
                        // Roll back per-track tick state so the next note is not
                        // detected as a chord extension of this grace.
                        if (savedPrevMidiTick >= 0) {
                            prevMidiTick[trackKey] = savedPrevMidiTick;
                        } else {
                            prevMidiTick.erase(trackKey);
                        }
                        if (hadLastChordPos) {
                            lastChordPos[trackKey] = savedLastChordPos;
                        } else {
                            lastChordPos.erase(trackKey);
                        }

                        // Build the detached grace chord with its duration and pitch.
                        DurationType graceDt = realDuration2DurationType(en->realDuration, en->faceValue);
                        Chord* gc = Factory::createChord(score->dummy()->segment());
                        gc->setTrack(track);
                        TDuration gdur(graceDt);
                        gc->setDurationType(gdur);
                        gc->setTicks(gdur.fraction());
                        gc->setDots(0);
                        gc->setNoteType(en->graceType() == EncGraceType::ACCIACCATURA
                                        ? NoteType::ACCIACCATURA : NoteType::APPOGGIATURA);

                        Note* gnote = Factory::createNote(gc);
                        gnote->setPitch(en->semiTonePitch);
                        gnote->setTpcFromPitch();
                        gc->add(gnote);

                        // Articulation on a grace is rare but Encore can encode it.
                        if (en->articulationUp == 0x20 || en->articulationDown == 0x20) {
                            Articulation* art = Factory::createArticulation(gc);
                            art->setSymId(SymId::fermataAbove);
                            gc->add(art);
                        }

                        pendingGraces[trackKey].push_back(gc);
                        continue;
                    }
                }

                // Skip notes with invalid faceValue (0 or > 8).
                quint8 safeFv = en->faceValue & 0x0F;
                if (safeFv == 0 || safeFv > 8) {
                    continue;
                }
                // Skip MIDI tie-continuation artifacts (very short realDuration < 15 ticks).
                // Exceptions for 64th/128th notes (fvBase ≤ 15):
                //   (a) Tie-start notes (TIE element with 0xfe direction at this tick):
                //       real short notes that SEND ties — must be placed.
                //   (b) Chord-extension notes (within CHORD_MIDI_THRESHOLD of the previous
                //       note in the same voice): e.g. E@476 is a chord extension of C@473
                //       even though TIE@476 is arc-only (0x02).
                // Tie artifacts have neither: they don't have TIE-START elements and are
                // not chord extensions of a recently placed note.
                // NOTE: use isChordExt (computed from the OLD prevMidiTick, before the
                // update at line ~1889) rather than recomputing here.  After the update,
                // prevMidiTick equals e->tick, so a fresh computation always yields delta=0
                // and would incorrectly bypass the filter for non-chord-extension notes.
                if (en->realDuration > 0 && en->realDuration < 15) {
                    int fvBase = faceValue2ticks(safeFv);
                    if (fvBase <= 15) {
                        bool bypass = isTieStart(staffIdx, voice, (int)e->tick)
                                      || isChordExt;
                        if (!bypass) {
                            // Record tie-senders so their continuation note is also filtered.
                            if ((en->grace1 & 0x0F) == 1) {
                                filteredTieSenderPitches.insert(
                                    { staffIdx, voice, (int)en->semiTonePitch });
                            }
                            continue;
                        }
                    } else {
                        // For 8th/16th/32nd+ notes: filter only if NOT at the chord-cluster
                        // boundary.  When realDuration == CHORD_CLUSTER_THRESHOLD,
                        // calculateRealDurations found the next non-clustered note exactly at
                        // the boundary (4 ticks away), which still indicates a live-recorded
                        // chord root whose cluster partner fell just outside the ±3-tick window.
                        // Keeping such notes ensures the chord root is placed so subsequent
                        // chord-extension notes (isChordExt) can join it correctly.
                        if (en->realDuration > CHORD_CLUSTER_THRESHOLD) {
                            continue;
                        }
                    }
                }

                // Cascade-filter: if this note is a tie-receiver (grace1 low == 2) whose
                // sender was a filtered MIDI artifact, filter this note too.  Encore hides
                // both the artifact and its dotted-note continuation from display.
                if ((en->grace1 & 0x0F) == 2) {
                    auto cascKey = std::make_tuple(staffIdx, voice, (int)en->semiTonePitch);
                    if (filteredTieSenderPitches.count(cascKey)) {
                        filteredTieSenderPitches.erase(cascKey);
                        continue;
                    }
                }

                // Determine if this note has an explicit standard tuplet byte.
                // This check is done before computing dt because explicit tuplet notes
                // use faceValue directly (rdur is MIDI timing and may be wrong when the
                // next MIDI event starts before the note's written duration ends).
                {
                    int preA = en->actualNotes(), preN = en->normalNotes();
                    bool stdE = (preA == 3 && preN == 2) || (preA == 5 && preN == 4) || (preA == 6 && preN == 4);
                    if (!stdE) {
                        preA = 0;
                        preN = 0;
                    }
                    if (preA == 0 && enc.header.isOldFormat() && (en->faceValue & 0x0F) >= 4
                        && (tuplets[trackKey].inTuplet() || impliedGroupMember.count(e))) {
                        preA = detectImpliedTuplet(en->realDuration, en->faceValue, preN);
                    }
                    // Store back for use below (only the stdE flag is needed here)
                    (void)preA;
                    (void)preN;
                }
                int preACheck = en->actualNotes(), preNCheck = en->normalNotes();
                bool isStandardExplicit = (preACheck == 3 && preNCheck == 2)
                                          || (preACheck == 5 && preNCheck == 4)
                                          || (preACheck == 6 && preNCheck == 4);

                // Duration type:
                // - Explicit tuplet notes: use faceValue directly (rdur = MIDI timing,
                //   can be truncated by a following event and give wrong dt).
                // - All other notes: use realDuration for whole-rest-in-partial-measure
                //   mapping and to handle MIDI timing drift gracefully.
                DurationType dt;
                int dots;
                if (isStandardExplicit) {
                    dt   = faceValue2DurationType(en->faceValue);
                    dots = 0;
                } else {
                    dt   = realDuration2DurationType(en->realDuration, en->faceValue);
                    // Use dotControl (actual sounding duration stored by Encore) for dot
                    // count.  dotControl stores the Encore sounding duration; realDuration
                    // comes from MIDI tick spacing and may have small timing drift (±2 ticks).
                    // Strategy: use dotControl when it identifies a dotted value (>0 dots);
                    // otherwise snap realDuration with tolerance for MIDI drift.
                    if (en->dotControl > 0) {
                        int dByCtrl = calcDots(static_cast<qint16>(en->dotControl),
                                               en->faceValue);
                        if (dByCtrl > 0) {
                            dots = dByCtrl;   // dotControl gives a clear dotted value
                        } else {
                            // dotControl didn't identify dots; try snapping realDuration
                            dots = calcDotsSnap(en->realDuration, en->faceValue);
                        }
                    } else {
                        dots = calcDotsSnap(en->realDuration, en->faceValue);
                    }
                }
                // Save the face-value dt before any capping modifies it.
                // Used below to check whether an isolated explicit note exactly fills
                // the remaining measure space as a partial tuplet.
                const DurationType dtFace = dt;

                // For non-tuplet notes, cap the chord duration to remaining measure space.
                {
                    const auto& ttPre = tuplets[trackKey];
                    int preA = isStandardExplicit ? preACheck : 0;
                    int preN = isStandardExplicit ? preNCheck : 0;
                    if (!isStandardExplicit) {
                        if (enc.header.isOldFormat() && (en->faceValue & 0x0F) >= 4
                            && ((ttPre.inTuplet() && !ttPre.groupFull()) || impliedGroupMember.count(e))) {
                            preA = detectImpliedTuplet(en->realDuration, en->faceValue, preN);
                        }
                    }

                    // Partial implied-tuplet-group guard:
                    // If starting a NEW implied tuplet group whose full advance wouldn't fit
                    // in the current MuseScore voice's remaining space, SKIP this note
                    // entirely.  Placing only part of a triplet group (e.g. 2 out of 3)
                    // leaves a remainder (e.g. 1/3072) that is mathematically inexpressible
                    // as standard note durations and causes "Incomplete measure" on reload.
                    //
                    // By skipping and restoring prevMidiTick, the NEXT element no longer
                    // sees this note as a chord root, so it starts fresh in the same voice
                    // and fills the remaining space cleanly with standard (non-tuplet) notes
                    // from the other interleaved recording streams.
                    if (!isStandardExplicit && !ttPre.inTuplet()
                        && !isChordExt && preA > 0 && preN > 0) {
                        Fraction singleAdv = TDuration(faceValue2DurationType(en->faceValue & 0x0F)).fraction()
                                             * Fraction(preN, preA);
                        Fraction fullGroupAdv = singleAdv * Fraction(preA, 1);
                        Fraction mRemaining = measure->ticks() - cumTick[trackKey];
                        if (fullGroupAdv > mRemaining) {
                            // Restore prevMidiTick so the next element is not detected as a
                            // chord extension of this skipped note.
                            if (savedPrevMidiTick >= 0) {
                                prevMidiTick[trackKey] = savedPrevMidiTick;
                            } else {
                                prevMidiTick.erase(trackKey);
                            }
                            continue;  // Skip this note; don't place, don't advance cumTick
                        }
                    }

                    // willBeTuplet: true only when the note WILL actually be placed in a
                    // tuplet group. A groupFull tuplet will be closed before this note is
                    // added, so it does NOT count as "in tuplet" for capping purposes.
                    // For explicit-but-not-validated notes: they are treated as plain.
                    bool willBeExplicit = isStandardExplicit && validTupletGroupMember.count(e);
                    bool willBeTuplet = (preA > 0 && preN > 0 && (willBeExplicit || !isStandardExplicit))
                                        || (ttPre.inTuplet() && !ttPre.groupFull());
                    if (!willBeTuplet) {
                        Fraction remaining = measure->ticks() - cumTick[trackKey];
                        // Include dots in the comparison: TDuration(dt) alone gives the
                        // undotted fraction, missing 1/2 or 3/4 of the actual note length.
                        TDuration fullDur(dt);
                        fullDur.setDots(dots);
                        if (remaining > Fraction(0, 1) && fullDur.fraction() > remaining) {
                            TDuration capped(remaining, true);
                            // If remaining is so small that TDuration cannot represent any
                            // standard duration (e.g. residual 1/3072 left by an earlier
                            // tuplet/cap mismatch), placing the note would create a
                            // zero-tick chord that breaks sanityCheck and later layout.
                            // Skip the note instead.
                            if (capped.fraction().numerator() == 0) {
                                if (savedPrevMidiTick >= 0) {
                                    prevMidiTick[trackKey] = savedPrevMidiTick;
                                } else {
                                    prevMidiTick.erase(trackKey);
                                }
                                continue;
                            }
                            dt   = capped.type();
                            dots = capped.dots();
                        }
                    }
                }

                Segment* seg = measure->getSegment(SegmentType::ChordRest, elemTick);
                Chord* chord = nullptr;
                if (seg->element(track) && seg->element(track)->isChord()) {
                    chord = toChord(seg->element(track));
                } else {
                    chord = Factory::createChord(seg);
                    chord->setTrack(track);
                    TDuration dur(dt);
                    dur.setDots(dots);
                    chord->setDurationType(dur);
                    chord->setTicks(dur.fraction());
                    chord->setDots(dots);
                    seg->add(chord);

                    // Tuplet handling.
                    auto& tt = tuplets[trackKey];
                    int actualN = isStandardExplicit ? preACheck : 0;
                    int normalN = isStandardExplicit ? preNCheck : 0;
                    // Implied tuplet detection (v0xC2 only, pre-validated groups).
                    // Use !tt.groupFull() so that a note arriving just as the previous
                    // implied group completes does NOT start a new unvalidated group via
                    // the tt.inTuplet() path (the full group will be closed immediately).
                    if (actualN == 0 && enc.header.isOldFormat() && (en->faceValue & 0x0F) >= 4
                        && ((tt.inTuplet() && !tt.groupFull()) || impliedGroupMember.count(e))) {
                        actualN = detectImpliedTuplet(en->realDuration, en->faceValue, normalN);
                    }

                    if (actualN > 0 && normalN > 0) {
                        // Close a completed group before starting a new one.
                        // For explicit tuplets: only start a new group when this element
                        // was pre-validated as part of a complete run.  Isolated notes
                        // that can't form a full group (e.g. note 4 after a completed
                        // 3:2 triplet) are treated as plain non-tuplet notes to avoid
                        // partial-tuplet checkMeasure overshoot.
                        if (tt.groupFull()) {
                            tt.closeTuplet();
                        }
                        if (!tt.inTuplet()) {
                            if (isStandardExplicit && !validTupletGroupMember.count(e)) {
                                // Isolated explicit note: not part of a pre-validated group.
                                // Special case: if its FACE-VALUE tuplet advance exactly fills
                                // the remaining measure space, create a partial tuplet so that
                                // checkMeasure sees the correct span and inserts no fills.
                                // Use dtFace (before any capping) for the advance check.
                                Fraction tupAdv = TDuration(dtFace).fraction()
                                                  * Fraction(normalN, actualN);
                                Fraction remaining = measure->ticks() - cumTick[trackKey];
                                if (tupAdv == remaining) {
                                    dt   = dtFace;  // restore face value (undo capping)
                                    dots = 0;
                                    // Update the chord's duration to face value
                                    TDuration faceD(dtFace);
                                    chord->setDurationType(faceD);
                                    chord->setTicks(faceD.fraction());
                                    chord->setDots(0);
                                    tt.startTuplet(measure, elemTick, actualN, normalN, dt, track);
                                } else {
                                    actualN = 0;
                                    normalN = 0;               // treat as plain note
                                }
                            } else {
                                tt.startTuplet(measure, elemTick, actualN, normalN, dt, track);
                            }
                        }
                    }
                    if (actualN > 0 && normalN > 0) {
                        chord->setTuplet(tt.currentTuplet);
                        tt.currentTuplet->add(chord);

                        tt.faceTicks += TDuration(dt).fraction();
                    } else {
                        if (tt.groupFull()) {
                            tt.closeTuplet();
                        }
                        if (tt.inTuplet()) {
                            tt.closeTuplet();                 // non-tuplet note exits group
                        }
                    }

                    // Advance cumulative position by the written duration.
                    // Grace notes are routed via the grace short-circuit above and
                    // never reach this branch, so the advance is unconditional here.
                    {
                        Fraction advance = tt.inTuplet()
                                           ? TDuration(dt).fraction() * Fraction(tt.normalN, tt.actualN)
                                           : dottedAdvance(dt, dots);
                        // Cap to remaining measure space to prevent overflow.
                        // For tuplet notes: if the advance must be capped, the note's
                        // face-value ticks exceed the capped advance, making actualTicks()
                        // > advance and causing sanityCheck overshoot. Remove the note from
                        // the tuplet and assign the capped duration as a plain note instead.
                        Fraction remaining = measure->ticks() - cumTick[trackKey];
                        if (advance > remaining && remaining > Fraction(0, 1)) {
                            advance = TDuration(remaining, true).fraction();
                            if (advance.numerator() == 0) {
                                // Remaining is smaller than any standard duration. The
                                // chord we just placed would become a zero-tick element
                                // (or, worse, get assigned garbage ticks by
                                // TDuration(0/0) further down). Remove it.
                                if (tt.inTuplet()) {
                                    chord->setTuplet(nullptr);
                                    tt.currentTuplet->remove(chord);
                                    tt.faceTicks -= TDuration(dtFace).fraction();
                                }
                                seg->remove(chord);
                                delete chord;
                                chord = nullptr;
                                if (savedPrevMidiTick >= 0) {
                                    prevMidiTick[trackKey] = savedPrevMidiTick;
                                } else {
                                    prevMidiTick.erase(trackKey);
                                }
                                continue;  // skip note add, ties, articulations
                            }
                            if (chord) {
                                if (tt.inTuplet()) {
                                    chord->setTuplet(nullptr);
                                    tt.currentTuplet->remove(chord);
                                    tt.faceTicks -= TDuration(dtFace).fraction(); // undo face contribution
                                }
                                // Always update the chord duration to match the capped
                                // advance: otherwise chord->actualTicks() (still the face
                                // value set at creation) exceeds the advance applied to
                                // cumTick, and the difference shows up as a sanityCheck
                                // overshoot (e.g. chord ticks=1/16, advance capped to 1/32
                                // produces a 1/32 voice overrun per such note).
                                TDuration cappedDur(advance);
                                chord->setDurationType(cappedDur);
                                chord->setTicks(cappedDur.fraction());
                                chord->setDots(0);
                            }
                        }
                        cumTick[trackKey] += advance;
                        if (tt.inTuplet()) {
                            tt.placedTicks += advance;
                        }
                    }
                }

                // Drain pending grace chords onto this main chord. Done for both the
                // new-chord and reused-chord paths so a grace queued just before this
                // note always gets attached at the first opportunity.
                {
                    auto& pg = pendingGraces[trackKey];
                    for (Chord* gc : pg) {
                        chord->add(gc);     // sets parent, inserts into m_graceNotes
                    }
                    pg.clear();
                }

                Note* note = Factory::createNote(chord);
                note->setPitch(en->semiTonePitch);
                note->setTpcFromPitch();
                chord->add(note);

                // Complete pending tie: if a prior note of same (staffIdx, voice, pitch)
                // was a tie-start, create the Tie object from that note to this one.
                {
                    auto tieKey = std::make_tuple(staffIdx, voice, (int)en->semiTonePitch);
                    auto it = pendingTieNote.find(tieKey);
                    if (it != pendingTieNote.end()) {
                        Note* startNote = it->second;
                        Tie* tie = Factory::createTie(startNote);
                        tie->setStartNote(startNote);
                        tie->setEndNote(note);
                        tie->setTrack(startNote->track());
                        startNote->add(tie);
                        pendingTieNote.erase(it);
                    }
                }

                // Articulations
                if (en->articulationUp == 0x20 || en->articulationDown == 0x20) {
                    Articulation* art = Factory::createArticulation(chord);
                    art->setSymId(SymId::fermataAbove);
                    chord->add(art);
                }

                // Register tie-start if a TIE element exists at this note's position, OR
                // (v0xC2 only) if the note's grace1 lower nibble == 1, which marks it as a
                // tie-sender in Encore's live-recording encoding.  The TIE element at the
                // chord root (e.g. tick=141) is within isTieStart's ±3-tick window for notes
                // at ticks 141–144, but not for notes at ticks 145–146 (gap=4–5).  The g1low
                // indicator fills this gap for those outlying chord members.
                {
                    bool hasTieStart = isTieStart(staffIdx, voice, (int)e->tick)
                                       || (enc.header.isOldFormat()
                                           && (en->grace1 & 0x0F) == 1);
                    if (hasTieStart) {
                        pendingTieNote[{ staffIdx, voice, (int)en->semiTonePitch }] = note;
                    }
                }
            }

            // -- Rests --
            if (et == EncElemType::REST) {
                const EncRest* er = static_cast<const EncRest*>(e);
                quint8 safeFvR = er->faceValue & 0x0F;
                if (safeFvR == 0 || safeFvR > 8) {
                    continue;
                }
                if (er->realDuration > 0 && er->realDuration < 15) {
                    continue;
                }
                DurationType dt = realDuration2DurationType(er->realDuration, er->faceValue);
                // Use dotControl (actual sounding duration stored by Encore) for dot count.
                // realDuration comes from MIDI tick spacing and may be wrong due to timing drift.
                qint16 durForDots = (er->dotControl > 0)
                                    ? static_cast<qint16>(er->dotControl)
                                    : er->realDuration;
                int dots = calcDots(durForDots, er->faceValue);
                // Cap rest duration to remaining measure space (rests are rarely in tuplets).
                // Cap when not currently in a tuplet, OR when the current group is full
                // (it will be closed before this rest is processed, so the rest is plain).
                {
                    const auto& ttPre = tuplets[trackKey];
                    if (!ttPre.inTuplet() || ttPre.groupFull()) {
                        Fraction remaining = measure->ticks() - cumTick[trackKey];
                        TDuration fullDur(dt);
                        fullDur.setDots(dots);
                        if (remaining > Fraction(0, 1) && fullDur.fraction() > remaining) {
                            TDuration capped(remaining, true);
                            dt   = capped.type();
                            dots = capped.dots();
                        }
                    }
                }

                Segment* seg = measure->getSegment(SegmentType::ChordRest, elemTick);
                if (!seg->element(track)) {
                    TDuration dur(dt);
                    dur.setDots(dots);
                    Rest* rest = Factory::createRest(seg, dur);
                    rest->setTrack(track);
                    rest->setTicks(dur.fraction());
                    rest->setDots(dots);
                    seg->add(rest);

                    auto& tt = tuplets[trackKey];
                    int actualNr = er->actualNotes();
                    int normalNr = er->normalNotes();
                    bool isStdExplicitR = (actualNr == 3 && normalNr == 2)
                                          || (actualNr == 5 && normalNr == 4)
                                          || (actualNr == 6 && normalNr == 4);
                    if (!isStdExplicitR) {
                        actualNr = 0;
                        normalNr = 0;
                    }
                    if (actualNr == 0 && enc.header.isOldFormat() && (er->faceValue & 0x0F) >= 4
                        && ((tt.inTuplet() && !tt.groupFull()) || impliedGroupMember.count(e))) {
                        actualNr = detectImpliedTuplet(er->realDuration, er->faceValue, normalNr);
                    }
                    if (actualNr > 0 && normalNr > 0) {
                        if (tt.groupFull()) {
                            tt.closeTuplet();
                        }
                        if (!tt.inTuplet()) {
                            if (isStdExplicitR && !validTupletGroupMember.count(e)) {
                                Fraction tupAdv = TDuration(dt).fraction()
                                                  * Fraction(normalNr, actualNr);
                                Fraction remaining = measure->ticks() - cumTick[trackKey];
                                if (tupAdv == remaining) {
                                    tt.startTuplet(measure, elemTick, actualNr, normalNr, dt, track);
                                } else {
                                    actualNr = 0;
                                    normalNr = 0;
                                }
                            } else {
                                tt.startTuplet(measure, elemTick, actualNr, normalNr, dt, track);
                            }
                        }
                    }
                    if (actualNr > 0 && normalNr > 0) {
                        rest->setTuplet(tt.currentTuplet);
                        tt.currentTuplet->add(rest);

                        tt.faceTicks += TDuration(dt).fraction();
                    } else {
                        if (tt.groupFull()) {
                            tt.closeTuplet();
                        }
                        if (tt.inTuplet()) {
                            tt.closeTuplet();
                        }
                    }

                    // Advance cumulative position. Mirror the chord path: when the cap
                    // shortens advance, also update the rest's ticks so that
                    // cr->actualTicks() matches the actual cumTick advance. Without this
                    // the rest claims its uncapped face value (e.g. 1/4) while cumTick
                    // moved by less, producing a sanityCheck overshoot of the difference.
                    Fraction advance = tt.inTuplet()
                                       ? TDuration(dt).fraction() * Fraction(tt.normalN, tt.actualN)
                                       : dottedAdvance(dt, dots);
                    Fraction remaining = measure->ticks() - cumTick[trackKey];
                    if (advance > remaining && remaining > Fraction(0, 1)) {
                        advance = TDuration(remaining, true).fraction();
                        if (advance.numerator() == 0) {
                            // Remaining too small to fit any standard duration: drop the
                            // rest we just placed rather than leave a zero-tick element.
                            if (tt.inTuplet()) {
                                rest->setTuplet(nullptr);
                                tt.currentTuplet->remove(rest);
                                tt.faceTicks -= TDuration(dt).fraction();
                            }
                            seg->remove(rest);
                            delete rest;
                            if (savedPrevMidiTick >= 0) {
                                prevMidiTick[trackKey] = savedPrevMidiTick;
                            } else {
                                prevMidiTick.erase(trackKey);
                            }
                            continue;
                        }
                        if (tt.inTuplet()) {
                            rest->setTuplet(nullptr);
                            tt.currentTuplet->remove(rest);
                            tt.faceTicks -= TDuration(dt).fraction();
                        }
                        TDuration cappedDur(advance);
                        rest->setDurationType(cappedDur);
                        rest->setTicks(cappedDur.fraction());
                        rest->setDots(0);
                    }
                    cumTick[trackKey] += advance;
                    if (tt.inTuplet()) {
                        tt.placedTicks += advance;
                    }
                }
            }

            // -- Chord symbols --
            if (et == EncElemType::CHORD) {
                const EncChordSym* ec = static_cast<const EncChordSym*>(e);
                if (!ec->teksto.isEmpty()) {
                    Segment* seg = measure->getSegment(SegmentType::ChordRest, elemTick);
                    Harmony* h = Factory::createHarmony(score->dummy()->segment());
                    h->setTrack(track);
                    h->setHarmony(String(ec->teksto));
                    seg->add(h);
                }
            }

            // -- Ornaments (slurs, hairpins, tempo, staff text) --
            if (et == EncElemType::ORNAMENT) {
                const EncOrnament* eo = static_cast<const EncOrnament*>(e);
                auto key = std::make_pair(staffIdx, static_cast<int>(eo->xoffset));

                switch (eo->ornType()) {
                case EncOrnamentType::SLURSTART:
                case EncOrnamentType::SLURSTOP:
                    // TODO: slur import disabled until reliable endpoint matching is implemented.
                    // Corrupted Encore files can create slurs with invalid endpoints causing NaN.
                    break;
                case EncOrnamentType::WEDGESTART: {
                    Hairpin* hp = Factory::createHairpin(score->dummy()->segment());
                    hp->setTrack(track);
                    hp->setTick(elemTick);
                    // speguleco: 0=crescendo, other=diminuendo (from enc2ly)
                    hp->setHairpinType(eo->speguleco == 0
                                       ? HairpinType::CRESC_HAIRPIN
                                       : HairpinType::DIM_HAIRPIN);
                    score->addElement(hp);
                    pendingHairpins[key] = hp;
                    break;
                }
                case EncOrnamentType::WEDGESTOP: {
                    auto it = pendingHairpins.find(key);
                    if (it != pendingHairpins.end()) {
                        if (elemTick > it->second->tick()) {
                            it->second->setTrack2(track);
                            it->second->setTick2(elemTick);
                        } else {
                            // Zero or negative span: stop tick not after start.
                            // Drop the hairpin rather than asserting in setTicks().
                            score->removeElement(it->second);
                            delete it->second;
                        }
                        pendingHairpins.erase(it);
                    }
                    break;
                }
                case EncOrnamentType::TEMPO: {
                    if (eo->tempo > 0) {
                        Segment* seg = measure->getSegment(SegmentType::ChordRest, elemTick);
                        if (!seg) {
                            seg = measure->getSegment(SegmentType::ChordRest, measTick);
                        }
                        TempoText* tt2 = Factory::createTempoText(seg);
                        tt2->setTrack(track);
                        double bps = eo->tempo / 60.0;
                        tt2->setTempo(BeatsPerSecond(bps));
                        tt2->setXmlText(String(u"♩ = %1").arg(eo->tempo));
                        seg->add(tt2);
                        score->setTempo(elemTick, BeatsPerSecond(bps));
                    }
                    break;
                }
                case EncOrnamentType::STAFFTEXT:
                    // not yet implemented
                    break;
                default:
                    break;
                }
            }

            // -- Key changes --
            if (et == EncElemType::KEYCHANGE) {
                const EncKeyChange* ekc = static_cast<const EncKeyChange*>(e);
                if (ekc->tipo != 0) {
                    Key newKey = Key(encKeyToFifths(ekc->tipo));
                    Staff* staff = score->staff(staffIdx);
                    if (!staff) {
                        continue;
                    }
                    KeySigEvent ke;
                    ke.setConcertKey(newKey);
                    ke.setKey(newKey);
                    staff->setKey(elemTick, ke);
                    Segment* seg = measure->getSegment(SegmentType::KeySig, elemTick);
                    KeySig* ks = Factory::createKeySig(seg);
                    ks->setTrack(track);
                    ks->setKey(newKey, newKey);
                    seg->add(ks);
                }
            }
        }

        // Before checkMeasure: correct ticks on any tuplets still open at measure end
        // whose actual placed content differs from the default baseLen*normalN.
        // checkMeasure uses tuplet->ticks() via skipTuplet(); an incorrect value
        // inserts fill rests at wrong positions or misses gaps.
        // beam.cpp uses TDuration(ticks, true) so non-standard fractions are safe.
        for (auto& [key, tt] : tuplets) {
            if (tt.inTuplet() && tt.placedTicks > Fraction(0, 1)) {
                const Fraction expected = TDuration(tt.currentTuplet->baseLen()).fraction()
                                          * tt.currentTuplet->ratio().denominator();
                const bool mixedOvershoot = (tt.placedTicks > expected)
                                            && (tt.faceTicks > tt.fullFaceSum);
                if (tt.placedTicks < expected || mixedOvershoot) {
                    tt.currentTuplet->setTicks(tt.placedTicks);
                }
            }
        }

        // Fill any remaining gaps with invisible rests.
        // With faceValue-cumulative placement, notes always land at canonical positions
        // so gaps are only genuine rests not explicitly written in the Encore score.
        for (int si = 0; si < totalStaves; ++si) {
            measure->checkMeasure(static_cast<staff_idx_t>(si));
        }

        // Post-checkMeasure micro-correction: fix tiny over/undershoots (≤ 1/48)
        // that result from non-standard gaps that toRhythmicDurationList cannot
        // fill or match exactly.
        //
        // Overshoot (voiceSum > mLen ≤ 1/48): gap rests from cascade fills went
        //   1/64+1/256+1/1024 = 21/1024 when the actual gap was smaller (or zero).
        //   Remove gap rests smallest-first until voiceSum ≤ mLen.
        //
        // Undershoot (voiceSum < mLen ≤ 1/48): cascade left a sub-standard residual
        //   (e.g. 1/3072 = 1/48 - 21/1024). Add a V_MEASURE gap rest for the exact
        //   deficit (V_MEASURE accepts non-standard ticks without TDuration assertion).
        {
            const Fraction mLen_fix = measure->ticks();
            const Fraction maxDelta(1, 48);
            for (int si = 0; si < totalStaves; ++si) {
                for (voice_idx_t v = 0; v < VOICES; ++v) {
                    track_idx_t tr = static_cast<track_idx_t>(si * VOICES + v);
                    // Compute voice sum
                    Fraction voiceSum(0, 1);
                    bool hasContent = false;
                    std::vector<Rest*> gapRests;
                    for (Segment* seg = measure->first(SegmentType::ChordRest);
                         seg; seg = seg->next(SegmentType::ChordRest)) {
                        EngravingItem* el = seg->element(tr);
                        if (!el) {
                            continue;
                        }
                        hasContent = true;
                        ChordRest* cr = toChordRest(el);
                        voiceSum += cr->actualTicks();
                        if (el->isRest() && toRest(el)->isGap()) {
                            gapRests.push_back(toRest(el));
                        }
                    }
                    if (!hasContent) {
                        continue;
                    }

                    // Overshoot: remove gap rests smallest-first
                    if (voiceSum > mLen_fix && (voiceSum - mLen_fix) <= maxDelta) {
                        std::stable_sort(gapRests.begin(), gapRests.end(),
                                         [](Rest* a, Rest* b){
                            return a->actualTicks() < b->actualTicks();
                        });
                        for (Rest* gr : gapRests) {
                            if (voiceSum <= mLen_fix) {
                                break;
                            }
                            Fraction at = gr->actualTicks();
                            voiceSum -= at;
                            Segment* gseg = gr->segment();
                            gseg->remove(gr);
                            delete gr;
                        }
                    }

                    // Undershoot: add exact V_MEASURE gap rest for residual
                    const Fraction deficit = mLen_fix - voiceSum;
                    if (deficit > Fraction(0, 1) && deficit <= maxDelta) {
                        const Fraction fillTick = measure->tick() + voiceSum;
                        Segment* fillSeg = measure->getSegment(
                            SegmentType::ChordRest, fillTick);
                        if (!fillSeg->element(tr)) {
                            Rest* r = Factory::createRest(
                                fillSeg, TDuration(DurationType::V_MEASURE));
                            r->setTicks(deficit);
                            r->setTrack(tr);
                            r->setGap(true);
                            fillSeg->add(r);
                        }
                    }
                }
            }
        }

        ++measIdx;
    }

    // Discard any grace chords still queued after the final measure (no main chord
    // followed them). They have no parent in the score tree, so delete them here.
    for (auto& [key, vec] : pendingGraces) {
        for (Chord* gc : vec) {
            LOGW() << "Encore import: discarding dangling grace chord at end of score"
                   << " (staff " << key.first << ", voice " << key.second << ")";
            delete gc;
        }
    }
    pendingGraces.clear();

    // Remove open-ended slurs/hairpins (no matching STOP ornament).
    for (auto& [key, slur] : pendingSlurs) {
        score->removeElement(slur);
    }
    for (auto& [key, hp] : pendingHairpins) {
        score->removeElement(hp);
    }

    // Remove slurs whose start or end note doesn't exist (corrupted files).
    // Such slurs cause NaN in Bezier layout.
    {
        std::vector<Spanner*> toRemove;
        for (auto& [tick, spanner] : score->spannerMap().map()) {
            if (spanner->isSlur()) {
                spanner->computeStartElement();
                spanner->computeEndElement();
                if (!spanner->startElement() || !spanner->endElement()) {
                    toRemove.push_back(spanner);
                }
            }
        }
        for (Spanner* sp : toRemove) {
            score->removeElement(sp);
        }
    }

    score->spell();
    addTitleFrame(score, enc.titleBlock);
    score->setUpTempoMap();
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

Err importEncore(MasterScore* score, const QString& path)
{
    if (!QFileInfo::exists(path)) {
        return Err::FileNotFound;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return Err::FileOpenError;
    }

    // Check for the legacy ZBOT format (Encore 4; uses a different on-disk
    // layout and is not handled by this importer).  See discussion in
    // musescore/MuseScore#24341.
    {
        QByteArray magic4 = file.read(4);
        file.seek(0);
        if (magic4 == "ZBOT") {
            LOGW("Encore: ZBOT format (Encore 4) is not supported. "
                 "Please re-save the file using Encore 5 to convert it first.");
            return Err::FileBadFormat;
        }
    }

    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::LittleEndian);

    EncFile enc;
    if (!enc.read(ds)) {
        return Err::FileBadFormat;
    }

    if (enc.instruments.empty() || enc.measures.empty()) {
        return Err::FileBadFormat;
    }

    buildScore(score, enc);

    muse::Ret integrity = score->sanityCheck();
    if (!integrity) {
        LOGW() << "Encore import: score corruption detected:\n" << integrity.text();
    }

    return Err::NoError;
}
} // namespace mu::iex::encore
