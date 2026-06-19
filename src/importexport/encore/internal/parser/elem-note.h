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

#pragma once

#include <memory>
#include <vector>

#include <QDataStream>

#include "elem-enums.h"

namespace mu::iex::enc {
// Notes within this many Encore ticks treated as simultaneous (MIDI timing drift).
inline constexpr int CHORD_CLUSTER_THRESHOLD = 4;   // Encore ticks (~8ms at 120bpm)

// faceValue byte accessors: low nibble = duration (1=whole..8=128th), high nibble = notehead type.
inline quint8 fvLow(quint8 fv) { return fv & 0x0F; }             // duration nibble
inline quint8 fvHigh(quint8 fv) { return static_cast<quint8>((fv >> 4) & 0x0F); } // notehead nibble

// ---------------------------------------------------------------------------
// Base class for all measure elements
// ---------------------------------------------------------------------------

struct EncMeasureElem {
    quint16 tick  { 0 };
    quint8 type  { 0 };
    quint8 voice { 0 };
    quint8 size  { 0 };
    quint8 staffIdx    { 0 };   // low 6 bits of raw staff byte: staff index in system
    quint8 staffWithin { 0 };   // high 2 bits (>> 6): staff index within instrument (0=first, 1=second, ...)
    quint8 xoffset  { 0 };
    qint16 realDuration { -1 };

    // Nonzero = tuplet member; sort tuplet notes first at their tick so they create the chord.
    virtual quint8 tupletByte() const { return 0; }

    EncMeasureElem() = default;
    EncMeasureElem(quint16 t, quint8 tp, quint8 v)
        : tick(t), type(tp), voice(v) {}
    virtual ~EncMeasureElem() = default;

    virtual bool read(QDataStream& ds);
};

struct EncNote : EncMeasureElem {
    quint8 faceValue       { 0 };
    quint8 grace1          { 0 };
    quint8 grace2          { 0 };
    qint8 position        { 0 };
    quint8 tuplet          { 0 };
    quint8 dotControl      { 0 };
    quint8 semiTonePitch   { 0 };
    quint16 playbackDurTicks{ 0 };
    quint8 velocity        { 0 };
    quint8 options         { 0 };
    quint8 alterationGlyph { 0 };
    quint8 articulationUp  { 0 };
    quint8 articulationDown{ 0 };
    // Set by calculateRealDurations() for v0xA6: note is a non-leading grace
    // within a grace group (shorter duration than the leading grace).
    bool isInnerGrace           { false };
    // Set by postProcessElement() for formats where grace1 low nibble encodes tie-sender (v0xC2).
    bool isTieSender            { false };
    // Set by calculateRealDurations() Phase 4 for v0xC2: note belongs to an implied tuplet group
    // (rdur/faceValue mismatch identifies the ratio). Used by computeImpliedTupletMembers so
    // notes with incidental MIDI timing drift in other formats are never misidentified.
    bool isImpliedTupletMember  { false };
    // Set by fixDottedEighthPattern() (v0xC2): the note is the dotted-eighth in the
    // dotted-eighth+sixteenth anomaly.  Forces dots=1 in the emitter without relying
    // on the dotControl bit-0 fallback, which may spuriously fire on raw binary values.
    bool forceDotted            { false };

    using EncMeasureElem::EncMeasureElem;

    quint8 tupletByte() const override { return tuplet; }
    int actualNotes() const { return tuplet >> 4; }
    int normalNotes() const { return tuplet & 0x0F; }

    EncGraceType graceType() const;

    bool read(QDataStream& ds) override;
};

struct EncRest : EncMeasureElem {
    quint8 faceValue  { 0 };
    quint8 tuplet     { 0 };
    quint8 dotControl { 0 };
    // +15 (v0xC4 only): Encore multi-measure rest display count.
    // When > 1 this single MEAS block represents that many consecutive empty display
    // measures (Encore shows one rest symbol with this number above it).
    // Only meaningful when the MEAS block contains exactly this one REST element.
    quint8 mrestCount { 1 };
    // Set by calculateRealDurations() Phase 4 for v0xC2 (same semantics as EncNote::isImpliedTupletMember).
    bool isImpliedTupletMember { false };

    using EncMeasureElem::EncMeasureElem;

    quint8 tupletByte() const override { return tuplet; }
    int actualNotes() const { return tuplet >> 4; }
    int normalNotes() const { return tuplet & 0x0F; }

    bool read(QDataStream& ds) override;
};

struct EncKeyChange : EncMeasureElem {
    quint8 tipo { 0 };

    using EncMeasureElem::EncMeasureElem;

    bool read(QDataStream& ds) override;
};

struct EncGenericElem : EncMeasureElem {
    using EncMeasureElem::EncMeasureElem;

    bool read(QDataStream& ds) override;
};

using MeasureElemVec    = std::vector<std::unique_ptr<EncMeasureElem> >;
using MeasureElemRefVec = std::vector<const EncMeasureElem*>;
} // namespace mu::iex::enc
