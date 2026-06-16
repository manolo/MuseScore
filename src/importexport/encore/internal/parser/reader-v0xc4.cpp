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

#include "reader-v0xc4.h"
#include "reader-v0xc4-base.h"

#include <QDataStream>

#include "elements.h"

namespace mu::iex::encore {

// Encore 5.x (v0xC4) format reader.
// All defaults inherited from EncFormatReader_V0xC4Base are correct for v0xC4.
// The only v0xC4-specific post-processing is zeroing articulation bytes when
// the element is smaller than 27 bytes (bytes lie beyond the element boundary).
struct EncFormatReader_V0xC4 final : EncFormatReader_V0xC4Base
{
    const char* formatName() const override { return "v0xC4"; }

    bool postProcessElement(EncMeasureElem* elem, QDataStream& /*ds*/, qint64 /*rawElemStart*/) const override
    {
        EncNote* en = dynamic_cast<EncNote*>(elem);
        if (!en) {
            return false;
        }
        // Clear artic bytes that were read beyond the element boundary for size<27.
        if (en->size < 27) {
            en->articulationUp   = 0;
            en->articulationDown = 0;
        }
        return false;
    }
};

std::unique_ptr<EncFormatReader> makeFormatReader_V0xC4()
{
    return std::make_unique<EncFormatReader_V0xC4>();
}

} // namespace mu::iex::encore
