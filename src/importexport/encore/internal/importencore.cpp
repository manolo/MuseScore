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
// Encore packs the note/rest duration in the low nibble of faceValue: 1=whole,
// 2=half ... 8=256th. Values 0 and 9..15 are invalid; the high nibble carries
// unrelated flags.
static inline bool isValidFaceValue(quint8 faceValue)
{
    const quint8 fv = faceValue & 0x0F;
    return fv > 0 && fv <= 8;
}

static inline void applyConcertPitch(Note* n, int semitone)
{
    n->setPitch(semitone);
    n->setTpcFromPitch();
}

static void buildScore(MasterScore* score, const EncFile& enc)
{
    // Initialize chord list for harmony parsing
    score->style().set(Sid::chordsXmlFile, true);
    score->chordList()->read(u"chords.xml");

    // --------------- Setup: parts and staves ---------------
    // Per-staff chromatic offset to apply when calling Note::setPitch().
    // Encore stores the WRITTEN pitch in EncNote::semiTonePitch and plays
    // it shifted by the per-instrument "Key" field from the Staff Sheet
    // (signed semitones in the .enc binary; see EncInstrument::
    // keyTransposeSemitones). MuseScore plays at m_pitch directly, so the
    // importer adds that offset on every note placement to keep playback
    // aligned with the source.
    std::vector<int> staffPitchOffset;
    // Per-staff template clefs (concert + transposing). Encore stores plain
    // G / F clefs for many octave-transposing instruments while the
    // MuseScore template carries an octave-bassa concert clef and a plain
    // transposing clef (bass-guitar: F8_VB / F). pickStaffClef below picks
    // the appropriate one when the Encore Key matches the concert clef's
    // octave decoration; otherwise the Encore clef wins.
    // ClefType::INVALID marks staves with no matched template.
    std::vector<ClefType> staffTemplateConcertClef;
    std::vector<ClefType> staffTemplateTransposingClef;
    int totalStaves = 0;
    for (const auto& instr : enc.instruments) {
        int ns = instr.nstaves > 0 ? instr.nstaves : 1;
        Part* part = new Part(score);

        // Find the best matching instrument template.
        // Strategy: combined name + MIDI score first, then a drum-kit shortcut
        // for percussion tracks, then a pure MIDI lookup (v0xC4 only).
        // When the instrument name is too short to be reliable (e.g. "S",
        // "C", "T", "B" used as SATB choir labels) the name match returns
        // nullptr and the MIDI program field is usually unreliable too on
        // such files (compact TK blocks invalidate the PRG_BASE + n*PRG_STEP
        // formula). Skip both name-keyword and MIDI fallbacks in that case
        // and let the chain fall through to Grand Piano so the user can
        // pick the right voice from the instrument browser afterwards.
        const int encMidi = instr.midiProgram > 0 ? instr.midiProgram - 1 : -1;
        const bool nameTooShort = instr.name.trimmed().size() < 4;
        const InstrumentTemplate* tmpl = findEncoreInstrumentTemplate(instr.name, encMidi);
        if (!tmpl && !nameTooShort) {
            // Encore percussion tracks usually leave midiProgram at 1 (Grand
            // Piano), which makes the MIDI fallback below pick the wrong
            // instrument (and the wrong playback sound). Recognise the name
            // and route to a drum-kit template directly.
            const QString lname = instr.name.toLower();
            if (lname.contains(QStringLiteral("percus"))
                || lname.contains(QStringLiteral("drum"))
                || lname.contains(QStringLiteral("bater"))) {
                tmpl = searchTemplate(String(u"drumset"));
            }
        }
        if (!tmpl && !nameTooShort && instr.midiProgram > 0) {
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

        const int pitchOffset = static_cast<int>(instr.keyTransposeSemitones);
        for (int s = 0; s < ns; ++s) {
            Staff* staff = Factory::createStaff(part);
            score->appendStaff(staff);
            staffPitchOffset.push_back(pitchOffset);
            ClefType cClef = ClefType::INVALID;
            ClefType tClef = ClefType::INVALID;
            if (tmpl) {
                const ClefTypeList ctl = tmpl->clefType(static_cast<staff_idx_t>(s));
                cClef = ctl.concertClef;
                tClef = ctl.transposingClef;
            }
            staffTemplateConcertClef.push_back(cClef);
            staffTemplateTransposingClef.push_back(tClef);
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
            for (int s = 0; s < totalStaves; ++s) {
                measure->setEndBarLineType(type, static_cast<track_idx_t>(s) * VOICES);
            }
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
            const ClefType cClef = si < static_cast<int>(staffTemplateConcertClef.size())
                                   ? staffTemplateConcertClef[si] : ClefType::INVALID;
            const ClefType tClef = si < static_cast<int>(staffTemplateTransposingClef.size())
                                   ? staffTemplateTransposingClef[si] : ClefType::INVALID;
            const int keyOffset = si < static_cast<int>(staffPitchOffset.size())
                                  ? staffPitchOffset[si] : 0;
            addInitialClef(score, si, pickStaffClef(sd.clef, cClef, tClef, keyOffset));
        }
    }

    // --------------- Notes, rests, ornaments, chord symbols ---------------
    // Index of Measure pointers by 0-based measure index. Used to resolve a
    // hairpin's end measure (current measIdx + EncOrnament::alMezuro) without
    // re-walking the score.
    std::vector<Measure*> measuresByIdx;
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (mb->isMeasure()) {
            measuresByIdx.push_back(toMeasure(mb));
        }
    }

    // SLURSTART intents collected during the measure pass, resolved after the
    // pass when destination measures are populated. Encore .enc binaries do
    // not emit SLURSTOP markers; the end measure offset lives in alMezuro and
    // we anchor the slur on the last ChordRest in the target measure on this
    // track (an approximation, since xoffset2 is a layout x and not a tick).
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

    // WEDGESTART intents collected during the measure pass, resolved after the
    // pass so the hairpin can end at the next Dynamic on the same track
    // (Encore renders a `mf<f>mf` chain with each hairpin terminating
    // exactly at the next dynamic glyph, not at the end of its alMezuro
    // target measure). Without the post-pass two adjacent hairpins
    // overlap because both extend to the bar line.
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

    // ARPEGGIO intents collected during the measure pass. The ORN element
    // comes before the chord's notes in MEAS order, so the chord doesn't
    // exist yet at the time the ornament is parsed -- the attachment is
    // deferred to a post-measure pass.
    struct PendingArpeggio {
        Fraction tick;
        track_idx_t track;
    };
    std::vector<PendingArpeggio> pendingArpeggios;

    // ORN-based single-chord tremolos (tipo 0xAF / 0xEF). The ORN element
    // precedes or follows the chord in MEAS order, so attachment is deferred.
    // Tick may be at or past the measure's durTicks when Encore places the
    // tremolo at the end of a tied or long note; the post-pass falls back to
    // the latest chord before that tick.
    struct PendingOrnTremolo {
        Fraction tick;
        Fraction measTick;
        int staffIdx;
        int msVoice;
        TremoloType tremType;
    };
    std::vector<PendingOrnTremolo> pendingOrnTremolos;

    // Chord-level Trill ornaments (ORN tipo 0x35/0x36/0x37). Same pattern
    // as PendingArpeggio: the chord does not exist yet when the ornament
    // is parsed, so the attachment is deferred to a post-measure pass.
    struct PendingTrill {
        Fraction tick;
        track_idx_t track;
    };
    std::vector<PendingTrill> pendingTrills;

    // Chord-level Staccato markers (ORN tipo=0xC9). Same deferred pattern
    // as ARPEGGIO/TRILL: Encore writes the ORN before the chord notes in
    // MEAS order so the chord doesn't exist yet at parse time.
    struct PendingStaccato {
        Fraction tick;
        track_idx_t track;
    };
    std::vector<PendingStaccato> pendingStaccatos;

    // Section markers (Segno / Coda). Stored as size-16 ORN tipos 0xA2
    // and 0xA6. Encore attaches them to the measure (not a chord), so
    // the importer adds a Marker on the measure containing the ORN tick.
    struct PendingMarker {
        Fraction tick;
        MarkerType type;
    };
    std::vector<PendingMarker> pendingMarkers;

    // Active Volta being extended across consecutive measures that share the
    // same repeatAlternative bitmask. Encore stores the alt bits on every
    // measure inside the ending (e.g. m16=0x01, m17=0x01, m18=0x02), not just
    // the first one. We coalesce equal-bitmask runs into one Volta spanning
    // their combined tick range.
    Volta* activeVolta = nullptr;
    quint8 activeVoltaBits = 0;

    // Tuplet state per (staffIdx, msVoice) — keyed by MuseScore voice/track
    std::map<std::pair<int, int>, TupletTracker> tuplets;

    // Pending tie-start notes: key = (staffIdx, voice, pitch), value = Note* to tie FROM.
    // Populated when a TIE element exists at a note's position; cleared when the
    // tie-end note (same staffIdx, voice, pitch) is placed.  Persists across measures
    // to handle ties across barlines.
    std::map<std::tuple<int, int, int>, Note*> pendingTieNote;

    // Lyric syllables queued for attachment to ChordRest segments on the
    // same track. Each entry carries the syllable's raw Encore tick so we
    // can anchor it to the closest chord at the end of the measure pass
    // (a queue position would otherwise shift everything by the count of
    // separator elements present in the binary).
    struct PendingLyric {
        int encTick;        // raw Encore tick within the current measure
        String text;        // the syllable text (separators are pre-filtered)
        bool hyphenBefore;  // a "-" LYRIC element preceded this syllable
        bool hyphenAfter;   // a "-" LYRIC element follows this syllable
    };
    std::map<track_idx_t, std::vector<PendingLyric> > pendingLyrics;
    // Tracks whether the next syllable enqueued on each track follows a
    // hyphen separator. Reset by either a syllable consuming the flag, an
    // empty-string LYRIC (word break in Encore), or a measure boundary.
    std::map<track_idx_t, bool> nextLyricHyphenBefore;

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

    // v0xA6 inner-grace tracking: when a leading grace (g1=0x20) is placed
    // its faceValue is stored here. Inner graces (g1=0x10) with a higher fv
    // (= shorter note) are then classified as graces too. Cleared when the
    // grace queue is flushed (non-grace chord follows).
    std::map<std::pair<int, int>, quint8> v0xA6LeadingGraceFv;

    // Total face ticks of the grace group most recently flushed on each
    // trackKey. After the flush the face-grid snap also checks this so
    // notes whose Encore tick is offset by exactly the grace duration do
    // not produce a spurious gap rest (boda.enc m87: grace 32nd borrowed
    // 30 ticks, shifting note4 onto the 30-tick grid but leaving a
    // 30-tick gap in cumTick).
    std::map<std::pair<int, int>, int> v0xA6GraceStolenTicks;

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

        // closeTupletWithFill: like TupletTracker::closeTuplet, but for a
        // partial group whose placedTicks does not fit a TDuration (e.g. 2 of
        // 3 notes of a 3:2 triplet -> 1/12 actual), first synthesizes
        // invisible rests inside the tuplet so the chord-sum equals canonical
        // (baseLen * normalN). Without this, sanityCheck reports the missing
        // slot as an incomplete measure, and Beam::calcBeamBreaks asserts on
        // a non-TDuration tuplet ticks value. Uses the surrounding scope for
        // measure / cumTick / score access.
        auto closeTupletWithFill = [&](TupletTracker& tt,
                                       std::pair<int, int> trackKey) {
            if (!tt.inTuplet() || tt.placedTicks <= Fraction(0, 1)) {
                tt.closeTuplet();
                return;
            }
            const Fraction expectedTup = TDuration(tt.currentTuplet->baseLen()).fraction()
                                         * tt.currentTuplet->ratio().denominator();
            TDuration snap(tt.placedTicks, true /*truncate*/);
            const bool fitsTD = snap.isValid()
                                && snap.fraction() == tt.placedTicks;
            if (tt.placedTicks < expectedTup && !fitsTD) {
                // Two cases produce placedTicks < expected with a non-
                // TDuration value:
                //   - Strictly partial: fewer than actualN notes placed
                //     (e.g. 2 of 3 in a 3:2 quarter triplet). Fill the
                //     remaining slots with invisible rests so the tuplet's
                //     chord-sum reaches canonical and beam layout stays
                //     TDuration-safe.
                //   - Mixed-value: actualN members placed but with a shorter
                //     face duration than baseLen (e.g. quarter rest + eighth
                //     + eighth in a 3:2 quarter triplet, face sum 1/2 vs
                //     fullFaceSum 3/4). The tuplet is structurally complete;
                //     filling more rests would exceed actualN. The remaining
                //     content gap is left for the post-checkMeasure micro-
                //     correction (maxDelta covers up to a triplet 16th = 1/24).
                if (static_cast<int>(tt.currentTuplet->elements().size())
                    < tt.actualN) {
                    track_idx_t trk = static_cast<track_idx_t>(trackKey.first) * VOICES
                                      + trackKey.second;
                    DurationType baseLen = tt.currentTuplet->baseLen().type();
                    Fraction perNote = TDuration(baseLen).fraction()
                                       * Fraction(tt.normalN, tt.actualN);
                    int safety = tt.actualN + 1;
                    while (tt.placedTicks < expectedTup && safety-- > 0
                           && static_cast<int>(tt.currentTuplet->elements().size())
                           < tt.actualN
                           && cumTick[trackKey] + perNote <= measure->ticks()) {
                        Fraction restTick = measure->tick() + cumTick[trackKey];
                        Segment* seg = measure->getSegment(SegmentType::ChordRest, restTick);
                        if (seg->element(trk)) {
                            break;
                        }
                        TDuration dur(baseLen);
                        Rest* rest = Factory::createRest(seg, dur);
                        rest->setTrack(trk);
                        // setTicks takes the FACE duration; globalTicks() (the
                        // tuplet-scaled actualTicks) is computed as
                        // m_duration / tuplet->ratio() at read time.
                        rest->setTicks(dur.fraction());
                        rest->setVisible(false);
                        rest->setTuplet(tt.currentTuplet);
                        tt.currentTuplet->add(rest);
                        seg->add(rest);
                        tt.placedTicks += perNote;
                        cumTick[trackKey] += perNote;
                    }
                }
            }
            tt.closeTuplet();
        };

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
        v0xA6GraceStolenTicks.clear();

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

        // Volta (first/second endings) from repeatAlternative bitmask. Encore
        // tags every measure inside the ending with the bitmask; runs of equal
        // bitmask collapse into a single Volta that spans them.
        if (encMeas.repeatAlternative != 0) {
            if (activeVolta && activeVoltaBits == encMeas.repeatAlternative) {
                // Extend the open Volta to cover the current measure.
                activeVolta->setTick2(measTick + measure->ticks());
            } else {
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
                // Set the visible label ("1.", "2.", "1., 2.", ...) so the
                // number shows above the bracket. Without it the bracket
                // renders blank even though setEndings is correct semantically.
                String voltaText;
                for (int number : endings) {
                    if (!voltaText.empty()) {
                        voltaText += u", ";
                    }
                    voltaText += String::number(number);
                }
                voltaText += u".";
                volta->setText(voltaText);
                score->addElement(volta);
                activeVolta = volta;
                activeVoltaBits = encMeas.repeatAlternative;
            }
        } else {
            activeVolta = nullptr;
            activeVoltaBits = 0;
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

            // Skip notes/rests strictly at or beyond measure end (they would
            // write past the bar). Ornaments at `tick == durTicks` mark the
            // end-of-measure boundary (e.g. a WEDGESTART that visually starts
            // on the bar line); they belong to the current measure and must
            // not be discarded, otherwise hairpins like the f -> p diminuendo
            // that spans m1 -> m3 in Beethoven Plectro disappear silently.
            if ((et == EncElemType::NOTE || et == EncElemType::REST)
                && e->tick >= encMeas.durTicks) {
                continue;
            }
            if (et == EncElemType::ORNAMENT && e->tick > encMeas.durTicks) {
                // Dynamics and STAFFTEXT ornaments stored at a tick past the
                // measure end are Encore's "end-of-measure" markers (observed
                // in sirena.enc m21: a 2/4 measure with the 1st-volta "pp"
                // dynamic + "la 2ª" stafftext at tick=960 instead of 480).
                // Let them through; the per-case clamp later attaches them
                // to the last existing segment of the current measure.
                const EncOrnament* eoFilt = static_cast<const EncOrnament*>(e);
                const EncOrnamentType ot = eoFilt->ornType();
                const bool isDyn = (ot >= EncOrnamentType::DYN_PPP
                                    && ot <= EncOrnamentType::DYN_FP)
                                   || ot == EncOrnamentType::DYN_FZ
                                   || ot == EncOrnamentType::DYN_SF;
                const bool isText = (ot == EncOrnamentType::STAFFTEXT);
                if (!isDyn && !isText) {
                    continue;
                }
            }

            int staffIdx = static_cast<int>(e->staffIdx);
            int voice    = static_cast<int>(e->voice);

            if (staffIdx >= totalStaves) {
                continue;
            }
            // Encore stores system-level ornaments (dynamics, mordents,
            // tremolos, technical markings, ...) with voice=4 (one slot
            // above the four real voices). MuseScore has no voice 4, but
            // these marks anchor visually to voice 0 of the same staff.
            //
            // Some Encore files also write regular NOTE / REST / BEAM
            // elements with voice=4 on a specific staff (observed on the
            // bass staff of a 4-voice mixed choral file where 176 of the
            // 184 bass notes carry voice=4 while the matching LYRIC
            // elements stay on voice=0). The notes are real content and
            // belong on voice 0 of that staff so the LYRIC anchoring
            // (which walks voice-0 chord segments) can pick them up;
            // otherwise the bass staff imports empty. Map every out-of-
            // range voice down to 0 and let the multi-stream / chord-
            // extension machinery handle any conflict with existing
            // voice-0 content the same way it does for normal voices.
            if (voice >= static_cast<int>(VOICES)) {
                voice = 0;
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
                    // Honor implicit gaps encoded by Encore's absolute tick: when
                    // the binary places a NOTE/REST significantly later than the
                    // cumulative sum of face-value advances AND the tick lands on
                    // the user's notation grid (a multiple of the element's face
                    // value in Encore ticks), snap cumTick to the Encore tick.
                    //
                    // Encore does NOT always emit explicit REST elements for
                    // leading or interior silences (e.g. a 3/4 measure encoded
                    // as two NOTE elements at ticks 240 and 480, with the
                    // leading quarter rest implicit). Without the snap, those
                    // notes get squashed to beats 1-2 instead of 2-3 and the
                    // trailing rest moves to the end, altering the timing.
                    //
                    // The face-grid gate avoids interfering with live-recorded
                    // files where every NOTE drifts by a few ticks from its
                    // notated position and the cumTick approach is what aligns
                    // the playback grid. Live recordings carry sub-grid ticks
                    // like 41, 105, 199 that never match `tick % faceTicks ==
                    // 0`, so the snap stays off and the cumulative-face
                    // placement keeps the multi-stream and implied-tuplet
                    // bookkeeping intact.
                    //
                    // The 8-Encore-tick threshold (= CHORD_MIDI_THRESHOLD)
                    // ignores micro-drift inside a chord cluster while still
                    // catching any intentional silence: the smallest non-
                    // degenerate face-value advance is the 64th (15 Encore
                    // ticks), so a real grid-aligned silence is always strictly
                    // above the threshold.
                    if (isNoteOrRest) {
                        quint8 elemFv = 0;
                        if (et == EncElemType::NOTE) {
                            elemFv = static_cast<const EncNote*>(e)->faceValue;
                        } else if (et == EncElemType::REST) {
                            elemFv = static_cast<const EncRest*>(e)->faceValue;
                        }
                        const int faceTicks = faceValue2ticks(elemFv);
                        const bool onFaceGrid = faceTicks > 0
                                                && ((int)e->tick % faceTicks) == 0;
                        // Suppress the gap-snap when a grace note is queued.
                        // In v0xA6, grace notes occupy real tick positions that
                        // push subsequent notes forward; their ticks often land
                        // on the face grid and would otherwise trigger a gap rest
                        // equal to the grace's face value. The grace is visual
                        // only -- no real time passes -- so cumTick should NOT
                        // advance here.
                        const bool gracePending = !pendingGraces[trackKey].empty();
                        // Also suppress when the gap matches stolen grace ticks:
                        // after a grace group flushes onto the first regular note,
                        // the NEXT regular note may be on the face grid with a gap
                        // equal to the grace duration. Without this, the snap
                        // creates a spurious rest = grace face value (boda.enc m87:
                        // 32nd grace steals 30 ticks, note4 (16th at tick=180) is
                        // on the 30-tick grid, snap fires → Rest 32nd inserted).
                        const int stolenTicks = v0xA6GraceStolenTicks.count(trackKey)
                                                ? v0xA6GraceStolenTicks.at(trackKey) : 0;
                        if (onFaceGrid && !gracePending) {
                            // wholeTicks = ticks per whole note. Encore stores
                            // beatTicks per time-signature beat (= 240 for x/4,
                            // 120 for x/8, etc.), so wholeTicks = beatTicks *
                            // timeSigDen. Using `4 * beatTicks` only works when
                            // the denominator is 4; in x/8 it returned half the
                            // correct value and the snap pushed cumTick well
                            // beyond the measure end (regression on v0xA6 3/8
                            // and 6/8 scores). Fall back to 960 (= 240 * 4)
                            // when the header carries no time signature.
                            const int wholeTicks
                                = (encMeas.beatTicks && encMeas.timeSigDen)
                                  ? encMeas.beatTicks * encMeas.timeSigDen
                                  : 960;
                            const Fraction encTickFrac((int)e->tick, wholeTicks);
                            if (encTickFrac > cumTick[trackKey]) {
                                const Fraction gap = encTickFrac - cumTick[trackKey];
                                const int gapEncTicks
                                    = (gap.numerator() * wholeTicks)
                                      / std::max(1, gap.denominator());
                                // Suppress if the gap matches recently stolen grace ticks.
                                // Keep stolenTicks for ALL subsequent notes (don't erase)
                                // so each note displaced by the grace gets the suppression.
                                // The remaining time deficit shows as a trailing rest at
                                // measure end (less disruptive than an inter-note rest).
                                const bool gapIsGraceArtifact
                                    = (stolenTicks > 0 && gapEncTicks <= stolenTicks);
                                if (gapEncTicks > CHORD_MIDI_THRESHOLD && !gapIsGraceArtifact) {
                                    cumTick[trackKey] = encTickFrac;
                                }
                            }
                        }
                    }
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
                    // Grace requires eighth-or-shorter (low nibble >= 4) in addition
                    // to the standard face-value range check.
                    // v0xA6 inner-grace detection: after a leading grace
                    // (g1=0x20 = APPOGGIATURA) Encore places additional
                    // inner grace notes marked with g1=0x10. Inner graces
                    // are shorter (higher fv number) than the leading grace.
                    // Regular notes following the group are longer (lower fv)
                    // and must NOT be classified as graces (boda.enc: m57
                    // has a 64th inner grace after a 32nd leading grace, while
                    // m75 has regular 16ths after a 32nd leading grace).
                    const bool isV0xA6InnerGrace
                        = (en->size == 10)
                          && ((en->grace1 & 0x30) == 0x10)
                          && v0xA6LeadingGraceFv.count(trackKey)
                          && ((en->faceValue & 0x0F) > v0xA6LeadingGraceFv.at(trackKey));
                    if (isValidFaceValue(en->faceValue) && (en->faceValue & 0x0F) >= 4
                        && (en->graceType() != EncGraceType::NORMAL || isV0xA6InnerGrace)) {
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
                        applyConcertPitch(gnote, en->semiTonePitch + staffPitchOffset[staffIdx]);
                        gc->add(gnote);

                        // Articulation on a grace is rare but Encore can encode it.
                        for (quint8 ab : { en->articulationUp, en->articulationDown }) {
                            for (SymId sid : encArticulation2SymIds(ab)) {
                                if (sid == SymId::noSym) {
                                    continue;
                                }
                                Articulation* art = Factory::createArticulation(gc);
                                art->setSymId(sid);
                                gc->add(art);
                            }
                        }

                        pendingGraces[trackKey].push_back(gc);
                        // Track the leading grace fv for inner-grace detection.
                        const quint8 curFv = en->faceValue & 0x0F;
                        auto it = v0xA6LeadingGraceFv.find(trackKey);
                        if (it == v0xA6LeadingGraceFv.end()) {
                            v0xA6LeadingGraceFv[trackKey] = curFv;
                        } else {
                            it->second = std::max(it->second, curFv);
                        }
                        // Accumulate stolen ticks for the post-flush snap guard.
                        v0xA6GraceStolenTicks[trackKey] += faceValue2ticks(curFv);
                        continue;
                    }
                }

                if (!isValidFaceValue(en->faceValue)) {
                    continue;
                }
                const quint8 safeFv = en->faceValue & 0x0F;
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
                            closeTupletWithFill(tt, trackKey);
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
                            closeTupletWithFill(tt, trackKey);
                        }
                        if (tt.inTuplet()) {
                            closeTupletWithFill(tt, trackKey);   // non-tuplet note exits group
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
                        // Insert at the END of the grace-note list so graces
                        // appear in the same left-to-right order they were
                        // queued (= ascending tick order). The default
                        // graceIndex=0 would prepend each one, reversing the
                        // group when there are multiple grace notes.
                        gc->setGraceIndex(chord->graceNotes().size());
                        chord->add(gc);
                    }
                    // stolen ticks accumulated when graces were pushed.
                    pg.clear();
                    v0xA6LeadingGraceFv.erase(trackKey);
                    // DO NOT erase v0xA6GraceStolenTicks yet: the snap guard
                    // for the NEXT regular note needs to read it.
                }

                Note* note = Factory::createNote(chord);
                applyConcertPitch(note, en->semiTonePitch + staffPitchOffset[staffIdx]);
                chord->add(note);

                // Fingering glyphs and the open-string marker travel in the
                // same artic byte slot. Fingerings 1..5 (bytes 0x0D..0x11)
                // become a Fingering text element; 0x46 becomes a
                // Fingering with STRING_NUMBER style and text "0" so the
                // MusicXML exporter emits <open-string/>.
                for (quint8 ab : { en->articulationUp, en->articulationDown }) {
                    int n = encArticByteToFingerNumber(ab);
                    if (n > 0) {
                        Fingering* fg = Factory::createFingering(note);
                        fg->setTrack(track);
                        fg->setXmlText(String::number(n));
                        note->add(fg);
                        break;
                    }
                    if (encArticByteIsOpenString(ab)) {
                        Fingering* fg = Factory::createFingering(
                            note, mu::engraving::TextStyleType::STRING_NUMBER);
                        fg->setTrack(track);
                        fg->setXmlText(u"0");
                        note->add(fg);
                        break;
                    }
                }

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

                // Articulations (fermata, staccato, accent, marcato, tenuto,
                // staccatissimo, up/down bow) and ornaments (trill-mark,
                // mordent, inverted-mordent). Encore packs the glyph index
                // in articulationUp / articulationDown and may pair two
                // glyphs in a single byte. SymIds in the ornament family
                // must be wrapped in an Ornament element (Articulation
                // subclass) so MuseScore's MusicXML export emits them under
                // <ornaments> instead of <articulations>.
                auto isOrnamentSymId = [](SymId s) {
                    return s == SymId::ornamentTrill
                           || s == SymId::ornamentShortTrill
                           || s == SymId::ornamentMordent;
                };
                auto isFermataSymId = [](SymId s) {
                    return s == SymId::fermataAbove
                           || s == SymId::fermataBelow
                           || s == SymId::fermataShortAbove
                           || s == SymId::fermataShortBelow
                           || s == SymId::fermataLongAbove
                           || s == SymId::fermataLongBelow;
                };
                Segment* chordSeg = chord->segment();
                for (int slot = 0; slot < 2; ++slot) {
                    const quint8 ab = slot == 0 ? en->articulationUp
                                      : en->articulationDown;
                    const bool isAbove = slot == 0;
                    for (SymId sid : encArticulation2SymIds(ab)) {
                        if (sid == SymId::noSym) {
                            continue;
                        }
                        // Fermatas anchor on the ChordRest segment (not the
                        // chord) and produce MusicXML <fermata>. Slot drives
                        // the upright/inverted variant (articUp -> above,
                        // articDown -> below).
                        if (isFermataSymId(sid) && chordSeg) {
                            Fermata* ferm = Factory::createFermata(chordSeg);
                            ferm->setTrack(track);
                            SymId resolved = sid;
                            if (sid == SymId::fermataAbove || sid == SymId::fermataBelow) {
                                resolved = isAbove ? SymId::fermataAbove
                                           : SymId::fermataBelow;
                            } else if (sid == SymId::fermataShortAbove
                                       || sid == SymId::fermataShortBelow) {
                                resolved = isAbove ? SymId::fermataShortAbove
                                           : SymId::fermataShortBelow;
                            }
                            ferm->setSymId(resolved);
                            ferm->setPlacement(isAbove ? mu::engraving::PlacementV::ABOVE
                                               : mu::engraving::PlacementV::BELOW);
                            ferm->setPropertyFlags(mu::engraving::Pid::PLACEMENT,
                                                   mu::engraving::PropertyFlags::UNSTYLED);
                            chordSeg->add(ferm);
                            continue;
                        }
                        // Other ornaments (trill, mordent, ...) need to be
                        // wrapped in Ornament so MusicXML emits them under
                        // <ornaments>. Plain articulations stay as
                        // Articulation under <articulations>.
                        Articulation* art = isOrnamentSymId(sid)
                                            ? static_cast<Articulation*>(Factory::createOrnament(chord))
                                            : Factory::createArticulation(chord);
                        art->setSymId(sid);
                        chord->add(art);
                    }
                }
                // Single-note tremolos. Encore encodes the stroke count in
                // the low nibble of articulationUp / articulationDown for
                // bytes that the regular articulation table does not match.
                // Observed patterns in encore-symbols.enc:
                //   m1 tick=480: au=0x41 -> tremolo 1 stroke
                //   m2 tick=  0: ad=0x42 -> tremolo 2 strokes
                //   m2 tick=240: ad=0x03 -> tremolo 3 strokes
                //   m2 tick=480: ad=0x43 -> tremolo 3 strokes (REF: 4, off by 1)
                auto tremoloStrokeFromByte = [](quint8 b) -> int {
                    // Encore packs single-note tremolos into the artic byte
                    // with stroke count in the low nibble. The 0x40 flag
                    // appears for stroke counts 1..3 (0x41/0x42/0x43); a bare
                    // 0x03 also encodes "tremolo with 3 strokes" (m2 tick=240
                    // in encore-symbols.enc). Stop at 3 strokes: 0x44 and up
                    // are technical markings (fingering, thumb-position,
                    // harmonic, open-string), not tremolos. 4-stroke
                    // tremolos appear stored as 0x43 in the corpus and are
                    // rendered as 3 strokes for now.
                    if (b == 0x41 || b == 0x42 || b == 0x43) {
                        return b & 0x0F;
                    }
                    if (b == 0x03) {
                        return 3;
                    }
                    return 0;
                };
                int strokes = std::max(tremoloStrokeFromByte(en->articulationUp),
                                       tremoloStrokeFromByte(en->articulationDown));
                if (strokes > 0 && !chord->tremoloSingleChord()) {
                    TremoloSingleChord* trem = Factory::createTremoloSingleChord(chord);
                    TremoloType type = TremoloType::R8;
                    switch (strokes) {
                    case 1: type = TremoloType::R8;
                        break;
                    case 2: type = TremoloType::R16;
                        break;
                    case 3: type = TremoloType::R32;
                        break;
                    case 4: type = TremoloType::R64;
                        break;
                    default: break;
                    }
                    trem->setTremoloType(type);
                    chord->add(trem);
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
                if (!isValidFaceValue(er->faceValue)) {
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
                            closeTupletWithFill(tt, trackKey);
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
                            closeTupletWithFill(tt, trackKey);
                        }
                        if (tt.inTuplet()) {
                            closeTupletWithFill(tt, trackKey);
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

            // -- Lyrics --
            // Encore stores lyric syllables, hyphen-continuation markers
            // ("-") and word-break markers ("") as separate LYRIC elements,
            // each with its own tick. Treating "-" / "" as plain syllables
            // shifts every following syllable by one chord-rest slot. We
            // therefore filter the stream:
            //   - "-" never produces a Lyrics; it just marks the previous
            //     syllable as hyphenated and the next as having a
            //     hyphen-before flag (mapped to LyricsSyllabic later).
            //   - "" is a word boundary; it never produces a Lyrics and
            //     resets the hyphen-before flag for the next syllable.
            //   - real syllables enter the queue tagged with the hyphen
            //     state and their raw Encore tick so the attach pass can
            //     anchor them on the closest chord, instead of consuming
            //     the next available slot in order.
            if (et == EncElemType::LYRIC) {
                const EncLyric* el = static_cast<const EncLyric*>(e);
                const String text(el->text);
                auto& queue = pendingLyrics[track];
                if (text == u"-") {
                    if (!queue.empty()) {
                        queue.back().hyphenAfter = true;
                    }
                    nextLyricHyphenBefore[track] = true;
                } else if (text.isEmpty()) {
                    nextLyricHyphenBefore[track] = false;
                } else {
                    PendingLyric pl;
                    pl.encTick = static_cast<int>(e->tick);
                    pl.text = text;
                    auto it = nextLyricHyphenBefore.find(track);
                    pl.hyphenBefore = (it != nextLyricHyphenBefore.end()) && it->second;
                    pl.hyphenAfter = false;
                    nextLyricHyphenBefore[track] = false;
                    queue.push_back(std::move(pl));
                }
            }

            // -- Ornaments (slurs, hairpins, tempo, staff text) --
            if (et == EncElemType::ORNAMENT) {
                const EncOrnament* eo = static_cast<const EncOrnament*>(e);

                // Snap an attached ornament's tick back to the chord-rest
                // whose layout xoffset matches the ornament's drawn position.
                // Encore stores the ornament at the CHORD-REST AT OR AFTER
                // its visible position but records the visible position in
                // `eo->xoffset`. When the ornament's xoff is less than the
                // stored chord-rest's xoff, the rendered position belongs to
                // the previous chord-rest (Encore visually pulls the glyph
                // back). The importer otherwise placed dynamics and hairpins
                // one chord later than the user saw in Encore. We only snap
                // when stored tick > 0 so measure-start dynamics (where
                // Encore's xoff is irrelevant and may be large for layout
                // padding) stay anchored on the bar line.
                auto snapTickByXoffset = [&](Fraction defaultTick) -> Fraction {
                    if (defaultTick <= measTick) {
                        return defaultTick;
                    }
                    const Fraction relTick = defaultTick - measTick;
                    if (encMeas.beatTicks == 0 || encMeas.timeSigDen == 0) {
                        return defaultTick;
                    }
                    const int wholeTicks = encMeas.beatTicks * encMeas.timeSigDen;
                    const int defaultEncTick = (relTick.numerator() * wholeTicks)
                                               / std::max(1, relTick.denominator());
                    const int ornXoff = static_cast<int>(eo->xoffset);
                    // Locate the chord-rest at the default tick and read its
                    // xoffset. If the ornament's xoffset is not strictly less
                    // than it, keep the default tick.
                    int defaultCrXoff = -1;
                    for (const auto& elem : encMeas.elements) {
                        const EncMeasureElem* em = elem.get();
                        if (em->type != static_cast<quint8>(EncElemType::NOTE)
                            && em->type != static_cast<quint8>(EncElemType::REST)) {
                            continue;
                        }
                        if (em->staffIdx != staffIdx || em->voice != voice) {
                            continue;
                        }
                        if (static_cast<int>(em->tick) != defaultEncTick) {
                            continue;
                        }
                        if (em->type == static_cast<quint8>(EncElemType::NOTE)) {
                            defaultCrXoff = static_cast<int>(static_cast<const EncNote*>(em)->xoffset);
                        } else {
                            defaultCrXoff = static_cast<int>(static_cast<const EncRest*>(em)->xoffset);
                        }
                        break;
                    }
                    if (defaultCrXoff >= 0 && ornXoff >= defaultCrXoff) {
                        return defaultTick;
                    }
                    // Find the latest NOTE/REST strictly before the default
                    // tick whose xoffset is <= the ornament's xoff. Also
                    // fires when defaultCrXoff < 0 (no chord-rest at the
                    // default tick, e.g. a WEDGESTART placed exactly at
                    // durTicks = the bar line). In that case the scan finds
                    // the last note in the measure and anchors the hairpin
                    // start there instead of spilling over the bar line.
                    int bestTick = -1;
                    for (const auto& elem : encMeas.elements) {
                        const EncMeasureElem* em = elem.get();
                        if (em->type != static_cast<quint8>(EncElemType::NOTE)
                            && em->type != static_cast<quint8>(EncElemType::REST)) {
                            continue;
                        }
                        if (em->staffIdx != staffIdx || em->voice != voice) {
                            continue;
                        }
                        if (static_cast<int>(em->tick) >= defaultEncTick) {
                            continue;
                        }
                        int xoff = 0;
                        if (em->type == static_cast<quint8>(EncElemType::NOTE)) {
                            xoff = static_cast<int>(static_cast<const EncNote*>(em)->xoffset);
                        } else {
                            xoff = static_cast<int>(static_cast<const EncRest*>(em)->xoffset);
                        }
                        if (xoff > ornXoff) {
                            continue;
                        }
                        if (static_cast<int>(em->tick) > bestTick) {
                            bestTick = static_cast<int>(em->tick);
                        }
                    }
                    if (bestTick < 0) {
                        return defaultTick;
                    }
                    return measTick + Fraction(bestTick, wholeTicks);
                };

                switch (eo->ornType()) {
                case EncOrnamentType::SLURSTART: {
                    // Encore .enc binaries do NOT emit SLURSTOP. alMezuro is the
                    // count of measures forward to the end measure. Endpoint is
                    // resolved in a post-pass once destination measures are
                    // populated; see the loop after the main measure loop.
                    int endIdx = measIdx + static_cast<int>(eo->alMezuro);
                    if (endIdx < 0 || endIdx >= static_cast<int>(measuresByIdx.size())) {
                        endIdx = measIdx;
                    }
                    PendingSlur ps;
                    ps.startTick = elemTick;
                    ps.track = track;
                    ps.startMeasIdx = measIdx;
                    ps.endMeasIdx = endIdx;
                    ps.alMezuro = static_cast<int>(eo->alMezuro);
                    ps.slurXoffset = static_cast<int>(eo->xoffset);
                    ps.slurXoffset2 = static_cast<int>(eo->xoffset2);
                    ps.staffIdx = staffIdx;
                    ps.encVoice = voice;
                    pendingSlurs.push_back(ps);
                    break;
                }
                case EncOrnamentType::SLURSTOP:
                    // Encore .enc files do not contain SLURSTOP markers; the
                    // endpoint is encoded inside the SLURSTART (see above).
                    break;
                case EncOrnamentType::WEDGESTART: {
                    // Encore .enc files do NOT emit a separate WEDGESTOP
                    // element. alMezuro carries the count of measures
                    // forward to the end measure (upper bound on the
                    // span). The actual visual endpoint is at the next
                    // Dynamic on the same track inside that span; we
                    // collect the hairpin here and resolve the precise
                    // tick2 in a post-pass once every Dynamic is placed.
                    int endIdx = measIdx + static_cast<int>(eo->alMezuro);
                    if (endIdx < 0 || endIdx >= static_cast<int>(measuresByIdx.size())) {
                        endIdx = measIdx;
                    }
                    Measure* endMeas = measuresByIdx[endIdx];
                    Fraction maxEnd = endMeas->tick() + endMeas->ticks();
                    // Snap the hairpin start to the chord-rest whose
                    // xoffset matches Encore's drawn position. Without it
                    // the hairpin attaches one chord later than the user
                    // saw in Encore (real-file case: a half note + eighth
                    // pair where Encore renders the crescendo under the
                    // half note but the binary tags the eighth).
                    const Fraction snappedStart = snapTickByXoffset(elemTick);
                    if (maxEnd <= snappedStart) {
                        break;
                    }
                    // speguleco direction: bit 0 selects diminuendo vs
                    // crescendo. Encore 5 also sets bit 1 on the same byte
                    // (so real files show 0x02 for crescendo and 0x03 for
                    // diminuendo, not the legacy 0x00 / 0x01 pair). The old
                    // `== 0` check treated every Encore 5 hairpin as
                    // diminuendo and flipped every cresc/dim pair on disk.
                    const HairpinType hpType
                        = ((eo->speguleco & 0x01) == 0)
                          ? HairpinType::CRESC_HAIRPIN
                          : HairpinType::DIM_HAIRPIN;
                    PendingHairpin ph;
                    ph.startTick = snappedStart;
                    ph.maxEndTick = maxEnd;
                    ph.track = track;
                    ph.type = hpType;
                    ph.endMeasIdx = endIdx;
                    ph.hairpinXoffset2 = static_cast<int>(eo->xoffset2);
                    ph.staffIdx = staffIdx;
                    ph.encVoice = voice;
                    pendingHairpins.push_back(ph);
                    break;
                }
                case EncOrnamentType::WEDGESTOP:
                    // Encore .enc files do not contain WEDGESTOP markers; the
                    // endpoint is encoded inside the WEDGESTART. Kept as a no-op
                    // in case a future Encore variant ever emits one.
                    break;
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
                case EncOrnamentType::ARPEGGIO: {
                    // Encore writes the ORN before the chord's notes in MEAS
                    // order, so the chord does not exist yet at this point.
                    // Queue the intent; a post-measure pass resolves it once
                    // the chord segment is populated.
                    pendingArpeggios.push_back({ elemTick, track });
                    break;
                }
                case EncOrnamentType::TREMOLO_32:
                case EncOrnamentType::TREMOLO_32B: {
                    // Single-chord tremolo ORN (3 slashes = 32nd-note speed).
                    // The ORN tick may equal the chord tick (most cases) or
                    // fall at durTicks (Encore stores it after the last note
                    // on long tied passages). Both are resolved in the post-
                    // pass which searches backwards when no chord is found at
                    // the exact tick.
                    PendingOrnTremolo pt;
                    pt.tick = elemTick;
                    pt.measTick = measTick;
                    pt.staffIdx = staffIdx;
                    pt.msVoice = msVoice;
                    pt.tremType = TremoloType::R32;
                    pendingOrnTremolos.push_back(pt);
                    break;
                }
                case EncOrnamentType::TRILL_START:
                case EncOrnamentType::TRILL_ALT: {
                    // Same deferred attachment as ARPEGGIO -- the chord is
                    // not built yet at this point in the element loop.
                    // TRILL_END (0x35) is dropped because it marks the end
                    // of a trill+wavy-line span; the start markers already
                    // place the visible trill-mark glyph.
                    pendingTrills.push_back({ elemTick, track });
                    break;
                }
                case EncOrnamentType::TRILL_END:
                    break;
                case EncOrnamentType::SEGNO:
                case EncOrnamentType::TO_CODA:
                case EncOrnamentType::CODA: {
                    // Section markers attach to a measure, not a chord, so
                    // they don't need the chord-deferred pattern. Queue the
                    // tick + marker type; the post-measure pass adds the
                    // Marker to the measure that contains the tick.
                    MarkerType mt = MarkerType::CODA;
                    if (eo->ornType() == EncOrnamentType::SEGNO) {
                        mt = MarkerType::SEGNO;
                    } else if (eo->ornType() == EncOrnamentType::TO_CODA) {
                        mt = MarkerType::TOCODA;
                    }
                    pendingMarkers.push_back({ elemTick, mt });
                    break;
                }
                case EncOrnamentType::STACCATO: {
                    // Encore stores chord-level staccato as a separate ORN
                    // tipo=0xC9 at the same tick as the chord. Its own
                    // MusicXML exporter drops 0xC9 entirely, but the dot
                    // is visible in Encore's display. Defer attachment
                    // like ARPEGGIO/TRILL because the chord segment is
                    // not built yet at this point in the element loop.
                    pendingStaccatos.push_back({ elemTick, track });
                    break;
                }
                case EncOrnamentType::DYN_PPP:
                case EncOrnamentType::DYN_PP:
                case EncOrnamentType::DYN_P:
                case EncOrnamentType::DYN_MP:
                case EncOrnamentType::DYN_MF:
                case EncOrnamentType::DYN_F:
                case EncOrnamentType::DYN_FF:
                case EncOrnamentType::DYN_FFF:
                case EncOrnamentType::DYN_SFZ:
                case EncOrnamentType::DYN_SFFZ:
                case EncOrnamentType::DYN_FP:
                case EncOrnamentType::DYN_FZ:
                case EncOrnamentType::DYN_SF: {
                    // Size-16 dynamic markings. The size-16 ORN payload only
                    // carries the tipo byte reliably (later fields run past
                    // the element boundary in the parser); the tipo alone
                    // selects the DynamicType.
                    //
                    // Staff displacement: Encore stores a dynamic on staff N
                    // but draws it above that staff (yoffset > 0) when the
                    // user has dragged it up onto the staff above. The binary
                    // staffByte points to staff N but the glyph visually
                    // belongs to staff N-1. Reroute to N-1 so MuseScore
                    // places it on the correct instrument.
                    if (eo->yoffset > 0 && staffIdx > 0) {
                        staffIdx -= 1;
                        track = static_cast<track_idx_t>(staffIdx * VOICES + msVoice);
                    }
                    DynamicType dt = DynamicType::OTHER;
                    switch (eo->ornType()) {
                    case EncOrnamentType::DYN_PPP:  dt = DynamicType::PPP;
                        break;
                    case EncOrnamentType::DYN_PP:   dt = DynamicType::PP;
                        break;
                    case EncOrnamentType::DYN_P:    dt = DynamicType::P;
                        break;
                    case EncOrnamentType::DYN_MP:   dt = DynamicType::MP;
                        break;
                    case EncOrnamentType::DYN_MF:   dt = DynamicType::MF;
                        break;
                    case EncOrnamentType::DYN_F:    dt = DynamicType::F;
                        break;
                    case EncOrnamentType::DYN_FF:   dt = DynamicType::FF;
                        break;
                    case EncOrnamentType::DYN_FFF:  dt = DynamicType::FFF;
                        break;
                    case EncOrnamentType::DYN_SFZ:  dt = DynamicType::SFZ;
                        break;
                    case EncOrnamentType::DYN_SFFZ: dt = DynamicType::SFFZ;
                        break;
                    case EncOrnamentType::DYN_FP:   dt = DynamicType::FP;
                        break;
                    case EncOrnamentType::DYN_FZ:   dt = DynamicType::FZ;
                        break;
                    case EncOrnamentType::DYN_SF:   dt = DynamicType::SF;
                        break;
                    default: break;
                    }
                    // Snap the dynamic's stored tick back to the chord-rest
                    // whose xoffset matches Encore's drawn position; Encore
                    // tags the chord at or after the visible glyph, but
                    // the rendered position belongs to the preceding chord
                    // when the dynamic's xoffset is smaller. Without this
                    // a dynamic placed under the 2nd eighth of a measure
                    // attaches one chord later in MuseScore.
                    Fraction placeTick = snapTickByXoffset(elemTick);
                    // Section-end dynamics (e.g. the "pp" that applies on
                    // the repeat of a 1st-volta measure) are stored at the
                    // measure's last tick (measureDurTicks). cumTick has
                    // already been advanced to fill the measure by the
                    // time we reach the ORN, so the snap result still
                    // lands at the next measure's first segment; clamp
                    // such overflow back to the last existing ChordRest
                    // segment inside the current measure.
                    if (placeTick >= measTick + measure->ticks()) {
                        Segment* last = measure->last(SegmentType::ChordRest);
                        placeTick = last ? last->tick() : measTick;
                    }
                    Segment* seg = measure->getSegment(SegmentType::ChordRest, placeTick);
                    if (!seg) {
                        seg = measure->getSegment(SegmentType::ChordRest, measTick);
                    }
                    // Encore can write the same dynamic twice on the same
                    // (staff, voice) at the same tick (real-file case: two
                    // MF ORNs at tick=120 with xoff 37/38). Encore renders
                    // only one; skip the duplicate so MuseScore does not
                    // stack two Dynamics on the same segment.
                    bool dupDyn = false;
                    for (EngravingItem* ann : seg->annotations()) {
                        if (!ann || !ann->isDynamic() || ann->track() != track) {
                            continue;
                        }
                        if (toDynamic(ann)->dynamicType() == dt) {
                            dupDyn = true;
                            break;
                        }
                    }
                    if (dupDyn) {
                        break;
                    }
                    Dynamic* dyn = Factory::createDynamic(seg);
                    dyn->setTrack(track);
                    dyn->setDynamicType(dt);
                    dyn->setXmlText(Dynamic::dynamicText(dt));
                    if (eo->yoffset < 0) {
                        dyn->setPlacement(mu::engraving::PlacementV::BELOW);
                        dyn->setPropertyFlags(mu::engraving::Pid::PLACEMENT, mu::engraving::PropertyFlags::UNSTYLED);
                    }
                    seg->add(dyn);
                    break;
                }
                case EncOrnamentType::STAFFTEXT: {
                    // STAFFTEXT 0x1E is a position-only marker; the text
                    // payload lives in the TEXT block and is referenced by
                    // the ornament's tind byte (+32).
                    const int textIdx = static_cast<int>(eo->tind);
                    if (textIdx < 0
                        || textIdx >= static_cast<int>(enc.textBlock.entries.size())) {
                        break;
                    }
                    QString text = enc.textBlock.entries[textIdx];
                    if (text.isEmpty()) {
                        break;
                    }
                    // Same clamp as the dynamic case above: STAFFTEXT
                    // ornaments stored at the measure's final tick must
                    // attach to the last existing segment in the current
                    // measure rather than spill over the bar line.
                    Fraction placeTick = elemTick;
                    if (placeTick >= measTick + measure->ticks()) {
                        Segment* last = measure->last(SegmentType::ChordRest);
                        placeTick = last ? last->tick() : measTick;
                    }
                    Segment* seg = measure->getSegment(SegmentType::ChordRest, placeTick);
                    if (!seg) {
                        seg = measure->getSegment(SegmentType::ChordRest, measTick);
                    }
                    // Encore stores text y-offset with Cartesian sign (positive =
                    // upward). When yoffset is below the staff baseline (negative
                    // in that convention) the text was placed under the system in
                    // Encore, e.g. "ten" markers in Beethoven Plectro m3. Mirror
                    // that into MuseScore's PlacementV.
                    const bool placeBelow = (eo->yoffset < 0);

                    // Promote standard Italian tempo terms (Allegro, Andante,
                    // "a tempo", ...) to a TempoText so MuseScore tracks them
                    // as tempo rather than free-form staff text.
                    const double tempoBps = encTextToTempoBps(text);
                    if (tempoBps >= 0.0) {
                        TempoText* tt2 = Factory::createTempoText(seg);
                        tt2->setTrack(track);
                        tt2->setXmlText(String(text));
                        if (tempoBps > 0.0) {
                            tt2->setTempo(BeatsPerSecond(tempoBps));
                            tt2->setFollowText(true);
                            score->setTempo(elemTick, BeatsPerSecond(tempoBps));
                        }
                        if (placeBelow) {
                            tt2->setPlacement(mu::engraving::PlacementV::BELOW);
                            tt2->setPropertyFlags(mu::engraving::Pid::PLACEMENT, mu::engraving::PropertyFlags::UNSTYLED);
                        }
                        seg->add(tt2);
                        break;
                    }

                    StaffText* st = Factory::createStaffText(seg);
                    st->setTrack(track);
                    st->setXmlText(String(text));
                    if (placeBelow) {
                        st->setPlacement(mu::engraving::PlacementV::BELOW);
                        st->setPropertyFlags(mu::engraving::Pid::PLACEMENT, mu::engraving::PropertyFlags::UNSTYLED);
                    }
                    seg->add(st);
                    break;
                }
                default:
                    break;
                }
            }

            // -- Key changes --
            // KEYCHANGE elements can carry tipo=0 (= C major / A minor, 0 fifths).
            // A modulation BACK to no accidentals is a legitimate key change, so
            // we render it like any other; the previous "skip tipo == 0" guard
            // dropped ~24 of 40 changes on Beethoven's Plectro arrangement.
            if (et == EncElemType::KEYCHANGE) {
                const EncKeyChange* ekc = static_cast<const EncKeyChange*>(e);
                Staff* staff = score->staff(staffIdx);
                if (!staff) {
                    continue;
                }
                Key newKey = Key(encKeyToFifths(ekc->tipo));
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

        // Before checkMeasure: finalize any tuplets still open at measure end.
        // closeTupletWithFill fills partial groups whose placedTicks does not
        // fit a TDuration, then closes the tracker; TupletTracker::closeTuplet
        // handles the remaining cases (shrink to placedTicks when it fits, or
        // leave canonical for full / mixed-overshoot groups). Required before
        // checkMeasure -- otherwise the canonical tuplet ticks would skip
        // past the actual placed content and leave the measure incomplete.
        for (auto& [key, tt] : tuplets) {
            closeTupletWithFill(tt, key);
        }

        // Attach queued lyric syllables to ChordRest segments by tick. The
        // queue has already filtered out "-" and "" separator elements; each
        // remaining entry carries the raw Encore tick the syllable was
        // anchored to in the binary, and a hyphen flag pair that determines
        // its LyricsSyllabic when rendered. For every Chord segment in this
        // measure we pick the syllable whose encTick is closest to the
        // segment tick, within half a beat. Leftover lyrics (the binary
        // sometimes places end-of-measure syllables past the bar) persist
        // to the next measure's attach pass.
        // Encore PPQ derives from beatTicks (quarter-note tick count). For
        // a 240-PPQ quarter, 480 raw Encore ticks per whole note; one
        // MuseScore tick (DIVISION=480 per quarter) == 2 Encore ticks.
        const int encTicksPerQuarter = encMeas.beatTicks
                                       ? static_cast<int>(encMeas.beatTicks) : 240;
        const int matchThreshold = encTicksPerQuarter / 2;   // half-beat window
        for (auto& [lyTrack, entries] : pendingLyrics) {
            if (entries.empty()) {
                continue;
            }
            // Encore stores multi-verse lyrics on different voices of the
            // same staff: verse 1 = voice 0, verse 2 = voice 1, etc. Within
            // MuseScore every verse anchors to the same voice-0 chord and
            // uses Lyrics::setVerse() to disambiguate verse number.
            const int lyStaffIdx = static_cast<int>(lyTrack) / VOICES;
            const int lyVerseNo = static_cast<int>(lyTrack) % VOICES;
            const track_idx_t chordTrack = static_cast<track_idx_t>(lyStaffIdx) * VOICES;
            std::vector<bool> consumed(entries.size(), false);
            for (Segment* s = measure->first(SegmentType::ChordRest);
                 s; s = s->next(SegmentType::ChordRest)) {
                EngravingItem* el = s->element(chordTrack);
                if (!el || !el->isChord()) {
                    continue;
                }
                // Convert segment tick (relative to measure start) to raw
                // Encore PPQ for comparison with the queued lyric ticks.
                const Fraction relTick = s->tick() - measure->tick();
                const int segEncTick = (relTick.numerator() * encTicksPerQuarter * 4)
                                       / std::max(1, relTick.denominator());
                int bestIdx = -1;
                int bestDelta = matchThreshold + 1;
                for (size_t i = 0; i < entries.size(); ++i) {
                    if (consumed[i]) {
                        continue;
                    }
                    const int delta = std::abs(entries[i].encTick - segEncTick);
                    if (delta < bestDelta) {
                        bestDelta = delta;
                        bestIdx = static_cast<int>(i);
                    }
                }
                if (bestIdx < 0) {
                    continue;
                }
                Chord* c = toChord(el);
                Lyrics* ly = Factory::createLyrics(c);
                ly->setTrack(chordTrack);
                ly->setVerse(lyVerseNo);
                ly->setXmlText(entries[bestIdx].text);
                LyricsSyllabic syll = LyricsSyllabic::SINGLE;
                if (entries[bestIdx].hyphenBefore && entries[bestIdx].hyphenAfter) {
                    syll = LyricsSyllabic::MIDDLE;
                } else if (entries[bestIdx].hyphenBefore) {
                    syll = LyricsSyllabic::END;
                } else if (entries[bestIdx].hyphenAfter) {
                    syll = LyricsSyllabic::BEGIN;
                }
                ly->setSyllabic(syll);
                c->add(ly);
                consumed[bestIdx] = true;
            }
            // Lyric ticks are encoded relative to the measure they live in,
            // so unmatched leftovers cannot anchor anywhere in a later
            // measure -- discard them rather than letting the queue grow.
            entries.clear();
        }
        // The hyphen-after flag on the trailing syllable is preserved as
        // part of the just-rendered Lyrics. The hyphen-before flag for the
        // next syllable, however, lives in nextLyricHyphenBefore and must
        // survive bar lines (e.g. Encore's "RO -" at the end of a measure
        // carrying the hyphen into the next bar's first syllable).

        // Fill any remaining gaps with invisible rests.
        // With faceValue-cumulative placement, notes always land at canonical positions
        // so gaps are only genuine rests not explicitly written in the Encore score.
        for (int si = 0; si < totalStaves; ++si) {
            measure->checkMeasure(static_cast<staff_idx_t>(si));
        }

        // Post-checkMeasure micro-correction: fix tiny over/undershoots (≤ 1/24)
        // that result from non-standard gaps that toRhythmicDurationList cannot
        // fill or match exactly.
        //
        // Overshoot (voiceSum > mLen ≤ 1/24): gap rests from cascade fills went
        //   1/64+1/256+1/1024 = 21/1024 when the actual gap was smaller (or zero).
        //   Remove gap rests smallest-first until voiceSum ≤ mLen.
        //
        // Undershoot (voiceSum < mLen ≤ 1/24): cascade left a sub-standard residual
        //   (e.g. 1/3072 = 1/48 - 21/1024). Add a V_MEASURE gap rest for the exact
        //   deficit (V_MEASURE accepts non-standard ticks without TDuration assertion).
        //
        // The 1/24 ceiling covers triplet 16th gaps left by mixed-value tuplets
        // (Q rest + eighth + eighth in a 3:2 quarter triplet contributes 1/3
        // instead of canonical 1/2, leaving the measure 1/24 short after the
        // following non-tuplet element forces an early close).
        {
            const Fraction mLen_fix = measure->ticks();
            const Fraction maxDelta(1, 24);
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

    // Apply per-measure tempo from the MEAS header BPM field. Encore stores
    // a BPM value in every measure's 54-byte header (offset 0, quarter-note
    // BPM regardless of the time signature); the importer previously read
    // the field into EncMeasure::bpm but never used it, so an imported
    // score always defaulted to MuseScore's 120 quarter-BPM.
    //
    // Emit a TempoText only on the first measure and on every measure
    // whose BPM differs from the previous applied value, so back-to-back
    // identical measures do not get redundant tempo markings. Skip the
    // tempo update entirely when a TempoText already lives at the target
    // segment (an ORN TEMPO subtype 0x32 or a STAFFTEXT promoted from an
    // Italian tempo term has already set both the visible mark and the
    // tempo map; trust that instance).
    {
        quint16 lastBpm = 0;
        for (size_t mi = 0; mi < enc.measures.size() && mi < measuresByIdx.size(); ++mi) {
            const quint16 bpm = enc.measures[mi].bpm;
            if (bpm == 0) {
                continue;
            }
            if (mi > 0 && bpm == lastBpm) {
                continue;
            }
            Measure* m = measuresByIdx[mi];
            const Fraction measTick = m->tick();
            Segment* seg = m->getSegment(SegmentType::ChordRest, measTick);
            if (!seg) {
                continue;
            }
            bool hasExisting = false;
            for (EngravingItem* e : seg->annotations()) {
                if (e && e->isTempoText()) {
                    hasExisting = true;
                    break;
                }
            }
            if (!hasExisting) {
                const double bps = bpm / 60.0;
                TempoText* tt = Factory::createTempoText(seg);
                tt->setTrack(0);
                tt->setTempo(BeatsPerSecond(bps));
                tt->setXmlText(String(u"♩ = %1").arg(bpm));
                seg->add(tt);
                score->setTempo(measTick, BeatsPerSecond(bps));
            }
            lastBpm = bpm;
        }
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

    // Resolve SLURSTART intents collected during the measure pass. Encore
    // .enc binaries do not contain SLURSTOP markers; the endpoint measure
    // is in alMezuro. xoffset2 (a layout x, not a tick) is not used here:
    // we anchor on the last ChordRest in the target measure on this track,
    // which is correct for most slurs and acceptable for the rest until a
    // proper xoffset -> tick mapping is wired in.
    for (const PendingSlur& ps : pendingSlurs) {
        if (ps.endMeasIdx < 0 || ps.endMeasIdx >= static_cast<int>(measuresByIdx.size())) {
            continue;
        }
        Measure* endMeas = measuresByIdx[ps.endMeasIdx];
        Fraction endTick;
        bool resolved = false;

        // Pixel-span heuristic for same-measure slurs. Encore's slur
        // layout x at the start (slurXoffset) and end (slurXoffset2) are
        // a constant offset from the first/last note's xoffset along the
        // measure ruler; the constants are not equal but their DIFFERENCE
        // (slurXoffset2 - slurXoffset) matches the pixel distance between
        // the first and last covered notes on disk. So
        //   target_end_xoff = first_note_xoff + (slurXoffset2 - slurXoffset)
        // and the end note is the one in the target measure whose xoff is
        // closest to target_end_xoff. Confirmed against real Encore
        // exports: same-measure slurs whose XML stop sits on the 3rd note
        // produce a target inside ~3 layout units of that note's xoff.
        // For alMezuro > 0 (cross-measure spans) the heuristic does not
        // apply -- xoffsets reset at the bar line -- so we fall back to
        // the last existing ChordRest on the same track.
        if (ps.alMezuro == 0 && ps.startMeasIdx >= 0
            && ps.startMeasIdx < static_cast<int>(enc.measures.size())) {
            const EncMeasure& startEncMeas = enc.measures[ps.startMeasIdx];
            int firstNoteXoff = -1;
            // Walk the EncMeasure elements for the first NOTE on the slur's
            // (staffIdx, encVoice) whose tick matches the slur's start
            // tick (relative to measure start).
            const Fraction relStartTick = ps.startTick - measuresByIdx[ps.startMeasIdx]->tick();
            const int startEncTick = (relStartTick.numerator() * startEncMeas.beatTicks
                                      * startEncMeas.timeSigDen)
                                     / std::max(1, relStartTick.denominator());
            for (const auto& elem : startEncMeas.elements) {
                const EncMeasureElem* em = elem.get();
                if (em->type != static_cast<quint8>(EncElemType::NOTE)) {
                    continue;
                }
                if (em->staffIdx != ps.staffIdx || em->voice != ps.encVoice) {
                    continue;
                }
                if (static_cast<int>(em->tick) != startEncTick) {
                    continue;
                }
                firstNoteXoff = static_cast<int>(static_cast<const EncNote*>(em)->xoffset);
                break;
            }
            if (firstNoteXoff >= 0) {
                const int pixelSpan = ps.slurXoffset2 - ps.slurXoffset;
                const int targetEndXoff = firstNoteXoff + pixelSpan;
                int bestDist = std::numeric_limits<int>::max();
                int bestEncTick = -1;
                for (const auto& elem : startEncMeas.elements) {
                    const EncMeasureElem* em = elem.get();
                    if (em->type != static_cast<quint8>(EncElemType::NOTE)) {
                        continue;
                    }
                    if (em->staffIdx != ps.staffIdx || em->voice != ps.encVoice) {
                        continue;
                    }
                    const int xoff = static_cast<int>(static_cast<const EncNote*>(em)->xoffset);
                    const int dist = std::abs(xoff - targetEndXoff);
                    if (dist < bestDist) {
                        bestDist = dist;
                        bestEncTick = static_cast<int>(em->tick);
                    }
                }
                if (bestEncTick >= 0 && bestEncTick > startEncTick) {
                    // Snap the chosen end Encore tick to its ChordRest
                    // segment in the MuseScore measure.
                    const Fraction endRel(bestEncTick,
                                          startEncMeas.beatTicks * startEncMeas.timeSigDen);
                    endTick = measuresByIdx[ps.startMeasIdx]->tick() + endRel;
                    resolved = true;
                }
            }
        }

        if (!resolved) {
            Segment* lastSeg = nullptr;
            for (Segment* s = endMeas->first(SegmentType::ChordRest); s;
                 s = s->next(SegmentType::ChordRest)) {
                if (s->element(ps.track)) {
                    lastSeg = s;
                }
            }
            if (!lastSeg) {
                continue;
            }
            endTick = lastSeg->tick();
        }

        if (endTick <= ps.startTick) {
            continue;   // zero or negative span: drop
        }
        Slur* slur = Factory::createSlur(score->dummy());
        slur->setTrack(ps.track);
        slur->setTrack2(ps.track);
        slur->setTick(ps.startTick);
        slur->setTick2(endTick);
        score->addElement(slur);
    }

    // Resolve WEDGESTART intents. Encore renders a chain like `mf<f>mf`
    // with each hairpin terminating exactly at the next dynamic glyph
    // on the same track AND at the position recorded in `xoffset2`
    // within the alMezuro target measure. Two distinct cases:
    //
    //   - same-measure (`mf<f>` chain): the next Dynamic on the track
    //     marks the visible endpoint; the alMezuro upper bound is the
    //     bar line.
    //   - cross-measure dim before a downbeat dynamic: `xoffset2` in
    //     the target measure can be SMALLER than the first note's
    //     xoffset, meaning Encore draws the hairpin ending right at
    //     the bar line (before any rendered content of the target
    //     measure). Without honouring `xoffset2` the hairpin extends
    //     visibly into the target measure and overlaps with the next
    //     hairpin starting there.
    //
    // The resolved endpoint is the minimum of the next-dynamic tick
    // and the xoffset2-derived tick; if neither produces a valid
    // bound, fall back to the alMezuro upper bound.
    for (const PendingHairpin& ph : pendingHairpins) {
        Fraction endTick = ph.maxEndTick;

        // (1) next-dynamic-on-track endpoint. Priority over the xoffset2
        // bar-line clamp: if a Dynamic (mf, f, ...) exists after the
        // hairpin start and before the alMezuro upper bound, the hairpin
        // ends exactly there. This handles chains like mf<f>mf where each
        // hairpin terminates at the next dynamic.
        bool foundNextDynamic = false;
        for (Segment* s = score->firstSegment(SegmentType::ChordRest); s; s = s->next1(SegmentType::ChordRest)) {
            if (s->tick() <= ph.startTick) {
                continue;
            }
            if (s->tick() > ph.maxEndTick) {
                break;
            }
            bool stopHere = false;
            for (EngravingItem* ann : s->annotations()) {
                if (ann && ann->isDynamic() && ann->track() == ph.track) {
                    stopHere = true;
                    break;
                }
            }
            if (stopHere) {
                endTick = std::min(endTick, s->tick());
                foundNextDynamic = true;
                break;
            }
        }

        // (2) xoffset2 bar-line clamp (fallback when no next-dynamic found).
        // Encore stores the hairpin's layout end position in `xoffset2`
        // within the alMezuro target measure. When xoffset2 is smaller
        // than the first NOTE's xoffset in that measure, Encore draws the
        // hairpin ending right before any visible content (= at the bar
        // line). Only apply when no Dynamic was found in step (1), so a
        // cross-measure hairpin that ends at a mf/f does NOT get clamped
        // to the bar line just because its xoffset2 is also small.
        if (!foundNextDynamic
            && ph.endMeasIdx >= 0
            && ph.endMeasIdx < static_cast<int>(enc.measures.size())) {
            const EncMeasure& endEncMeas = enc.measures[ph.endMeasIdx];
            if (endEncMeas.beatTicks && endEncMeas.timeSigDen) {
                const int xoff2 = ph.hairpinXoffset2;
                int firstNoteXoff = -1;
                for (const auto& elem : endEncMeas.elements) {
                    const EncMeasureElem* em = elem.get();
                    if (em->type != static_cast<quint8>(EncElemType::NOTE)) {
                        continue;
                    }
                    if (em->staffIdx != ph.staffIdx || em->voice != ph.encVoice) {
                        continue;
                    }
                    const int xoff = static_cast<int>(
                        static_cast<const EncNote*>(em)->xoffset);
                    if (xoff > 0 && (firstNoteXoff < 0 || xoff < firstNoteXoff)) {
                        firstNoteXoff = xoff;
                    }
                }
                if (firstNoteXoff > 0 && xoff2 < firstNoteXoff) {
                    Fraction targetMeasTick = measuresByIdx[ph.endMeasIdx]->tick();
                    endTick = std::min(endTick, targetMeasTick);
                }
            }
        }

        if (endTick <= ph.startTick) {
            continue;
        }
        Hairpin* hp = Factory::createHairpin(score->dummy()->segment());
        hp->setTrack(ph.track);
        hp->setTrack2(ph.track);
        hp->setTick(ph.startTick);
        hp->setTick2(endTick);
        hp->setHairpinType(ph.type);
        score->addElement(hp);
    }

    // Resolve ARPEGGIO intents collected during the measure pass. The
    // ornament element is written before the chord notes inside MEAS, so
    // attachment has to wait until the chord segment exists.
    for (const PendingArpeggio& pa : pendingArpeggios) {
        Measure* m = score->tick2measure(pa.tick);
        if (!m) {
            continue;
        }
        Segment* seg = m->findSegment(SegmentType::ChordRest, pa.tick);
        if (!seg) {
            continue;
        }
        EngravingItem* el = seg->element(pa.track);
        if (!el || !el->isChord()) {
            continue;
        }
        Chord* c = toChord(el);
        if (c->arpeggio()) {
            continue;   // chord already carries an arpeggio
        }
        Arpeggio* arp = Factory::createArpeggio(c);
        arp->setTrack(pa.track);
        arp->setArpeggioType(ArpeggioType::NORMAL);
        c->add(arp);
    }

    // Resolve ORN-based single-chord tremolos (tipo 0xAF / 0xEF).
    // Find the chord at the stored tick; if none, search backwards in the
    // source measure for the latest chord (handles Encore placement at
    // tick == durTicks on long notes).
    for (const PendingOrnTremolo& pt : pendingOrnTremolos) {
        const track_idx_t trTrack = static_cast<track_idx_t>(pt.staffIdx * VOICES + pt.msVoice);
        // Try exact tick match first.
        Measure* m = score->tick2measure(pt.tick);
        if (!m) {
            m = score->tick2measure(pt.measTick);
        }
        if (!m) {
            continue;
        }
        Segment* seg = m->findSegment(SegmentType::ChordRest, pt.tick);
        if (!seg || !seg->element(trTrack) || !seg->element(trTrack)->isChord()) {
            // Fall back: search backwards in the measure that owns the source
            // ORN tick (pt.measTick). Encore can place the tremolo ORN at
            // tick == durTicks (after the last note), so the tick2measure
            // lookup above can land in the NEXT measure (which has only a
            // whole-rest filler and no chord). Re-anchor the search in the
            // source measure using pt.measTick.
            Measure* srcMeas = score->tick2measure(pt.measTick);
            if (!srcMeas) {
                srcMeas = m;
            }
            seg = nullptr;
            for (Segment* s = srcMeas->first(SegmentType::ChordRest); s;
                 s = s->next(SegmentType::ChordRest)) {
                if (s->element(trTrack) && s->element(trTrack)->isChord()) {
                    seg = s;   // keep updating: we want the LAST chord
                }
            }
        }
        if (!seg || !seg->element(trTrack)) {
            continue;
        }
        EngravingItem* el = seg->element(trTrack);
        if (!el || !el->isChord()) {
            continue;
        }
        Chord* c = toChord(el);
        if (c->tremoloSingleChord()) {
            continue;   // already has a tremolo from the articulation byte
        }
        TremoloSingleChord* trem = Factory::createTremoloSingleChord(c);
        trem->setTremoloType(pt.tremType);
        c->add(trem);
    }

    // Resolve SEGNO / CODA section markers on their measures.
    for (const PendingMarker& pm : pendingMarkers) {
        Measure* m = score->tick2measure(pm.tick);
        if (!m) {
            continue;
        }
        Marker* mk = Factory::createMarker(m);
        mk->setMarkerType(pm.type);
        mk->setTrack(0);
        m->add(mk);
    }

    // Resolve STACCATO intents collected during the measure pass.
    // Add the SymId::articStaccatoAbove articulation if the chord
    // does not already carry one (the per-note artic byte 0x1D
    // produces the same glyph and we don't want duplicates).
    for (const PendingStaccato& ps : pendingStaccatos) {
        Measure* m = score->tick2measure(ps.tick);
        if (!m) {
            continue;
        }
        Segment* seg = m->findSegment(SegmentType::ChordRest, ps.tick);
        if (!seg) {
            continue;
        }
        EngravingItem* el = seg->element(ps.track);
        if (!el || !el->isChord()) {
            continue;
        }
        Chord* c = toChord(el);
        bool alreadyHas = false;
        for (Articulation* a : c->articulations()) {
            if (a->symId() == SymId::articStaccatoAbove
                || a->symId() == SymId::articStaccatoBelow) {
                alreadyHas = true;
                break;
            }
        }
        if (alreadyHas) {
            continue;
        }
        Articulation* art = Factory::createArticulation(c);
        art->setTrack(ps.track);
        art->setSymId(SymId::articStaccatoAbove);
        c->add(art);
    }

    // Resolve TRILL intents collected during the measure pass. Same
    // pattern as ARPEGGIO -- the source ORN element is written before
    // the chord's notes inside MEAS, so the chord segment only exists
    // after the per-measure pass completes.
    for (const PendingTrill& pt : pendingTrills) {
        Measure* m = score->tick2measure(pt.tick);
        if (!m) {
            continue;
        }
        Segment* seg = m->findSegment(SegmentType::ChordRest, pt.tick);
        if (!seg) {
            continue;
        }
        EngravingItem* el = seg->element(pt.track);
        if (!el || !el->isChord()) {
            continue;
        }
        Chord* c = toChord(el);
        Ornament* orn = Factory::createOrnament(c);
        orn->setTrack(pt.track);
        orn->setSymId(SymId::ornamentTrill);
        c->add(orn);
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
