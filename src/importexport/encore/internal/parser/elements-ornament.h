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

#include "elements-note.h"

namespace mu::iex::enc {

struct EncOrnament : EncMeasureElem {
    // Field names follow the Encore binary format notation used throughout the spec
    quint8 tipo      { 0 };
    qint16 yoffset   { 0 };  // signed 16-bit Cartesian y (positive = upward in Encore)
    quint8 alMezuro  { 0 };
    quint8 xoffset2  { 0 };
    quint8 speguleco { 0 };
    quint8 noto      { 0 };
    quint8 tempo     { 0 };
    quint8 tind      { 0 };

    using EncMeasureElem::EncMeasureElem;

    EncOrnamentType ornType() const { return static_cast<EncOrnamentType>(tipo); }
    void setOrnType(EncOrnamentType t) { tipo = static_cast<quint8>(t); }

    bool read(QDataStream& ds) override;
};

} // namespace mu::iex::enc
