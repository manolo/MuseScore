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

#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/part.h"

using namespace mu::engraving;

namespace mu::iex::encore {
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
} // namespace mu::iex::encore
