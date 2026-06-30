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

// Debug-only summary of a parsed Encore file (logEncRootInfo). Never affects the imported
// score; the output goes to the debug log to help diagnose format/layout issues.

#include "debug-dump.h"

#include <map>
#include <string>

#include <QString>

#include "../parser/elem.h"
#include "page-layout.h"

#include "log.h"

namespace mu::iex::enc {
void logEncRootInfo(const EncRoot& enc)
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
    {
        // One line per system is useless noise on large scores (hundreds of near-identical
        // entries). Summarize as a histogram of measures-per-system instead; the per-system
        // start offsets are derivable from the counts and rarely needed.
        std::map<int, int> measPerSys;   // measures in a system -> how many systems
        for (const EncLine& ln : enc.lines) {
            ++measPerSys[static_cast<int>(ln.measureCount)];
        }
        std::string hist;
        for (auto it = measPerSys.rbegin(); it != measPerSys.rend(); ++it) {
            if (!hist.empty()) {
                hist += ", ";
            }
            hist += std::to_string(it->first) + "x" + std::to_string(it->second);
        }
        LOGD() << "  " << enc.lines.size() << " systems over " << h.pageCount
               << " pages  (measures/system: " << hist << ")";
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
    // Diagnostics: non-notation elements the importer intentionally drops. MIDI Control Change
    // events (sustain/volume/modulation) are playback-only and decoded here so the log says what
    // they are instead of one "unknown" line per event.
    {
        int ccSustain = 0, ccVolume = 0, ccMod = 0, ccOther = 0, unknown1 = 0;
        for (const EncMeasure& m : enc.measures) {
            for (const auto& ep : m.elements) {
                switch (static_cast<EncElemType>(ep->type)) {
                case EncElemType::MIDI_CC:
                    switch (static_cast<const EncMidiCc*>(ep.get())->controller) {
                    case 64: ++ccSustain; break;
                    case 7:  ++ccVolume;  break;
                    case 1:  ++ccMod;     break;
                    default: ++ccOther;   break;
                    }
                    break;
                case EncElemType::UNKNOWN1:
                    ++unknown1;
                    break;
                default:
                    break;
                }
            }
        }
        const int ccTotal = ccSustain + ccVolume + ccMod + ccOther;
        if (ccTotal || unknown1) {
            LOGD() << "---- Diagnostics ----";
            if (ccTotal) {
                std::string by;
                auto add = [&](const char* label, int n) {
                    if (n > 0) {
                        if (!by.empty()) {
                            by += " ";
                        }
                        by += std::string(label) + "=" + std::to_string(n);
                    }
                };
                add("sustain", ccSustain);
                add("volume", ccVolume);
                add("modulation", ccMod);
                add("other", ccOther);
                LOGD() << "  MIDI CC events (playback only, dropped): " << ccTotal
                       << "  (" << by << ")";
            }
            if (unknown1) {
                LOGD() << "  Unknown elements (type 0xA, dropped): " << unknown1;
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
            const double upi = winiUnitsPerInch(ps.rightEdge, ps.left, wIn);
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
} // namespace mu::iex::enc
