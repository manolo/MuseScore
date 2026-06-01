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
// The binary format was reverse-engineered by Leon Vinken (Enc2MusicXML project,
// https://github.com/lvinken/Enc2MusicXML, GPL v3+) building on enc2ly by Felipe Castro.
// This importer is based on that work.

#include "import.h"

#include "../parser/elements.h"
#include "mapping.h"
#include "../parser/ticks.h"
#include "tuplets.h"

#include <algorithm>
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
static inline bool isValidFaceValue(quint8 faceValue)
{
    const quint8 fv = faceValue & 0x0F;
    return fv > 0 && fv <= 8;
}

static inline void applyConcertPitch(Note* n, int semitone)
{
    n->setPitch(semitone);
    n->setTpcFromPitch();
}

// ---------------------------------------------------------------------------

static void clearImportState()
{
    // ctx.h globals persist across calls in same process (tests, batch) — reset before each import.
    pendingHairpins.clear();
    pendingSlurs.clear();
    pendingArpeggios.clear();
    pendingOrnTremolos.clear();
    pendingTrills.clear();
    pendingStaccatos.clear();
    pendingMarkers.clear();
    activeVolta    = nullptr;
    activeVoltaBits = 0;
    tuplets.clear();
    pendingTieNote.clear();
    pendingLyrics.clear();
    nextLyricHyphenBefore.clear();
    cumTick.clear();
    prevMidiTick.clear();
    prevEncVoice.clear();
    lastChordPos.clear();
    pendingGraces.clear();
    streamOffset.clear();
    v0xA6LeadingGraceFv.clear();
    v0xA6GraceStolenTicks.clear();
}

static void logEncFileInfo(const EncFile& enc)
{
    const EncHeader& h = enc.header;
    const char* fmtName = h.isVeryOldFormat() ? "v0xA6" : h.isOldFormat() ? "v0xC2" : "v0xC4";

    LOGD() << "---- Encore file info ----";
    LOGD() << "  Magic:          " << h.magic.toStdString();
    LOGD() << "  Format:         0x" << QString::number(h.chuMagio, 16).toUpper().toStdString()
           << " (" << fmtName << ")  version=" << h.chuVersio;
    LOGD() << "  Lines:"     << h.lineCount
           << "  Pages:"     << h.pageCount
           << "  Instruments:" << h.instrumentCount
           << "  Staves/sys:" << h.staffPerSystem
           << "  Measures:"  << h.measureCount;

    LOGD() << "---- Titles ----";
    if (!enc.titleBlock.title.isEmpty()) {
        LOGD() << "  Title:    " << enc.titleBlock.title.toStdString();
    }
    for (const QString& s : enc.titleBlock.subtitle) {
        if (!s.isEmpty()) { LOGD() << "  Subtitle: " << s.toStdString(); }
    }
    for (const QString& s : enc.titleBlock.author) {
        if (!s.isEmpty()) { LOGD() << "  Author:   " << s.toStdString(); }
    }
    for (const QString& s : enc.titleBlock.instruction) {
        if (!s.isEmpty()) { LOGD() << "  Instr:    " << s.toStdString(); }
    }
    for (const QString& s : enc.titleBlock.copyright) {
        if (!s.isEmpty()) { LOGD() << "  Copyrt:   " << s.toStdString(); }
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
            if (m.bpm) { lastBpm = m.bpm; }
        }
    }
    LOGD() << "--------------------------";
}

// Reduce spatium until the first several music systems each fit at least as many
// measures as the corresponding Encore LINE block specifies.  Runs before
// resolveAll() (no system breaks yet); the reduced spatium is then locked in
// so the layout engine honours the breaks placed by resolveAll().
// Checking only line 0 is insufficient: later lines may contain denser notation
// that needs more horizontal room than line 0.  We check the first 4 lines as a
// practical bound (diminishing returns beyond that).
static void fitSpatiumToLineBreaks(MasterScore* score, const EncFile& enc)
{
    if (enc.lines.empty()) {
        return;
    }

    // Build target list from the first few non-zero enc.lines entries.
    const int kCheckLines = static_cast<int>(std::min<size_t>(enc.lines.size(), 4));
    std::vector<int> targets;
    for (int i = 0; i < kCheckLines; ++i) {
        if (enc.lines[i].measureCount > 0) {
            targets.push_back(static_cast<int>(enc.lines[i].measureCount));
        }
    }
    if (targets.empty()) {
        return;
    }

    double spatium = score->style().spatium();

    for (int iter = 0; iter < 20; ++iter) {
        score->style().setSpatium(spatium);
        score->doLayout();

        // Collect real measure counts per music system, in document order.
        std::vector<int> sysCounts;
        for (const System* sys : score->systems()) {
            int mc = 0;
            for (const MeasureBase* mb : sys->measures()) {
                if (mb->isMeasure()) { ++mc; }
            }
            if (mc > 0) {
                sysCounts.push_back(mc);
            }
        }

        bool allFit = true;
        for (int j = 0; j < static_cast<int>(targets.size())
             && j < static_cast<int>(sysCounts.size()); ++j) {
            if (sysCounts[j] < targets[j]) {
                allFit = false;
                break;
            }
        }
        if (allFit) {
            break;
        }

        spatium *= 0.9;
        if (spatium < 0.01) {
            break;
        }
    }
    score->style().setSpatium(spatium);
}

static void buildScore(MasterScore* score, const EncFile& enc)
{
    clearImportState();

    score->style().set(Sid::chordsXmlFile, true);
    score->chordList()->read(u"chords.xml");

    BuildCtx ctx{ score, enc };
    buildParts(ctx);
    buildMeasures(ctx);
    buildInitialSignatures(ctx);
    buildNoteLoop(ctx);

    fitSpatiumToLineBreaks(score, enc);

    resolveAll(ctx);

    score->spell();
    addTitleFrame(score, enc.titleBlock);
    score->setUpTempoMap();
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

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

    EncFile enc;
    if (!enc.read(ds)) {
        return Err::FileBadFormat;
    }

    if (enc.instruments.empty() || enc.measures.empty()) {
        return Err::FileBadFormat;
    }

    logEncFileInfo(enc);
    buildScore(score, enc);

    muse::Ret integrity = score->sanityCheck();
    if (!integrity) {
        LOGW() << "Encore import: score corruption detected:\n" << integrity.text();
    }

    return Err::NoError;
}
} // namespace mu::iex::encore
