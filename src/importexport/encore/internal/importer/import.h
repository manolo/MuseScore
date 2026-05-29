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
// The binary format was reverse-engineered by Leon Vinken (Enc2MusicXML project,
// https://github.com/lvinken/Enc2MusicXML, GPL v3+) building on enc2ly by Felipe Castro.
// This importer is based on that work.

#ifndef MU_IMPORTEXPORT_ENC_IMPORT_IMPORT_H
#define MU_IMPORTEXPORT_ENC_IMPORT_IMPORT_H

#include "engraving/engravingerrors.h"

namespace mu::engraving {
class MasterScore;
}

namespace mu::iex::encore {
mu::engraving::Err importEncore(mu::engraving::MasterScore* score, const QString& path);
} // namespace mu::iex::encore

#endif // MU_IMPORTEXPORT_ENC_IMPORT_IMPORT_H
