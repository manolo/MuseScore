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

#include "mappers.h"

#include <QRegularExpression>

#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/part.h"

using namespace mu::engraving;

namespace mu::iex::enc {
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

// Minimum instrument name length for template search. Single-letter SATB labels
// ("S","A","T","B") match too broadly with substring scoring; skip them.
static constexpr int kMinInstrNameLen = 4;

// Instrument template matching scores (see findEncoreInstrumentTemplate).
static constexpr int kScoreTrackExact    = 4;  // trackName == needle
static constexpr int kScoreTrackContains = 2;  // trackName contains needle
static constexpr int kScoreLongExact     = 2;  // longName == needle
static constexpr int kScoreLongContains  = 1;  // longName contains needle
static constexpr int kScoreShortExact    = 1;  // shortName == needle
static constexpr int kScoreMidiMatch     = 6;  // MIDI program matches
static constexpr int kScoreCommonGenre   = 1;  // "common" genre tag

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
    if (encName.trimmed().size() < static_cast<qsizetype>(kMinInstrNameLen)) {
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
    for (QString word : norm.split(u' ', Qt::SkipEmptyParts)) {
        // Strip trailing punctuation so "Bandurr." matches "Bandurria" via contains.
        while (!word.isEmpty() && !word.back().isLetterOrNumber()) {
            word.chop(1);
        }
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
                    s += kScoreTrackExact;
                } else if (nt.contains(needle)) {
                    s += kScoreTrackContains;
                }
                if (nl == needle) {
                    s += kScoreLongExact;
                } else if (nl.contains(needle)) {
                    s += kScoreLongContains;
                }
                if (ns == needle) {
                    s += kScoreShortExact;
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
                        midiBonus = kScoreMidiMatch;
                        break;
                    }
                }
            }
            // "common" genre tag breaks ties between same-score templates (e.g. guitar-nylon vs soprano-guitar).
            int commonBonus = 0;
            for (const InstrumentGenre* gen : it->genres) {
                if (gen && gen->id == "common") {
                    commonBonus = kScoreCommonGenre;
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
    if (encName.trimmed().size() < static_cast<qsizetype>(kMinInstrNameLen)) {
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
    for (QString word : norm.split(u' ', Qt::SkipEmptyParts)) {
        while (!word.isEmpty() && !word.back().isLetterOrNumber()) {
            word.chop(1);
        }
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

// Strip a trailing parenthetical variant suffix: "Classical Guitar (tablature)" -> "Classical Guitar".
static QString stripVariantSuffix(const QString& trackName)
{
    QString s = trackName.trimmed();
    const int paren = s.lastIndexOf(u'(');
    if (paren > 0) {
        s = s.left(paren).trimmed();
    }
    return normalizeForCompare(s);
}

const InstrumentTemplate* findInstrumentVariant(const InstrumentTemplate* base, bool wantTab)
{
    if (!base) {
        return nullptr;
    }
    const bool baseIsTab = (base->staffGroup == StaffGroup::TAB);
    if (baseIsTab == wantTab) {
        return base;   // already the requested kind
    }
    const String baseXmlId = base->musicXmlId;
    const QString baseName = stripVariantSuffix(base->trackName.toQString());

    const InstrumentTemplate* best = nullptr;
    bool bestIsCommon = false;
    for (const InstrumentGroup* g : instrumentGroups) {
        for (const InstrumentTemplate* it : g->instrumentTemplates) {
            if (it->useDrumset) {
                continue;
            }
            if ((it->staffGroup == StaffGroup::TAB) != wantTab) {
                continue;
            }
            const bool sameByXmlId = !baseXmlId.isEmpty() && it->musicXmlId == baseXmlId;
            const bool sameByName = !baseName.isEmpty()
                                    && stripVariantSuffix(it->trackName.toQString()) == baseName;
            if (!sameByXmlId && !sameByName) {
                continue;
            }
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
            if (it->useDrumset || it->channel.empty()) {
                continue;
            }
            // Match only the first channel of each instrument. The first channel is the
            // instrument's primary sound; additional channels (tremolo, pizzicato, mute…)
            // are articulation variants that share programs across many instruments and
            // would produce false matches if included.
            if (it->channel.front().program() != encMidiProgram0indexed) {
                continue;
            }
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
        }
    }
    return best;
}
} // namespace mu::iex::enc
