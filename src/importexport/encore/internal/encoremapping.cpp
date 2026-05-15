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

// Translate Encore header/footer tokens (#P, #D, #T) to the matching
// MuseScore macros ($P, $D, $m). Without this rewrite MuseScore would
// print the literal text "#P" on every page instead of expanding it to
// the page number. Unknown "#X" sequences are left untouched so user
// text that legitimately starts with "#" survives.
static String translateHeaderFooterTokens(const String& s)
{
    String out = s;
    out.replace(u"#P", u"$P");
    out.replace(u"#D", u"$D");
    out.replace(u"#T", u"$m");
    return out;
}

void addTitleFrame(MasterScore* score, const EncTitle& titleBlock)
{
    // Encore's TITL block stores multi-line content as separate slots per
    // category: subtitle1..2, instruction1..3, author1..4, copyright1..6 and
    // header1..2 / footer1..2. Each slot is one visual line; non-empty slots
    // of the same category render stacked. Encore's own MusicXML exporter
    // joins them with newlines (e.g. composer of Mamae_eu_quero-Bateria.enc
    // emits author1\nauthor2\nauthor3), so MuseScore should do the same when
    // populating the VBox text fields and the Score Properties metadata.
    auto joinSlots = [](const std::vector<QString>& items) -> QString {
        QStringList nonEmpty;
        for (const QString& s : items) {
            if (!s.isEmpty()) {
                nonEmpty.append(s);
            }
        }
        return nonEmpty.join(QChar('\n'));
    };
    const QString joinedSubtitle    = joinSlots(titleBlock.subtitle);
    const QString joinedInstruction = joinSlots(titleBlock.instruction);
    const QString joinedAuthor      = joinSlots(titleBlock.author);
    const QString joinedCopyright   = joinSlots(titleBlock.copyright);

    const bool hasSubtitle    = !joinedSubtitle.isEmpty();
    const bool hasInstruction = !joinedInstruction.isEmpty();
    const bool hasAuthor      = !joinedAuthor.isEmpty();
    const bool hasCopyright   = !joinedCopyright.isEmpty();

    // Populate Score Properties metadata (File > Score Properties dialog).
    // These are independent of the VBox visual frame.
    if (!titleBlock.title.isEmpty()) {
        score->setMetaTag(u"workTitle", String(titleBlock.title));
    }
    if (hasSubtitle) {
        score->setMetaTag(u"subtitle", String(joinedSubtitle));
    }
    if (hasInstruction) {
        score->setMetaTag(u"lyricist", String(joinedInstruction));
    }
    if (hasAuthor) {
        score->setMetaTag(u"composer", String(joinedAuthor));
    }
    if (hasCopyright) {
        score->setMetaTag(u"copyright", String(joinedCopyright));
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
        t->setPlainText(String(joinedSubtitle));
        vbox->add(t);
    }
    if (hasInstruction) {
        Text* t = Factory::createText(vbox, TextStyleType::LYRICIST);
        t->setPlainText(String(joinedInstruction));
        vbox->add(t);
    }
    if (hasAuthor) {
        Text* t = Factory::createText(vbox, TextStyleType::COMPOSER);
        t->setPlainText(String(joinedAuthor));
        vbox->add(t);
    }

    // Encore's TITL block carries up to two header lines and two footer
    // lines, each with its own horizontal alignment byte. Map them onto
    // MuseScore's odd/even page header & footer style slots so the text
    // appears in the same screen corner Encore showed it in. When multiple
    // header (or footer) slots carry the SAME alignment, join their texts
    // with a newline so both lines stack at that page corner (mirrors
    // Encore's rendering and matches the slot-joining behavior used for
    // composer/lyricist/copyright above).
    auto applyHFGroup = [score](const std::vector<EncHeaderFooter>& items,
                                mu::engraving::Sid sidL,
                                mu::engraving::Sid sidC,
                                mu::engraving::Sid sidR,
                                mu::engraving::Sid sidEvenL,
                                mu::engraving::Sid sidEvenC,
                                mu::engraving::Sid sidEvenR) {
        std::map<EncTextAlign, QStringList> grouped;
        for (const EncHeaderFooter& hf : items) {
            if (hf.text.isEmpty()) {
                continue;
            }
            grouped[hf.align].append(hf.text);
        }
        for (const auto& [align, lines] : grouped) {
            mu::engraving::Sid sid     = sidL;
            mu::engraving::Sid sidEven = sidEvenL;
            if (align == EncTextAlign::CENTER) {
                sid     = sidC;
                sidEven = sidEvenC;
            } else if (align == EncTextAlign::RIGHT) {
                sid     = sidR;
                sidEven = sidEvenR;
            }
            const String text = translateHeaderFooterTokens(
                String(lines.join(QChar('\n'))));
            score->style().set(sid, text);
            score->style().set(sidEven, text);
        }
    };
    applyHFGroup(titleBlock.header,
                 mu::engraving::Sid::oddHeaderL, mu::engraving::Sid::oddHeaderC, mu::engraving::Sid::oddHeaderR,
                 mu::engraving::Sid::evenHeaderL, mu::engraving::Sid::evenHeaderC, mu::engraving::Sid::evenHeaderR);
    applyHFGroup(titleBlock.footer,
                 mu::engraving::Sid::oddFooterL, mu::engraving::Sid::oddFooterC, mu::engraving::Sid::oddFooterR,
                 mu::engraving::Sid::evenFooterL, mu::engraving::Sid::evenFooterC, mu::engraving::Sid::evenFooterR);
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

static int clefOctaveOffset(ClefType ct)
{
    switch (ct) {
    case ClefType::G8_VB:
    case ClefType::G8_VB_O:
    case ClefType::G8_VB_P:
    case ClefType::G8_VB_C:
    case ClefType::F8_VB:
    case ClefType::C4_8VB:
        return -12;
    case ClefType::G8_VA:
    case ClefType::F_8VA:
        return 12;
    case ClefType::G15_MB:
    case ClefType::F15_MB:
        return -24;
    case ClefType::G15_MA:
    case ClefType::F_15MA:
        return 24;
    default:
        return 0;
    }
}

static int clefGlyphFamily(ClefType ct)
{
    switch (ct) {
    case ClefType::G:
    case ClefType::G_1:
    case ClefType::G8_VB:
    case ClefType::G8_VA:
    case ClefType::G15_MA:
    case ClefType::G15_MB:
    case ClefType::G8_VB_O:
    case ClefType::G8_VB_P:
    case ClefType::G8_VB_C:
        return 1;   // G family
    case ClefType::F:
    case ClefType::F_B:
    case ClefType::F_C:
    case ClefType::F_F18C:
    case ClefType::F_19C:
    case ClefType::F15_MB:
    case ClefType::F8_VB:
    case ClefType::F_8VA:
    case ClefType::F_15MA:
        return 2;   // F family
    default:
        return 0;
    }
}

ClefType pickStaffClef(EncClefType encClef, ClefType concertClef, ClefType transposingClef,
                      int keyOffsetSemitones)
{
    const ClefType base = encClef2MuseScore(encClef);
    if (concertClef == ClefType::INVALID || concertClef == base) {
        return base;
    }
    // The whole override is gated by the concert clef carrying an octave
    // decoration that matches the staff Key offset and sharing the same
    // glyph family as the Encore clef.
    if (clefGlyphFamily(base) == 0 || clefGlyphFamily(base) != clefGlyphFamily(concertClef)) {
        return base;
    }
    if (clefOctaveOffset(concertClef) != keyOffsetSemitones) {
        return base;
    }
    // Prefer the transposing clef when it is distinct from the concert
    // clef and carries no octave decoration. The instrument's
    // transposeChromatic still moves the noteheads to the same staff
    // position the concert clef would render them at, but the clef GLYPH
    // stays identical to what Encore stored (e.g. plain F for bass-guitar).
    if (transposingClef != ClefType::INVALID
        && transposingClef != concertClef
        && clefOctaveOffset(transposingClef) == 0
        && clefGlyphFamily(transposingClef) == clefGlyphFamily(base)) {
        return transposingClef;
    }
    return concertClef;
}

void addInitialClef(MasterScore* score, int staffIdx, EncClefType ct)
{
    addInitialClef(score, staffIdx, encClef2MuseScore(ct));
}

void addInitialClef(MasterScore* score, int staffIdx, ClefType ct)
{
    Measure* m = score->tick2measure(Fraction(0, 1));
    if (!m) {
        return;
    }
    Segment* seg = m->getSegment(SegmentType::HeaderClef, Fraction(0, 1));
    Clef* clef = Factory::createClef(seg);
    clef->setTrack(staffIdx * VOICES);
    clef->setClefType(ct);
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

// Lowercased, accent-stripped form used for the diacritics-insensitive pass.
// Spanish/Portuguese Encore files routinely write "Laúd" or "Percusión", but
// the localized templates ship the same words and case-folding alone does not
// equate them; decomposing to NFD and dropping combining marks does.
static QString normalizeForCompare(const QString& s)
{
    const QString d = s.normalized(QString::NormalizationForm_D);
    QString out;
    out.reserve(d.size());
    for (const QChar& ch : d) {
        if (ch.category() != QChar::Mark_NonSpacing) {
            out.append(ch.toLower());
        }
    }
    return out;
}

// Find the best MuseScore instrument template for an Encore instrument name.
//
// One pass over every non-drumset template. Scoring combines diacritics-
// insensitive name overlap (an Encore "Laud" matches the Spanish "Laúd"
// template; "Guitarra B" matches "Guitarra clásica" by substring) with a
// fixed MIDI-program bonus that flips ties between equally-named templates
// (Spanish "Bajo" matches the choral Bass voice by trackName but the .enc
// midiProgram = Acoustic Bass, which the bonus then promotes).
//
// Score per template (with needles = accent-stripped, lowercased variants
// of the full name and each word of length >= 4):
//   trackName  == needle: +4    | trackName  contains needle: +2
//   longName   == needle: +2    | longName   contains needle: +1
//   shortName  == needle: +1
//   default channel program == encMidiProgram:                 +6
const InstrumentTemplate* findEncoreInstrumentTemplate(const QString& encName, int encMidiProgram)
{
    if (encName.isEmpty()) {
        return nullptr;
    }

    // Reject very short names (e.g. "S", "A", "T", "B" used as SATB choir
    // labels in a 4-voice mixed score). With a 1- to 3-character needle the
    // substring scoring matches almost any template that happens to contain
    // that letter (so "S" lands on Bass Clarinet, "C" on Piccolo, "T" on
    // Contrabassoon, ...). Below the four-character threshold the caller
    // falls back to MIDI program lookup and ultimately to the Grand Piano
    // template, which is musically neutral until the user picks the right
    // voice from the instrument browser.
    if (encName.trimmed().size() < 4) {
        return nullptr;
    }

    const QString norm = normalizeEncoreInstrName(encName);

    QStringList needles;
    auto addNeedle = [&](const QString& s) {
        const QString n = normalizeForCompare(s);
        if (!n.isEmpty() && !needles.contains(n)) {
            needles << n;
        }
    };
    addNeedle(encName);
    addNeedle(norm);
    for (const QString& word : norm.split(u' ', Qt::SkipEmptyParts)) {
        if (word.length() >= 4) {
            addNeedle(word);
        }
    }
    if (needles.isEmpty()) {
        return nullptr;
    }

    const InstrumentTemplate* best = nullptr;
    int bestScore = 0;
    int bestNameStrength = 0;
    for (const InstrumentGroup* g : instrumentGroups) {
        for (const InstrumentTemplate* it : g->instrumentTemplates) {
            if (it->useDrumset) {
                continue;
            }
            const QString nt = normalizeForCompare(it->trackName.toQString());
            const QString nl = normalizeForCompare(it->instrumentName.longName().toQString());
            const QString ns = normalizeForCompare(it->instrumentName.shortName().toQString());
            int nameStrength = 0;
            for (const QString& needle : needles) {
                int s = 0;
                if (nt == needle) {
                    s += 4;
                } else if (nt.contains(needle)) {
                    s += 2;
                }
                if (nl == needle) {
                    s += 2;
                } else if (nl.contains(needle)) {
                    s += 1;
                }
                if (ns == needle) {
                    s += 1;
                }
                if (s > nameStrength) {
                    nameStrength = s;
                }
            }
            if (nameStrength == 0) {
                continue;
            }
            int midiBonus = 0;
            if (encMidiProgram >= 0) {
                // Templates ship multiple channels (acoustic-bass has slap,
                // pop, pizzicato, arco, tremolo); any program match counts.
                for (const InstrChannel& ch : it->channel) {
                    if (ch.program() == encMidiProgram) {
                        midiBonus = 6;
                        break;
                    }
                }
            }
            // Tiebreaker for two equally-named templates that share a MIDI
            // program (e.g. soprano-guitar and guitar-nylon both ship with
            // GM program 24): prefer the one tagged as "common", which marks
            // the everyday classical/nylon guitar over the soprano variant.
            int commonBonus = 0;
            for (const InstrumentGenre* gen : it->genres) {
                if (gen && gen->id == "common") {
                    commonBonus = 1;
                    break;
                }
            }
            const int score = nameStrength + midiBonus + commonBonus;
            if (score > bestScore
                || (score == bestScore && nameStrength > bestNameStrength)) {
                bestScore = score;
                bestNameStrength = nameStrength;
                best = it;
            }
        }
    }
    return best;
}

double encTextToTempoBps(const QString& text)
{
    const QString t = text.trimmed().toLower();
    if (t.isEmpty()) {
        return -1.0;
    }
    // Italian tempo terms. BPM values mirror MuseScore's tempo palette
    // (palettecreator.cpp) so the imported pieces play back at the same
    // speed a user would get by dragging the term from the palette.
    struct Entry { const char* word; double bps; };
    static const Entry kAbsolute[] = {
        { "grave",        35.0 / 60.0 },
        { "largo",        50.0 / 60.0 },
        { "lento",        52.5 / 60.0 },
        { "larghetto",    63.0 / 60.0 },
        { "adagio",       71.0 / 60.0 },
        { "andante",      92.0 / 60.0 },
        { "andantino",    94.0 / 60.0 },
        { "moderato",    114.0 / 60.0 },
        { "allegretto",  116.0 / 60.0 },
        { "allegro",     144.0 / 60.0 },
        { "vivace",      172.0 / 60.0 },
        { "presto",      187.0 / 60.0 },
        { "prestissimo", 200.0 / 60.0 },
    };
    for (const Entry& e : kAbsolute) {
        if (t == QString::fromLatin1(e.word)) {
            return e.bps;
        }
    }
    // Relative markings: stay as TempoText (so MuseScore treats them as
    // tempo for layout/positioning) but carry no absolute BPS - the layout
    // engine and the user's expectation is that they fall back to the
    // previous tempo.
    static const char* kRelative[] = {
        "a tempo",
        "tempo i",
        "tempo 1",
        "tempo primo",
    };
    for (const char* r : kRelative) {
        if (t == QString::fromLatin1(r)) {
            return 0.0;
        }
    }
    return -1.0;
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
