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

#include "ctx.h"
#include "builders.h"
#include "resolvers.h"

// Encore (.enc) file importer for MuseScore.
// Binary format reverse-engineered by Leon Vinken (Enc2MusicXML, GPL v3+) building on enc2ly by Felipe Castro.

#include "import.h"

#include "../parser/elements.h"
#include "mapping.h"
#include "../parser/ticks.h"
#include "tuplets.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <map>
#include <set>
#include <vector>

#include <QDataStream>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include "engraving/dom/arpeggio.h"
#include "engraving/dom/box.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/dynamic.h"
#include "engraving/dom/fermata.h"
#include "engraving/dom/fingering.h"
#include "engraving/dom/ornament.h"
#include "engraving/dom/tremolosinglechord.h"
#include "engraving/dom/clef.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/hairpin.h"
#include "engraving/dom/harmony.h"
#include "engraving/dom/jump.h"
#include "engraving/dom/key.h"
#include "engraving/dom/keysig.h"
#include "engraving/dom/lyrics.h"
#include "engraving/dom/marker.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/instrtemplate.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/part.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/slur.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftext.h"
#include "engraving/dom/tempotext.h"
#include "engraving/dom/text.h"
#include "engraving/dom/tie.h"
#include "engraving/dom/timesig.h"
#include "engraving/dom/tuplet.h"
#include "engraving/dom/system.h"
#include "engraving/dom/volta.h"
#include "engraving/engravingerrors.h"

#include "log.h"

using namespace mu::engraving;

namespace mu::iex::encore {
// faceValue low nibble: 1=whole, 2=half ... 8=256th; 0 and 9..15 are invalid.
// High nibble carries unrelated flags.
bool isValidFaceValue(quint8 faceValue)
{
    const quint8 fv = faceValue & 0x0F;
    return fv > 0 && fv <= 8;
}

void applyConcertPitch(Note* n, int semitone)
{
    n->setPitch(semitone);
    n->setTpcFromPitch();
}

static void logEncRootInfo(const EncRoot& enc)
{
    const EncHeader& h = enc.header;
    const char* fmtName = enc.fmt ? enc.fmt->formatName() : "unknown";

    LOGD() << "---- Encore file info ----";
    LOGD() << "  Magic:          " << h.magic.toStdString();
    LOGD() << "  Format:         0x" << QString::number(h.chuMagio, 16).toUpper().toStdString()
           << " (" << fmtName << ")  version=" << h.chuVersio;
    LOGD() << "  Lines:" << h.lineCount
           << "  Pages:" << h.pageCount
           << "  Instruments:" << h.instrumentCount
           << "  Staves/sys:" << h.staffPerSystem
           << "  Measures:" << h.measureCount
           << "  Size:" << static_cast<int>(h.scoreSize);

    LOGD() << "---- Titles ----";
    if (!enc.titleBlock.title.isEmpty()) {
        LOGD() << "  Title:    " << enc.titleBlock.title.toStdString();
    }
    for (const QString& s : enc.titleBlock.subtitle) {
        if (!s.isEmpty()) {
            LOGD() << "  Subtitle: " << s.toStdString();
        }
    }
    for (const QString& s : enc.titleBlock.author) {
        if (!s.isEmpty()) {
            LOGD() << "  Author:   " << s.toStdString();
        }
    }
    for (const QString& s : enc.titleBlock.instruction) {
        if (!s.isEmpty()) {
            LOGD() << "  Instr:    " << s.toStdString();
        }
    }
    for (const QString& s : enc.titleBlock.copyright) {
        if (!s.isEmpty()) {
            LOGD() << "  Copyrt:   " << s.toStdString();
        }
    }

    LOGD() << "---- Texts ----";
    LOGD() << "  Entries: " << enc.textBlock.entries.size();
    for (size_t i = 0; i < enc.textBlock.entries.size(); ++i) {
        const QString& e = enc.textBlock.entries[i];
        const QString preview = e.length() > 60 ? e.left(57) + "..." : e;
        LOGD() << "  [" << i << "] \"" << preview.toStdString() << "\"";
    }

    LOGD() << "---- Instruments ----";
    for (size_t i = 0; i < enc.instruments.size(); ++i) {
        const EncInstrument& ins = enc.instruments[i];
        LOGD() << "  [" << i << "] \"" << ins.name.toStdString() << "\""
               << "  midi=" << ins.midiProgram
               << "  staves=" << ins.nstaves
               << "  key=" << ins.keyTransposeSemitones
               << (ins.showStaff ? "" : "  hidden");
    }

    LOGD() << "---- Lines ----";
    for (size_t i = 0; i < enc.lines.size(); ++i) {
        const EncLine& ln = enc.lines[i];
        LOGD() << "  [" << i << "] start=" << ln.start << "  count=" << (int)ln.measureCount;
    }

    LOGD() << "---- Measures ----";
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
        LOGD() << "  WINI: absent — using MuseScore defaults";
    }
    LOGD() << "--------------------------";
}

// Map Encore score-size (1–4) to MuseScore Staff Properties → Scale (Pid::MAG).
// 1=60%, 2=70%, 3=75%, 4=100%.  Global spatium is not changed.
static void applyStaffScale(MasterScore* score, const EncRoot& enc)
{
    static const double kScaleBySize[4] = { 0.60, 0.70, 0.75, 1.00 };
    const int sz = std::clamp(static_cast<int>(enc.header.scoreSize), 1, 4);
    const double scale = kScaleBySize[sz - 1];

    LOGD("enc importer: scoreSize=%d → staff scale=%.0f%%", sz, scale * 100.0);

    for (Staff* staff : score->staves()) {
        staff->setProperty(Pid::MAG, PropertyValue(scale));
    }
}

static void applyPageMargins(MasterScore* score, const EncPageSetup& ps)
{
    if (!ps.hasData) {
        return;
    }
    // WINI fields are in points (1/72 inch); clamp margins to avoid zero-margin files producing invalid pagePrintableWidth.
    static constexpr double kMinLR = 0.03;   // min left/right margin (inches)
    static constexpr double kMinTB = 0.10;   // min top/bottom margin (inches)
    static constexpr double kMaxM  = 0.60;   // max margin (inches)

    double topIn  = ps.top / 72.0;
    double leftIn = ps.left / 72.0;
    double printW = (ps.rightEdge - ps.left) / 72.0;
    double printH = (ps.bottomEdge - ps.top) / 72.0;
    const double pageHIn = score->style().styleD(Sid::pageHeight);
    const double pageWIn = score->style().styleD(Sid::pageWidth);

    topIn  = std::clamp(topIn,  kMinTB, kMaxM);
    leftIn = std::clamp(leftIn, kMinLR, kMaxM);

    const double maxPrintW = pageWIn - leftIn - kMinLR;
    if (printW > maxPrintW) {
        printW = maxPrintW;
    }

    double bottomIn = std::max(0.0, pageHIn - topIn - printH);
    bottomIn = std::clamp(bottomIn, kMinTB, kMaxM);

    score->style().set(Sid::pageOddTopMargin,     topIn);
    score->style().set(Sid::pageEvenTopMargin,    topIn);
    score->style().set(Sid::pageOddLeftMargin,    leftIn);
    score->style().set(Sid::pageEvenLeftMargin,   leftIn);
    score->style().set(Sid::pagePrintableWidth,   printW);
    score->style().set(Sid::pageOddBottomMargin,  bottomIn);
    score->style().set(Sid::pageEvenBottomMargin, bottomIn);
}

static void buildScore(MasterScore* score, const EncRoot& enc)
{
    score->style().set(Sid::chordsXmlFile, true);
    score->chordList()->read(u"chords.xml");

    // Enable multi-measure rest display only when the Encore file actually uses them.
    // A file with no mrestCount > 1 REST elements should show individual whole rests,
    // not collapsed multi-measure rests.
    const bool hasMMRest = std::any_of(enc.measures.begin(), enc.measures.end(),
                                       [](const EncMeasure& m) {
        if (m.elements.empty()) {
            return false;
        }
        for (const auto& ep : m.elements) {
            if (static_cast<EncElemType>(ep->type) != EncElemType::REST) {
                return false;
            }
        }
        return static_cast<const EncRest*>(m.elements[0].get())->mrestCount > 1;
    });
    score->style().set(Sid::createMultiMeasureRests, hasMMRest);

    // Encore positions tuplet brackets/numbers flush against note heads and stems
    // with no extra vertical gap, and never pushes them outside the staff.
    score->style().set(Sid::tupletOutOfStaff,      false);
    score->style().set(Sid::tupletVHeadDistance,   0.0);
    score->style().set(Sid::tupletVStemDistance,   0.0);

    BuildCtx ctx{ score, enc, enc.fmt.get() };
    ctx.impliedTuplets     = ctx.fmt->supportsImpliedTuplets();
    ctx.g1LowTieSender     = ctx.fmt->usesG1LowTieSender();
    ctx.alMezuroIsReliable = ctx.fmt->alMezuroIsReliable();
    buildParts(ctx);
    buildMeasures(ctx);
    buildInitialSignatures(ctx);
    buildNoteLoop(ctx);

    applyPageMargins(score, enc.pageSetup);
    applyStaffScale(score, enc);

    resolveAll(ctx);

    score->spell();
    addTitleFrame(score, enc.titleBlock);
    score->setUpTempoMap();
    score->doLayout();
}

Err importEncore(MasterScore* score, const QString& path)
{
    if (!QFileInfo::exists(path)) {
        return Err::FileNotFound;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return Err::FileOpenError;
    }

    // ZBOT = Encore 4 format, not supported here. See MuseScore#24341.
    {
        QByteArray magic4 = file.read(4);
        file.seek(0);
        if (magic4 == "ZBOT") {
            LOGW("Encore: ZBOT format (Encore 4) is not supported. "
                 "Please re-save the file using Encore 5 to convert it first.");
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
    buildScore(score, enc);

    muse::Ret integrity = score->sanityCheck();
    if (!integrity) {
        LOGW() << "Encore import: score corruption detected:\n" << integrity.text();
    }

    return Err::NoError;
}
} // namespace mu::iex::encore
