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

// Encore (.enc) file importer for MuseScore.
// Binary format reverse-engineered by Leon Vinken (Enc2MusicXML, GPL v3+) building on enc2ly by Felipe Castro.

#include "import.h"

#include "../parser/elem.h"
#include "../parser/readers.h"

#include <algorithm>

#include <QDataStream>
#include <QFile>
#include <QFileInfo>

#include "engraving/engravingerrors.h"

#include "log.h"

using namespace mu::engraving;

namespace mu::iex::enc {

// Derive display size (1-4) for a given instrument index.
static int staffDisplaySize(const EncRoot& enc, int instrIdx)
{
    if (!enc.lines.empty()) {
        for (const EncLineStaffData& lsd : enc.lines[0].staffData) {
            if (static_cast<int>(lsd.instrumentIndex()) == instrIdx) {
                return std::clamp(static_cast<int>(lsd.staffSizeHint) + 1, 1, 4);
            }
        }
    }
    return std::clamp(static_cast<int>(enc.header.scoreSize), 1, 4);
}

static void logEncRootInfo(const EncRoot& enc)
{
    const EncHeader& h = enc.header;
    const char* fmtName = enc.fmt ? enc.fmt->formatName() : "unknown";

    const char* encVer = (h.chuVersio >= 1000) ? "Encore 5.x"
                         : (h.chuVersio >= 700) ? "Encore 4.x"
                         : "Encore 2.x/3.x (legacy)";

    LOGD() << "---- Encore file info ----";
    LOGD() << "  Magic:" << h.magic.toStdString()
           << "  Format:0x" << QString::number(h.chuMagio, 16).toUpper().toStdString()
           << "(" << fmtName << ")  version=" << h.chuVersio << "(" << encVer << ")";
    LOGD() << "  Lines:" << h.lineCount
           << "  Pages:" << h.pageCount
           << "  Instruments:" << h.instrumentCount
           << "  Staves/sys:" << h.staffPerSystem
           << "  Measures:" << h.measureCount;

    LOGD() << "---- Titles ----";
    if (!enc.titleBlock.title.isEmpty()) {
        LOGD() << "  Title:    " << enc.titleBlock.title.toStdString();
    }
    if (!enc.titleBlock.subtitle.empty() && !enc.titleBlock.subtitle[0].isEmpty()) {
        LOGD() << "  Subtitle: " << enc.titleBlock.subtitle[0].toStdString();
    }
    if (!enc.titleBlock.author.empty() && !enc.titleBlock.author[0].isEmpty()) {
        LOGD() << "  Author:   " << enc.titleBlock.author[0].toStdString();
    }
    if (!enc.titleBlock.copyright.empty() && !enc.titleBlock.copyright[0].isEmpty()) {
        LOGD() << "  Copyrt:   " << enc.titleBlock.copyright[0].toStdString();
    }

    static const char* kSizeLabel[4] = { "60%", "70%", "75%", "100%" };

    LOGD() << "---- Instruments ----";
    for (size_t i = 0; i < enc.instruments.size(); ++i) {
        const EncInstrument& ins = enc.instruments[i];
        const int sz = staffDisplaySize(enc, static_cast<int>(i));
        LOGD() << "  [" << i << "] \"" << ins.name.toStdString() << "\""
               << "  midi=" << ins.midiProgram
               << "  staves=" << ins.nstaves
               << "  key=" << ins.keyTransposeSemitones
               << "  size=" << sz << "(" << kSizeLabel[sz - 1] << ")"
               << (ins.showStaff ? "" : "  hidden");
    }

    LOGD() << "---- Systems ----";
    for (size_t i = 0; i < enc.lines.size(); ++i) {
        const EncLine& ln = enc.lines[i];
        LOGD() << "  [" << i << "] start=" << ln.start << "  count=" << (int)ln.measureCount;
    }

    LOGD() << "---- Tempos ----";
    LOGD() << "  Total: " << enc.measures.size();
    quint8 lastNum = 0, lastDen = 0;
    quint16 lastBpm = 0;
    for (size_t i = 0; i < enc.measures.size(); ++i) {
        const EncMeasure& m = enc.measures[i];
        const bool timeSigChanged = (m.timeSigNum != lastNum || m.timeSigDen != lastDen);
        const bool bpmChanged = (m.bpm != 0 && m.bpm != lastBpm);
        if (i == 0 || timeSigChanged || bpmChanged) {
            LOGD() << "  [" << i << "] " << (int)m.timeSigNum << "/" << (int)m.timeSigDen
                   << (m.bpm ? (QString("  bpm=") + QString::number(m.bpm)).toStdString() : "");
            lastNum = m.timeSigNum;
            lastDen = m.timeSigDen;
            if (m.bpm) {
                lastBpm = m.bpm;
            }
        }
    }
    LOGD() << "---- Page setup ----";
    const EncPageSetup& ps = enc.pageSetup;
    if (ps.hasData) {
        LOGD() << "  WINI: top=" << ps.top << "  left=" << ps.left
               << "  bottomEdge=" << ps.bottomEdge << "  rightEdge=" << ps.rightEdge
               << "  (" << QString::number(ps.top / 72.0, 'f', 3).toStdString() << "\""
               << " / " << QString::number(ps.left / 72.0, 'f', 3).toStdString() << "\" margins)";
    } else {
        LOGD() << "  WINI: absent -- using MuseScore defaults";
    }
    LOGD() << "--------------------------";
}

// Stub: replaced in A6 when the score builder is added.
static void buildScore(mu::engraving::MasterScore*, const EncRoot&, const EncImportOptions&)
{
}

muse::String encoreLoadErrorMessage(const QString& path)
{
    QByteArray head;
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        head = file.read(5);
    }
    const QByteArray magic = head.left(4);
    const muse::String name = muse::String::fromQString(QFileInfo(path).fileName());

    // Older encrypted Encore container (ZBOT/ZBOP/ZBO6).
    if (magic == "ZBOT" || magic == "ZBOP" || magic == "ZBO6") {
        return muse::mtrc("engraving",
                          "“%1” is in an older, encrypted Encore format (%2) that this importer cannot read. "
                          "Open it in Encore and save it again, then import the saved file.")
               .arg(name).arg(muse::String::fromQString(QString::fromLatin1(magic)));
    }
    // Recognizable Encore header, but the file could not be parsed: unsupported variant, damaged,
    // or empty.
    if (magic == "SCOW" || magic == "SCO5") {
        const QString ver = QStringLiteral("%1").arg(
            head.size() >= 5 ? static_cast<unsigned char>(head[4]) : 0, 2, 16, QChar('0'));
        return muse::mtrc("engraving",
                          "“%1” could not be read as an Encore file (format version 0x%2). It may be damaged "
                          "or use an unsupported variant. Try opening it in Encore and saving it again, then "
                          "import the saved file.")
               .arg(name).arg(muse::String::fromQString(ver));
    }
    // No recognizable Encore header at all.
    return muse::mtrc("engraving",
                      "“%1” is not a recognized Encore file. Its header does not match any known Encore "
                      "format, so it may be corrupted or a different type of file.")
           .arg(name);
}

Err importEncore(MasterScore* score, const QString& path, const EncImportOptions& opts)
{
    if (!QFileInfo::exists(path)) {
        return Err::FileNotFound;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return Err::FileOpenError;
    }

    // ZBOT/ZBOP/ZBO6 are older encrypted Encore containers (only the first 42 bytes decrypt with
    // a known XOR key; see ENCORE_FORMAT.md). Not supported here. See MuseScore#24341.
    {
        QByteArray magic4 = file.read(4);
        file.seek(0);
        if (magic4 == "ZBOT" || magic4 == "ZBOP" || magic4 == "ZBO6") {
            LOGW() << "Encore: encrypted format (" << magic4.toStdString()
                   << ") is not supported; re-save the file in Encore as an unencrypted file first.";
            return Err::FileBadFormat;
        }
    }

    QDataStream ds(&file);
    ds.setByteOrder(QDataStream::LittleEndian);

    EncRoot enc;
    if (!enc.read(ds)) {
        return Err::FileBadFormat;
    }

    if (enc.instruments.empty() || enc.measures.empty()) {
        return Err::FileBadFormat;
    }

    logEncRootInfo(enc);
    buildScore(score, enc, opts);

    return Err::NoError;
}
} // namespace mu::iex::enc
