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

#include "reader-v0xc4-base.h"

#include <QDataStream>

#include "elements.h"
#include "encoding.h"

namespace mu::iex::enc {
namespace {

// Scans the first 4096 bytes for PAGE/LINE/MEAS block magic; returns ds.device()->size() if not found.
// includePageBlock=false skips PAGE (used by the compact-layout MIDI scan).
static qint64 findFirstBlockOffset(QDataStream& ds, bool includePageBlock = true)
{
    if (!ds.device()->seek(0)) {
        return ds.device()->size();
    }
    static constexpr int PROBE = 4096;
    const QByteArray buf = ds.device()->read(PROBE);
    for (int i = 0; i <= buf.size() - 4; ++i) {
        const char* p = buf.constData() + i;
        if (includePageBlock && p[0] == 'P' && p[1] == 'A' && p[2] == 'G' && p[3] == 'E') {
            return static_cast<qint64>(i);
        }
        if ((p[0] == 'L' && p[1] == 'I' && p[2] == 'N' && p[3] == 'E')
            || (p[0] == 'M' && p[1] == 'E' && p[2] == 'A' && p[3] == 'S')) {
            return static_cast<qint64>(i);
        }
    }
    return ds.device()->size();
}

void recoverMissingNames(std::vector<EncInstrument>& instruments, QDataStream& ds)
{
    // Primary layout (large-TK and most files): NAME_BASE=202, NAME_STEP=2158.
    // Compact v0xC2 layout: some Encore 3.x/4.x files store names at a different base.
    // The compact instrument table starts at a fixed offset (296 = 0x128) with
    // 112-byte entries; the name field is 18 bytes into each entry -> base = 314,
    // step = 112.  This is tried as a fallback when the primary offset has blank/spaces.
    static constexpr qint64 NAME_BASE = 202;
    static constexpr qint64 NAME_STEP = 2158;
    static constexpr qint64 COMPACT_NAME_BASE = 314;   // 296 (entry start) + 18 (name field)
    static constexpr qint64 COMPACT_NAME_STEP = 112;   // one compact instrument entry

    auto tryReadName = [&](qint64 off) -> QString {
        if (off + 2 >= static_cast<qint64>(ds.device()->size())) {
            return {};
        }
        if (!ds.device()->seek(off)) {
            return {};
        }
        quint8 b0 = 0, b1 = 0;
        ds >> b0 >> b1;
        if (b0 < 0x20 || b0 >= 0x7F) {
            return {};
        }
        const bool isLatin1 = (b1 != 0x00 && b1 >= 0x20 && b1 < 0xFF);
        if (!probeUtf16LE(b0, b1) && !isLatin1) {
            return {};
        }
        if (!ds.device()->seek(off)) {
            return {};
        }
        int remaining = static_cast<int>(ds.device()->size() - off);
        return readEncodedStringRemaining(ds, remaining);
    };

    for (size_t n = 0; n < instruments.size(); ++n) {
        if (!instruments[n].name.isEmpty()) {
            continue;
        }
        // Primary probe.
        const qint64 off = NAME_BASE + static_cast<qint64>(n) * NAME_STEP;
        instruments[n].name = tryReadName(off);
    }

    // Compact-layout fallback: if the primary slot is empty or all-spaces,
    // try the compact base (offset 314, step 112).  Compact entries are stored in
    // REVERSE instrument order (entry 0 = last instrument, entry 1 = second-to-last,
    // ...). This correctly maps the single recoverable entry for the last instrument
    // when earlier entries fall past the first block offset.
    for (size_t k = 0; k < instruments.size(); ++k) {
        const size_t target = instruments.size() - 1 - k;
        if (!instruments[target].name.trimmed().isEmpty()) {
            continue;
        }
        const qint64 cOff = COMPACT_NAME_BASE + static_cast<qint64>(k) * COMPACT_NAME_STEP;
        const QString candidate = tryReadName(cOff);
        if (!candidate.trimmed().isEmpty()) {
            instruments[target].name = candidate;
        }
    }
}

// No-TK layout (instruments[0].contentFilePos < 0).
static void readMidiProgramsNoTk(
    std::vector<EncInstrument>& instruments,
    QDataStream& ds,
    qint64 firstBlockOff)
{
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
    // Recover names even for no-TK files: some compact v0xC2 files store
    // instrument names at header offsets not covered by the MIDI-program read above.
    recoverMissingNames(instruments, ds);
}

// SmallTK layout (0 < offset <= 250).
static void readMidiProgramsSmallTk(
    std::vector<EncInstrument>& instruments,
    QDataStream& ds)
{
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
}

void readMidiPrograms(std::vector<EncInstrument>& instruments, QDataStream& ds)
{
    // MIDI table offsets vary by layout. See ENCORE_FORMAT.md §Instrument block.
    if (instruments.empty()) {
        return;
    }
    const bool noTkBlocks = (instruments[0].contentFilePos < 0);
    if (noTkBlocks) {
        const qint64 firstBlockOff = findFirstBlockOffset(ds, /*includePageBlock=*/true);
        readMidiProgramsNoTk(instruments, ds, firstBlockOff);
        return;
    }
    const bool compact = (instruments[0].offset == 0);
    const bool smallTK = (!compact && instruments[0].offset <= 250);

    if (smallTK) {
        readMidiProgramsSmallTk(instruments, ds);
        return;
    }

    // Large-TK or compact layout.
    qint64 firstBlockOff = ds.device()->size();
    if (compact) {
        firstBlockOff = findFirstBlockOffset(ds, /*includePageBlock=*/false);
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

static void readKeyTranspositionsNoTk(std::vector<EncInstrument>& instruments, QDataStream& ds, qint64 firstBlockOff)
{
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
}

static void readKeyTranspositionsSmallTk(std::vector<EncInstrument>& instruments, QDataStream& ds)
{
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
}

static void readKeyTranspositionsCompact(std::vector<EncInstrument>& instruments, QDataStream& ds)
{
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
}

void readKeyTranspositions(std::vector<EncInstrument>& instruments, QDataStream& ds)
{
    if (instruments.empty()) {
        return;
    }
    const bool noTkBlocks = (instruments[0].contentFilePos < 0);
    if (noTkBlocks) {
        const qint64 firstBlockOff = findFirstBlockOffset(ds, /*includePageBlock=*/true);
        readKeyTranspositionsNoTk(instruments, ds, firstBlockOff);
        return;
    }
    const bool compact = (instruments[0].offset == 0);
    const bool tkBased = (instruments[0].offset > 250);

    if (compact) {
        readKeyTranspositionsCompact(instruments, ds);
        return;
    }

    if (!tkBased) {
        readKeyTranspositionsSmallTk(instruments, ds);
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

bool EncFormatReader_V0xC4Base::readInstrumentMeta(std::vector<EncInstrument>& instruments,
                                                   QDataStream& ds,
                                                   const EncRoot& /*file*/) const
{
    recoverMissingNames(instruments, ds);
    readMidiPrograms(instruments, ds);
    readKeyTranspositions(instruments, ds);
    return true;
}

} // namespace mu::iex::enc
