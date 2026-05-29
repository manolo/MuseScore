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

#include "reader-v0xc4.h"

#include <QDataStream>

#include "elements.h"

namespace mu::iex::encore {

// Full class definition lives here, not in the header.
struct EncFormatReader_V0xC4 final : EncFormatReader
{
    explicit EncFormatReader_V0xC4(bool hasMetaTables)
        : m_hasMetaTables(hasMetaTables) {}

    quint32 elemBlockOffset() const override { return 0x36; }
    bool probeInstrumentEncoding() const override { return true; }

    bool readInstrumentMeta(std::vector<EncInstrument>& instruments,
                            QDataStream& ds,
                            const EncFile& file) const override;
private:
    bool m_hasMetaTables;
};

namespace {

void recoverMissingNames(std::vector<EncInstrument>& instruments, QDataStream& ds)
{
    static constexpr qint64 NAME_BASE = 202;
    static constexpr qint64 NAME_STEP = 2158;
    for (size_t n = 0; n < instruments.size(); ++n) {
        if (!instruments[n].name.isEmpty()) { continue; }
        const qint64 off = NAME_BASE + static_cast<qint64>(n) * NAME_STEP;
        if (off + 2 >= static_cast<qint64>(ds.device()->size())) { break; }
        if (!ds.device()->seek(off)) { break; }
        quint8 b0 = 0, b1 = 0;
        ds >> b0 >> b1;
        if (b0 < 0x20 || b0 >= 0x7F) { continue; }
        const bool isUtf16  = (b1 == 0x00);
        const bool isLatin1 = (b1 != 0x00 && b1 >= 0x20 && b1 < 0xFF);
        if (!isUtf16 && !isLatin1) { continue; }
        if (!ds.device()->seek(off)) { break; }
        QString recoveredName;
        if (isUtf16) {
            while (!ds.atEnd()) {
                quint8 lo = 0, hi = 0; ds >> lo >> hi;
                const QChar ch = QChar(char16_t((hi << 8) + lo));
                if (ch == u'\0') { break; }
                recoveredName.append(ch);
            }
        } else {
            while (!ds.atEnd()) {
                quint8 b = 0; ds >> b;
                if (b == 0 || b < 0x20) { break; }
                recoveredName.append(QChar(char16_t(b)));
            }
        }
        instruments[n].name = recoveredName;
    }
}

void readMidiPrograms(std::vector<EncInstrument>& instruments, QDataStream& ds)
{
    static constexpr qint64 PRG_BASE = 2278, PRG_STEP = 2158;
    for (size_t n = 0; n < instruments.size(); ++n) {
        const qint64 off = PRG_BASE + static_cast<qint64>(n) * PRG_STEP;
        if (off >= static_cast<qint64>(ds.device()->size())) { break; }
        if (!ds.device()->seek(off)) { break; }
        quint8 prg; ds >> prg;
        if (prg >= 1 && prg <= 128) { instruments[n].midiProgram = static_cast<int>(prg); }
    }
}

void readKeyTranspositions(std::vector<EncInstrument>& instruments, QDataStream& ds)
{
    if (!instruments.empty() && instruments[0].offset <= 250) { return; }
    static constexpr qint64 PRG_BASE = 2278, PRG_STEP = 2158, KEY_OFF = -23;
    for (size_t n = 0; n < instruments.size(); ++n) {
        const qint64 off = PRG_BASE + KEY_OFF + static_cast<qint64>(n) * PRG_STEP;
        if (off < 0 || off >= static_cast<qint64>(ds.device()->size())) { continue; }
        if (!ds.device()->seek(off)) { continue; }
        quint8 raw; ds >> raw;
        const qint8 sv = static_cast<qint8>(raw);
        if (sv >= -33 && sv <= 24) { instruments[n].keyTransposeSemitones = sv; }
    }
}

} // namespace

bool EncFormatReader_V0xC4::readInstrumentMeta(std::vector<EncInstrument>& instruments,
                                               QDataStream& ds,
                                               const EncFile& /*file*/) const
{
    if (!m_hasMetaTables) { return true; }
    recoverMissingNames(instruments, ds);
    readMidiPrograms(instruments, ds);
    readKeyTranspositions(instruments, ds);
    return true;
}

std::unique_ptr<EncFormatReader> makeFormatReader_V0xC4()
{
    return std::make_unique<EncFormatReader_V0xC4>(true);
}

std::unique_ptr<EncFormatReader> makeFormatReader_V0xC2()
{
    return std::make_unique<EncFormatReader_V0xC4>(false);
}

} // namespace mu::iex::encore
