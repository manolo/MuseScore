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
    bool ornC4IsAccent() const override { return !m_hasMetaTables; }
    // v0xC2 lyric text starts at element offset +18; v0xC4 at +20.
    quint8 lyricTextGapAfterKie() const override { return m_hasMetaTables ? 9 : 7; }

    bool postProcessElement(EncMeasureElem* elem, QDataStream& ds, qint64 rawElemStart) const override
    {
        EncNote* en = dynamic_cast<EncNote*>(elem);
        if (!en) {
            return false;
        }
        if (!m_hasMetaTables) {
            // v0xC2: in most files the MIDI pitch is at offset +13 (the tuplet slot) and
            // semiTonePitch is 0; swap them. When tuplet is 0 the pitch is already in the
            // semiTonePitch field (some Encore 4.x files), so leave it untouched.
            // See ENCORE_FORMAT.md §Note element.
            if (en->tuplet > 0) {
                en->semiTonePitch = en->tuplet;
                en->tuplet = 0;
            }
            // size=24 notes carry an articulation byte at +22 (2 bytes after alterGlyph at +21).
            if (en->size == 24 && ds.device()->seek(rawElemStart + 22)) {
                ds >> en->articulationUp;
                en->articulationDown = 0;
            } else {
                en->articulationUp   = 0;
                en->articulationDown = 0;
            }
            return false;
        }
        // v0xC4: clear artic bytes read beyond element boundary for size<27.
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
    // Primary layout (large-TK and most files): NAME_BASE=202, NAME_STEP=2158.
    // Compact v0xC2 layout: some Encore 3.x/4.x files store names at a different base.
    // The compact instrument table starts at a fixed offset (296 = 0x128) with
    // 112-byte entries; the name field is 18 bytes into each entry → base = 314,
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
    // …). This correctly maps the single recoverable entry for the last instrument
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
        // Recover names even for no-TK files: some compact v0xC2 files store
        // instrument names at header offsets not covered by the MIDI-program read above.
        recoverMissingNames(instruments, ds);
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
    // v0xC2 files (m_hasMetaTables=false) have no TK meta tables, but instrument names
    // can still be stored at compact header offsets.  Recover them so the importer does
    // not always fall back to "Part N" names.
    recoverMissingNames(instruments, ds);
    if (!m_hasMetaTables) {
        return true;
    }
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
