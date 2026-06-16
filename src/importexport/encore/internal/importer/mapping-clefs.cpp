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

#include "mapping.h"

#include "engraving/dom/clef.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/key.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/part.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/timesig.h"
#include "engraving/editing/transpose.h"

using namespace mu::engraving;

namespace mu::iex::enc {
ClefType encClef2MuseScore(EncClefType ct)
{
    switch (ct) {
    case EncClefType::G:    return ClefType::G;
    case EncClefType::F:    return ClefType::F;
    case EncClefType::C3L:  return ClefType::C3;
    case EncClefType::C4L:  return ClefType::C4;
    case EncClefType::G8P:  return ClefType::G8_VA;
    case EncClefType::G8M:  return ClefType::G8_VB;
    case EncClefType::F8M:  return ClefType::F8_VB;
    case EncClefType::PERC: return ClefType::PERC;
    case EncClefType::TAB:  return ClefType::TAB;
    default:                return ClefType::G;
    }
}

// Encore key byte is an index 0..14 into { C, F, Bb, Eb, Ab, Db, Gb, Cb, G, D, A, E, B, F#, C# }
// mapping to fifths {0,-1,-2,-3,-4,-5,-6,-7,1,2,3,4,5,6,7}. See ENCORE_FORMAT.md §Key encoding.
int encKeyToFifths(quint8 key)
{
    static const int table[] = { 0, -1, -2, -3, -4, -5, -6, -7, 1, 2, 3, 4, 5, 6, 7 };
    if (key < 15) {
        return table[key];
    }
    return 0;
}

void addInitialKeySig(MasterScore* score, int staffIdx, quint8 encKey)
{
    int fifths = encKeyToFifths(encKey);
    Staff* staff = score->staff(staffIdx);
    if (!staff) {
        return;
    }
    Key writtenKey = Key(fifths);
    // Encore's key field is the written key for the instrument. Convert to concert
    // key for the staff timeline; the written key is stored explicitly for display.
    Interval v = staff->part()->instrument()->transpose();
    Key concertKey = v.isZero() ? writtenKey : Transpose::transposeKey(writtenKey, v);
    // Prefer sharp enharmonics (F# over Gb, B over Cb) when transposeKey returns an extreme
    // flat key. Transpose::transposeKey(Key::C, Interval(3,6)) returns Key::Gb (-6) for a
    // +6 (augmented-4th) transposing instrument, causing pitch2tpc to spell concert B4 as Cb
    // and the written note as Gbb. Using F# instead gives the correct spelling (F).
    if (static_cast<int>(concertKey) <= -6) {
        concertKey = Key(static_cast<int>(concertKey) + 12);
    }
    // Always store the concert key on the staff's key timeline so Staff::concertKey() returns
    // the normalized value. Without this, C-major (fifths==0) returns early and the staff
    // computes the concert key on-the-fly from the instrument transposition, which may return
    // the flat enharmonic and produce double-flat note spellings.
    Fraction tick = Fraction(0, 1);
    KeySigEvent ke;
    ke.setConcertKey(concertKey);
    ke.setKey(writtenKey);
    staff->setKey(tick, ke);
    // For C major written key, no visible key signature is needed in the score.
    if (fifths == 0) {
        return;
    }

    Measure* m = score->tick2measure(tick);
    if (!m) {
        return;
    }
    Segment* seg = m->getSegment(SegmentType::KeySig, tick);
    KeySig* ks = Factory::createKeySig(seg);
    ks->setTrack(staffIdx * VOICES);
    ks->setKey(concertKey, writtenKey);
    seg->add(ks);
}

void addInitialTimeSig(MasterScore* score, int nstaves, Fraction ts, TimeSigType tsType)
{
    Measure* m = score->tick2measure(Fraction(0, 1));
    if (!m) {
        return;
    }
    for (int staffIdx = 0; staffIdx < nstaves; ++staffIdx) {
        Segment* seg = m->getSegment(SegmentType::TimeSig, Fraction(0, 1));
        TimeSig* tsig = Factory::createTimeSig(seg);
        tsig->setTrack(staffIdx * VOICES);
        tsig->setSig(ts, tsType);
        seg->add(tsig);
    }
}

static int clefOctaveOffset(ClefType ct)
{
    switch (ct) {
    case ClefType::G8_VB:
    case ClefType::G8_VB_O:
    case ClefType::G8_VB_P:
    case ClefType::G8_VB_C:
    case ClefType::F8_VB:
    case ClefType::C4_8VB:
        return -12;
    case ClefType::G8_VA:
    case ClefType::F_8VA:
        return 12;
    case ClefType::G15_MB:
    case ClefType::F15_MB:
        return -24;
    case ClefType::G15_MA:
    case ClefType::F_15MA:
        return 24;
    default:
        return 0;
    }
}

static int clefGlyphFamily(ClefType ct)
{
    switch (ct) {
    case ClefType::G:
    case ClefType::G_1:
    case ClefType::G8_VB:
    case ClefType::G8_VA:
    case ClefType::G15_MA:
    case ClefType::G15_MB:
    case ClefType::G8_VB_O:
    case ClefType::G8_VB_P:
    case ClefType::G8_VB_C:
        return 1;   // G family
    case ClefType::F:
    case ClefType::F_B:
    case ClefType::F_C:
    case ClefType::F_F18C:
    case ClefType::F_19C:
    case ClefType::F15_MB:
    case ClefType::F8_VB:
    case ClefType::F_8VA:
    case ClefType::F_15MA:
        return 2;   // F family
    default:
        return 0;
    }
}

ClefType pickStaffClef(EncClefType encClef, ClefType /*concertClef*/, ClefType /*transposingClef*/,
                       int keyOffsetSemitones)
{
    const ClefType base = encClef2MuseScore(encClef);
    if (keyOffsetSemitones == 0 || clefGlyphFamily(base) == 0) {
        return base;
    }
    // When Key is +-12 or +-24 semitones, pick the matching octave-decorated clef
    // in the same glyph family. Other offsets fall back to plain base clef.
    static const std::array<ClefType, 8> kCandidates = {
        ClefType::G8_VB, ClefType::G8_VA,
        ClefType::G15_MB, ClefType::G15_MA,
        ClefType::F8_VB, ClefType::F_8VA,
        ClefType::F15_MB, ClefType::F_15MA,
    };
    const int family = clefGlyphFamily(base);
    for (ClefType c : kCandidates) {
        if (clefGlyphFamily(c) == family && clefOctaveOffset(c) == keyOffsetSemitones) {
            return c;
        }
    }
    return base;
}

void addInitialClef(MasterScore* score, int staffIdx, EncClefType ct)
{
    addInitialClef(score, staffIdx, encClef2MuseScore(ct));
}

void addInitialClef(MasterScore* score, int staffIdx, ClefType ct)
{
    Measure* m = score->tick2measure(Fraction(0, 1));
    if (!m) {
        return;
    }
    Segment* seg = m->getSegment(SegmentType::HeaderClef, Fraction(0, 1));
    Clef* clef = Factory::createClef(seg);
    clef->setTrack(staffIdx * VOICES);
    clef->setClefType(ct);
    seg->add(clef);
}
} // namespace mu::iex::enc
