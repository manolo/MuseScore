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

#include "elem.h"

#include "readers.h"
#include "ticks.h"
#include "log.h"

namespace mu::iex::enc {
// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

static std::unique_ptr<EncMeasureElem> createMeasureElement(
    quint16 tick, quint8 tp, quint8 vo,
    const EncFormatReader& fmt)
{
    switch (static_cast<EncElemType>(tp)) {
    case EncElemType::NOTE:
        return std::make_unique<EncNote>(tick, tp, vo);
    case EncElemType::REST:
        return std::make_unique<EncRest>(tick, tp, vo);
    case EncElemType::CHORD:
        return std::make_unique<EncChordSym>(tick, tp, vo);
    case EncElemType::ORNAMENT:
    {
        auto orn = std::make_unique<EncOrnament>(tick, tp, vo);
        orn->tindOffset = fmt.staffTextTindOffset();
        orn->yoffOffset = fmt.staffTextYoffsetOffset();
        return orn;
    }
    case EncElemType::LYRIC:
    {
        auto lyr = std::make_unique<EncLyric>(tick, tp, vo);
        lyr->preKieSkip = fmt.lyricPreKieSkip();
        lyr->textGapAfterKie = fmt.lyricTextGapAfterKie();
        lyr->spacingFactor = static_cast<quint8>(fmt.elemSpacing(1));   // element slot = size * factor
        return lyr;
    }
    case EncElemType::KEYCHANGE:
        return std::make_unique<EncKeyChange>(tick, tp, vo);
    case EncElemType::TIE:
        return std::make_unique<EncTie>(tick, tp, vo);
    case EncElemType::UNKNOWN1:
        // Genuinely unknown element type; counted and reported once by logEncRootInfo.
        return std::make_unique<EncGenericElem>(tick, tp, vo);
    case EncElemType::MIDI_CC:
        // Inline MIDI Control Change (sustain/volume/modulation), playback only; captured for
        // the diagnostic summary in logEncRootInfo and otherwise dropped (no notation).
        return std::make_unique<EncMidiCc>(tick, tp, vo);
    case EncElemType::CLEF:
        return std::make_unique<EncClefChange>(tick, tp, vo);
    case EncElemType::NONE:
    case EncElemType::BEAM:
    // BEAM is intentionally not modeled: MuseScore auto-beams from note durations and time
    // signature, so Encore's explicit beam groups are dropped. See ENCORE_FORMAT.md.
    default:
        return std::make_unique<EncGenericElem>(tick, tp, vo);
    }
}

// Phase 1: set realDuration from MIDI tick gaps (with optional grace time-borrowing).
// boundaryTicks: sorted non-note ticks (CLEF, KEYCHANGE) that cap the gap even when no
// note follows. Prevents a lonely quarter rest from being stretched to fill the whole measure
// when a mid-measure clef change is the next event at that voice/staff.
void computeElementDurations(
    std::vector<EncMeasureElem*>& elems,
    qint16 durTicks,
    bool hasGraceTimeBorrowing,
    const std::vector<qint16>& boundaryTicks)
{
    for (size_t i = 0; i < elems.size(); ++i) {
        size_t j = i + 1;
        // Skip same-tick chord members, then near-simultaneous cluster notes.
        while (j < elems.size() && elems[j]->tick == elems[i]->tick) {
            ++j;
        }
        while (j < elems.size()
               && elems[j]->tick - elems[i]->tick < CHORD_CLUSTER_THRESHOLD) {
            ++j;
        }
        qint16 nextTick = (j < elems.size()) ? elems[j]->tick : durTicks;
        for (qint16 bt : boundaryTicks) {
            if (bt > elems[i]->tick && bt < nextTick) {
                nextTick = bt;
            }
        }
        qint16 dur = nextTick - elems[i]->tick;
        // v0xA6 grace time-borrowing: grace notes shorten next note's gap; see ENCORE_FORMAT.md §v0xA6 grace note time-borrowing.
        const EncNote* enCur = dynamic_cast<const EncNote*>(elems[i]);
        if (hasGraceTimeBorrowing && enCur && dur > 0) {
            const qint16 faceTicks = faceValue2ticks(enCur->faceValue);
            if (faceTicks > dur && faceTicks <= durTicks) {
                qint16 totalGraceFace = 0;
                for (size_t k = 0; k < i; ++k) {
                    const EncNote* en = dynamic_cast<const EncNote*>(elems[k]);
                    if (en && en->graceType() != EncGraceType::NORMAL) {
                        totalGraceFace += faceValue2ticks(en->faceValue);
                    }
                }
                if (totalGraceFace == faceTicks - dur) {
                    dur = faceTicks;
                } else if (dur > faceTicks
                           && totalGraceFace > 0
                           && (dur - faceTicks) <= totalGraceFace) {
                    dur = faceTicks;
                }
            }
        }
        // A following GRACE note has no rhythmic footprint, so it must not extend (inflate) the
        // current principal note's held duration. Cap the duration at the written face value so a
        // note trailed by an ornament (e.g. a percussion beat followed by a grace flourish) stays
        // its written value plus a rest, instead of being promoted to a dotted/longer note when the
        // gap to the grace happens to match a dotted ratio.
        if (enCur && enCur->graceType() == EncGraceType::NORMAL && j < elems.size()) {
            const EncNote* enNext = dynamic_cast<const EncNote*>(elems[j]);
            const qint16 faceTicks = faceValue2ticks(enCur->faceValue);
            if (enNext && enNext->graceType() != EncGraceType::NORMAL
                && faceTicks > 0 && dur > faceTicks) {
                dur = faceTicks;
            }
        }
        if (dur > 0) {
            elems[i]->realDuration = dur;
        }
    }
}

// Encore records a chord's notes with staggered playback ticks (a per-chord "strum" / live-recording
// drift), but every note in one chord shares the same notated horizontal column, stored in the note
// xoffset byte. Collapse each run of consecutive notes that share the same nonzero xoffset and face
// value to the run's earliest tick, so the duration and chord-grouping logic downstream sees them as
// simultaneous chord members instead of splitting the chord into separate short notes.
// elems is one (staff, voice) group already sorted by tick. A run is treated as one chord only when
// the members stay within a small cluster window of the anchor: real chord members (including chords
// inside a tuplet) drift by at most a few dozen ticks, while the shortest sequential subdivision is
// farther apart. The window is capped at one notated duration so a short face value stays tight, and
// at CHORD_STRUM_MAX_SPAN so a long face value never absorbs a genuine later note that happens to
// reuse the column. Notes with a zero xoffset (no stored column) are left untouched.
// See ENCORE_FORMAT.md §Chord column (xoffset).
static void normalizeChordColumnTicks(std::vector<EncMeasureElem*>& elems)
{
    // Observed chord "strum" spans reach ~30 Encore ticks; the tightest sequential subdivision
    // Encore emits stays well above this, so 48 cleanly separates a chord column from a run of notes.
    constexpr int CHORD_STRUM_MAX_SPAN = 48;
    size_t i = 0;
    while (i < elems.size()) {
        EncNote* anchor = dynamic_cast<EncNote*>(elems[i]);
        if (!anchor || anchor->xoffset == 0) {
            ++i;
            continue;
        }
        const int faceTicks = faceValue2ticks(anchor->faceValue);
        const int window = std::min(faceTicks, CHORD_STRUM_MAX_SPAN);
        size_t j = i + 1;
        while (j < elems.size()) {
            EncNote* n = dynamic_cast<EncNote*>(elems[j]);
            if (!n || n->xoffset != anchor->xoffset
                || fvLow(n->faceValue) != fvLow(anchor->faceValue)
                || window <= 0
                || (n->tick - anchor->tick) >= window) {
                break;
            }
            n->tick = anchor->tick;
            ++j;
        }
        i = j;
    }
}

// ---------------------------------------------------------------------------
// EncMeasure
// ---------------------------------------------------------------------------

bool EncMeasure::read(QDataStream& ds, const quint32 vs, const EncFormatReader& fmt)
{
    varsize = vs;
    qint64 measStart = ds.device()->pos();

    ds >> bpm >> timeSigGlyph;
    ds.skipRawData(1);
    ds >> beatTicks >> durTicks;

    ds.device()->seek(measStart + 0x08);
    ds >> timeSigNum >> timeSigDen;

    ds.device()->seek(measStart + 0x0C);
    ds >> barTypeStart >> barTypeEnd;
    ds.skipRawData(1);
    ds >> repeatAlternative;

    ds.device()->seek(measStart + 0x1A);
    ds >> coda;

    const qint64 elemOffset = static_cast<qint64>(fmt.elemBlockOffset());
    ds.device()->seek(measStart + elemOffset);
    // varsize is an untrusted file value; clampMeasureEnd computes the end in qint64 and never lets
    // it run past the device so the element loop below is bounded even for a corrupt/oversized size.
    const qint64 measEnd = clampMeasureEnd(measStart, varsize, elemOffset, ds.device()->size());

    // Guard the first read the same way the loop body does. After the seek, a truncated file has
    // no element bytes left: reading would return a zero-filled tick (0, not 0xFFFF) and the loop
    // would mis-classify the truncated measure as a real one starting at tick 0 instead of bailing.
    if (ds.device()->pos() >= measEnd - 2 || ds.status() != QDataStream::Ok) {
        ds.device()->seek(measEnd);
        return true;
    }

    quint16 tick;
    ds >> tick;
    if (tick == 0xFFFF) {
        ds.device()->seek(measEnd);
        return true;
    }

    const int MAX_ELEMENTS = 10000;
    int elemCount = 0;

    while (tick != 0xFFFF) {
        // A read that ran past EOF (truncated/corrupt file) leaves the stream non-Ok; stop
        // instead of fabricating zero-filled elements from the past-EOF zero fill.
        if (ds.status() != QDataStream::Ok) {
            break;
        }
        if (++elemCount > MAX_ELEMENTS) {
            break;
        }
        if (ds.device()->pos() >= measEnd - 2) {
            break;
        }

        qint64 elemStart = ds.device()->pos() - 2;

        quint8 typeVoice;
        ds >> typeVoice;
        if (typeVoice == 0xFF) {
            quint8 skip;
            ds >> skip;
            break;
        }

        const quint8 tp = typeVoice >> 4;
        const quint8 vo = typeVoice & 0x0F;

        auto elem = createMeasureElement(tick, tp, vo, fmt);

        elem->read(ds);
        EncMeasureElem* elemRaw = elem.get();

        fmt.postProcessElement(elemRaw, ds, elemStart);

        if (static_cast<EncElemType>(tp) == EncElemType::REST
            && fmt.deduplicateRest(elements, elemRaw)) {
            if (elemRaw->size > 0) {
                ds.device()->seek(elemStart + fmt.elemSpacing(elemRaw->size));
            } else {
                ds.device()->seek(ds.device()->pos() + 1);
            }
            if (fmt.isMeasureNearEnd(ds, measEnd)) {
                break;
            }
            ds >> tick;
            continue;
        }

        if (static_cast<EncElemType>(tp) != EncElemType::NONE) {
            elements.push_back(std::move(elem));
        }

        if (elemRaw->size > 0) {
            ds.device()->seek(elemStart + fmt.elemSpacing(elemRaw->size));
        } else {
            ds.device()->seek(ds.device()->pos() + 1);
        }

        if (fmt.isMeasureNearEnd(ds, measEnd)) {
            break;
        }

        ds >> tick;
    }

    ds.device()->seek(measEnd);
    return true;
}

void EncMeasure::calculateRealDurations(bool hasGraceTimeBorrowing, const EncFormatReader& fmt)
{
    // Collect per-staff boundary ticks from non-note elements (CLEF, KEYCHANGE).
    // These cap the MIDI-gap duration of a preceding rest so it doesn't stretch to fill
    // the whole measure when the next event is a clef or key change rather than a note.
    std::map<int, std::vector<qint16> > boundaryByStaff;
    for (auto& elem : elements) {
        const EncElemType et = static_cast<EncElemType>(elem->type);
        if ((et == EncElemType::CLEF || et == EncElemType::KEYCHANGE)
            && elem->tick > 0 && elem->tick < durTicks) {
            boundaryByStaff[elem->staffIdx].push_back(elem->tick);
        }
    }

    std::map<std::pair<int, int>, std::vector<EncMeasureElem*> > groups;
    for (auto& elem : elements) {
        EncMeasureElem* e = elem.get();
        if (e->tick >= durTicks) {
            continue;
        }
        if (dynamic_cast<EncNote*>(e) || dynamic_cast<EncRest*>(e)) {
            groups[{ e->staffIdx, e->voice }].push_back(e);
        }
    }
    for (auto& [key, elems] : groups) {
        std::sort(elems.begin(), elems.end(), [](const EncMeasureElem* a, const EncMeasureElem* b) {
            return a->tick < b->tick;
        });
        const auto bIt = boundaryByStaff.find(key.first);
        const std::vector<qint16>& boundaries
            = (bIt != boundaryByStaff.end()) ? bIt->second : std::vector<qint16> {};
        if (fmt.clustersChordsByXoffset()) {
            normalizeChordColumnTicks(elems);
        }
        computeElementDurations(elems, durTicks, hasGraceTimeBorrowing, boundaries);
        fmt.postProcessVoiceGroup(elems, durTicks);
    }

    reconcileStaleNoteTicksByColumn();
}

// Encore lays notes out left-to-right, so a note's xoffset column identifies its beat and is
// consistent across the staves of a system. A note edited in Encore can keep a stale MIDI
// tick that no longer matches its column: it draws at the column's beat but the importer,
// trusting the tick, places it later (e.g. a half note stored at tick 480 with the beat-1
// xoffset imports as rest+note instead of note+rest). Snap such a note back to its column's
// tick, keeping the realDuration already computed from its stored tick so its notated value
// (and the trailing rest the gap becomes) are preserved. Runs after duration computation so
// rdur is not recomputed from the corrected tick.
void EncMeasure::reconcileStaleNoteTicksByColumn()
{
    // Column (xoffset) -> earliest tick a non-grace note occupies it at, across all staves.
    std::map<int, qint16> colTick;
    for (auto& elem : elements) {
        const EncNote* en = dynamic_cast<const EncNote*>(elem.get());
        if (!en || en->graceType() != EncGraceType::NORMAL || en->tick >= durTicks) {
            continue;
        }
        const int xo = static_cast<int>(static_cast<quint8>(en->xoffset));
        if (xo <= 0) {
            continue;
        }
        auto it = colTick.find(xo);
        if (it == colTick.end() || en->tick < it->second) {
            colTick[xo] = en->tick;
        }
    }
    // Plan the moves against the ORIGINAL ticks, then apply them, so chord siblings (which
    // share a tick and column) all move together rather than the first move making itself look
    // like an "earlier voice-mate" that blocks the rest.
    std::vector<std::pair<EncNote*, qint16> > moves;
    for (auto& elem : elements) {
        EncNote* en = dynamic_cast<EncNote*>(elem.get());
        if (!en || en->graceType() != EncGraceType::NORMAL || en->tick >= durTicks) {
            continue;
        }
        const int xo = static_cast<int>(static_cast<quint8>(en->xoffset));
        if (xo <= 0) {
            continue;
        }
        auto it = colTick.find(xo);
        if (it == colTick.end() || it->second >= en->tick) {
            continue;   // already the column's earliest tick (or later note is genuine)
        }
        const qint16 target = it->second;
        // Reconcile only a note that is the EARLIEST in its own staff/voice: the stale-tick
        // artifact is a whole voice drawn one column too far right, established as "too late"
        // by the other staves' aligned columns. A note that already has an earlier voice-mate
        // is part of a genuine sequence (its tick is meaningful relative to its neighbours,
        // and same-xoffset neighbours are just a low-resolution/synthetic layout) -- leave it.
        // Never merge two independent notes either: block when a DIFFERENT column already sits
        // at the target tick on this staff/voice (same-column notes are chord siblings).
        bool hasEarlierVoiceMate = false;
        bool occupied = false;
        for (auto& other : elements) {
            const EncNote* on = dynamic_cast<const EncNote*>(other.get());
            if (!on || on == en || on->staffIdx != en->staffIdx || on->voice != en->voice) {
                continue;
            }
            if (on->tick < en->tick) {
                hasEarlierVoiceMate = true;
                break;
            }
            if (on->tick == target
                && static_cast<int>(static_cast<quint8>(on->xoffset)) != xo) {
                occupied = true;
            }
        }
        if (!hasEarlierVoiceMate && !occupied) {
            moves.emplace_back(en, target);
        }
    }
    for (auto& [en, target] : moves) {
        en->tick = target;
    }
}
} // namespace mu::iex::enc
