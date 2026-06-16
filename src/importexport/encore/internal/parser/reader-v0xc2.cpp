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

#include "reader-v0xc2.h"
#include "reader-v0xc4-base.h"

#include <QDataStream>

#include "elements.h"

namespace mu::iex::encore {

// Encore 3.x / 4.x (v0xC2) format reader.
// Differences from v0xC4:
//   - ORN 0xC4 is an accent, not an up-bow
//   - Implied tuplets are supported (realDuration/faceValue mismatch)
//   - grace1 low nibble encodes the tie-sender flag
//   - alMezuro field in ornaments is unreliable
//   - Lyric text starts at element offset +18 (not +20)
//   - NOTE: MIDI pitch is in tuplet slot; semiTonePitch is 0 (swap them in postProcess)
//   - Instrument metadata: names only (no TK-based MIDI/key tables)
struct EncFormatReader_V0xC2 final : EncFormatReader_V0xC4Base
{
    bool supportsImpliedTuplets() const override { return true; }
    bool usesG1LowTieSender() const override { return true; }
    const char* formatName() const override { return "v0xC2"; }
    bool alMezuroIsReliable() const override { return false; }
    bool ornC4IsAccent() const override { return true; }
    quint8 lyricTextGapAfterKie() const override { return 7; }

    bool postProcessElement(EncMeasureElem* elem, QDataStream& ds, qint64 rawElemStart) const override
    {
        EncNote* en = dynamic_cast<EncNote*>(elem);
        if (!en) {
            return false;
        }
        // v0xC2: MIDI pitch is at offset +13 (the tuplet slot) and semiTonePitch is 0;
        // swap them. When tuplet is 0 the pitch is already in semiTonePitch (some Encore
        // 4.x files), so leave it untouched. See ENCORE_FORMAT.md §Note element.
        if (en->tuplet > 0) {
            en->semiTonePitch = en->tuplet;
            en->tuplet = 0;
        }
        // size=24 notes carry an articulation byte at +22 (2 bytes after alterGlyph at +21).
        if (en->size == 24 && ds.device()->seek(rawElemStart + 22)) {
            ds >> en->articulationUp;
            en->articulationDown = 0;
        } else {
            en->articulationUp   = 0;
            en->articulationDown = 0;
        }
        return false;
    }

    // v0xC2 has no TK-based MIDI/key meta tables; only recover instrument names.
    bool readInstrumentMeta(std::vector<EncInstrument>& instruments,
                            QDataStream& ds,
                            const EncFile& file) const override
    {
        // Delegate to base only for name recovery (the base would also read MIDI/key,
        // which do not exist in v0xC2 files). Call the simpler path directly via the
        // no-TK branch in the base helpers by using the full base if instruments is empty,
        // or just call base which will do recoverMissingNames + readMidiPrograms +
        // readKeyTranspositions. For v0xC2 files the MIDI/key helpers will gracefully
        // no-op because contentFilePos==-1 and the block probe finds no large-TK block.
        // So calling the base is safe and avoids code duplication.
        return EncFormatReader_V0xC4Base::readInstrumentMeta(instruments, ds, file);
    }
};

std::unique_ptr<EncFormatReader> makeFormatReader_V0xC2()
{
    return std::make_unique<EncFormatReader_V0xC2>();
}

} // namespace mu::iex::encore
