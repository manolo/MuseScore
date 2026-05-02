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

#ifndef MU_IMPORTEXPORT_ENCORERHYTHM_H
#define MU_IMPORTEXPORT_ENCORERHYTHM_H

#include <QtGlobal>

#include "engraving/dom/durationtype.h"

namespace mu::iex::encore {

int faceValue2ticks(quint8 fv);
mu::engraving::DurationType faceValue2DurationType(quint8 fv);
mu::engraving::DurationType realDuration2DurationType(qint16 realDur, quint8 fv);
int calcDots(qint16 realDur, quint8 fv);
int calcDotsSnap(qint16 dur, quint8 fv);
int detectImpliedTuplet(qint16 realDur, quint8 fv, int& normalNotes);
mu::engraving::Fraction dottedAdvance(mu::engraving::DurationType durationType, int dots);

} // namespace mu::iex::encore

#endif // MU_IMPORTEXPORT_ENCORERHYTHM_H
