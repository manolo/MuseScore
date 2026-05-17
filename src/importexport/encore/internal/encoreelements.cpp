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

#include "encoreelements.h"

#include <algorithm>

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
    // articulationUp/Down sit near the end of the NOTE slot. v0xA6 NOTEs are
    // 10 bytes long, well before that offset, so the reads above consume
    // bytes from the next slot's preamble (tick, typeVoice, size,
    // faceValue, ...). Zero the fields out for the formats that don't
    // actually carry them; downstream creates fingerings, articulations,
    // tremolos, fermatas, open-string markers off these bytes and would
    // otherwise sprinkle thousands of spurious glyphs across the score.
    if (size < 22) {
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
        bool done = false;
        for (int j = 0; j < 2 * 18;) {
            quint8 lower, upper;
            ds >> lower;
            ++j;
            ds >> upper;
            ++j;
            QChar ch = QChar(char16_t((upper << 8) + lower));
            if (ch == '\0') {
                done = true;
            }
            if (!done) {
                teksto.append(ch);
            }
        }
        int toSkip = static_cast<int>(size) - 5 - 9 - 2 * 18;
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
    ds.skipRawData(1);
    ds >> tind;
    int toSkip = static_cast<int>(size) - 5 - 28;
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
    // The TIE element appears in both halves of a tie. Encore stores the
    // arc-direction byte at offset +5 (low bit = arc-only endpoint, high
    // bit = outgoing tie) AND a separate tie-start flag at +6 (high bit
    // set when the note sends a tie forward, regardless of arc direction).
    // The Beethoven Sinfonia 7 II Allegretto Plectro corpus showed that
    // ~32 % of outgoing ties (50/157) carry the high bit on +6 with +5 as
    // an arc-only value (e.g. 0x04). Recognising either byte's high bit
    // as a tie-start marker recovers them; END-only markers are still
    // dropped (no high bit set on either byte) since the receiving note
    // is matched by (staffIdx, voice, pitch) when it is placed.
    isTieStart = ((dirByte & 0x80) != 0) || ((startFlag & 0x80) != 0);
    int consumed = (size > 5 ? 1 : 0) + (size > 6 ? 1 : 0);
    int toSkip = static_cast<int>(size) - 5 - consumed;
    if (toSkip > 0) {
        ds.skipRawData(toSkip);
    }
    return true;
}

// ---------------------------------------------------------------------------
// EncMeasure
// ---------------------------------------------------------------------------

bool EncMeasure::read(QDataStream& ds, const quint32 vs, bool /*oldFormat*/, bool veryOldFormat)
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

    // v0xA6 (very old) blocks: header ends at 0x1A, elements start there, giving
    // measEnd = measStart + varsize + 0x1A (confirmed from block-gap analysis).
    // v0xC2/v0xC4: elements start at 0x36, measEnd = measStart + varsize + 0x36.
    qint64 elemOffset = veryOldFormat ? 0x1A : 0x36;
    ds.device()->seek(measStart + elemOffset);
    qint64 measEnd = measStart + varsize + elemOffset;

    quint16 tick;
    ds >> tick;
    if (tick == 0xFFFF) {
        ds.device()->seek(measEnd);
        return true;
    }

    const int MAX_ELEMENTS = 10000;   // guard against a corrupt size field
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
            elem = std::make_unique<EncLyric>(tick, tp, vo);
            break;
        case EncElemType::KEYCHANGE:
            elem = std::make_unique<EncKeyChange>(tick, tp, vo);
            break;
        case EncElemType::TIE:
            elem = std::make_unique<EncTie>(tick, tp, vo);
            break;
        case EncElemType::NONE:
        case EncElemType::CLEF:
        case EncElemType::BEAM:
        case EncElemType::UNKNOWN1:
        case EncElemType::UNKNOWN2:
        default:
            elem = std::make_unique<EncGenericElem>(tick, tp, vo);
            break;
        }

        elem->read(ds);
        EncMeasureElem* elemRaw = elem.get();

        // In v0xA6 (size=10), MIDI pitch is at byte +11 (absolute MIDI
        // number, not an offset). Real Encore 2.x files store the
        // playable MIDI pitch there directly. Earlier code assumed byte
        // +9 carried a signed offset from C4=60; that turned out to be
        // a staff-position-like field on real files (e.g. 11 for B4 in
        // treble clef -- diatonic line count rather than chromatic).
        if (veryOldFormat && static_cast<EncElemType>(tp) == EncElemType::NOTE) {
            EncNote* en = static_cast<EncNote*>(elemRaw);
            if (en->size == 10) {
                qint64 savedPos = ds.device()->pos();
                ds.device()->seek(elemStart + 11);
                quint8 pitchByte;
                ds >> pitchByte;
                en->semiTonePitch = pitchByte;
                // v0xA6 also stores the tuplet byte at +7 (where v0xC4 has
                // grace2). The v0xC4-shaped EncNote::read picks up byte
                // +13 which lands in padding for v0xA6 (= 0). Override
                // with the real byte so explicit triplets (0x32) are
                // recognised.
                ds.device()->seek(elemStart + 7);
                quint8 tupByte;
                ds >> tupByte;
                en->tuplet = tupByte;
                ds.device()->seek(savedPos);
            }
        }

        // v0xA6 occasionally stores two identical REST elements back-to-
        // back at the same tick/staff/voice/faceValue (observed once per
        // ~500 rests on real Encore 2.x files). Encore renders the pair
        // as a single rest, so the importer must dedupe them; otherwise
        // the second rest pushes cumTick past the measure end and the
        // following note spills into a second MuseScore voice. Only
        // consecutive duplicates from the same staff/voice/fv collapse;
        // genuine multi-voice rests on different voices stay.
        if (static_cast<EncElemType>(tp) == EncElemType::REST && veryOldFormat
            && !elements.empty()) {
            const EncMeasureElem* prev = elements.back().get();
            const EncRest* prevR = dynamic_cast<const EncRest*>(prev);
            const EncRest* curR  = dynamic_cast<const EncRest*>(elemRaw);
            if (prevR && curR
                && prev->tick == curR->tick
                && prev->staffIdx == curR->staffIdx
                && prev->voice == curR->voice
                && prevR->faceValue == curR->faceValue) {
                // drop this duplicate
                if (elemRaw->size > 0) {
                    qint64 spacing = elemRaw->size * 2;
                    ds.device()->seek(elemStart + spacing);
                } else {
                    ds.device()->seek(ds.device()->pos() + 1);
                }
                if (ds.device()->pos() >= measEnd - 4) {
                    break;
                }
                ds >> tick;
                continue;
            }
        }

        if (static_cast<EncElemType>(tp) != EncElemType::NONE) {
            elements.push_back(std::move(elem));
        }

        if (elemRaw->size > 0) {
            qint64 spacing = veryOldFormat ? elemRaw->size * 2 : elemRaw->size;
            ds.device()->seek(elemStart + spacing);
        } else {
            ds.device()->seek(ds.device()->pos() + 1);
        }

        if (veryOldFormat && ds.device()->pos() >= measEnd - 4) {
            break;
        }

        ds >> tick;
    }

    ds.device()->seek(measEnd);
    return true;
}

void EncMeasure::calculateRealDurations()
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
            // Skip same-tick elements (exact chord; existing behavior)
            while (j < elems.size() && elems[j]->tick == elems[i]->tick) {
                ++j;
            }
            // Skip near-simultaneous elements within the cluster threshold so that
            // the first note of a chord cluster gets a realistic rdur.
            while (j < elems.size()
                   && elems[j]->tick - elems[i]->tick < CHORD_CLUSTER_THRESHOLD) {
                ++j;
            }
            qint16 nextTick = (j < elems.size()) ? elems[j]->tick : durTicks;
            qint16 dur = nextTick - elems[i]->tick;
            if (dur > 0) {
                elems[i]->realDuration = dur;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// EncInstrument
// ---------------------------------------------------------------------------

bool EncInstrument::read(QDataStream& ds, quint32 vs, bool probeEncoding)
{
    offset = vs & 0xFFFF;
    // Detect UTF-16 LE by probing: if the first byte of the name is a
    // printable ASCII character and the second byte is 0x00 (the high
    // byte of a UTF-16 LE code unit), the name is encoded as UTF-16.
    // This correctly handles v0xC4 files saved by Encore 5.0.2 which
    // write instrument names in UTF-16 even when the offset field is
    // ≤ 250 (otherwise charSize() would return ONE_BYTE).
    EncCharSize cs = charSize();
    if (probeEncoding && cs == EncCharSize::ONE_BYTE) {
        const qint64 savedPos = ds.device()->pos();
        quint8 b0 = 0, b1 = 0;
        ds >> b0 >> b1;
        ds.device()->seek(savedPos);
        if (b0 >= 0x20 && b0 < 0x7F && b1 == 0x00) {
            cs = EncCharSize::TWO_BYTES;
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
    ds.skipRawData(14);
    qint8 ct;
    ds >> ct;
    clef = static_cast<EncClefType>(ct);
    ds >> key >> pageIdx;
    quint8 skip0, skip1, showByte;
    ds >> skip0 >> skip1 >> showByte;
    showStaff = (showByte != 0);
    (void)skip0;
    (void)skip1;
    quint8 st;
    ds >> st;
    staffType = static_cast<EncStaffType>(st);
    ds >> instrStaffIdx;
    ds.skipRawData(8);
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

// Read the 30-byte line prefix plus the 1024- or 64-byte text payload of a
// TITL line. For header/footer lines the prefix carries an alignment byte
// at +12 (0x02=right, 0x04=left, 0x06=center); other line kinds keep that
// byte at 0x00 so the default alignment applies.
static EncHeaderFooter readTitleLine(QDataStream& ds, EncCharSize cs)
{
    QByteArray prefix(30, 0);
    int got = ds.readRawData(prefix.data(), 30);
    // Alignment byte sits at prefix offset 14 for header/footer lines
    // (0x02 = right, 0x04 = left, 0x06 = center). Other line kinds carry
    // 0 here so the default LEFT mapping applies.
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
    // Determine encoding from the block's own varsize rather than from the
    // TK00-derived cs parameter.  Encore 5.0.2 (v0xC4) can produce files
    // where TK00 offset <= 250 (→ ONE_BYTE) but the TITL block content is
    // always UTF-16 LE (TWO_BYTES).  Fixed sizes:
    //   ONE_BYTE  items: 2 + 20×96  + 504 = 2 426
    //   TWO_BYTE  items: 2 + 20×1056 + 120 = 21 242
    // Any varsize well above 2426 unambiguously indicates TWO_BYTE.
    if (vs >= 10000) {
        cs = EncCharSize::TWO_BYTES;
    }
    // Some Encore files (e.g. Mamae_eu_quero-Bateria.enc) save TWO TITL
    // blocks with identical content.  Clear the slot vectors at the start
    // of every read() so a second pass replaces the first one's data
    // instead of doubling every line.
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
    // v0xA6 (Encore 2.x) has a shorter file header that ends at 0xA6,
    // right where TK00 begins. v0xC2 and v0xC4 share the longer 0xC2
    // header layout. Skipping past 0xC2 on a v0xA6 file would consume
    // TK00 and shift every per-instrument metadata field by one slot.
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
        // Text is in payload[14..entrySize-4] as UTF-16 LE.
        QString text;
        if (entrySize >= 18) {
            const int textBytes = entrySize - 14 - 4;
            text = QString::fromUtf16(
                reinterpret_cast<const char16_t*>(payload.constData() + 14),
                textBytes / 2);
            // Strip trailing nulls.
            int nullIdx = text.indexOf(QChar(QChar::Null));
            if (nullIdx >= 0) {
                text = text.left(nullIdx);
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

// ---------------------------------------------------------------------------
// EncFile - top-level container
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
           || isInstrumentMagic(magic);
}

QString findNextKnownMagic(QDataStream& ds)
{
    QString magic;
    for (int i = 0; i < 4 && !ds.atEnd(); ++i) {
        quint8 ch;
        ds >> ch;
        magic.append(QChar(ch));
    }
    // Cap the byte-by-byte resync window. The largest Encore block (TKxx) is
    // ~2 KiB; 1 MiB of junk between magics indicates a corrupt file and we
    // stop fishing rather than walking the entire payload.
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

bool EncFile::read(QDataStream& ds)
{
    if (!header.read(ds)) {
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
            meas.read(ds, varSize, header.isOldFormat(), header.isVeryOldFormat());
            meas.calculateRealDurations();
            // The file header carries a measureCount field that Encore uses to
            // decide how many MEAS blocks are part of the rendered score. The
            // file itself can contain trailing "ghost" MEAS blocks left over
            // from prior edits (observed in Mamae_eu_quero-Bateria with 36
            // rendered + 20 ghost). Stop appending past the header count so
            // the import matches what Encore displays.
            if (header.measureCount > 0
                && static_cast<int>(measures.size()) >= header.measureCount) {
                continue;
            }
            measures.push_back(std::move(meas));
        } else if (nextId == "TITL") {
            titleBlock.read(ds, varSize, charsize);
        } else if (nextId == "TEXT") {
            textBlock.read(ds, varSize);
        } else if (isInstrumentMagic(nextId)) {
            EncInstrument instr;
            // v0xA6 (Encore 2.x) TK blocks are 56-byte content slots that
            // carry the per-instrument Key transposition byte at content
            // offset +42 (signed int8 semitones; -12 = "Octave Lower").
            // Read it BEFORE EncInstrument::read so we can restore the
            // stream position and let the existing reader walk the
            // block. v0xC4 stores the same field in the formula-based
            // table outside the TK block, handled later.
            if (header.isVeryOldFormat()) {
                const qint64 contentStart = ds.device()->pos();
                if (ds.device()->seek(contentStart + 42)) {
                    quint8 raw = 0;
                    ds >> raw;
                    const qint8 signedRaw = static_cast<qint8>(raw);
                    if (signedRaw >= -33 && signedRaw <= 24) {
                        instr.keyTransposeSemitones = signedRaw;
                    }
                    ds.device()->seek(contentStart);
                }
            }
            // Probe encoding for v0xC4 files: Encore 5.0.2 writes names
            // in UTF-16 LE even when offset ≤ 250.  Older v0xC4 files
            // (e.g. bazo.enc) use ONE_BYTE; the probe distinguishes them.
            const bool probe = !header.isOldFormat() && !header.isVeryOldFormat();
            instr.read(ds, varSize, probe);
            charsize = instr.charSize();
            instruments.push_back(std::move(instr));
        } else {
            ds.skipRawData(varSize);
        }
    }

    if (instruments.empty()) {
        for (int i = 0; i < header.instrumentCount; ++i) {
            EncInstrument instr;
            instr.name = QString("Part %1").arg(i + 1);
            instruments.push_back(std::move(instr));
        }
    }

    // Encore 5.0.2 v0xC4 files can have fewer TK blocks than instrumentCount
    // (some instruments lack a TK block entirely).  Pad with empty entries so
    // that all instruments referenced by the LINE staffData are represented.
    while (static_cast<int>(instruments.size()) < header.instrumentCount) {
        instruments.emplace_back();
    }

    // Recover instrument names for entries that lack a TK block.
    // In v0xC4 files saved by Encore 5.0.2 the name content is always
    // present at (NAME_BASE + N * NAME_STEP) even when the TK block header
    // ("TKnn" magic + varsize) is absent.  Binary analysis of pachbel.enc
    // confirms: "Guitarra" lives at exactly formula_pos+8 = 0x2282 with
    // no TK04 header preceding it.
    //   NAME_BASE = header(194) + TK_header(8) = 202
    //   NAME_STEP = 2158  (same step as PRG table)
    // Only applies to v0xC4 files with the UTF-16 name encoding.
    if (!header.isOldFormat() && !header.isVeryOldFormat()) {
        static constexpr qint64 NAME_BASE = 202;
        static constexpr qint64 NAME_STEP = 2158;
        for (size_t n = 0; n < instruments.size(); ++n) {
            if (!instruments[n].name.isEmpty()) {
                continue;   // already have name from TK block
            }
            const qint64 off = NAME_BASE + static_cast<qint64>(n) * NAME_STEP;
            if (off + 2 >= static_cast<qint64>(ds.device()->size())) {
                break;
            }
            if (!ds.device()->seek(off)) {
                break;
            }
            // Probe: UTF-16 LE if b0 is printable ASCII and b1 == 0x00
            quint8 b0 = 0, b1 = 0;
            ds >> b0 >> b1;
            if (b0 < 0x20 || b0 >= 0x7F || b1 != 0x00) {
                continue;   // no readable UTF-16 name at this position
            }
            if (!ds.device()->seek(off)) {
                break;
            }
            QString recoveredName;
            while (!ds.atEnd()) {
                quint8 lo = 0, hi = 0;
                ds >> lo >> hi;
                const QChar ch = QChar(char16_t((hi << 8) + lo));
                if (ch == u'\0') {
                    break;
                }
                recoveredName.append(ch);
            }
            instruments[n].name = recoveredName;
        }
    }

    // Read the per-instrument MIDI program from the fixed-offset MIDI channel
    // table that follows each TK block.  Determined by binary diff analysis:
    //   PRG_BASE  = header(194) + TK_block(120) + intra_data_offset(1964) = 2278
    //   PRG_STEP  = TK_block(120) + data_block(2038) = 2158
    // Each entry is 8 identical bytes whose value is the 1-indexed GM program.
    // Only applies to v0xC4 files; in v0xC2 the data blocks contain text and
    // follow a different layout (a separate analysis is required).
    if (!header.isOldFormat() && !header.isVeryOldFormat()) {
        static constexpr qint64 PRG_BASE = 2278;
        static constexpr qint64 PRG_STEP = 2158;
        for (size_t n = 0; n < instruments.size(); ++n) {
            const qint64 off = PRG_BASE + static_cast<qint64>(n) * PRG_STEP;
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

        // Read the per-instrument "Key" transposition byte. Encore's Staff
        // Sheet exposes a Key dropdown that ranges from "2 Octaves Higher"
        // (+24 semitones) to "Major 20th Lower" (-33 semitones); the value
        // is stored on disk as a SIGNED int8 in semitones, 23 bytes BEFORE
        // the MIDI-program byte (= PRG_BASE - 23 + n * PRG_STEP). The note
        // pitch in the binary is the WRITTEN pitch and Encore plays it
        // shifted by this Key; the importer mirrors that by adding the
        // value to EncNote::semiTonePitch before calling Note::setPitch.
        // Confirmed by binary-diffing a controlled re-save where only one
        // staff's Key was changed from "Sounds as Written" (0x00) to
        // "Octave Lower" (0xf4 = -12); a single byte flipped at the
        // formula-derived offset.
        //
        // Compact-TK files (TK varsize <= 250, e.g. v0xC4 saved by Encore
        // 5.0.2 with offset = 112) do NOT follow the PRG_BASE + n * PRG_STEP
        // layout: the formula reads garbage and any non-zero byte would
        // mis-shift every pitch on that staff. Skip the Key read entirely
        // in that case; defaulting to 0 is correct because such files lack
        // the staff-sheet "Key" feature anyway. The sanity bound on the
        // value (-33..+24, the UI range) catches the remaining edge cases
        // (regular-TK files where the byte still lands on random data).
        const bool compactTk = !instruments.empty()
                               && instruments[0].offset <= 250;
        if (!compactTk) {
            static constexpr qint64 KEY_OFFSET_FROM_PRG = -23;
            for (size_t n = 0; n < instruments.size(); ++n) {
                const qint64 off = PRG_BASE + KEY_OFFSET_FROM_PRG
                                   + static_cast<qint64>(n) * PRG_STEP;
                if (off < 0 || off >= static_cast<qint64>(ds.device()->size())) {
                    continue;
                }
                if (!ds.device()->seek(off)) {
                    continue;
                }
                quint8 raw;
                ds >> raw;
                const qint8 signedRaw = static_cast<qint8>(raw);
                if (signedRaw < -33 || signedRaw > 24) {
                    continue;
                }
                instruments[n].keyTransposeSemitones = signedRaw;
            }
        }
    }

    if (!lines.empty()) {
        const auto& firstLine = lines.at(0);
        for (size_t i = 0; i < instruments.size(); ++i) {
            int cnt = 0;
            bool show = true;
            for (const auto& sd : firstLine.staffData) {
                if (sd.instrumentIndex() == i) {
                    ++cnt;
                    if (cnt == 1) {
                        show = sd.showStaff;   // visibility from first staff entry
                    }
                }
            }
            instruments[i].nstaves   = cnt;
            instruments[i].showStaff = show;
        }
    }

    addSpannerEnds(measures);
    return true;
}
} // namespace mu::iex::encore
