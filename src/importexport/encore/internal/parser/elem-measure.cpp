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
#include "ticks.h"
#include "log.h"

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
        {
            auto lyr = std::make_unique<EncLyric>(tick, tp, vo);
            lyr->textGapAfterKie = fmt.lyricTextGapAfterKie();
            elem = std::move(lyr);
        }
        break;
        case EncElemType::KEYCHANGE:
            elem = std::make_unique<EncKeyChange>(tick, tp, vo);
            break;
        case EncElemType::TIE:
            elem = std::make_unique<EncTie>(tick, tp, vo);
            break;
        case EncElemType::UNKNOWN1:
            LOGD() << QString("Encore: unknown element type UNKNOWN1 (0xA) at tick %1")
                          .arg(static_cast<int>(tick));
            elem = std::make_unique<EncGenericElem>(tick, tp, vo);
            break;
        case EncElemType::UNKNOWN2:
            // MIDI CC event (sustain, volume, modulation) - expected in live-recorded files.
            LOGD() << QString("Encore: MIDI CC event (UNKNOWN2/MIDI-CC) at tick %1")
                          .arg(static_cast<int>(tick));
            elem = std::make_unique<EncGenericElem>(tick, tp, vo);
            break;
        case EncElemType::NONE:
        case EncElemType::CLEF:
        case EncElemType::BEAM:
        default:
            elem = std::make_unique<EncGenericElem>(tick, tp, vo);
            break;
        }

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

void EncMeasure::calculateRealDurations(bool hasGraceTimeBorrowing, bool fixDottedEighth)
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
            // Skip same-tick chord members, then near-simultaneous cluster notes.
            while (j < elems.size() && elems[j]->tick == elems[i]->tick) {
                ++j;
            }
            while (j < elems.size()
                   && elems[j]->tick - elems[i]->tick < CHORD_CLUSTER_THRESHOLD) {
                ++j;
            }
            qint16 nextTick = (j < elems.size()) ? elems[j]->tick : durTicks;
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
            if (dur > 0) {
                elems[i]->realDuration = dur;
            }
        }

        // v0xA6 inner-grace marking: NORMAL notes with grace1&0x30==0x10 and fv > leading grace fv
        // are inner graces; isInnerGrace routes them through the grace path in the noteloop.
        if (hasGraceTimeBorrowing) {
            quint8 leadingFv = 0;
            for (EncMeasureElem* e : elems) {
                EncNote* en = dynamic_cast<EncNote*>(e);
                if (!en || en->size != 10) {
                    leadingFv = 0;
                    continue;
                }
                if (en->graceType() != EncGraceType::NORMAL) {
                    if (leadingFv == 0) {
                        leadingFv = en->faceValue & 0x0F;
                    }
                    continue;
                }
                if ((en->grace1 & 0x30) == 0x10 && leadingFv != 0) {
                    const quint8 fv = en->faceValue & 0x0F;
                    if (fv > leadingFv) {
                        en->isInnerGrace = true;
                        leadingFv = std::max(leadingFv, fv);  // keep running max
                        continue;
                    }
                }
                leadingFv = 0;  // regular note ends the grace group
            }
        }
    }

    // v0xC2: Encore places the sixteenth of a dotted-eighth+sixteenth group at tick+120
    // (the undotted gap), not tick+180, so realDuration=120 is indistinguishable from a
    // plain eighth. dotControl bit 0 is also 0, so the bit-0 fallback in computeDotCount
    // does not fire. Fix: detect fv=4/rdur=120 followed by fv=5/rdur=60 exactly 120 ticks
    // later in the same voice, then set dotControl bit 0 to force the dot.
    if (fixDottedEighth) {
        for (auto& [key, elems] : groups) {
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
                            en->dotControl |= 1;
                            break;
                        }
                    }
                }
            }
        }
    }
}
} // namespace mu::iex::encore
