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
#include <QPageSize>
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
    n->setPitch(semitone);
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

static bool precPageSizeInches(const EncPrintSetup& pr, double& wIn, double& hIn);

static void logEncRootInfo(const EncRoot& enc)
{
    const EncHeader& h = enc.header;
    const char* fmtName = enc.fmt ? enc.fmt->formatName() : "unknown";

    const char* encVer = (h.chuVersio >= 1000) ? "Encore 5.x"
                         : (h.chuVersio >= 700) ? "Encore 4.x"
                         : (h.chuVersio >= 580) ? "Encore 4.0"
                         : "Encore 3.x or older";

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
    } else if (enc.header.magic == "SCO5") {
        LOGD() << "  WINI: absent — margins set to 0.25 inches";
    } else {
        LOGD() << "  WINI: absent — margins from MuseScore defaults";
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
        LOGD() << "  PREC: absent — page size from WINI/defaults";
    }
    LOGD() << "--------------------------";
}

// Map Encore score-size (1–4) to MuseScore Staff Properties → Scale (Pid::MAG).
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

// Detect standard paper size from typographic-point WINI coordinates.
// rightEdge and bottomEdge are the right/bottom edges of the printable area
// in pts (1/72 inch); the full page is at least that large.  Returns the
// smallest standard size (by area) that contains the printable area.
// Returns false when no standard size fits (custom page or rightEdge exceeds
// all known widths, which signals screen-pixel format instead).
//
// 1 pt tolerance: ISO metric page heights (e.g. A4 297mm = 841.89pt) are
// stored as integers in the WINI block, so bottomEdge may be 1pt larger than
// the QPageSize fractional value.  Without tolerance, A4 files with
// bottomEdge=842 fall through to a non-A4 page (wrong size and margins).
static bool detectPtsPageSize(qint32 rightEdge, qint32 bottomEdge,
                              double& outWidthIn, double& outHeightIn)
{
    static constexpr double kTol = 1.0;   // pts tolerance for metric rounding
    double bestArea = 1e18;
    bool found = false;
    for (int id = 0; id <= static_cast<int>(QPageSize::LastPageSize); ++id) {
        if (id == static_cast<int>(QPageSize::Custom)) {
            continue;
        }
        const QSizeF sz = QPageSize::size(static_cast<QPageSize::PageSizeId>(id),
                                          QPageSize::Inch);
        const double wPts = sz.width() * 72.0;
        const double hPts = sz.height() * 72.0;
        if (wPts + kTol < static_cast<double>(rightEdge)
            || hPts + kTol < static_cast<double>(bottomEdge)) {
            continue;
        }
        const double area = wPts * hPts;
        if (area < bestArea) {
            bestArea    = area;
            outWidthIn  = sz.width();
            outHeightIn = sz.height();
            found       = true;
        }
    }
    return found;
}

// Try to identify the paper size from WINI screen-pixel coordinates.
// pageWUnits = rightEdge + left, pageHUnits = bottomEdge + top.
//
// Two-pass approach:
//   Pass 1 — ISO A-series only (A0..A10).  All AN sizes share the 1:√2 aspect
//   ratio, so for A-series WINI data the only ambiguity is WHICH AN size — and
//   that is resolved by smallest |dpiW−dpiH|.  Checking A-series first prevents
//   non-A formats (e.g. 12"×18") from incorrectly winning when their
//   accidentally smaller delta would beat the correct AN with a unified scan.
//   Pass 2 — all remaining standard sizes, pick smallest delta.
//
// Returns false when no standard size matches within tolerance (custom page).
static bool detectWiniPageSize(int pageWUnits, int pageHUnits,
                               double& outWidthIn, double& outHeightIn)
{
    static constexpr double kDpiMin   = 60.0;   // minimum plausible screen DPI
    static constexpr double kDpiMax   = 135.0;  // maximum plausible screen DPI
    static constexpr double kMaxDelta = 6.0;    // max |dpiW - dpiH|

    // ISO A-series IDs in Qt's QPageSize enum (Qt 6).
    static const QPageSize::PageSizeId kASeriesIds[] = {
        QPageSize::A0, QPageSize::A1, QPageSize::A2, QPageSize::A3,
        QPageSize::A4, QPageSize::A5, QPageSize::A6, QPageSize::A7,
        QPageSize::A8, QPageSize::A9, QPageSize::A10,
    };

    auto tryCandidate = [&](QPageSize::PageSizeId id,
                            double& bestDelta,
                            double& bestW, double& bestH) -> bool {
        const QSizeF sz = QPageSize::size(id, QPageSize::Inch);
        const double w  = sz.width();
        const double h  = sz.height();
        if (w <= 0.0 || h <= 0.0) {
            return false;
        }
        const double dpiW = pageWUnits / w;
        const double dpiH = pageHUnits / h;
        if (dpiW < kDpiMin || dpiW > kDpiMax || dpiH < kDpiMin || dpiH > kDpiMax) {
            return false;
        }
        const double delta = std::abs(dpiW - dpiH);
        if (delta < kMaxDelta && delta < bestDelta) {
            bestDelta = delta;
            bestW = w;
            bestH = h;
            return true;
        }
        return false;
    };

    // Build a set of A-series IDs for fast exclusion in pass 2.
    std::set<int> aSeriesSet;
    for (const auto id : kASeriesIds) {
        aSeriesSet.insert(static_cast<int>(id));
    }

    // Pass 1: ISO A-series.
    double bestDelta = kMaxDelta;
    bool found = false;
    for (const auto id : kASeriesIds) {
        if (tryCandidate(id, bestDelta, outWidthIn, outHeightIn)) {
            found = true;
        }
    }
    if (found) {
        return true;
    }

    // Pass 2: all other standard sizes (Letter, Legal, B-series, etc.).
    for (int id = 0; id <= static_cast<int>(QPageSize::LastPageSize); ++id) {
        if (id == static_cast<int>(QPageSize::Custom)) {
            continue;
        }
        if (aSeriesSet.count(id)) {
            continue;   // already tried in pass 1
        }
        if (tryCandidate(static_cast<QPageSize::PageSizeId>(id), bestDelta, outWidthIn, outHeightIn)) {
            found = true;
        }
    }
    return found;
}

// Map a Windows DEVMODE dmPaperSize (DMPAPER_*) to a Qt page size. Returns Custom for
// values without a standard mapping; the caller then falls back to dmPaperLength/Width or
// to the WINI geometry heuristic.
static QPageSize::PageSizeId dmPaperToQt(int dmPaper)
{
    switch (dmPaper) {
    case 1:  return QPageSize::Letter;
    case 5:  return QPageSize::Legal;
    case 7:  return QPageSize::Executive;
    case 8:  return QPageSize::A3;
    case 9:  return QPageSize::A4;
    case 11: return QPageSize::A5;
    case 12: return QPageSize::B4;     // DMPAPER_B4 (JIS)
    case 13: return QPageSize::B5;     // DMPAPER_B5 (JIS)
    default: return QPageSize::Custom;
    }
}

// Resolve the page size (inches) from the PREC (DEVMODE) block: dmPaperSize enum, falling back
// to dmPaperLength/Width (tenths of a millimetre) for custom sizes, with the landscape swap
// applied. Returns false when PREC has no usable size (caller falls back to WINI geometry).
static bool precPageSizeInches(const EncPrintSetup& pr, double& wIn, double& hIn)
{
    if (!pr.hasData) {
        return false;
    }
    const QPageSize::PageSizeId id = dmPaperToQt(pr.paperSize);
    if (id != QPageSize::Custom) {
        const QSizeF sz = QPageSize::size(id, QPageSize::Inch);
        wIn = sz.width();
        hIn = sz.height();
    } else if (pr.paperLength > 0 && pr.paperWidth > 0) {
        wIn = pr.paperWidth / 254.0;
        hIn = pr.paperLength / 254.0;
    } else {
        return false;
    }
    if (pr.orientation == 2) {   // DMORIENT_LANDSCAPE
        std::swap(wIn, hIn);
    }
    return true;
}

// Apply page size, orientation and notation scale from the PREC (DEVMODE) block. Returns
// true when the page size was set (so the WINI margin pass must not override it). PREC is
// present in almost every Encore file across all formats, while WINI (margins) exists only
// in v0xC4 — so this is the primary source of the page size for v0xA6/v0xC2 and for the
// many v0xC4 files without a WINI block.
static bool applyPagePrintSetup(MasterScore* score, const EncPrintSetup& pr)
{
    double wIn = 0.0, hIn = 0.0;
    if (!precPageSizeInches(pr, wIn, hIn)) {
        return false;   // unknown paper: let the WINI geometry heuristic decide
    }
    score->style().set(Sid::pageWidth,  wIn);
    score->style().set(Sid::pageHeight, hIn);
    // NOT IMPLEMENTED: dmScale (the score "Zoom" / notation-size percent the user sets in Encore)
    // is parsed and logged but not applied. MuseScore has no global percentage scale; the closest
    // equivalent is the page "Staff space" (spatium), expressed in inches/mm, not a percent. Applying
    // dmScale would mean converting the percent into a spatium reduction factor and reconciling it
    // with the per-staff size from applyStaffScale (Pid::MAG), which would otherwise compound. That
    // mapping needs investigation, so for now the value is only surfaced in the debug log.
    LOGD() << "  PREC: orientation=" << pr.orientation << " paperSize=" << pr.paperSize
           << " paper=" << pr.paperWidth << "x" << pr.paperLength << "(0.1mm)"
           << " scale(zoom)=" << pr.scale << "%"
           << " -> " << QString::number(wIn, 'f', 2).toStdString()
           << "x" << QString::number(hIn, 'f', 2).toStdString() << "in";
    return true;
}

static void applyPageMargins(MasterScore* score, const EncPageSetup& ps, bool pageSizeLocked)
{
    if (!ps.hasData) {
        return;
    }
    // WINI fields are nominally in typographic points (1/72 inch), but some
    // Encore versions store them in screen pixels at the monitor's DPI (~84-85
    // PPI on older hardware).  Symptom: rightEdge or bottomEdge exceeds the
    // page dimensions in pts (e.g. rightEdge=672 > A4_width_pts=595).
    //
    // For pts format: detectPtsPageSize picks the smallest standard page that
    // contains the printable area, which is locale-independent.
    // For screen-pixel format: detectWiniPageSize matches via DPI ratio.
    // Cap each margin to a fraction of the page so a misread WINI cannot produce an absurd
    // margin, while still allowing legitimately large margins (2"+ are common on A3/landscape).
    static constexpr double kMaxMarginFrac = 0.45;

    double pageHIn = score->style().styleD(Sid::pageHeight);
    double pageWIn = score->style().styleD(Sid::pageWidth);

    // 1 pt tolerance mirrors detectPtsPageSize: metric page heights convert to
    // fractional pts (A4 297mm = 841.89pt → stored as 842) so the integer
    // WINI value can exceed floor(pageH*72) by 1 without being screen-pixels.
    static constexpr double kPixelTol = 1.0;
    const bool screenPixelFmt = (ps.rightEdge > static_cast<qint32>(pageWIn * 72.0 + kPixelTol))
                                || (ps.bottomEdge > static_cast<qint32>(pageHIn * 72.0 + kPixelTol));
    double scaleUpi = 72.0;
    if (pageSizeLocked) {
        // The page size is known (from PREC), so derive the WINI unit directly from the printable
        // extent rather than guessing: (rightEdge + left) / pageWidth ≈ 72 means the WINI is in
        // typographic points, a clearly larger value (~84) means screen pixels at the monitor DPI.
        // Snap the near-72 case to exactly 72 (points). The pixel estimate is exact only when the
        // left/right margins are symmetric; with asymmetric margins it is ~2% low.
        const double estUpi = static_cast<double>(ps.rightEdge + ps.left) / pageWIn;
        scaleUpi = (estUpi <= 76.0) ? 72.0 : estUpi;
    } else if (screenPixelFmt) {
        const int pageWUnits = ps.rightEdge + ps.left;
        const int pageHUnits = ps.bottomEdge + ps.top;
        double detectedW = 0.0, detectedH = 0.0;
        if (detectWiniPageSize(pageWUnits, pageHUnits, detectedW, detectedH)) {
            pageWIn  = detectedW;
            pageHIn  = detectedH;
            score->style().set(Sid::pageWidth,  pageWIn);
            score->style().set(Sid::pageHeight, pageHIn);
        }
        scaleUpi = static_cast<double>(pageWUnits) / pageWIn;
    } else {
        double detectedW = 0.0, detectedH = 0.0;
        if (detectPtsPageSize(ps.rightEdge, ps.bottomEdge, detectedW, detectedH)) {
            pageWIn  = detectedW;
            pageHIn  = detectedH;
            score->style().set(Sid::pageWidth,  pageWIn);
            score->style().set(Sid::pageHeight, pageHIn);
        }
        // scaleUpi stays 72.0 (pts = 1/72 inch by definition)
    }

    double topIn  = ps.top / scaleUpi;
    double leftIn = ps.left / scaleUpi;
    double printW = (ps.rightEdge - ps.left) / scaleUpi;
    double printH = (ps.bottomEdge - ps.top) / scaleUpi;

    LOGD() << "  enc margins (in): T=" << QString::number(topIn,  'f', 3).toStdString()
           << "  L=" << QString::number(leftIn, 'f', 3).toStdString()
           << "  R=" << QString::number(pageWIn - leftIn - printW, 'f', 3).toStdString()
           << "  B=" << QString::number(pageHIn - topIn - printH, 'f', 3).toStdString()
           << "  paper=" << QString::number(pageWIn * 25.4, 'f', 1).toStdString()
           << "x" << QString::number(pageHIn * 25.4, 'f', 1).toStdString() << "mm"
           << (screenPixelFmt ? "  [pixels]" : "  [pts]");

    topIn  = std::clamp(topIn,  0.0, pageHIn * kMaxMarginFrac);
    leftIn = std::clamp(leftIn, 0.0, pageWIn * kMaxMarginFrac);

    const double maxPrintW = pageWIn - leftIn;
    if (printW > maxPrintW) {
        printW = maxPrintW;
    }

    double bottomIn = std::max(0.0, pageHIn - topIn - printH);
    bottomIn = std::min(bottomIn, pageHIn * kMaxMarginFrac);

    LOGD() << "  applied (in):     T=" << QString::number(topIn,   'f', 3).toStdString()
           << "  L=" << QString::number(leftIn,   'f', 3).toStdString()
           << "  R=" << QString::number(pageWIn - leftIn - printW, 'f', 3).toStdString()
           << "  B=" << QString::number(bottomIn, 'f', 3).toStdString()
           << "  paper=" << QString::number(pageWIn * 25.4, 'f', 1).toStdString()
           << "x" << QString::number(pageHIn * 25.4, 'f', 1).toStdString() << "mm";

    score->style().set(Sid::pageOddTopMargin,     topIn);
    score->style().set(Sid::pageEvenTopMargin,    topIn);
    score->style().set(Sid::pageOddLeftMargin,    leftIn);
    score->style().set(Sid::pageEvenLeftMargin,   leftIn);
    score->style().set(Sid::pagePrintableWidth,   printW);
    score->style().set(Sid::pageOddBottomMargin,  bottomIn);
    score->style().set(Sid::pageEvenBottomMargin, bottomIn);
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

    LOGD() << "  importPageLayout=" << (ctx.opts.importPageLayout ? "true" : "false");
    if (ctx.opts.importPageLayout) {
        const bool sizeFromPrec = applyPagePrintSetup(score, enc.printSetup);
        applyPageMargins(score, enc.pageSetup, sizeFromPrec);
        // SCO5 (macOS Encore 5) does not store document margins in any importable block:
        // WINI holds only window state, the PREC plist holds only printer rects, and some
        // files have no PREC at all. Apply a clean, symmetric 0.25" margin: forcing 0 looks
        // cramped (edge to edge), and MuseScore's default margins are tuned for A4 so they
        // come out asymmetric on Letter. A small uniform margin is the better default.
        if (enc.header.magic == "SCO5") {
            constexpr double kMacMarginIn = 0.25;
            const double pageWIn = score->style().styleD(Sid::pageWidth);
            score->style().set(Sid::pageOddTopMargin,     kMacMarginIn);
            score->style().set(Sid::pageEvenTopMargin,    kMacMarginIn);
            score->style().set(Sid::pageOddLeftMargin,    kMacMarginIn);
            score->style().set(Sid::pageEvenLeftMargin,   kMacMarginIn);
            score->style().set(Sid::pageOddBottomMargin,  kMacMarginIn);
            score->style().set(Sid::pageEvenBottomMargin, kMacMarginIn);
            score->style().set(Sid::pagePrintableWidth,   pageWIn - 2.0 * kMacMarginIn);
        }
    }
    if (ctx.opts.importStaffSize) {
        applyStaffScale(score, enc);
    }

    resolveAll(ctx);

    score->spell();
    respellTransposingStaves(score);
    addTitleFrame(score, enc.titleBlock);
    score->setUpTempoMap();
    score->doLayout();
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
    buildScore(score, enc, opts);

    muse::Ret integrity = score->sanityCheck();
    if (!integrity) {
        LOGW() << "Encore import: score corruption detected:\n" << integrity.text();
    }

    return Err::NoError;
}
} // namespace mu::iex::enc
