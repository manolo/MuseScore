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

#include "ticks.h"

namespace mu::iex::encore {
// ---------------------------------------------------------------------------
// EncMeasureElem and derived element types
// ---------------------------------------------------------------------------

bool EncMeasureElem::read(QDataStream& ds)
{
    ds >> size >> staffIdx;
    staffIdx &= 0x3F;
    return true;
}

EncGraceType EncNote::graceType() const
{
    quint8 g1 = grace1 & 0x30;
    quint8 g2 = grace2 & 0x05;
    if (g1 == 0x20 && g2 == 0x04) {
        return EncGraceType::ACCIACCATURA;
    }
    if (g1 > 0x10 && g2 != 0x01) {
        return EncGraceType::APPOGGIATURA;
    }
    return EncGraceType::NORMAL;
}

bool EncNote::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);
    bool needsPitchFix = (size == 22);

    ds >> faceValue >> grace1 >> grace2;
    ds.skipRawData(2);
    ds >> xoffset;
    ds.skipRawData(1);
    ds >> position >> tuplet >> dotControl >> semiTonePitch >> playbackDurTicks;
    ds.skipRawData(1);
    ds >> velocity >> options >> alterationGlyph;
    ds.skipRawData(2);
    ds >> articulationUp;
    ds.skipRawData(1);
    ds >> articulationDown;
    int toSkip = static_cast<int>(size) - 27;
    if (toSkip > 0) {
        ds.skipRawData(toSkip);
    }
    if (needsPitchFix) {
        semiTonePitch = tuplet;
        tuplet = 0;
    }
    // articulationUp is at byte +24, articulationDown at +26.
    // Elements shorter than 27 bytes don't contain valid data at those offsets;
    // zero out to suppress spurious articulation glyphs (e.g. size-22 v0xA6 notes
    // whose padding bytes contain 0x20, which maps to fermataAbove).
    if (size < 27) {
        articulationUp   = 0;
        articulationDown = 0;
    }
    return true;
}

bool EncRest::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);
    ds >> faceValue;
    ds.skipRawData(4);
    ds >> xoffset;
    ds.skipRawData(2);
    ds >> tuplet >> dotControl;
    int toSkip = static_cast<int>(size) - 10 - 5;
    if (toSkip > 0) {
        ds.skipRawData(toSkip);
    }
    return true;
}

bool EncChordSym::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);
    ds >> toniko >> tipo;
    ds.skipRawData(3);
    ds >> xoffset;
    ds.skipRawData(1);
    ds >> radiko >> baso;
    const bool hasText = (tipo & 1);
    if (hasText) {
        // The chord text occupies a fixed 36-byte slot. Encoding is detected
        // per element to match the LYRIC / TK-name / TEXT-block convention:
        // a printable ASCII byte followed by 0x00 means UTF-16 LE (the
        // modern Encore 5 default); otherwise Latin-1 (single byte per
        // char, present in legacy Spanish/Portuguese scores). Forcing
        // UTF-16 on a Latin-1 payload merges byte pairs into one BMP code
        // unit and produces Chinese-looking gibberish.
        QByteArray payload(36, 0);
        const int got = ds.readRawData(payload.data(), 36);
        if (got > 0) {
            const quint8 b0 = static_cast<quint8>(payload[0]);
            const quint8 b1 = got >= 2 ? static_cast<quint8>(payload[1]) : 0;
            const bool isUtf16 = (b0 >= 0x20 && b0 < 0x7F && b1 == 0x00);
            if (isUtf16) {
                for (int j = 0; j + 1 < got; j += 2) {
                    const quint8 lo = static_cast<quint8>(payload[j]);
                    const quint8 hi = static_cast<quint8>(payload[j + 1]);
                    const QChar ch = QChar(char16_t((hi << 8) | lo));
                    if (ch == u'\0') {
                        break;
                    }
                    teksto.append(ch);
                }
            } else {
                for (int j = 0; j < got; ++j) {
                    const quint8 b = static_cast<quint8>(payload[j]);
                    if (b == 0) {
                        break;
                    }
                    teksto.append(QChar(char16_t(b)));
                }
            }
        }
        int toSkip = static_cast<int>(size) - 5 - 9 - 36;
        if (toSkip > 0) {
            ds.skipRawData(toSkip);
        }
    } else {
        int toSkip = static_cast<int>(size) - 5 - 9;
        if (toSkip > 0) {
            ds.skipRawData(toSkip);
        }
    }
    return true;
}

bool EncOrnament::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);
    ds >> tipo;
    ds.skipRawData(4);
    ds >> xoffset;
    ds.skipRawData(1);
    ds >> yoffset;
    ds.skipRawData(4);
    ds >> alMezuro;
    ds.skipRawData(1);
    ds >> xoffset2;
    ds.skipRawData(5);
    ds >> speguleco;
    speguleco &= 3;
    ds.skipRawData(1);
    ds >> noto;
    ds.skipRawData(1);
    ds >> tempo;
    // Size-32 ornaments (v0xC2) end at byte[31]; tind overlaps with tempo at byte[30].
    // Size-33+ ornaments have a skip byte + tind byte after tempo.
    if (static_cast<int>(size) >= 33) {
        ds.skipRawData(1);
        ds >> tind;
    } else {
        tind = tempo;
    }
    int toSkip = static_cast<int>(size) - 5 - (static_cast<int>(size) >= 33 ? 28 : 26);
    if (toSkip > 0) {
        ds.skipRawData(toSkip);
    }
    return true;
}

bool EncLyric::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);   // consumed: size + staffIdx (5 bytes from elemStart)

    // Layout (offsets from elemStart):
    //   +0..+1: tick
    //   +2:     type/voice
    //   +3:     size (full element span; variable!)
    //   +4:     staffIdx
    //   +5..+9: 5 unknown bytes
    //   +0xA:   kie (text anchor; enc2ly's "adr0 + 0x05" where adr0 = +5)
    //   +0xB..+0x13: 9 unknown bytes
    //   +0x14..: text, null-terminated, followed by 0..6 bytes of padding
    //           so the element occupies `size` bytes total. Encoding is
    //           detected per element: UTF-16 LE in modern v0xC4 files, but
    //           Latin-1 (1 byte/char) still appears in v0xC4 lyrics of
    //           Portuguese/Spanish scores (e.g. Fe_cega_faca_amolada_tk.enc
    //           stores "txã" as 74 78 E3 00...).
    int remaining = static_cast<int>(size) - 5;
    if (remaining < 15) {
        if (remaining > 0) {
            ds.skipRawData(remaining);
        }
        return true;
    }

    ds.skipRawData(5);
    ds >> kie;
    ds.skipRawData(9);
    remaining -= 15;

    QString s;
    if (remaining >= 2) {
        // Encoding probe (mirrors EncInstrument::read): a printable ASCII
        // byte followed by 0x00 is UTF-16 LE; otherwise Latin-1.
        const qint64 savedPos = ds.device()->pos();
        quint8 b0 = 0, b1 = 0;
        ds >> b0 >> b1;
        ds.device()->seek(savedPos);
        const bool isUtf16 = (b0 >= 0x20 && b0 < 0x7F && b1 == 0x00);

        if (isUtf16) {
            while (remaining >= 2) {
                quint8 lo = 0, hi = 0;
                ds >> lo >> hi;
                remaining -= 2;
                const char16_t ch = static_cast<char16_t>((hi << 8) | lo);
                if (ch == 0) {
                    break;
                }
                s.append(QChar(ch));
            }
        } else {
            while (remaining >= 1) {
                quint8 b = 0;
                ds >> b;
                remaining -= 1;
                if (b == 0) {
                    break;
                }
                s.append(QChar(static_cast<char16_t>(b)));
            }
        }
    }
    text = s;

    if (remaining > 0) {
        ds.skipRawData(remaining);
    }
    return true;
}

bool EncKeyChange::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);
    ds >> tipo;
    int toSkip = static_cast<int>(size) - 5 - 1;
    if (toSkip > 0) {
        ds.skipRawData(toSkip);
    }
    return true;
}

bool EncGenericElem::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);
    int toSkip = static_cast<int>(size) - 5;
    if (toSkip > 0) {
        ds.skipRawData(toSkip);
    }
    return true;
}

bool EncTie::read(QDataStream& ds)
{
    EncMeasureElem::read(ds);   // reads size + staffIdx
    quint8 dirByte = 0;
    quint8 startFlag = 0;
    if (size > 5) {
        ds >> dirByte;          // tie arc direction byte at element offset +5
    }
    if (size > 6) {
        ds >> startFlag;        // tie-start flag at element offset +6
    }
    // Byte +5: arc direction. Bit 7 (0x80) = arc-above outgoing; bit 1 (0x02) = arc-below outgoing.
    // Values 0x02 and 0x03 both carry bit 1 and mean "tie starts here, arc drawn below".
    // Byte +6: tie-start flag (high bit = sends tie forward).
    // Any outgoing indicator means TIE-START.
    isTieStart = ((dirByte & 0x80) != 0) || ((startFlag & 0x80) != 0) || ((dirByte & 0x02) != 0);
    int consumed = (size > 5 ? 1 : 0) + (size > 6 ? 1 : 0);
    int toSkip = static_cast<int>(size) - 5 - consumed;
    if (toSkip > 0) {
        ds.skipRawData(toSkip);
    }
    return true;
}

} // namespace mu::iex::encore
