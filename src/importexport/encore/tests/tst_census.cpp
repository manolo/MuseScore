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

// Corpus census: walks a directory of .enc files with the production parser and writes two CSVs,
// one scalar row per file and one long-form histogram table. Used to measure which format variants
// and which byte-level features actually occur, in a real corpus versus in the test fixtures.
//
// Inert unless ENC_CENSUS_DIR is set, so it costs nothing in a normal test run:
//
//   ENC_CENSUS_DIR=~/Scores ENC_CENSUS_OUT=/tmp/corpus \
//     ./bin/iex_encore_tests --gtest_filter='Census.*'
//
// Writes <ENC_CENSUS_OUT>-files.csv and <ENC_CENSUS_OUT>-hist.csv.
//
// This is parse level only (EncRoot::read). Whether a file then imports into a score is measured
// separately, one process per file, so that a single bad file cannot take the whole run down.

#include <gtest/gtest.h>

#include <map>

#include <QBuffer>
#include <QByteArray>
#include <QDataStream>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include "importexport/encore/internal/parser/elem.h"
#include "importexport/encore/internal/parser/readers.h"

// The decryption for the encrypted containers is not part of every branch. Where it is present the
// census decrypts and parses those files like any other; where it is not, it records them as
// encrypted and moves on, so the same file builds either way.
#if __has_include("importexport/encore/internal/parser/zbot.h")
#include "importexport/encore/internal/parser/zbot.h"
#define ENC_CENSUS_HAS_DECRYPT 1
#endif

using namespace mu::iex::enc;

namespace {
// One histogram row: which file, which kind of tally, which key, how many.
struct HistKey {
    QString kind;
    int key = 0;

    bool operator<(const HistKey& o) const { return kind == o.kind ? key < o.key : kind < o.kind; }
};

using Hist = std::map<HistKey, int>;

void bump(Hist& h, const char* kind, int key)
{
    ++h[HistKey{ QString::fromLatin1(kind), key }];
}

// Scalar facts about one file, in the order they are written to the CSV.
struct FileRow {
    QString path;
    qint64 bytes = 0;
    QString outerMagic;      // container magic as found on disk (SCOW, SCO5, ZBOT, ...)
    bool encrypted = false;  // outer magic was a ZBOT-family container
    QString innerMagic;      // magic after decryption; equals outerMagic for plaintext files
    int verByte = -1;        // format version at 0x04, -1 when the magic carries no version
    int appVer = -1;         // Encore app version at 0x28
    int formatRev = -1;      // format revision at 0x3E
    int scoreSize = -1;      // global staff-size selector
    QString reader;          // which EncFormatReader the importer selects
    int hdrLines = -1;
    int hdrPages = -1;
    int hdrInstruments = -1;
    int hdrStaffPerSystem = -1;
    int hdrMeasures = -1;
    bool parsed = false;     // EncRoot::read returned true
    int gotInstruments = 0;
    int gotLines = 0;
    int gotMeasures = 0;
    int gotElements = 0;
    int entryStride = -1;    // observed instrument entry stride, -1 when not derivable
    QString note;            // why a file was skipped or failed
};

QString csvQuote(const QString& s)
{
    QString v = s;
    v.replace('"', "\"\"");
    return '"' + v + '"';
}

// Distance between the first two instrument content positions, which is the instrument entry
// stride the reader derived from the file. Reported so the 112 / 242 / 2158 claim can be checked
// against the corpus instead of trusted.
int deriveEntryStride(const EncRoot& enc)
{
    qint64 prev = -1;
    for (const EncInstrument& in : enc.instruments) {
        if (in.contentFilePos < 0) {
            continue;
        }
        if (prev >= 0) {
            return static_cast<int>(in.contentFilePos - prev);
        }
        prev = in.contentFilePos;
    }
    return -1;
}

void censusOneFile(const QString& path, FileRow& row, Hist& hist)
{
    row.path = path;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        row.note = "open failed";
        return;
    }
    QByteArray data = f.readAll();
    f.close();
    row.bytes = data.size();

    if (data.size() < 8) {
        row.note = "too short";
        return;
    }

    row.outerMagic = QString::fromLatin1(data.left(4));
#ifdef ENC_CENSUS_HAS_DECRYPT
    if (isZbotMagic(data.left(4))) {
        row.encrypted = true;
        zbotDecrypt(data);

        // Optional: drop a plaintext copy next to the census output, so an encrypted file can be
        // inspected with ordinary byte tools. Set ENC_CENSUS_DECRYPT_OUT to a directory.
        const QByteArray outDir = qgetenv("ENC_CENSUS_DECRYPT_OUT");
        if (!outDir.isEmpty()) {
            QDir().mkpath(QString::fromLocal8Bit(outDir));
            QFile out(QString::fromLocal8Bit(outDir) + '/' + QFileInfo(path).fileName());
            if (out.open(QIODevice::WriteOnly)) {
                out.write(data);
            }
        }
    }
#else
    if (row.outerMagic.startsWith("ZBO")) {
        row.encrypted = true;
        row.note = "encrypted, no decoder on this branch";
    }
#endif
    row.innerMagic = QString::fromLatin1(data.left(4));

    // Read the header axes straight off the bytes, before any parsing, so a file the parser
    // rejects still contributes its version coordinates to the inventory.
    const bool bigEndian = (row.innerMagic == "SCO5");
    auto u16 = [&](int off) -> int {
        if (off + 1 >= data.size()) {
            return -1;
        }
        const quint8 a = static_cast<quint8>(data[off]);
        const quint8 b = static_cast<quint8>(data[off + 1]);
        return bigEndian ? ((a << 8) | b) : ((b << 8) | a);
    };
    auto u8 = [&](int off) -> int {
        return off < data.size() ? static_cast<quint8>(data[off]) : -1;
    };

    if (row.innerMagic == "SCOW") {
        row.verByte = u8(0x04);
    }
    row.appVer = u16(0x28);
    row.formatRev = u8(0x3E);

    QBuffer buf(&data);
    buf.open(QIODevice::ReadOnly);
    QDataStream ds(&buf);
    ds.setByteOrder(QDataStream::LittleEndian);

    EncRoot enc;
    row.parsed = enc.read(ds);
    if (enc.fmt) {
        row.reader = QString::fromLatin1(enc.fmt->formatName());
    }
    if (!row.parsed && row.note.isEmpty()) {
        row.note = "parse failed";
    }

    row.scoreSize = enc.header.scoreSize;
    row.hdrLines = enc.header.lineCount;
    row.hdrPages = enc.header.pageCount;
    row.hdrInstruments = enc.header.instrumentCount;
    row.hdrStaffPerSystem = enc.header.staffPerSystem;
    row.hdrMeasures = enc.header.measureCount;
    row.gotInstruments = static_cast<int>(enc.instruments.size());
    row.gotLines = static_cast<int>(enc.lines.size());
    row.gotMeasures = static_cast<int>(enc.measures.size());
    row.entryStride = deriveEntryStride(enc);

    for (const EncInstrument& in : enc.instruments) {
        bump(hist, "instr_nstaves", in.nstaves);
        bump(hist, "instr_midiprog", in.midiProgram);
        bump(hist, "instr_keytranspose", in.keyTransposeSemitones);
        if (in.tabTuning.hasData) {
            bump(hist, "tab_strings", in.tabTuning.strings());
        }
    }
    int lineIdx = 0;
    for (const EncLine& ln : enc.lines) {
        int staffSlot = 0;
        for (const EncLineStaffData& sd : ln.staffData) {
            // Positional key too, so a difference between two saves of one score can be pointed at
            // a specific system and staff instead of just a changed histogram.
            bump(hist, "line_key_at", lineIdx * 1000 + staffSlot * 100 + sd.key);
            ++staffSlot;
            bump(hist, "line_clef", static_cast<int>(sd.clef));
            bump(hist, "line_key", sd.key);
            bump(hist, "line_stafftype", static_cast<int>(sd.staffType));
            bump(hist, "line_staffsize", sd.staffSizeHint);
            bump(hist, "line_hidden", sd.showStaff ? 0 : 1);
        }
        bump(hist, "line_staffkeys_present", ln.staffKeys.empty() ? 0 : 1);
        ++lineIdx;
    }

    for (const EncMeasure& m : enc.measures) {
        bump(hist, "meas_timesig_num", m.timeSigNum);
        bump(hist, "meas_timesig_den", m.timeSigDen);
        bump(hist, "meas_beatticks", m.beatTicks);
        bump(hist, "meas_barstart", m.barTypeStart);
        bump(hist, "meas_barend", m.barTypeEnd);
        for (const auto& up : m.elements) {
            const EncMeasureElem* e = up.get();
            if (!e) {
                continue;
            }
            ++row.gotElements;
            bump(hist, "elem_type", e->type);
            bump(hist, "elem_voice", e->voice);
            bump(hist, "elem_size", e->size);
            // Type and size together: the pair the size-threshold branches actually key off.
            bump(hist, "elem_type_size", e->type * 1000 + e->size);

            if (const EncNote* n = dynamic_cast<const EncNote*>(e)) {
                bump(hist, "note_size", n->size);
                bump(hist, "note_facevalue", n->faceValue);
                bump(hist, "note_grace1", n->grace1);
                bump(hist, "note_grace2", n->grace2);
                bump(hist, "note_tuplet", n->tuplet);
                bump(hist, "note_dotcontrol", n->dotControl);
                // Staff position, signed: the same note keeps it across generations, so a
                // conversion pair that disagrees here means one of the two layouts is misread.
                bump(hist, "note_position", n->position);
                bump(hist, "note_pitch", n->semiTonePitch);
                bump(hist, "note_options", n->options);
                bump(hist, "note_alterglyph", n->alterationGlyph);
                if (n->articulationUp) {
                    bump(hist, "note_artic_up", n->articulationUp);
                }
                if (n->articulationDown) {
                    bump(hist, "note_artic_down", n->articulationDown);
                }
            } else if (const EncOrnament* o = dynamic_cast<const EncOrnament*>(e)) {
                bump(hist, "orn_subtype", o->tipo);
                bump(hist, "orn_size", o->size);
                bump(hist, "orn_subtype_size", o->tipo * 1000 + o->size);
                bump(hist, "orn_speguleco", o->speguleco);
                bump(hist, "orn_almezuro", o->alMezuro);
                bump(hist, "orn_altmezuro", o->altMezuro);
                bump(hist, "orn_noto", o->noto);

                // Per-subtype views of the fields the far offsets feed. A spanner whose forward
                // measure count lands outside the score, or a staff text whose TEXT index lands
                // outside the TEXT block, is a field that was read from the wrong offset.
                const EncOrnamentType ot = o->ornType();
                const int measIdx = static_cast<int>(&m - enc.measures.data());
                const int nMeas = static_cast<int>(enc.measures.size());
                const int nText = static_cast<int>(enc.textBlock.entries.size());
                if (ot == EncOrnamentType::SLURSTART || ot == EncOrnamentType::WEDGESTART) {
                    const bool isSlur = (ot == EncOrnamentType::SLURSTART);
                    bump(hist, isSlur ? "slur_size" : "hairpin_size", o->size);
                    bump(hist, isSlur ? "slur_almezuro" : "hairpin_almezuro", o->alMezuro);
                    bump(hist, isSlur ? "slur_altmezuro" : "hairpin_altmezuro", o->altMezuro);
                    const bool inRange = (measIdx + o->alMezuro) >= 0 && (measIdx + o->alMezuro) < nMeas;
                    bump(hist, isSlur ? "slur_end_in_range" : "hairpin_end_in_range", inRange ? 1 : 0);
                }
                if (ot == EncOrnamentType::STAFFTEXT) {
                    bump(hist, "stafftext_size", o->size);
                    bump(hist, "stafftext_tind", o->tind);
                    bump(hist, "stafftext_tind_in_range", (o->tind < nText) ? 1 : 0);
                }
                if (ot == EncOrnamentType::TEMPO) {
                    bump(hist, "tempo_size", o->size);
                    bump(hist, "tempo_beatunit", o->noto);
                    bump(hist, "tempo_bpm", o->tempo);
                    // A tempo mark outside 20..400 bpm was read from the wrong slot.
                    bump(hist, "tempo_bpm_plausible", (o->tempo >= 20 && o->tempo <= 400) ? 1 : 0);
                }
            } else if (const EncRest* r = dynamic_cast<const EncRest*>(e)) {
                bump(hist, "rest_size", r->size);
                bump(hist, "rest_facevalue", r->faceValue);
                bump(hist, "rest_tuplet", r->tuplet);
            } else if (const EncClefChange* c = dynamic_cast<const EncClefChange*>(e)) {
                bump(hist, "clefchange_type", static_cast<int>(c->clefType));
            } else if (const EncKeyChange* k = dynamic_cast<const EncKeyChange*>(e)) {
                bump(hist, "keychange_type", k->tipo);
            } else if (const EncMidiCc* cc = dynamic_cast<const EncMidiCc*>(e)) {
                bump(hist, "midicc_controller", cc->controller);
            }
        }
    }

    bump(hist, "text_entries", static_cast<int>(enc.textBlock.entries.size()));
    bump(hist, "prec_present", enc.printSetup.hasData ? 1 : 0);
    bump(hist, "wini_present", enc.pageSetup.hasData ? 1 : 0);
    if (enc.printSetup.hasData) {
        bump(hist, "prec_papersize", enc.printSetup.paperSize);
        bump(hist, "prec_orientation", enc.printSetup.orientation);
        bump(hist, "prec_scale", enc.printSetup.scale);
    }
}
} // namespace

TEST(Census, walk_directory)
{
    const QByteArray dirEnv = qgetenv("ENC_CENSUS_DIR");
    if (dirEnv.isEmpty()) {
        GTEST_SKIP() << "set ENC_CENSUS_DIR to run the corpus census";
    }

    QString dir = QString::fromLocal8Bit(dirEnv);
    if (dir.startsWith('~')) {
        dir.replace(0, 1, QDir::homePath());
    }
    ASSERT_TRUE(QFileInfo(dir).isDir()) << "not a directory: " << dir.toStdString();

    QString out = QString::fromLocal8Bit(qgetenv("ENC_CENSUS_OUT"));
    if (out.isEmpty()) {
        out = QDir::tempPath() + "/enc-census";
    }

    QStringList paths;
    QDirIterator it(dir, QStringList() << "*.enc" << "*.ENC", QDir::Files,
                    QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while (it.hasNext()) {
        paths << it.next();
    }
    paths.sort();
    ASSERT_FALSE(paths.isEmpty()) << "no .enc files under " << dir.toStdString();

    QFile filesCsv(out + "-files.csv");
    QFile histCsv(out + "-hist.csv");
    ASSERT_TRUE(filesCsv.open(QIODevice::WriteOnly | QIODevice::Text));
    ASSERT_TRUE(histCsv.open(QIODevice::WriteOnly | QIODevice::Text));
    QTextStream fs(&filesCsv);
    QTextStream hs(&histCsv);

    fs << "file,bytes,outer_magic,encrypted,inner_magic,ver_byte,app_ver,format_rev,score_size,reader,"
       << "hdr_lines,hdr_pages,hdr_instruments,hdr_staff_per_system,hdr_measures,"
       << "parsed,got_instruments,got_lines,got_measures,got_elements,entry_stride,note\n";
    hs << "file,kind,key,count\n";

    int parsedOk = 0;
    for (const QString& p : paths) {
        FileRow row;
        Hist hist;
        censusOneFile(p, row, hist);
        if (row.parsed) {
            ++parsedOk;
        }

        // Path relative to the scan root, not the bare file name: two directories in one scan can
        // hold the same score (an original and its conversion) and the rows must stay distinct.
        QString base = QDir(dir).relativeFilePath(p);
        fs << csvQuote(base) << ',' << row.bytes << ',' << csvQuote(row.outerMagic) << ','
           << (row.encrypted ? 1 : 0) << ',' << csvQuote(row.innerMagic) << ','
           << row.verByte << ',' << row.appVer << ',' << row.formatRev << ',' << row.scoreSize << ','
           << csvQuote(row.reader) << ','
           << row.hdrLines << ',' << row.hdrPages << ',' << row.hdrInstruments << ','
           << row.hdrStaffPerSystem << ',' << row.hdrMeasures << ','
           << (row.parsed ? 1 : 0) << ',' << row.gotInstruments << ',' << row.gotLines << ','
           << row.gotMeasures << ',' << row.gotElements << ',' << row.entryStride << ','
           << csvQuote(row.note) << '\n';

        for (const auto& kv : hist) {
            hs << csvQuote(base) << ',' << csvQuote(kv.first.kind) << ',' << kv.first.key << ','
               << kv.second << '\n';
        }
    }

    fs.flush();
    hs.flush();
    std::cout << "census: " << paths.size() << " files, " << parsedOk << " parsed, wrote "
              << (out + "-files.csv").toStdString() << " and "
              << (out + "-hist.csv").toStdString() << std::endl;
}
