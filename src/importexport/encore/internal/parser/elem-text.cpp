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


namespace mu::iex::encore {
// ---------------------------------------------------------------------------
// EncHeader
// ---------------------------------------------------------------------------

bool EncHeader::read(QDataStream& ds)
{
    for (int i = 0; i < 4; ++i) {
        quint8 ch;
        ds >> ch;
        magic.append(QChar(ch));
    }
    if (magic == "SCOW") {
        ds.setByteOrder(QDataStream::LittleEndian);
    } else if (magic == "SCO5") {
        ds.setByteOrder(QDataStream::BigEndian);
    } else {
        return false;
    }
    ds >> chuMagio;
    ds.skipRawData(0x28 - 5);
    ds >> chuVersio >> nekon1 >> fiksa1 >> lineCount >> pageCount;
    ds >> instrumentCount >> staffPerSystem >> measureCount;
    // v0xA6 header ends at 0xA6 (TK00 starts there). v0xC2/v0xC4 go to 0xC2.
    // Reading past 0xC2 on v0xA6 would consume TK00 and shift all instrument slots.
    const qint64 headerEnd = isVeryOldFormat() ? 0xA6 : 0xC2;
    ds.skipRawData(headerEnd - 0x36);
    return true;
}

// ---------------------------------------------------------------------------
// EncTextBlock - indexed text payload for STAFFTEXT 0x1E ornaments
// ---------------------------------------------------------------------------

bool EncTextBlock::read(QDataStream& ds, quint32 varSize)
{
    // Block layout (varSize bytes total):
    //   +0..+1: 0x0000 sync
    //   +2..+3: entry count
    //   +4..+7: content size (= sum of all entries)
    //   then `count` entries; each:
    //     +0..+1: payload size S
    //     +2..+S+1: payload
    //       +0..+13: 14 bytes of fields not fully decoded
    //       +14..+S-5: UTF-16 LE text
    //       +S-4..+S-1: 0x04 0x00 0x00 0x00 terminator
    //
    // The N-th entry is referenced by an ornament's `tind` byte (+32).
    // Encore writes the text in storage order regardless of measure order;
    // the ornament's tind picks the matching entry directly.
    if (varSize < 8) {
        ds.skipRawData(varSize);
        return true;
    }
    quint16 sync = 0;
    quint16 count = 0;
    quint32 contentSize = 0;
    ds >> sync >> count >> contentSize;
    quint32 consumed = 8;
    entries.clear();
    entries.reserve(count);
    for (quint16 i = 0; i < count && consumed + 2 <= varSize; ++i) {
        quint16 entrySize = 0;
        ds >> entrySize;
        consumed += 2;
        if (entrySize == 0 || consumed + entrySize > varSize) {
            break;
        }
        QByteArray payload(entrySize, 0);
        int rd = ds.readRawData(payload.data(), entrySize);
        if (rd != entrySize) {
            break;
        }
        consumed += entrySize;
        // Text at payload[14], ends at first 0x04 0x00 terminator.
        // Probe: printable b14 + b15==0 → UTF-16 LE; else Latin-1.
        // Old reader forced UTF-16 and mis-decoded Latin-1 (e.g. "la 1ª vez" → gibberish).
        QString text;
        if (entrySize >= 16) {
            const quint8 b14 = static_cast<quint8>(payload[14]);
            const quint8 b15 = static_cast<quint8>(payload[15]);
            const bool isUtf16 = (b14 >= 0x20 && b14 < 0x7F && b15 == 0x00);
            // Scan for the `04 00` terminator starting at offset 14.
            int textEnd = entrySize;
            for (int i = 14; i + 1 < entrySize; ++i) {
                if (static_cast<quint8>(payload[i]) == 0x04
                    && static_cast<quint8>(payload[i + 1]) == 0x00) {
                    textEnd = i;
                    break;
                }
            }
            const int textBytes = textEnd - 14;
            if (textBytes > 0) {
                if (isUtf16) {
                    text = QString::fromUtf16(
                        reinterpret_cast<const char16_t*>(payload.constData() + 14),
                        textBytes / 2);
                } else {
                    text = QString::fromLatin1(payload.constData() + 14, textBytes);
                }
                int nullIdx = text.indexOf(QChar(QChar::Null));
                if (nullIdx >= 0) {
                    text = text.left(nullIdx);
                }
            }
        }
        entries.push_back(text);
    }
    // Skip any remaining bytes inside the block (padding or unparsed tail).
    if (consumed < varSize) {
        ds.skipRawData(varSize - consumed);
    }
    return true;
}

} // namespace mu::iex::encore
