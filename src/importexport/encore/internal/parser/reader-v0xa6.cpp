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

#include "reader-v0xa6.h"

#include <QDataStream>

#include "elements.h"

namespace mu::iex::encore {

// v0xA6 NOTE (size=10): MIDI pitch at +11 (absolute, not offset) and tuplet
// at +7. EncNote::read() uses v0xC4 offsets, so we fix both values here.
bool EncFormatReader_V0xA6::postProcessElement(EncMeasureElem* elem,
                                               QDataStream& ds,
                                               qint64 rawElemStart) const
{
    EncNote* en = dynamic_cast<EncNote*>(elem);
    if (!en || en->size != 10) {
        return false;   // not a v0xA6 note — nothing to fix
    }
    const qint64 savedPos = ds.device()->pos();

    ds.device()->seek(rawElemStart + 11);
    quint8 pitchByte;
    ds >> pitchByte;
    en->semiTonePitch = pitchByte;

    ds.device()->seek(rawElemStart + 7);
    quint8 tupByte;
    ds >> tupByte;
    en->tuplet = tupByte;

    ds.device()->seek(savedPos);
    return false;   // element is kept
}

// v0xA6 sometimes writes two identical REST elements back-to-back
// (same tick/staff/voice/faceValue). Encore shows only one.
// Keeping both pushes cumTick past measure end and breaks voice routing.
// Only consecutive duplicates removed; different-voice rests are kept.
bool EncFormatReader_V0xA6::deduplicateRest(
    std::vector<std::unique_ptr<EncMeasureElem> >& elements,
    EncMeasureElem* candidate) const
{
    if (elements.empty()) {
        return false;
    }
    const EncRest* prevR = dynamic_cast<const EncRest*>(elements.back().get());
    const EncRest* curR  = dynamic_cast<const EncRest*>(candidate);
    if (!prevR || !curR) {
        return false;
    }
    if (prevR->tick      == curR->tick
        && prevR->staffIdx == curR->staffIdx
        && prevR->voice    == curR->voice
        && prevR->faceValue == curR->faceValue) {
        return true;   // drop the duplicate
    }
    return false;
}

// v0xA6 measures end before the 4-byte sentinel — stop when < 4 bytes remain.
bool EncFormatReader_V0xA6::isMeasureNearEnd(QDataStream& ds, qint64 measEnd) const
{
    return ds.device()->pos() >= measEnd - 4;
}


void EncFormatReader_V0xA6::readKeyFromTKBlock(EncInstrument& instr,
                                               QDataStream& ds,
                                               qint64 contentStart) const
{
    if (!ds.device()->seek(contentStart + 42)) {
        return;
    }
    quint8 raw = 0;
    ds >> raw;
    const qint8 signedRaw = static_cast<qint8>(raw);
    if (signedRaw >= -33 && signedRaw <= 24) {
        instr.keyTransposeSemitones = signedRaw;
    }
    ds.device()->seek(contentStart);
}

} // namespace mu::iex::encore
