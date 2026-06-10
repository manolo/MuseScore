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

#include "mapping.h"

#include <QRegularExpression>

#include "engraving/style/style.h"
#include "engraving/dom/box.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/jump.h"
#include "engraving/dom/key.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/part.h"
#include "engraving/editing/transpose.h"
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

// Encore key byte is an index 0..14 into { C, F, Bb, Eb, Ab, Db, Gb, Cb, G, D, A, E, B, F#, C# }
// mapping to fifths {0,-1,-2,-3,-4,-5,-6,-7,1,2,3,4,5,6,7}. See ENCORE_FORMAT.md §Key encoding.
int encKeyToFifths(quint8 key)
{
    static const int table[] = { 0, -1, -2, -3, -4, -5, -6, -7, 1, 2, 3, 4, 5, 6, 7 };
    if (key < 15) {
        return table[key];
    }
    return 0;
}

// Translate Encore page tokens (#P, #D, #T) to MuseScore macros ($P, $D, $m).
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
    // TITL stores multi-line content as separate slots (subtitle1..2, author1..4, etc.).
    // Join non-empty slots with newline, same as Encore's MusicXML exporter does.
    auto joinSlots = [](const std::vector<QString>& items) -> QString {
        QStringList nonEmpty;
        for (const QString& s : items) {
            if (!s.isEmpty()) {
                nonEmpty.append(s);
            }
        }
        return nonEmpty.join(QChar('\n'));
    };
    // Promote the first non-empty subtitle to title when the title slot is blank (TITL fixed-line layout quirk).
    QString effectiveTitle = titleBlock.title;
    std::vector<QString> subtitleSlots = titleBlock.subtitle;
    if (effectiveTitle.isEmpty()) {
        for (QString& slot : subtitleSlots) {
            if (!slot.isEmpty()) {
                effectiveTitle = slot;
                slot = QString();
                break;
            }
        }
    }

    const QString joinedSubtitle    = joinSlots(subtitleSlots);
    const QString joinedInstruction = joinSlots(titleBlock.instruction);
    const QString joinedAuthor      = joinSlots(titleBlock.author);
    const QString joinedCopyright   = joinSlots(titleBlock.copyright);

    const bool hasSubtitle    = !joinedSubtitle.isEmpty();
    const bool hasInstruction = !joinedInstruction.isEmpty();
    const bool hasAuthor      = !joinedAuthor.isEmpty();
    const bool hasCopyright   = !joinedCopyright.isEmpty();

    if (!effectiveTitle.isEmpty()) {
        score->setMetaTag(u"workTitle", String(effectiveTitle));
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

    if (effectiveTitle.isEmpty() && !hasSubtitle && !hasAuthor && !hasInstruction) {
        return;
    }

    VBox* vbox = Factory::createTitleVBox(score->dummy()->system());
    vbox->setNext(score->first());
    score->measures()->add(vbox);

    if (!effectiveTitle.isEmpty()) {
        Text* t = Factory::createText(vbox, TextStyleType::TITLE);
        t->setPlainText(String(effectiveTitle));
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

    // Header/footer: map alignment to odd/even Sid slots. Same-alignment slots joined with newline.
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
    Staff* staff = score->staff(staffIdx);
    if (!staff) {
        return;
    }
    Key writtenKey = Key(fifths);
    // Encore's key field is the written key for the instrument. Convert to concert
    // key for the staff timeline; the written key is stored explicitly for display.
    Interval v = staff->part()->instrument()->transpose();
    Key concertKey = v.isZero() ? writtenKey : Transpose::transposeKey(writtenKey, v);
    // Prefer sharp enharmonics (F# over Gb, B over Cb) when transposeKey returns an extreme
    // flat key. Transpose::transposeKey(Key::C, Interval(3,6)) returns Key::Gb (-6) for a
    // +6 (augmented-4th) transposing instrument, causing pitch2tpc to spell concert B4 as Cb
    // and the written note as Gbb. Using F# instead gives the correct spelling (F).
    if (static_cast<int>(concertKey) <= -6) {
        concertKey = Key(static_cast<int>(concertKey) + 12);
    }
    // Always store the concert key on the staff's key timeline so Staff::concertKey() returns
    // the normalized value. Without this, C-major (fifths==0) returns early and the staff
    // computes the concert key on-the-fly from the instrument transposition, which may return
    // the flat enharmonic and produce double-flat note spellings.
    Fraction tick = Fraction(0, 1);
    KeySigEvent ke;
    ke.setConcertKey(concertKey);
    ke.setKey(writtenKey);
    staff->setKey(tick, ke);
    // For C major written key, no visible key signature is needed in the score.
    if (fifths == 0) {
        return;
    }

    Measure* m = score->tick2measure(tick);
    if (!m) {
        return;
    }
    Segment* seg = m->getSegment(SegmentType::KeySig, tick);
    KeySig* ks = Factory::createKeySig(seg);
    ks->setTrack(staffIdx * VOICES);
    ks->setKey(concertKey, writtenKey);
    seg->add(ks);
}

void addInitialTimeSig(MasterScore* score, int nstaves, Fraction ts)
{
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

ClefType pickStaffClef(EncClefType encClef, ClefType /*concertClef*/, ClefType /*transposingClef*/,
                       int keyOffsetSemitones)
{
    const ClefType base = encClef2MuseScore(encClef);
    if (keyOffsetSemitones == 0 || clefGlyphFamily(base) == 0) {
        return base;
    }
    // When Key is +-12 or +-24 semitones, pick the matching octave-decorated clef
    // in the same glyph family. Other offsets fall back to plain base clef.
    static const std::array<ClefType, 8> kCandidates = {
        ClefType::G8_VB, ClefType::G8_VA,
        ClefType::G15_MB, ClefType::G15_MA,
        ClefType::F8_VB, ClefType::F_8VA,
        ClefType::F15_MB, ClefType::F_15MA,
    };
    const int family = clefGlyphFamily(base);
    for (ClefType c : kCandidates) {
        if (clefGlyphFamily(c) == family && clefOctaveOffset(c) == keyOffsetSemitones) {
            return c;
        }
    }
    return base;
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
    case EncRepeatType::CODA1: {
        // CODA1=0x85 is "To Coda" (jump source); CODA2=0x89 is the Coda destination.
        Marker* m = Factory::createMarker(measure);
        m->setMarkerType(MarkerType::TOCODA);
        m->setTrack(0);
        measure->add(m);
        break;
    }
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
        j->setPlayRepeats(true);
        j->setTrack(0);
        measure->add(j);
        break;
    }
    case EncRepeatType::DS: {
        Jump* j = Factory::createJump(measure);
        j->setJumpType(JumpType::DS);
        j->setPlayRepeats(true);
        j->setTrack(0);
        measure->add(j);
        break;
    }
    case EncRepeatType::DCALFINE: {
        Jump* j = Factory::createJump(measure);
        j->setJumpType(JumpType::DC_AL_FINE);
        j->setPlayRepeats(true);
        j->setTrack(0);
        measure->add(j);
        break;
    }
    case EncRepeatType::DSALFINE: {
        Jump* j = Factory::createJump(measure);
        j->setJumpType(JumpType::DS_AL_FINE);
        j->setPlayRepeats(true);
        j->setTrack(0);
        measure->add(j);
        break;
    }
    case EncRepeatType::DCALCODA: {
        Jump* j = Factory::createJump(measure);
        j->setJumpType(JumpType::DC_AL_CODA);
        j->setPlayRepeats(true);
        j->setTrack(0);
        measure->add(j);
        break;
    }
    case EncRepeatType::DSALCODA: {
        Jump* j = Factory::createJump(measure);
        j->setJumpType(JumpType::DS_AL_CODA);
        j->setPlayRepeats(true);
        j->setTrack(0);
        measure->add(j);
        break;
    }
    default:
        break;
    }
}

// Strip trailing ordinal numbers from instrument names ("Bandurria 1ª" -> "Bandurria"; standalone "ª"/"º" also removed).
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

// Lowercase + accent-strip so "Laúd" matches "Laud" and "Percusión" matches "Percusion".
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

// Transposition compatibility: octave-only (chromatic%12==0) always passes (handled by pickStaffClef);
// non-octave requires matching mod-12 with encKey; rejects when Encore says C-instrument (encKey%12==0).
static bool transpCompatibleWith(int tmplChromatic, int encKeySemitones)
{
    if (tmplChromatic % 12 == 0) {
        return true;
    }
    if (encKeySemitones % 12 == 0) {
        return false;
    }
    const auto mod12 = [](int x) { return ((x % 12) + 12) % 12; };
    return mod12(tmplChromatic) == mod12(encKeySemitones);
}

// Find best non-drumset template by name+MIDI score (trackName exact +4, contain +2; MIDI +6; "common" +1).
// With encKeySemitones filter, prefers transposition-compatible match; falls back to best name+MIDI
// match when no compatible match exists (e.g. encKey=0 and no C-pitched variant for this MIDI program).
const InstrumentTemplate* findEncoreInstrumentTemplate(const QString& encName, int encMidiProgram,
                                                       int encKeySemitones)
{
    if (encName.isEmpty()) {
        return nullptr;
    }

    // Names < 4 chars (e.g. SATB labels "S","A","T","B") match too broadly; skip.
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

    const bool filterTransp = (encKeySemitones != ENC_KEY_NO_FILTER);
    const InstrumentTemplate* best = nullptr;
    const InstrumentTemplate* bestCompatible = nullptr;
    int bestScore = 0;
    int bestCompatibleScore = 0;
    int bestNameStrength = 0;
    int bestCompatibleNameStrength = 0;
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
                for (const InstrChannel& ch : it->channel) {
                    if (ch.program() == encMidiProgram) {
                        midiBonus = 6;
                        break;
                    }
                }
            }
            // "common" genre tag breaks ties between same-score templates (e.g. guitar-nylon vs soprano-guitar).
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
            if (filterTransp && transpCompatibleWith(it->transpose.chromatic, encKeySemitones)) {
                if (score > bestCompatibleScore
                    || (score == bestCompatibleScore && nameStrength > bestCompatibleNameStrength)) {
                    bestCompatibleScore = score;
                    bestCompatibleNameStrength = nameStrength;
                    bestCompatible = it;
                }
            }
        }
    }
    return filterTransp ? (bestCompatible ? bestCompatible : best) : best;
}

// Find best drumset template by name score (exact match only, no substring).
const InstrumentTemplate* findDrumsetTemplate(const QString& encName)
{
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
    for (const InstrumentGroup* g : instrumentGroups) {
        for (const InstrumentTemplate* it : g->instrumentTemplates) {
            if (!it->useDrumset) {
                continue;
            }
            const QString nt = normalizeForCompare(it->trackName.toQString());
            const QString nl = normalizeForCompare(
                it->instrumentName.longName().toQString());
            const QString ns = normalizeForCompare(
                it->instrumentName.shortName().toQString());
            int score = 0;
            for (const QString& needle : needles) {
                if (nt == needle) {
                    score += 4;
                }
                if (nl == needle) {
                    score += 2;
                }
                if (ns == needle) {
                    score += 1;
                }
            }
            if (score > bestScore) {
                bestScore = score;
                best = it;
            }
        }
    }
    return best;
}

const InstrumentTemplate* findTemplateByMidi(int encMidiProgram0indexed)
{
    if (encMidiProgram0indexed < 0) {
        return nullptr;
    }
    const InstrumentTemplate* best = nullptr;
    bool bestIsCommon = false;
    for (const InstrumentGroup* g : instrumentGroups) {
        for (const InstrumentTemplate* it : g->instrumentTemplates) {
            if (it->useDrumset) {
                continue;
            }
            for (const InstrChannel& ch : it->channel) {
                if (ch.program() == encMidiProgram0indexed) {
                    bool isCommon = false;
                    for (const InstrumentGenre* gen : it->genres) {
                        if (gen && gen->id == "common") {
                            isCommon = true;
                            break;
                        }
                    }
                    if (!best || (isCommon && !bestIsCommon)) {
                        best = it;
                        bestIsCommon = isCommon;
                    }
                    break;
                }
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
    // Italian tempo terms; BPM values match MuseScore's tempo palette (palettecreator.cpp) for consistent playback.
    struct Entry {
        const char* word;
        double bps;
    };
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
    // Relative markings: become TempoText with BPS=0 (falls back to previous tempo).
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
    // Byte encodes one or two glyphs (e.g. 0x24=tenuto+staccato). See ENCORE_FORMAT.md §Articulation bytes.
    switch (articByte) {
    // Trill/mordent from m16-m17: 0x04..0x07=trill, 0x08=turn, 0x09=wave, 0x0A/0x0C=inv-mordent, 0x0B/0x2F=mordent.
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07: return { SymId::ornamentTrill };
    case 0x08: return { SymId::ornamentTurn };
    case 0x09: return { SymId::ornamentTrill };
    case 0x0A: return { SymId::ornamentShortTrill };    // <inverted-mordent>
    case 0x0C: return { SymId::ornamentTremblement };   // <inverted-mordent long="yes">
    case 0x0B:
    case 0x2F: return { SymId::ornamentMordent };
    case 0x2E: return { SymId::ornamentTurnInverted };  // inverted turn
    case 0x12: return { SymId::articAccentAbove };
    case 0x13: return { SymId::articMarcatoAbove };
    case 0x14: return { SymId::articMarcatoAbove, SymId::articStaccatoAbove };
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
    case 0x22: return { SymId::articAccentAbove, SymId::articTenutoAbove };
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
    case 0x1B: return { SymId::brassMuteClosed };        // technical/stopped (+)
    case 0x30: return { SymId::brassMuteHalfClosed };   // technical/stopped (tick/half stopped)
    // String markings (m3, m4, m18): 0x1E/0x1F=harmonic, 0x44/0x45=thumb-position.
    // 0x46=open-string: handled in encArticByteIsOpenString() (no SymId; uses Fingering "0").
    // 0x47=string-1: handled in encArticByteToStringNumber().
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
    // Finger 1..5 map to bytes 0x0D..0x11.
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
    // 0x46=open-string; emitted as Fingering "0" (STRING_NUMBER style).
    return articByte == 0x46;
}

int encArticByteToStringNumber(quint8 articByte)
{
    // Open string (0x46) is handled separately as plain Fingering "0".
    // 0x47 is "stick" (drumstick technique), not a string number; left unmapped.
    (void)articByte;
    return 0;
}

int encArticByteToScaleStringNumber(quint8 articByte)
{
    // Bytes 0x39..0x40 encode string numbers 1..8 as (byte - 0x38).
    // These appear as explicit anchors in scale exercises; their presence in a
    // measure enables options-bit-0 string number display on all other notes.
    if (articByte >= 0x39 && articByte <= 0x40) {
        return static_cast<int>(articByte) - 0x38;
    }
    return 0;
}

mu::engraving::OrnamentInterval encArticByteToTrillInterval(quint8 articByte)
{
    using mu::engraving::IntervalStep;
    using mu::engraving::IntervalType;
    // Trill artic bytes 0x04..0x07 share the trill glyph but carry an accidental:
    //   0x04: no accidental (AUTO = use key context)
    //   0x05: flat  → minor second above (MINOR)
    //   0x06: sharp → augmented second above (AUGMENTED)
    //   0x07: natural → major second above (MAJOR)
    switch (articByte) {
    case 0x05: return { IntervalStep::SECOND, IntervalType::MINOR };
    case 0x06: return { IntervalStep::SECOND, IntervalType::AUGMENTED };
    case 0x07: return { IntervalStep::SECOND, IntervalType::MAJOR };
    default:   return {};   // AUTO (default)
    }
}
} // namespace mu::iex::encore
