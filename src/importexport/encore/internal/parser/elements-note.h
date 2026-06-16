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

#include "elements-enums.h"

namespace mu::iex::encore {

// Notes within this many Encore ticks treated as simultaneous (MIDI timing drift).
inline constexpr int CHORD_CLUSTER_THRESHOLD = 4;   // Encore ticks (~8ms at 120bpm)

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
    bool isInnerGrace      { false };

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

} // namespace mu::iex::encore
