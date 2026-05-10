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

#include "encoremapping.h"

#include <QRegularExpression>

#include "engraving/style/style.h"
#include "engraving/dom/box.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/jump.h"
#include "engraving/dom/key.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/marker.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/text.h"
#include "engraving/dom/timesig.h"

using namespace mu::engraving;

namespace mu::iex::encore {

ClefType encClef2MuseScore(EncClefType ct)
{
    switch (ct) {
    case EncClefType::G:    return ClefType::G;
    case EncClefType::F:    return ClefType::F;
    case EncClefType::C3L:  return ClefType::C3;
    case EncClefType::C4L:  return ClefType::C4;
    case EncClefType::G8P:  return ClefType::G8_VA;
    case EncClefType::G8M:  return ClefType::G8_VB;
    case EncClefType::F8M:  return ClefType::F8_VB;
    case EncClefType::PERC: return ClefType::PERC;
    case EncClefType::TAB:  return ClefType::TAB;
    default:                return ClefType::G;
    }
}

// Encore stores key as an index (0-14) into the circle of fifths.
// From Enc2MusicXML: { C, F, Bb, Eb, Ab, Db, Gb, Cb, G, D, A, E, B, F#, C# }
//                  = { 0,-1, -2, -3, -4, -5, -6, -7, 1, 2, 3, 4, 5,  6,  7 }
int encKeyToFifths(quint8 key)
{
    static const int table[] = { 0, -1, -2, -3, -4, -5, -6, -7, 1, 2, 3, 4, 5, 6, 7 };
    if (key < 15) {
        return table[key];
    }
    return 0;
}

// ---------------------------------------------------------------------------
// MuseScore DOM construction
// ---------------------------------------------------------------------------

void addTitleFrame(MasterScore* score, const EncTitle& titleBlock)
{
    const bool hasSubtitle    = !titleBlock.subtitle.empty()
                                && !titleBlock.subtitle[0].isEmpty();
    const bool hasInstruction = !titleBlock.instruction.empty()
                                && !titleBlock.instruction[0].isEmpty();
    const bool hasAuthor      = !titleBlock.author.empty()
                                && !titleBlock.author[0].isEmpty();
    const bool hasCopyright   = !titleBlock.copyright.empty()
                                && !titleBlock.copyright[0].isEmpty();

    // Populate Score Properties metadata (File > Score Properties dialog).
    // These are independent of the VBox visual frame.
    if (!titleBlock.title.isEmpty()) {
        score->setMetaTag(u"workTitle", String(titleBlock.title));
    }
    if (hasSubtitle) {
        score->setMetaTag(u"subtitle", String(titleBlock.subtitle[0]));
    }
    if (hasInstruction) {
        score->setMetaTag(u"lyricist", String(titleBlock.instruction[0]));
    }
    if (hasAuthor) {
        score->setMetaTag(u"composer", String(titleBlock.author[0]));
    }
    if (hasCopyright) {
        score->setMetaTag(u"copyright", String(titleBlock.copyright[0]));
    }

    if (titleBlock.title.isEmpty() && !hasAuthor && !hasInstruction) {
        return;
    }

    // Build the VBox title frame for visual display on the first page.
    VBox* vbox = Factory::createTitleVBox(score->dummy()->system());
    vbox->setNext(score->first());
    score->measures()->add(vbox);

    if (!titleBlock.title.isEmpty()) {
        Text* t = Factory::createText(vbox, TextStyleType::TITLE);
        t->setPlainText(String(titleBlock.title));
        vbox->add(t);
    }
    if (hasSubtitle) {
        Text* t = Factory::createText(vbox, TextStyleType::SUBTITLE);
        t->setPlainText(String(titleBlock.subtitle[0]));
        vbox->add(t);
    }
    if (hasInstruction) {
        Text* t = Factory::createText(vbox, TextStyleType::LYRICIST);
        t->setPlainText(String(titleBlock.instruction[0]));
        vbox->add(t);
    }
    if (hasAuthor) {
        Text* t = Factory::createText(vbox, TextStyleType::COMPOSER);
        t->setPlainText(String(titleBlock.author[0]));
        vbox->add(t);
    }

    // Encore's TITL block carries up to two header lines and two footer
    // lines, each with its own horizontal alignment byte. Map them onto
    // MuseScore's odd/even page header & footer style slots so the text
    // appears in the same screen corner Encore showed it in.
    auto applyHF = [score](const EncHeaderFooter& hf,
                           mu::engraving::Sid sidL,
                           mu::engraving::Sid sidC,
                           mu::engraving::Sid sidR,
                           mu::engraving::Sid sidEvenL,
                           mu::engraving::Sid sidEvenC,
                           mu::engraving::Sid sidEvenR) {
        if (hf.text.isEmpty()) {
            return;
        }
        mu::engraving::Sid sid     = sidL;
        mu::engraving::Sid sidEven = sidEvenL;
        if (hf.align == EncTextAlign::CENTER) {
            sid     = sidC;
            sidEven = sidEvenC;
        } else if (hf.align == EncTextAlign::RIGHT) {
            sid     = sidR;
            sidEven = sidEvenR;
        }
        const String text = String(hf.text);
        score->style().set(sid, text);
        score->style().set(sidEven, text);
    };
    for (const EncHeaderFooter& hf : titleBlock.header) {
        applyHF(hf,
                mu::engraving::Sid::oddHeaderL, mu::engraving::Sid::oddHeaderC, mu::engraving::Sid::oddHeaderR,
                mu::engraving::Sid::evenHeaderL, mu::engraving::Sid::evenHeaderC, mu::engraving::Sid::evenHeaderR);
    }
    for (const EncHeaderFooter& hf : titleBlock.footer) {
        applyHF(hf,
                mu::engraving::Sid::oddFooterL, mu::engraving::Sid::oddFooterC, mu::engraving::Sid::oddFooterR,
                mu::engraving::Sid::evenFooterL, mu::engraving::Sid::evenFooterC, mu::engraving::Sid::evenFooterR);
    }
}

void addInitialKeySig(MasterScore* score, int staffIdx, quint8 encKey)
{
    int fifths = encKeyToFifths(encKey);
    if (fifths == 0) {
        return;
    }
    Staff* staff = score->staff(staffIdx);
    if (!staff) {
        return;
    }
    Fraction tick = Fraction(0, 1);
    KeySigEvent ke;
    Key k = Key(fifths);
    ke.setConcertKey(k);
    ke.setKey(k);
    staff->setKey(tick, ke);

    Measure* m = score->tick2measure(tick);
    if (!m) {
        return;
    }
    Segment* seg = m->getSegment(SegmentType::KeySig, tick);
    KeySig* ks = Factory::createKeySig(seg);
    ks->setTrack(staffIdx * VOICES);
    ks->setKey(k, k);
    seg->add(ks);
}

void addInitialTimeSig(MasterScore* score, int nstaves, const EncMeasure& firstMeas)
{
    int num = firstMeas.timeSigNum > 0 ? firstMeas.timeSigNum : 4;
    int den = firstMeas.timeSigDen > 0 ? firstMeas.timeSigDen : 4;
    Fraction ts(num, den);

    Measure* m = score->tick2measure(Fraction(0, 1));
    if (!m) {
        return;
    }
    for (int staffIdx = 0; staffIdx < nstaves; ++staffIdx) {
        Segment* seg = m->getSegment(SegmentType::TimeSig, Fraction(0, 1));
        TimeSig* tsig = Factory::createTimeSig(seg);
        tsig->setTrack(staffIdx * VOICES);
        tsig->setSig(ts);
        seg->add(tsig);
    }
}

void addInitialClef(MasterScore* score, int staffIdx, EncClefType ct)
{
    Measure* m = score->tick2measure(Fraction(0, 1));
    if (!m) {
        return;
    }
    Segment* seg = m->getSegment(SegmentType::HeaderClef, Fraction(0, 1));
    Clef* clef = Factory::createClef(seg);
    clef->setTrack(staffIdx * VOICES);
    clef->setClefType(encClef2MuseScore(ct));
    seg->add(clef);
}

void addRepeatMark(Score* /*score*/, Measure* measure, EncRepeatType rt)
{
    switch (rt) {
    case EncRepeatType::SEGNO: {
        Marker* m = Factory::createMarker(measure);
        m->setMarkerType(MarkerType::SEGNO);
        m->setTrack(0);
        measure->add(m);
        break;
    }
    case EncRepeatType::CODA1:
    case EncRepeatType::CODA2: {
        Marker* m = Factory::createMarker(measure);
        m->setMarkerType(MarkerType::CODA);
        m->setTrack(0);
        measure->add(m);
        break;
    }
    case EncRepeatType::FINE: {
        Marker* m = Factory::createMarker(measure);
        m->setMarkerType(MarkerType::FINE);
        m->setTrack(0);
        measure->add(m);
        break;
    }
    case EncRepeatType::DC: {
        Jump* j = Factory::createJump(measure);
        j->setJumpType(JumpType::DC);
        j->setTrack(0);
        measure->add(j);
        break;
    }
    case EncRepeatType::DS: {
        Jump* j = Factory::createJump(measure);
        j->setJumpType(JumpType::DS);
        j->setTrack(0);
        measure->add(j);
        break;
    }
    case EncRepeatType::DCALFINE: {
        Jump* j = Factory::createJump(measure);
        j->setJumpType(JumpType::DC_AL_FINE);
        j->setTrack(0);
        measure->add(j);
        break;
    }
    case EncRepeatType::DSALFINE: {
        Jump* j = Factory::createJump(measure);
        j->setJumpType(JumpType::DS_AL_FINE);
        j->setTrack(0);
        measure->add(j);
        break;
    }
    case EncRepeatType::DCALCODA: {
        Jump* j = Factory::createJump(measure);
        j->setJumpType(JumpType::DC_AL_CODA);
        j->setTrack(0);
        measure->add(j);
        break;
    }
    case EncRepeatType::DSALCODA: {
        Jump* j = Factory::createJump(measure);
        j->setJumpType(JumpType::DS_AL_CODA);
        j->setTrack(0);
        measure->add(j);
        break;
    }
    default:
        break;
    }
}

// Strip trailing number + ordinal marker from an instrument name so that
// "Bandurria 1ª", "Bandurria 2ª" etc. all reduce to "Bandurria".
// Also strips standalone ordinals like "ª" and "º".
QString normalizeEncoreInstrName(const QString& name)
{
    QString s = name.trimmed();
    // Remove:  whitespace + digits + optional ordinal (ª º °)
    static const QRegularExpression trailingNum(QStringLiteral("\\s+\\d+[\xaa\xb0\xba]*$"));
    // Remove:  trailing ordinals with no preceding digit
    static const QRegularExpression trailingOrd(QStringLiteral("[\xaa\xb0\xba]+$"));
    s.remove(trailingNum);
    s.remove(trailingOrd);
    return s.trimmed();
}

// Find the best MuseScore instrument template for an Encore instrument name.
//
// searchTemplateForInstrNameList matches against the trackName, longName and
// shortName fields of every loaded template.  Those fields are populated at
// XML-load time via mtrc("engraving/instruments", ...) using the user's
// current locale, so a Spanish-locale user gets templates whose trackName is
// "Guitarra" and "Guitarra" from an Encore file matches automatically.  A
// user with an English locale only sees the English template names ("Guitar"
// etc.); for those users a Spanish Encore name will fall through these passes
// and the Encore name is kept as-is via setLongName in buildScore.
//
// Search strategy (stops at first match):
//   1. Exact case-insensitive match of the full name.
//   2. Exact match of the normalized name (trailing numbers/ordinals stripped).
//   3. Exact match for each word in the normalized name (length >= 4).
const InstrumentTemplate* findEncoreInstrumentTemplate(const QString& encName)
{
    if (encName.isEmpty()) {
        return nullptr;
    }

    // Pass 1: full name
    const InstrumentTemplate* tmpl
        = searchTemplateForInstrNameList({ String(encName) }, false, false);
    if (tmpl) {
        return tmpl;
    }

    // Pass 2: normalized name
    const QString norm = normalizeEncoreInstrName(encName);
    if (norm != encName && !norm.isEmpty()) {
        tmpl = searchTemplateForInstrNameList({ String(norm) }, false, false);
        if (tmpl) {
            return tmpl;
        }
    }

    // Pass 3: each word of the normalized name (skip short words to avoid noise)
    for (const QString& word : norm.split(u' ')) {
        if (word.length() >= 4) {
            tmpl = searchTemplateForInstrNameList({ String(word) }, false, false);
            if (tmpl) {
                return tmpl;
            }
        }
    }

    return nullptr;
}

std::vector<mu::engraving::SymId> encArticulation2SymIds(quint8 articByte)
{
    using mu::engraving::SymId;
    // Encore packs articulation glyphs into a single byte. Simple bytes carry
    // one glyph (0x1C = tenuto, 0x1D = staccato, ...); combo bytes carry two
    // (0x24 = tenuto + staccato, 0x27 = marcato + tenuto, ...). The mapping
    // was confirmed against `encore-symbols.enc` measure-by-measure: m8-m12
    // each line up one byte per ChordRest with the per-note articulation
    // list in Encore's MusicXML export, so each combo decomposes uniquely.
    switch (articByte) {
    // 0x04..0x07: trill-mark glyphs.
    // 0x0A, 0x0C:    inverted-mordent.
    // 0x0B, 0x2F:    mordent.
    // Mapping derived from encore-symbols.enc m16 (4 trill-marks at
    // articUp 0x04..0x07 across 4 notes) and m17 (4 mordents at
    // articUp 0x0a 0x0b 0x0c 0x2f).
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07: return { SymId::ornamentTrill };
    case 0x0A:
    case 0x0C: return { SymId::ornamentShortTrill };   // <inverted-mordent>
    case 0x0B:
    case 0x2F: return { SymId::ornamentMordent };
    case 0x12: return { SymId::articAccentAbove };
    case 0x13: return { SymId::articMarcatoAbove };
    case 0x14: return { SymId::articAccentAbove, SymId::articTenutoAbove };
    case 0x15: return { SymId::articMarcatoAbove, SymId::articStaccatoAbove };
    case 0x16: return { SymId::articAccentAbove, SymId::articStaccatissimoAbove };
    case 0x17: return { SymId::articAccentAbove, SymId::articStaccatoAbove };
    case 0x18: return { SymId::stringsUpBow };
    case 0x19: return { SymId::stringsDownBow };
    case 0x1A: return { SymId::articMarcatoAbove };
    case 0x1C: return { SymId::articTenutoAbove };
    case 0x1D: return { SymId::articStaccatoAbove };
    case 0x20:
    case 0x21: return { SymId::fermataAbove };
    case 0x22: return { SymId::fermataShortAbove };  // short / square fermata
    case 0x23: return { SymId::articAccentAbove, SymId::articTenutoAbove };
    case 0x24: return { SymId::articTenutoAbove, SymId::articStaccatoAbove };
    case 0x25: return { SymId::articMarcatoAbove, SymId::articTenutoAbove };
    case 0x26: return { SymId::articMarcatoAbove, SymId::articStaccatissimoAbove };
    case 0x27: return { SymId::articMarcatoAbove, SymId::articTenutoAbove };
    case 0x28:
    case 0x29: return { SymId::articStaccatissimoAbove };
    case 0x2A: return { SymId::articStaccatissimoAbove, SymId::articStaccatoAbove };
    case 0x2B: return { SymId::articAccentAbove, SymId::articStaccatissimoAbove };
    case 0x2C: return { SymId::articStaccatissimoAbove };
    case 0x2D: return { SymId::articTenutoAbove, SymId::articStaccatissimoAbove };
    // String technical markings derived from encore-symbols.enc m3, m4, m18:
    //   0x1E, 0x1F -> harmonic (natural and artificial variants share the
    //                 same MuseScore Symbol because MuseScore only has one
    //                 stringsHarmonic glyph; the artificial variant is
    //                 emitted as <harmonic><artificial/></harmonic> by the
    //                 MusicXML exporter when the note carries a separate
    //                 sounding pitch which we do not yet read).
    //   0x44, 0x45 -> thumb-position
    //   0x46 is open-string and is handled separately in
    //   encArticByteToOpenString() because MuseScore lacks a dedicated
    //   SymId; it must be emitted as a Fingering with STRING_NUMBER text "0".
    case 0x1E:
    case 0x1F: return { SymId::stringsHarmonic };
    case 0x44:
    case 0x45: return { SymId::stringsThumbPosition };
    default:
        return {};
    }
}

int encArticByteToFingerNumber(quint8 articByte)
{
    // Per-note fingering glyphs. Encore numbers the fingers 1..5 with the
    // contiguous byte range 0x0D..0x11. Other low-byte values in the
    // articulation slot belong to ornament glyphs (trill, mordent) and
    // must not be confused for fingerings.
    switch (articByte) {
    case 0x0D: return 1;
    case 0x0E: return 2;
    case 0x0F: return 3;
    case 0x10: return 4;
    case 0x11: return 5;
    default:   return 0;
    }
}

bool encArticByteIsOpenString(quint8 articByte)
{
    // Encore stores open-string with byte 0x46 in the articulationUp slot.
    // MuseScore has no dedicated SymId for open-string; the exporter emits
    // <open-string/> when it sees a Fingering with TextStyleType
    // STRING_NUMBER and text "0".
    return articByte == 0x46;
}

} // namespace mu::iex::encore
