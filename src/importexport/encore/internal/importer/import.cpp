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
#include "page-layout.h"

// Encore (.enc) file importer for MuseScore.
// Binary format reverse-engineered by Leon Vinken (Enc2MusicXML, GPL v3+) building on enc2ly by Felipe Castro.

#include "import.h"

#include "../parser/elem.h"
#include "mappers.h"
#include "../parser/ticks.h"
#include "emitters-tuplets.h"

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

#include "engraving/editing/editenharmonicspelling.h"

#include "log.h"

using namespace mu::engraving;

namespace mu::iex::enc {
// faceValue low nibble: 1=whole, 2=half ... 8=256th; 0 and 9..15 are invalid.
// High nibble carries unrelated flags.
bool isValidFaceValue(quint8 faceValue)
{
    const quint8 fv = faceValue & 0x0F;
    return fv > 0 && fv <= 8;
}

void applyConcertPitch(Note* n, int semitone)
{
    // A transposed or garbage Encore semitone can land outside MIDI's [0,127]. Note::setPitch
    // only asserts the range (no clamp), and downstream drumset lookups index a 128-entry table
    // by pitch, so an out-of-range value is undefined behaviour. Clamp once, here, at the single
    // choke point both the main and grace note paths go through.
    n->setPitch(std::clamp(semitone, 0, 127));
    n->setTpcFromPitch();
}

// score->spell() re-spells the whole score with a context-based heuristic that mishandles
// transposing instruments: it can spell concert pitches with double-flats (e.g. a concert E in
// A major rendered as a written double-flat) instead of the plain note the key wants. After
// spell(), re-derive the TPC of notes on TRANSPOSING staves from the sounding pitch + concert key
// + staff transposition (which honours the key); the pitch is unchanged. Non-transposing staves
// keep spell()'s result, which is correct for them.
static void respellTransposingStaves(MasterScore* score)
{
    for (MeasureBase* mb = score->first(); mb; mb = mb->next()) {
        if (!mb->isMeasure()) {
            continue;
        }
        Measure* m = toMeasure(mb);
        for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            for (track_idx_t t = 0; t < score->ntracks(); ++t) {
                EngravingItem* e = s->element(t);
                if (!e || !e->isChord()) {
                    continue;
                }
                Chord* chord = toChord(e);
                if (!chord->staff() || chord->staff()->transpose(chord->tick()).isZero()) {
                    continue;   // non-transposing staff: keep spell()'s spelling
                }
                for (Chord* gc : chord->graceNotes()) {
                    for (Note* n : gc->notes()) {
                        n->setTpcFromPitch();
                    }
                }
                for (Note* n : chord->notes()) {
                    n->setTpcFromPitch();
                }
            }
        }
    }
}

// Derive display size (1-4) for a given instrument index.
// LINE staff entry byte +13 (0-indexed 0-3) holds per-instrument size in both 4.x and 5.x.
// header.scoreSize (byte 0x52) is a global fallback for files without LINE data.
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
        // Derive all four margins (inches) for the summary: top/left are stored directly, while
        // right/bottom come from the printable edges and the page size. The WINI unit (points vs
        // screen pixels) is resolved from the PREC page size, same as applyPageMargins.
        std::string marginStr;
        double wIn = 0.0, hIn = 0.0;
        if (precPageSizeInches(enc.printSetup, wIn, hIn) && wIn > 0.0 && hIn > 0.0) {
            const double est = static_cast<double>(ps.rightEdge + ps.left) / wIn;
            const double upi = (est <= 76.0) ? 72.0 : est;
            marginStr = ("  (in: T=" + QString::number(ps.top / upi, 'f', 3)
                         + " L=" + QString::number(ps.left / upi, 'f', 3)
                         + " R=" + QString::number(wIn - ps.rightEdge / upi, 'f', 3)
                         + " B=" + QString::number(hIn - ps.bottomEdge / upi, 'f', 3) + ")").toStdString();
        }
        LOGD() << "  WINI: top=" << ps.top << "  left=" << ps.left
               << "  bottomEdge=" << ps.bottomEdge << "  rightEdge=" << ps.rightEdge << marginStr;
    } else if (enc.fmt && enc.fmt->usesUniformPageMargins()) {
        LOGD() << "  WINI: absent, margins set to 0.25 inches";
    } else {
        LOGD() << "  WINI: absent, margins from MuseScore defaults";
    }
    const EncPrintSetup& pr = enc.printSetup;
    if (pr.hasData) {
        LOGD() << "  PREC: orientation=" << pr.orientation
               << " (" << (pr.orientation == 2 ? "landscape" : "portrait") << ")"
               << "  paperSize=" << pr.paperSize
               << "  paper=" << pr.paperWidth << "x" << pr.paperLength << " (0.1mm)"
               << "  scale/zoom=" << pr.scale << "%"
               << "  [scale not applied: needs spatium mapping]";
    } else {
        LOGD() << "  PREC: absent, page size from WINI/defaults";
    }
    LOGD() << "--------------------------";
}

// Map Encore score-size (1 to 4) to MuseScore Staff Properties → Scale (Pid::MAG).
// 1=60%, 2=75%, 3=100%, 4=130%.  Global spatium is not changed.
static void applyStaffScale(MasterScore* score, const EncRoot& enc)
{
    static const double kScaleBySize[4] = { 0.60, 0.75, 1.00, 1.30 };
    staff_idx_t msStaffIdx = 0;
    for (size_t instrIdx = 0; instrIdx < enc.instruments.size(); ++instrIdx) {
        const int sz = staffDisplaySize(enc, static_cast<int>(instrIdx));
        const double scale = kScaleBySize[sz - 1];
        const int ns = enc.instruments[instrIdx].nstaves > 0 ? enc.instruments[instrIdx].nstaves : 1;
        for (int s = 0; s < ns && msStaffIdx < score->staves().size(); ++s, ++msStaffIdx) {
            score->staves()[msStaffIdx]->setProperty(Pid::MAG, PropertyValue(scale));
        }
    }
}

static void buildScore(MasterScore* score, const EncRoot& enc, const EncImportOptions& opts)
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

    BuildCtx ctx{ score, enc, opts };
    buildParts(ctx);
    buildMeasures(ctx);
    buildInitialSignatures(ctx);
    emitMeasures(ctx);

    applyPageSetup(ctx);
    if (ctx.opts.importStaffSize) {
        applyStaffScale(score, enc);
    }

    resolveAll(ctx);

    EditEnharmonicSpelling::spell(score);
    respellTransposingStaves(score);
    addTitleFrame(score, enc.titleBlock);
    // Assign MIDI ports/channels to every part. The file read path does this on load,
    // but a direct import builds the score in memory without it, leaving each channel
    // at -1; that makes Part::midiPort() index m_midiMapping[-1] and crash on a
    // straight-to-MusicXML export.
    score->rebuildMidiMapping();
    score->setUpTempoMap();
    score->doLayout();
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

    muse::Ret integrity = score->sanityCheck();
    if (!integrity) {
        LOGW() << "Encore import: score corruption detected:\n" << integrity.text();
    }

    return Err::NoError;
}
} // namespace mu::iex::enc
