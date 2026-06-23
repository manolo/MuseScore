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

#include "readers-v0xc2.h"
#include "readers-v0xc4-base.h"

#include <QDataStream>

#include "elem.h"
#include "ticks.h"

namespace mu::iex::enc {
// v0xC2: Encore places the sixteenth of a dotted-eighth+sixteenth group at tick+120
// (the undotted gap), not tick+180. Fix: detect eighth(rdur=120) + sixteenth(rdur=60,
// tick+120) and set forceDotted.
//
// Guard: the fix must only fire when the measure is SHORT by exactly 60t (the amount
// the dotted-eighth anomaly steals).  Without this guard the fix produces false
// positives on any genuine 8th+16th sequence inside a full measure (e.g. tapada.enc
// m40 staff 2: face sum=360t in a 480t measure is short by 120t, not 60t → no fix).
static void fixDottedEighthPattern(std::vector<EncMeasureElem*>& elems, qint16 durTicks)
{
    // Pre-compute face-value sum to decide whether the measure needs correction.
    int faceSum = 0;
    for (const EncMeasureElem* e : elems) {
        quint8 fv = 0;
        if (const auto* en = dynamic_cast<const EncNote*>(e)) {
            fv = en->faceValue & 0x0F;
        } else if (const auto* er = dynamic_cast<const EncRest*>(e)) {
            fv = er->faceValue & 0x0F;
        }
        faceSum += faceValue2ticks(fv);
    }
    // The dotted-8th anomaly: one 8th is stored as plain (120t) instead of
    // dotted (180t), making the measure exactly 60t short.
    // If the deficit is not 60t, the measure is already correct or has a
    // different issue — don't apply the dotted fix.
    if (faceSum + 60 != static_cast<int>(durTicks)) {
        return;
    }

    for (size_t i = 0; i < elems.size(); ++i) {
        EncNote* en = dynamic_cast<EncNote*>(elems[i]);
        if (!en || (en->faceValue & 0x0F) != 4 || en->realDuration != 120) {
            continue;
        }
        const qint16 targetTick = static_cast<qint16>(elems[i]->tick + 120);
        for (size_t j = i + 1; j < elems.size(); ++j) {
            if (elems[j]->tick > targetTick) {
                break;
            }
            if (elems[j]->tick == targetTick) {
                const EncNote* enNext = dynamic_cast<const EncNote*>(elems[j]);
                if (enNext
                    && (enNext->faceValue & 0x0F) == 5
                    && enNext->realDuration == 60) {
                    en->dotControl |= 1;   // kept for documentation; dot is forced via forceDotted
                    en->forceDotted = true;
                    break;
                }
            }
        }
    }
}

// v0xC2: mark consecutive notes/rests whose rdur/faceValue ratio identifies an implied tuplet.
// Groups same-tick elements as chords before scanning, matching the grouping in
// computeImpliedTupletMembers so the two passes agree on group boundaries.
static void markImpliedTupletMembers(std::vector<EncMeasureElem*>& elems)
{
    std::vector<std::vector<EncMeasureElem*> > chords;
    for (EncMeasureElem* e : elems) {
        if (!chords.empty() && chords.back()[0]->tick == e->tick) {
            chords.back().push_back(e);
        } else {
            chords.push_back({ e });
        }
    }
    int n = static_cast<int>(chords.size());
    int i = 0;
    while (i < n) {
        EncMeasureElem* first = chords[i][0];
        quint8 fv = 0;
        if (auto* en = dynamic_cast<EncNote*>(first)) {
            fv = en->faceValue & 0x0F;
        } else if (auto* er = dynamic_cast<EncRest*>(first)) {
            fv = er->faceValue & 0x0F;
        }
        if (fv < 4) {
            ++i;
            continue;
        }
        int normalN = 0;
        int actualN = detectImpliedTuplet(first->realDuration, fv, normalN);
        if (actualN < 2 || i + actualN > n) {
            ++i;
            continue;
        }
        bool allMatch = true;
        for (int k = 1; k < actualN; ++k) {
            EncMeasureElem* ek = chords[i + k][0];
            quint8 fvk = 0;
            if (auto* en = dynamic_cast<EncNote*>(ek)) {
                fvk = en->faceValue & 0x0F;
            } else if (auto* er = dynamic_cast<EncRest*>(ek)) {
                fvk = er->faceValue & 0x0F;
            }
            int nk = 0;
            if (fvk < 4 || detectImpliedTuplet(ek->realDuration, fvk, nk) != actualN || nk != normalN) {
                allMatch = false;
                break;
            }
        }
        if (allMatch) {
            for (int k = 0; k < actualN; ++k) {
                for (EncMeasureElem* e : chords[i + k]) {
                    if (auto* en = dynamic_cast<EncNote*>(e)) {
                        en->isImpliedTupletMember = true;
                    } else if (auto* er = dynamic_cast<EncRest*>(e)) {
                        er->isImpliedTupletMember = true;
                    }
                }
            }
            i += actualN;
        } else {
            ++i;
        }
    }
}

// Encore 3.x / 4.x (v0xC2) format reader.
// Differences from v0xC4:
//   - ORN 0xC4 is an accent, not an up-bow
//   - grace1 low nibble encodes the tie-sender flag
//   - alMezuro field in ornaments is unreliable
//   - Lyric text starts at element offset +18 (not +20)
//   - NOTE: MIDI pitch is in tuplet slot; semiTonePitch is 0 (swap them in postProcess)
//   - Instrument metadata: names only (no TK-based MIDI/key tables)
struct EncFormatReader_V0xC2 final : EncFormatReader_V0xC4Base
{
    const char* formatName() const override { return "v0xC2"; }
    quint8 lyricTextGapAfterKie() const override { return 7; }

    bool postProcessElement(EncMeasureElem* elem, QDataStream& ds, qint64 rawElemStart) const override
    {
        if (EncOrnament* orn = dynamic_cast<EncOrnament*>(elem)) {
            // v0xC2: tipo 0xC4 (UPBOW in v0xC4) encodes accent above in this format.
            if (orn->tipo == static_cast<quint8>(EncOrnamentType::UPBOW)) {
                orn->tipo = static_cast<quint8>(EncOrnamentType::ACCENT);
            }
            // v0xC2: alMezuro has no valid measure-count semantics for spanning ornaments.
            orn->alMezuroValid = false;
            return false;
        }

        EncNote* en = dynamic_cast<EncNote*>(elem);
        if (!en) {
            return false;
        }
        // v0xC2: MIDI pitch usually occupies the tuplet slot (+13) with semiTonePitch
        // (+15) empty; swap it across. But some Encore 4.x files store the pitch directly
        // in semiTonePitch, in which case the tuplet slot holds a genuine tuplet ratio
        // (e.g. 0x32 = 3:2) that must be preserved, not mistaken for the pitch. Only swap
        // when semiTonePitch is empty. See ENCORE_FORMAT.md §Note element.
        if (en->tuplet > 0 && en->semiTonePitch == 0) {
            en->semiTonePitch = en->tuplet;
            en->tuplet = 0;
        }
        // Decode tie-sender flag from grace1 low nibble (v0xC2 only).
        en->isTieSender = ((en->grace1 & 0x0F) == 1);
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

    void postProcessVoiceGroup(std::vector<EncMeasureElem*>& elems, qint16 durTicks) const override
    {
        fixDottedEighthPattern(elems, durTicks);
        markImpliedTupletMembers(elems);
    }

    // v0xC2 has no TK-based MIDI/key tables; the base helpers no-op gracefully when
    // contentFilePos==-1, so delegating to the base is safe and avoids duplication.
    bool readInstrumentMeta(std::vector<EncInstrument>& instruments,
                            QDataStream& ds,
                            const EncRoot& file) const override
    {
        return EncFormatReader_V0xC4Base::readInstrumentMeta(instruments, ds, file);
    }
};

std::unique_ptr<EncFormatReader> makeFormatReader_V0xC2()
{
    return std::make_unique<EncFormatReader_V0xC2>();
}
} // namespace mu::iex::enc
