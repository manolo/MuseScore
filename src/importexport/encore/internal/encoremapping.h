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

#ifndef MU_IMPORTEXPORT_ENCOREMAPPING_H
#define MU_IMPORTEXPORT_ENCOREMAPPING_H

#include <QString>

#include "engraving/dom/clef.h"

#include "encoreelements.h"

namespace mu::engraving {
class MasterScore;
class Measure;
class Score;
class InstrumentTemplate;
}

namespace mu::iex::encore {

mu::engraving::ClefType encClef2MuseScore(EncClefType ct);

int encKeyToFifths(quint8 key);

void addTitleFrame(mu::engraving::MasterScore* score, const EncTitle& titleBlock);
void addInitialKeySig(mu::engraving::MasterScore* score, int staffIdx, quint8 encKey);
void addInitialTimeSig(mu::engraving::MasterScore* score, int nstaves, const EncMeasure& firstMeas);
void addInitialClef(mu::engraving::MasterScore* score, int staffIdx, EncClefType ct);
void addRepeatMark(mu::engraving::Score* score, mu::engraving::Measure* measure, EncRepeatType rt);

QString normalizeEncoreInstrName(const QString& name);
const mu::engraving::InstrumentTemplate* findEncoreInstrumentTemplate(const QString& encName);

} // namespace mu::iex::encore

#endif // MU_IMPORTEXPORT_ENCOREMAPPING_H
