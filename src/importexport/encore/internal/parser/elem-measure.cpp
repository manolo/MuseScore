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

#include "elements.h"

#include "reader.h"

namespace mu::iex::encore {
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
    qint64 measEnd = measStart + varsize + elemOffset;

    quint16 tick;
    ds >> tick;
    if (tick == 0xFFFF) {
        ds.device()->seek(measEnd);
        return true;
    }

    const int MAX_ELEMENTS = 10000;
    int elemCount = 0;

    while (tick != 0xFFFF) {
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

        std::unique_ptr<EncMeasureElem> elem;
        switch (static_cast<EncElemType>(tp)) {
        case EncElemType::NOTE:
            elem = std::make_unique<EncNote>(tick, tp, vo);
            break;
        case EncElemType::REST:
            elem = std::make_unique<EncRest>(tick, tp, vo);
            break;
        case EncElemType::CHORD:
            elem = std::make_unique<EncChordSym>(tick, tp, vo);
            break;
        case EncElemType::ORNAMENT:
            elem = std::make_unique<EncOrnament>(tick, tp, vo);
            break;
        case EncElemType::LYRIC:
            elem = std::make_unique<EncLyric>(tick, tp, vo);
            break;
        case EncElemType::KEYCHANGE:
            elem = std::make_unique<EncKeyChange>(tick, tp, vo);
            break;
        case EncElemType::TIE:
            elem = std::make_unique<EncTie>(tick, tp, vo);
            break;
        case EncElemType::NONE:
        case EncElemType::CLEF:
        case EncElemType::BEAM:
        case EncElemType::UNKNOWN1:
        case EncElemType::UNKNOWN2:
        default:
            elem = std::make_unique<EncGenericElem>(tick, tp, vo);
            break;
        }

        elem->read(ds);
        EncMeasureElem* elemRaw = elem.get();

        // Format-specific element fixups (e.g. v0xA6 pitch/tuplet byte offsets)
        fmt.postProcessElement(elemRaw, ds, elemStart);

        // Format-specific REST deduplication (e.g. v0xA6 back-to-back duplicates)
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

void EncMeasure::calculateRealDurations()
{
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
        for (size_t i = 0; i < elems.size(); ++i) {
            size_t j = i + 1;
            // Skip same-tick elements (exact chord; existing behavior)
            while (j < elems.size() && elems[j]->tick == elems[i]->tick) {
                ++j;
            }
            // Skip near-simultaneous notes within cluster threshold for realistic rdur.
            while (j < elems.size()
                   && elems[j]->tick - elems[i]->tick < CHORD_CLUSTER_THRESHOLD) {
                ++j;
            }
            qint16 nextTick = (j < elems.size()) ? elems[j]->tick : durTicks;
            qint16 dur = nextTick - elems[i]->tick;
            // v0xA6 grace-note time-borrowing: grace notes stored at real tick
            // positions, which shortens the following note's gap. Sum grace
            // faceValues before this note; if they match the deficit, restore
            // rdur to faceValue.
            const EncNote* enCur = dynamic_cast<const EncNote*>(elems[i]);
            if (enCur && enCur->size == 10 && dur > 0) {
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
                        // grace shortened last note's gap.
                        dur = faceTicks;
                    } else if (dur > faceTicks
                               && totalGraceFace > 0
                               && (dur - faceTicks) <= totalGraceFace) {
                        // grace(s) left extra ticks in real duration that Encore ignores.
                        dur = faceTicks;
                    }
                }
            }
            if (dur > 0) {
                elems[i]->realDuration = dur;
            }
        }
    }
}

} // namespace mu::iex::encore
