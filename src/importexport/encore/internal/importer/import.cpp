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

// Encore (.enc) importer entry point. Parser and score builder not yet implemented.

#include "import.h"

using namespace mu::iex::enc;
using namespace mu::engraving;

Err mu::iex::enc::importEncore(MasterScore*, const QString&, const EncImportOptions&)
{
    // Stub: replaced when the parser and builder layers are added.
    return Err::UnknownError;
}
