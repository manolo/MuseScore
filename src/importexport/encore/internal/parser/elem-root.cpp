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

namespace mu::iex::enc {
// ---------------------------------------------------------------------------
// EncRoot - top-level container
// ---------------------------------------------------------------------------

bool isInstrumentMagic(const QString& magic)
{
    return magic.length() == 4
           && magic.at(0) == 'T' && magic.at(1) == 'K'
           && magic.at(2).isDigit() && magic.at(3).isDigit();
}

bool isKnownMagic(const QString& magic)
{
    return magic == "LINE" || magic == "MEAS" || magic == "TITL" || magic == "TEXT"
           || magic == "WINI" || isInstrumentMagic(magic);
}

QString findNextKnownMagic(QDataStream& ds)
{
    QString magic;
    for (int i = 0; i < 4 && !ds.atEnd(); ++i) {
        quint8 ch;
        ds >> ch;
        magic.append(QChar(ch));
    }
    // Limit scan to 1 MiB; TK blocks max ~2 KiB, more gap means corrupt file.
    constexpr int kMaxScanBytes = 1 << 20;
    int scanned = 0;
    while (!isKnownMagic(magic) && !ds.atEnd() && scanned < kMaxScanBytes) {
        magic.remove(0, 1);
        quint8 ch;
        ds >> ch;
        magic.append(QChar(ch));
        ++scanned;
    }
    if (!isKnownMagic(magic)) {
        magic.clear();
    }
    return magic;
}

void addSpannerEnds(std::vector<EncMeasure>& measures)
{
    std::vector<MeasureElemVec> extra(measures.size());

    for (size_t i = 0; i < measures.size(); ++i) {
        for (const auto& elem : measures[i].elements) {
            EncMeasureElem* e = elem.get();
            if (auto* orna = dynamic_cast<EncOrnament*>(e)) {
                EncOrnamentType ot = orna->ornType();
                if (ot == EncOrnamentType::SLURSTART || ot == EncOrnamentType::WEDGESTART) {
                    EncOrnamentType endType = (ot == EncOrnamentType::SLURSTART)
                                              ? EncOrnamentType::SLURSTOP
                                              : EncOrnamentType::WEDGESTOP;
                    auto endOrna = std::make_unique<EncOrnament>(*orna);
                    endOrna->setOrnType(endType);
                    endOrna->xoffset = orna->xoffset2;
                    int endMeas = static_cast<int>(i) + orna->alMezuro;
                    if (endMeas >= 0 && static_cast<size_t>(endMeas) < extra.size()) {
                        extra[endMeas].push_back(std::move(endOrna));
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < measures.size(); ++i) {
        for (auto& e : extra[i]) {
            measures[i].elements.push_back(std::move(e));
        }
    }
}

bool EncRoot::read(QDataStream& ds)
{
    if (!header.readMagicAndVersion(ds)) {
        return false;
    }
    fmt = EncFormatReader::create(header.chuMagio);
    if (!header.read(ds, *fmt)) {
        return false;
    }
    EncCharSize charsize = EncCharSize::ONE_BYTE;

    while (!ds.atEnd()) {
        QString nextId = findNextKnownMagic(ds);
        if (nextId.isEmpty()) {
            break;
        }
        quint32 varSize;
        ds >> varSize;

        if (nextId == "LINE") {
            EncLine line;
            line.read(ds, varSize, header.staffPerSystem);
            lines.push_back(std::move(line));
        } else if (nextId == "MEAS") {
            EncMeasure meas;
            meas.read(ds, varSize, *fmt);
            meas.calculateRealDurations(fmt->hasGraceTimeBorrowing(),
                                    fmt->supportsImpliedTuplets());
            // Skip extra "ghost" MEAS blocks beyond the declared measureCount.
            if (header.measureCount > 0
                && static_cast<int>(measures.size()) >= header.measureCount) {
                continue;
            }
            measures.push_back(std::move(meas));
        } else if (nextId == "TITL") {
            titleBlock.read(ds, varSize, charsize);
        } else if (nextId == "TEXT") {
            textBlock.read(ds, varSize);
        } else if (nextId == "WINI") {
            // WINI: page setup block. Layout: 21 uint16 LE values (42 bytes).
            // Margins as int32 LE (two adjacent uint16s, high word always 0):
            //   [12,13] = top margin, [14,15] = left margin,
            //   [16,17] = page_height_pts - bottom_margin, [18,19] = page_width_pts - right_margin.
            // All values in typographic points (1/72 inch).
            if (varSize >= 40) {
                qint32 top, left, bottomEdge, rightEdge;
                ds.skipRawData(24);   // skip fields 0..11 (window/screen data)
                ds >> top >> left >> bottomEdge >> rightEdge;
                ds.skipRawData(static_cast<int>(varSize) - 40);
                if (bottomEdge > 0 && rightEdge > 0 && bottomEdge > top && rightEdge > left) {
                    pageSetup.hasData    = true;
                    pageSetup.top        = top;
                    pageSetup.left       = left;
                    pageSetup.bottomEdge = bottomEdge;
                    pageSetup.rightEdge  = rightEdge;
                }
            } else {
                ds.skipRawData(varSize);
            }
        } else if (isInstrumentMagic(nextId)) {
            EncInstrument instr;
            instr.contentFilePos = ds.device()->pos();
            // v0xA6: Key transposition is at content+42; read before EncInstrument::read.
            // v0xC4 reads Key from outside the TK block in readInstrumentMeta instead.
            fmt->readKeyFromTKBlock(instr, ds, ds.device()->pos());
            // v0xC4: Encore 5.0.2 may use UTF-16 LE names; probe determines the encoding.
            instr.read(ds, varSize, fmt->probeInstrumentEncoding());
            charsize = instr.charSize();
            instruments.push_back(std::move(instr));
        } else {
            ds.skipRawData(varSize);
        }
    }

    if (instruments.empty()) {
        // No TK blocks found; seed empty entries so readInstrumentMeta can recover names.
        for (int i = 0; i < header.instrumentCount; ++i) {
            instruments.emplace_back();
        }
    }

    // Pad to instrumentCount: some v0xC4 files have fewer TK blocks than declared.
    while (static_cast<int>(instruments.size()) < header.instrumentCount) {
        instruments.emplace_back();
    }

    fmt->readInstrumentMeta(instruments, ds, *this);

    // "Part N" fallback for any instrument whose name is still empty after recovery.
    for (int i = 0; i < static_cast<int>(instruments.size()); ++i) {
        if (instruments[i].name.isEmpty()) {
            instruments[i].name = QString("Part %1").arg(i + 1);
        }
    }

    // Grand-staff instruments have two LINE entries with same instrumentIndex(), staffIndex() 0 and 1.
    if (!lines.empty()) {
        for (const auto& lsd : lines[0].staffData) {
            const int ii = static_cast<int>(lsd.instrumentIndex());
            const int si = static_cast<int>(lsd.staffIndex());
            if (ii >= 0 && ii < static_cast<int>(instruments.size())) {
                instruments[ii].nstaves = std::max(instruments[ii].nstaves, si + 1);
            }
        }
    }

    addSpannerEnds(measures);
    return true;
}
} // namespace mu::iex::enc
