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

#include "encoding.h"

namespace mu::iex::enc {
// ---------------------------------------------------------------------------
// EncInstrument
// ---------------------------------------------------------------------------

bool EncInstrument::read(QDataStream& ds, quint32 vs, bool probeEncoding)
{
    offset = vs & 0xFFFF;
    // Encoding probe overrides charSize(); see ENCORE_FORMAT.md §Encoding probe.
    EncCharSize cs = charSize();
    if (probeEncoding) {
        const qint64 savedPos = ds.device()->pos();
        quint8 b0 = 0, b1 = 0;
        ds >> b0 >> b1;
        ds.device()->seek(savedPos);
        if (probeUtf16LE(b0, b1)) {
            cs = EncCharSize::TWO_BYTES;
        } else if (b0 >= 0x20 && b0 < 0x7F && b1 != 0x00 && b1 >= 0x20 && b1 < 0xFF) {
            cs = EncCharSize::ONE_BYTE;
        }
    }
    int nread = 8;
    QChar ch;
    bool done = false;
    while (!done) {
        if (cs == EncCharSize::ONE_BYTE) {
            quint8 b;
            ds >> b;
            ch = QChar(char16_t(b));
            nread += 1;
        } else {
            quint8 lo, hi;
            ds >> lo >> hi;
            ch = QChar(char16_t((hi << 8) + lo));
            nread += 2;
        }
        if (ch == '\0') {
            done = true;
        } else {
            name.append(ch);
        }
    }
    int toSkip = static_cast<int>(offset) - nread;
    if (toSkip > 0) {
        ds.skipRawData(toSkip);
    }
    return true;
}

// ---------------------------------------------------------------------------
// EncLineStaffData / EncLine
// ---------------------------------------------------------------------------

bool EncLineStaffData::read(QDataStream& ds)
{
    // Bytes 0-13: visual layout data (Y-coordinates for staff lines, etc.)
    ds.skipRawData(14);
    qint8 ct;
    ds >> ct;                                   // byte 14: clef type
    clef = static_cast<EncClefType>(ct);
    ds >> key >> pageIdx;                       // bytes 15-16
    quint8 skip0, skip1, showByte;
    ds >> skip0 >> skip1 >> showByte;           // bytes 17-19
    showStaff = (showByte != 0);
    (void)skip0;
    (void)skip1;
    quint8 st;
    ds >> st;                                   // byte 20: staff type (MELODY/TAB/RHYTHM)
    staffType = static_cast<EncStaffType>(st);
    ds >> instrStaffIdx;                        // byte 21
    ds.skipRawData(8);                          // bytes 22-29
    return true;
}

bool EncLine::read(QDataStream& ds, quint32 vs, int staffPerSystem)
{
    offset = vs;
    ds.skipRawData(10);
    ds >> start >> measureCount;
    for (int i = 0; i < staffPerSystem; ++i) {
        EncLineStaffData lsd;
        lsd.read(ds);
        staffData.push_back(lsd);
    }
    const int toSkip = static_cast<int>(offset) + 8 - 21 - 30 * staffPerSystem;
    if (toSkip > 0) {
        ds.skipRawData(toSkip);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Title block
// ---------------------------------------------------------------------------

// Read 30-byte prefix + text payload of one TITL line.
// Alignment at prefix+14: 0x02=right, 0x04=left, 0x06=center; 0x00=LEFT.
static EncHeaderFooter readTitleLine(QDataStream& ds, EncCharSize cs)
{
    QByteArray prefix(30, 0);
    int got = ds.readRawData(prefix.data(), 30);
    quint8 alignByte = (got >= 15) ? static_cast<quint8>(prefix[14]) : 0;

    QString item;
    bool done = false;
    if (cs == EncCharSize::ONE_BYTE) {
        for (int j = 0; j < 66; ++j) {
            quint8 b;
            ds >> b;
            if (b == 0) {
                done = true;
            }
            if (!done) {
                item.append(QChar(char16_t(b)));
            }
        }
    } else {
        for (int j = 0; j < 1026;) {
            quint8 lo, hi;
            ds >> lo;
            ++j;
            ds >> hi;
            ++j;
            QChar ch = QChar(char16_t((hi << 8) + lo));
            if (ch == '\0') {
                done = true;
            }
            if (!done) {
                item.append(ch);
            }
        }
    }

    EncHeaderFooter out;
    out.text = item;
    switch (alignByte) {
    case static_cast<quint8>(EncTextAlign::CENTER): out.align = EncTextAlign::CENTER;
        break;
    case static_cast<quint8>(EncTextAlign::RIGHT):  out.align = EncTextAlign::RIGHT;
        break;
    default:                                        out.align = EncTextAlign::LEFT;
        break;
    }
    return out;
}

QString readTextItem(QDataStream& ds, EncCharSize cs)
{
    return readTitleLine(ds, cs).text;
}

bool EncTitle::read(QDataStream& ds, quint32 vs, EncCharSize cs)
{
    // Detect encoding from varsize alone (not from TK-derived cs):
    //   ONE_BYTE  layout: 2 + 20×96   + 504 =  2 426
    //   TWO_BYTES layout: 2 + 20×1056 + 120 = 21 242
    // Values differ by 10×, so varsize resolves unambiguously.
    // Encore 5.0.2 can write UTF-16 in TITL even when TK offset ≤ 250.
    if (vs >= 10000) {
        cs = EncCharSize::TWO_BYTES;
    } else if (vs > 0 && vs < 5000) {
        cs = EncCharSize::ONE_BYTE;
    }
    // Some files save two identical TITL blocks. Clear vectors so the
    // second read replaces the first instead of doubling all lines.
    subtitle.clear();
    instruction.clear();
    author.clear();
    header.clear();
    footer.clear();
    copyright.clear();

    ds.skipRawData(2);
    title = readTextItem(ds, cs);
    for (int i = 0; i < 2; ++i) {
        subtitle.push_back(readTextItem(ds, cs));
    }
    for (int i = 0; i < 3; ++i) {
        instruction.push_back(readTextItem(ds, cs));
    }
    for (int i = 0; i < 4; ++i) {
        author.push_back(readTextItem(ds, cs));
    }
    for (int i = 0; i < 2; ++i) {
        header.push_back(readTitleLine(ds, cs));
    }
    for (int i = 0; i < 2; ++i) {
        footer.push_back(readTitleLine(ds, cs));
    }
    for (int i = 0; i < 6; ++i) {
        copyright.push_back(readTextItem(ds, cs));
    }
    ds.skipRawData(cs == EncCharSize::ONE_BYTE ? 504 : 120);
    return true;
}
} // namespace mu::iex::enc
