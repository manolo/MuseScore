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

#include "elem.h"
#include "readers.h"

namespace mu::iex::enc {
// ---------------------------------------------------------------------------
// EncHeader
// ---------------------------------------------------------------------------

bool EncHeader::readMagicAndVersion(QDataStream& ds)
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
    return true;
}

bool EncHeader::read(QDataStream& ds, const EncFormatReader& fmt)
{
    ds.skipRawData(0x28 - 5);   // from +5 (just past the 5-byte magic) to the header fields at 0x28
    ds >> chuVersio >> nekon1 >> fiksa1 >> lineCount >> pageCount;
    ds >> instrumentCount >> staffPerSystem >> measureCount;
    // Global staff-size selector (1=small … 4=default). v0xC2/C4/C5 store it at 0x52;
    // v0xA6 stores it at 0x8D (byte 0x52 is an unrelated field there). Offset from fmt.
    const qint64 szOff = fmt.scoreSizeOffset();
    if (fmt.headerEnd() > szOff) {
        ds.skipRawData(static_cast<int>(szOff - 0x36));   // skip from 0x36 to the size byte
        quint8 sz;
        ds >> sz;
        if (sz >= 1 && sz <= 4) {
            scoreSize = sz;
        }
        ds.skipRawData(static_cast<int>(fmt.headerEnd() - szOff - 1)); // skip to header end
    } else {
        ds.skipRawData(static_cast<int>(fmt.headerEnd() - 0x36));
    }
    return true;
}

// ---------------------------------------------------------------------------
// EncTextBlock - indexed text payload for STAFFTEXT 0x1E ornaments
// ---------------------------------------------------------------------------

bool EncTextBlock::read(QDataStream& ds, quint32 varSize)
{
    // See ENCORE_FORMAT.md §TEXT block for layout. Entry N referenced by ORN tind byte (+32).
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
        // Payload text at +14, probe picks UTF-16 LE or Latin-1; see ENCORE_FORMAT.md §Encoding probe.
        QString text;
        if (entrySize >= 16) {
            const quint8 b14 = static_cast<quint8>(payload[14]);
            const quint8 b15 = static_cast<quint8>(payload[15]);
            const bool isUtf16 = (b14 >= 0x20 && b14 < 0x7F && b15 == 0x00);
            // Decode the whole text region from +14, then post-process. Multi-line
            // comments separate lines with U+0004 and terminate the string with a
            // U+0000 null. The previous code stopped at the first U+0004, truncating
            // every line but the first; see ENCORE_FORMAT.md §TEXT block.
            const int textBytes = entrySize - 14;
            if (textBytes > 0) {
                if (isUtf16) {
                    text = QString::fromUtf16(
                        reinterpret_cast<const char16_t*>(payload.constData() + 14),
                        textBytes / 2);
                } else {
                    text = QString::fromLatin1(payload.constData() + 14, textBytes);
                }
                // Truncate at the U+0000 null terminator.
                int nullIdx = text.indexOf(QChar(QChar::Null));
                if (nullIdx >= 0) {
                    text = text.left(nullIdx);
                }
                // U+0004 separates lines within a comment; convert to newline.
                text.replace(QChar(0x0004), QChar(u'\n'));
                // Each line (including the last) is followed by U+0004, leaving a
                // trailing newline after conversion; drop trailing newlines.
                while (text.endsWith(QChar(u'\n'))) {
                    text.chop(1);
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
} // namespace mu::iex::enc
