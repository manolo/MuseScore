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
#include "encoding.h"

namespace mu::iex::encore {
struct EncFormatReader_V0xC4 final : EncFormatReader
{
    explicit EncFormatReader_V0xC4(bool hasMetaTables)
        : m_hasMetaTables(hasMetaTables) {}

    quint32 elemBlockOffset() const override { return 0x36; }
    bool probeInstrumentEncoding() const override { return true; }

    bool supportsImpliedTuplets() const override { return !m_hasMetaTables; }
    bool usesG1LowTieSender() const override { return !m_hasMetaTables; }
    const char* formatName() const override { return m_hasMetaTables ? "v0xC4" : "v0xC2"; }
    bool alMezuroIsReliable() const override { return m_hasMetaTables; }
    // v0xC2 lyric text starts at element offset +18; v0xC4 at +20.
    quint8 lyricTextGapAfterKie() const override { return m_hasMetaTables ? 9 : 7; }

    bool postProcessElement(EncMeasureElem* elem, QDataStream& ds, qint64 rawElemStart) const override
    {
        (void)ds;
        (void)rawElemStart;
        EncNote* en = dynamic_cast<EncNote*>(elem);
        if (!en) {
            return false;
        }
        // Size=22: pitch in tuplet slot (v0xC2 layout). See ENCORE_FORMAT.md §Note element.
        if (en->size == 22) {
            en->semiTonePitch = en->tuplet;
            en->tuplet = 0;
        }
        // Clear artic bytes read beyond boundary for size<27.
        if (en->size < 27) {
            en->articulationUp   = 0;
            en->articulationDown = 0;
        }
        return false;
    }

    bool readInstrumentMeta(std::vector<EncInstrument>& instruments, QDataStream& ds, const EncFile& file) const override;
private:
    bool m_hasMetaTables;
};

namespace {
void recoverMissingNames(std::vector<EncInstrument>& instruments, QDataStream& ds)
{
    static constexpr qint64 NAME_BASE = 202;
    static constexpr qint64 NAME_STEP = 2158;
    for (size_t n = 0; n < instruments.size(); ++n) {
        if (!instruments[n].name.isEmpty()) {
            continue;
        }
        const qint64 off = NAME_BASE + static_cast<qint64>(n) * NAME_STEP;
        if (off + 2 >= static_cast<qint64>(ds.device()->size())) {
            break;
        }
        if (!ds.device()->seek(off)) {
            break;
        }
        quint8 b0 = 0, b1 = 0;
        ds >> b0 >> b1;
        if (b0 < 0x20 || b0 >= 0x7F) {
            continue;
        }
        const bool isLatin1 = (b1 != 0x00 && b1 >= 0x20 && b1 < 0xFF);
        if (!probeUtf16LE(b0, b1) && !isLatin1) {
            continue;
        }
        if (!ds.device()->seek(off)) {
            break;
        }
        int remaining = static_cast<int>(ds.device()->size() - off);
        instruments[n].name = readEncodedStringRemaining(ds, remaining);
    }
}

void readMidiPrograms(std::vector<EncInstrument>& instruments, QDataStream& ds)
{
    // MIDI table offsets vary by layout. See ENCORE_FORMAT.md §Instrument block.
    if (instruments.empty()) {
        return;
    }
    const bool noTkBlocks = (instruments[0].contentFilePos < 0);
    if (noTkBlocks) {
        // Determine compact vs large-TK by scanning for the first block.
        qint64 firstBlockOff = ds.device()->size();
        if (ds.device()->seek(0)) {
            static constexpr int PROBE = 4096;
            const QByteArray buf = ds.device()->read(PROBE);
            for (int i = 0; i <= buf.size() - 4; ++i) {
                const char* p = buf.constData() + i;
                if ((p[0] == 'P' && p[1] == 'A' && p[2] == 'G' && p[3] == 'E')
                    || (p[0] == 'L' && p[1] == 'I' && p[2] == 'N' && p[3] == 'E')
                    || (p[0] == 'M' && p[1] == 'E' && p[2] == 'A' && p[3] == 'S')) {
                    firstBlockOff = static_cast<qint64>(i);
                    break;
                }
            }
        }
        static constexpr qint64 LT_BASE = 2278, LT_STEP = 2158;
        static constexpr qint64 CMP_BASE = 390,  CMP_STEP = 276;
        const bool useLargeTk = (firstBlockOff > LT_BASE);
        const qint64 base = useLargeTk ? LT_BASE : CMP_BASE;
        const qint64 step = useLargeTk ? LT_STEP : CMP_STEP;
        for (size_t n = 0; n < instruments.size(); ++n) {
            const qint64 off = base + static_cast<qint64>(n) * step;
            if (off >= firstBlockOff || off >= static_cast<qint64>(ds.device()->size())) {
                break;
            }
            if (!ds.device()->seek(off)) {
                break;
            }
            quint8 prg;
            ds >> prg;
            if (prg >= 1 && prg <= 128) {
                instruments[n].midiProgram = static_cast<int>(prg);
            }
        }
        return;
    }
    const bool compact = (instruments[0].offset == 0);
    const bool smallTK = (!compact && instruments[0].offset <= 250);

    if (smallTK) {
        // MIDI programs are stored 76 bytes into the extra-data region that follows
        // the TK block content. The content is `instr.offset` bytes long, so the
        // absolute position is contentFilePos + offset + 76.
        static constexpr qint64 MIDI_AFTER_CONTENT = 76;
        for (auto& instr : instruments) {
            if (instr.contentFilePos < 0) {
                continue;
            }
            const qint64 off = instr.contentFilePos + static_cast<qint64>(instr.offset) + MIDI_AFTER_CONTENT;
            if (off >= static_cast<qint64>(ds.device()->size())) {
                continue;
            }
            if (!ds.device()->seek(off)) {
                continue;
            }
            quint8 prg;
            ds >> prg;
            if (prg >= 1 && prg <= 128) {
                instr.midiProgram = static_cast<int>(prg);
            }
        }
        return;
    }

    qint64 firstBlockOff = ds.device()->size();
    if (compact && ds.device()->seek(0)) {
        static constexpr int PROBE = 4096;
        const QByteArray buf = ds.device()->read(PROBE);
        for (int i = 0; i <= buf.size() - 4; ++i) {
            const char* p = buf.constData() + i;
            if ((p[0] == 'L' && p[1] == 'I' && p[2] == 'N' && p[3] == 'E')
                || (p[0] == 'M' && p[1] == 'E' && p[2] == 'A' && p[3] == 'S')) {
                firstBlockOff = static_cast<qint64>(i);
                break;
            }
        }
    }

    const qint64 base = compact ? 390 : 2278;
    const qint64 step = compact ? 276 : 2158;
    for (size_t n = 0; n < instruments.size(); ++n) {
        const qint64 off = base + static_cast<qint64>(n) * step;
        if (off >= firstBlockOff) {
            break;
        }
        if (off >= static_cast<qint64>(ds.device()->size())) {
            break;
        }
        if (!ds.device()->seek(off)) {
            break;
        }
        quint8 prg;
        ds >> prg;
        if (prg >= 1 && prg <= 128) {
            instruments[n].midiProgram = static_cast<int>(prg);
        }
    }
}

void readKeyTranspositions(std::vector<EncInstrument>& instruments, QDataStream& ds)
{
    if (instruments.empty()) {
        return;
    }
    const bool noTkBlocks = (instruments[0].contentFilePos < 0);
    if (noTkBlocks) {
        qint64 firstBlockOff = ds.device()->size();
        if (ds.device()->seek(0)) {
            static constexpr int PROBE = 4096;
            const QByteArray buf = ds.device()->read(PROBE);
            for (int i = 0; i <= buf.size() - 4; ++i) {
                const char* p = buf.constData() + i;
                if ((p[0] == 'P' && p[1] == 'A' && p[2] == 'G' && p[3] == 'E')
                    || (p[0] == 'L' && p[1] == 'I' && p[2] == 'N' && p[3] == 'E')
                    || (p[0] == 'M' && p[1] == 'E' && p[2] == 'A' && p[3] == 'S')) {
                    firstBlockOff = static_cast<qint64>(i);
                    break;
                }
            }
        }
        static constexpr qint64 LT_BASE = 2278, LT_STEP = 2158, KEY_OFF = -23;
        static constexpr qint64 CMP_BASE = 390;
        const bool useLargeTk = (firstBlockOff > LT_BASE);
        if (useLargeTk) {
            for (size_t n = 0; n < instruments.size(); ++n) {
                const qint64 off = LT_BASE + KEY_OFF + static_cast<qint64>(n) * LT_STEP;
                if (off < 0 || off >= static_cast<qint64>(ds.device()->size())) {
                    continue;
                }
                if (!ds.device()->seek(off)) {
                    continue;
                }
                quint8 raw;
                ds >> raw;
                const qint8 sv = static_cast<qint8>(raw);
                if (sv >= -33 && sv <= 24) {
                    instruments[n].keyTransposeSemitones = sv;
                }
            }
        } else {
            if (instruments.size() != 1) {
                return;
            }
            const qint64 off = CMP_BASE + KEY_OFF;
            if (off >= static_cast<qint64>(ds.device()->size())) {
                return;
            }
            if (!ds.device()->seek(off)) {
                return;
            }
            quint8 raw;
            ds >> raw;
            const qint8 sv = static_cast<qint8>(raw);
            if (sv >= -33 && sv <= 24) {
                instruments[0].keyTransposeSemitones = sv;
            }
        }
        return;
    }
    const bool compact = (instruments[0].offset == 0);
    const bool tkBased = (instruments[0].offset > 250);

    if (compact) {
        if (instruments.size() != 1) {
            return;
        }
        static constexpr qint64 MIDI_BASE = 390, KEY_OFF = -23;
        const qint64 off = MIDI_BASE + KEY_OFF;
        if (off >= static_cast<qint64>(ds.device()->size())) {
            return;
        }
        if (!ds.device()->seek(off)) {
            return;
        }
        quint8 raw;
        ds >> raw;
        const qint8 sv = static_cast<qint8>(raw);
        if (sv >= -33 && sv <= 24) {
            instruments[0].keyTransposeSemitones = sv;
        }
        return;
    }

    if (!tkBased) {
        // smallTK layout: key is 23 bytes before the MIDI position (same relative
        // offset as in the large-TK table: KEY_OFF = -23 from MIDI base).
        static constexpr qint64 KEY_AFTER_CONTENT = 76 - 23;   // = 53
        for (auto& instr : instruments) {
            if (instr.contentFilePos < 0) {
                continue;
            }
            const qint64 off = instr.contentFilePos + static_cast<qint64>(instr.offset) + KEY_AFTER_CONTENT;
            if (off < 0 || off >= static_cast<qint64>(ds.device()->size())) {
                continue;
            }
            if (!ds.device()->seek(off)) {
                continue;
            }
            quint8 raw;
            ds >> raw;
            const qint8 sv = static_cast<qint8>(raw);
            if (sv >= -33 && sv <= 24) {
                instr.keyTransposeSemitones = sv;
            }
        }
        return;
    }

    static constexpr qint64 PRG_BASE = 2278, PRG_STEP = 2158, KEY_OFF = -23;
    for (size_t n = 0; n < instruments.size(); ++n) {
        const qint64 off = PRG_BASE + KEY_OFF + static_cast<qint64>(n) * PRG_STEP;
        if (off < 0 || off >= static_cast<qint64>(ds.device()->size())) {
            continue;
        }
        if (!ds.device()->seek(off)) {
            continue;
        }
        quint8 raw;
        ds >> raw;
        const qint8 sv = static_cast<qint8>(raw);
        if (sv >= -33 && sv <= 24) {
            instruments[n].keyTransposeSemitones = sv;
        }
    }
}
} // namespace

bool EncFormatReader_V0xC4::readInstrumentMeta(std::vector<EncInstrument>& instruments,
                                               QDataStream& ds,
                                               const EncFile& /*file*/) const
{
    if (!m_hasMetaTables) {
        return true;
    }
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
