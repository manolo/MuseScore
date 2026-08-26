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

// Walk a directory of .enc files, import each one for real, and write what the score came out
// holding: one row per part and one per (part, measure, voice), with everything positional left
// out. Two directories holding the same scores saved by different Encore versions can then be
// compared row by row, which is what turns "the importer reads this file differently" into a
// difference someone can look at.
//
// This is the import level. The parse level lives in tst_census.cpp, and the two answer different
// questions: the census says what the bytes hold, this says what the user ends up with.
//
//   ENC_DIGEST_DIR=~/Scores/downloads/bandurriator/4.3 \
//   ENC_DIGEST_OUT=/tmp/digest-43 ctest -R iex_encore_tests
//
// Skipped unless ENC_DIGEST_DIR is set, so a normal test run never touches a corpus.

#include <gtest/gtest.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include "engraving/compat/scoreaccess.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/chordrest.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/part.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/text.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/volta.h"

#include "importexport/encore/internal/importer/import.h"
#include "engraving/infrastructure/localfileinfoprovider.h"
#include <iostream>

using namespace mu::engraving;

namespace {
QString csvQuote(const QString& s)
{
    QString r = s;
    r.replace('"', "\"\"");
    return '"' + r + '"';
}

// One event of a voice, written so that a difference in the text is a difference in the music:
// offset from the bar line, kind, pitches, duration, and the bracket it belongs to.
QString eventText(const ChordRest* cr, const Fraction& measTick)
{
    QString s = (cr->tick() - measTick).toString() + (cr->isChord() ? "n" : "r");
    if (cr->isChord()) {
        const Chord* c = toChord(cr);
        std::vector<int> pitches;
        for (const Note* n : c->notes()) {
            pitches.push_back(n->pitch());
        }
        std::sort(pitches.begin(), pitches.end());
        for (size_t i = 0; i < pitches.size(); ++i) {
            s += (i ? "." : ":") + QString::number(pitches[i]);
        }
        bool tied = false;
        for (const Note* n : c->notes()) {
            tied = tied || n->tieFor() != nullptr;
        }
        if (tied) {
            s += "~";
        }
        if (!c->graceNotes().empty()) {
            s += "g" + QString::number(c->graceNotes().size());
        }
    }
    s += "/" + cr->ticks().toString();
    if (const Tuplet* t = cr->tuplet()) {
        s += QString("[%1:%2]").arg(t->ratio().numerator()).arg(t->ratio().denominator());
    }
    return s;
}

struct MeasureRow {
    int part = 0;
    int measure = 0;
    int voice = 0;
    QString timesig;
    QString nominal;
    QString actual;
    int fifths = 0;
    QString clefs;
    QString repeats;
    QString events;
    QString lyrics;
    QString directions;
};

void collectStaffTexts(const Measure* m, staff_idx_t staffFrom, staff_idx_t staffTo, QString& out)
{
    for (const EngravingItem* e : m->el()) {
        if (e && e->isTextBase() && e->staffIdx() >= staffFrom && e->staffIdx() < staffTo) {
            out += (out.isEmpty() ? "" : "|") + toTextBase(e)->plainText();
        }
    }
    for (const Segment* s = m->first(); s; s = s->next()) {
        for (const EngravingItem* e : s->annotations()) {
            if (!e || e->staffIdx() < staffFrom || e->staffIdx() >= staffTo) {
                continue;
            }
            if (e->isDynamic() || e->isTextBase()) {
                out += (out.isEmpty() ? "" : "|") + toTextBase(e)->plainText();
            }
        }
    }
}
} // namespace

TEST(Digest, walk_directory)
{
    const QByteArray dirEnv = qgetenv("ENC_DIGEST_DIR");
    if (dirEnv.isEmpty()) {
        GTEST_SKIP() << "set ENC_DIGEST_DIR to digest a corpus";
    }
    QString dir = QString::fromLocal8Bit(dirEnv);
    if (dir.startsWith('~')) {
        dir.replace(0, 1, QDir::homePath());
    }
    ASSERT_TRUE(QFileInfo(dir).isDir()) << "not a directory: " << dir.toStdString();

    QString out = QString::fromLocal8Bit(qgetenv("ENC_DIGEST_OUT"));
    if (out.isEmpty()) {
        out = QDir::tempPath() + "/enc-digest";
    }

    QStringList paths;
    QDirIterator it(dir, QStringList() << "*.enc" << "*.ENC" << "*.mus", QDir::Files,
                    QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while (it.hasNext()) {
        paths << it.next();
    }
    paths.sort();
    ASSERT_FALSE(paths.isEmpty()) << "no .enc files under " << dir.toStdString();

    // A file that crashes the importer takes the whole process with it, and no try/catch can help.
    // The driver script restarts the walk past the offender, so the walk starts where it is told and
    // appends rather than truncating.
    const int startAt = qgetenv("ENC_DIGEST_START").toInt();
    const QIODevice::OpenMode mode = startAt > 0
                                     ? (QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)
                                     : (QIODevice::WriteOnly | QIODevice::Text);

    QFile filesCsv(out + "-files.csv");
    QFile partsCsv(out + "-parts.csv");
    QFile measCsv(out + "-measures.csv");
    ASSERT_TRUE(filesCsv.open(mode));
    ASSERT_TRUE(partsCsv.open(mode));
    ASSERT_TRUE(measCsv.open(mode));
    QTextStream fs(&filesCsv), ps(&partsCsv), ms(&measCsv);
    if (startAt == 0) {
        fs << "file,imported,sane,parts,staves,measures,note\n";
        ps << "file,part,staves,instrument_id,program,transpose_chromatic,transpose_diatonic,long_name\n";
        ms << "file,part,measure,voice,timesig,nominal,actual,fifths,clefs,repeats,events,lyrics,directions\n";
    }

    int imported = 0, sane = 0;
    for (int idx = startAt; idx < paths.size(); ++idx) {
        const QString path = paths.at(idx);
        const QString base = QDir(dir).relativeFilePath(path);
        // Unbuffered and flushed: when the next file takes the process down, this line is the record
        // of how far the walk got.
        std::cerr << "DIGEST_AT " << idx << ' ' << base.toStdString() << std::endl;
        MasterScore* score = compat::ScoreAccess::createMasterScoreWithBaseStyle(nullptr);
        score->setFileInfoProvider(std::make_shared<LocalFileInfoProvider>(path));
        Err err = Err::NoError;
        QString note;
        try {
            err = mu::iex::enc::importEncore(score, path, mu::iex::enc::EncImportOptions {});
        } catch (...) {
            note = "threw";
        }
        const bool ok = note.isEmpty() && err == Err::NoError && score->firstMeasure();
        if (!ok) {
            if (note.isEmpty()) {
                note = QString("import err %1").arg(static_cast<int>(err));
            }
            fs << csvQuote(base) << ",0,0,0,0,0," << csvQuote(note) << '\n';
            delete score;
            continue;
        }
        ++imported;
        muse::Ret ret = score->sanityCheck();
        if (ret) {
            ++sane;
        } else {
            note = "corrupt";
        }
        fs << csvQuote(base) << ",1," << (ret ? 1 : 0) << ',' << score->parts().size() << ','
           << score->nstaves() << ',' << score->nmeasures() << ',' << csvQuote(note) << '\n';

        int partIdx = 0;
        for (const Part* part : score->parts()) {
            const Instrument* ins = part->instrument();
            ps << csvQuote(base) << ',' << partIdx << ',' << part->nstaves() << ','
               << csvQuote(ins ? ins->id().toQString() : QString())
               << ',' << (ins && !ins->channel().empty() ? ins->channel(0)->program() : -1)
               << ',' << (ins ? ins->transpose().chromatic : 0)
               << ',' << (ins ? ins->transpose().diatonic : 0)
               << ',' << csvQuote(part->partName()) << '\n';

            const staff_idx_t staffFrom = part->staves().empty() ? 0 : part->staves().front()->idx();
            const staff_idx_t staffTo = staffFrom + part->nstaves();
            int measIdx = 0;
            for (const Measure* m = score->firstMeasure(); m; m = m->nextMeasure(), ++measIdx) {
                QString clefs, repeats, lyrics, directions;
                for (const Segment* s = m->first(); s; s = s->next()) {
                    for (staff_idx_t st = staffFrom; st < staffTo; ++st) {
                        const EngravingItem* e = s->element(st * VOICES);
                        if (e && e->isClef()) {
                            clefs += (clefs.isEmpty() ? "" : ".")
                                     + QString::number(static_cast<int>(toClef(e)->clefType()));
                        }
                    }
                }
                if (m->repeatStart()) {
                    repeats += "S";
                }
                if (m->repeatEnd()) {
                    repeats += "E";
                }
                for (auto& pair : score->spanner()) {
                    const Spanner* sp = pair.second;
                    if (sp && sp->isVolta() && sp->tick() == m->tick()) {
                        for (int e : toVolta(sp)->endings()) {
                            repeats += "v" + QString::number(e);
                        }
                    }
                }
                collectStaffTexts(m, staffFrom, staffTo, directions);

                for (int v = 0; v < static_cast<int>(VOICES); ++v) {
                    QString events;
                    Fraction actual(0, 1);
                    for (const Segment* s = m->first(SegmentType::ChordRest); s;
                         s = s->next(SegmentType::ChordRest)) {
                        for (staff_idx_t st = staffFrom; st < staffTo; ++st) {
                            const EngravingItem* e = s->element(st * VOICES + v);
                            if (!e || !e->isChordRest()) {
                                continue;
                            }
                            const ChordRest* cr = toChordRest(e);
                            events += (events.isEmpty() ? "" : " ") + eventText(cr, m->tick());
                            actual += cr->ticks();
                            for (const Lyrics* l : cr->lyrics()) {
                                if (l) {
                                    lyrics += (lyrics.isEmpty() ? "" : "|") + l->plainText();
                                }
                            }
                        }
                    }
                    if (events.isEmpty()) {
                        continue;
                    }
                    ms << csvQuote(base) << ',' << partIdx << ',' << measIdx << ',' << v << ','
                       << csvQuote(m->timesig().toString()) << ',' << csvQuote(m->ticks().toString())
                       << ',' << csvQuote(actual.toString()) << ',' << 0 << ','
                       << csvQuote(clefs) << ',' << csvQuote(repeats) << ','
                       << csvQuote(events) << ',' << csvQuote(lyrics) << ','
                       << csvQuote(directions) << '\n';
                }
            }
            ++partIdx;
        }
        delete score;
        fs.flush();
        ps.flush();
        ms.flush();
    }

    fs.flush();
    ps.flush();
    ms.flush();
    std::cout << "digest: " << paths.size() << " files, " << imported << " imported, "
              << sane << " sane, wrote " << (out + "-files.csv").toStdString() << ", "
              << (out + "-parts.csv").toStdString() << " and "
              << (out + "-measures.csv").toStdString() << std::endl;
}
