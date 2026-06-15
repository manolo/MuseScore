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
#include "notationencreader.h"

#include "importer/import.h"

#include "engraving/engravingerrors.h"
#include "translation.h"

#include <QFile>

using namespace mu::iex::encore;
using namespace mu::engraving;

muse::Ret NotationEncoreReader::read(MasterScore* score, const muse::io::path_t& path, const Options&)
{
    // ZBOT / ZBOP / ZBO6 are encrypted formats produced by very old versions
    // of Encore (prior to 4.x). Decryption is not supported; show a specific
    // message so the user knows what to do instead of a generic "Bad format".
    {
        QFile f(path.toQString());
        if (f.open(QIODevice::ReadOnly)) {
            const QByteArray magic = f.read(4);
            if (magic.startsWith("ZBO")) {
                return make_ret(Err::FileBadFormat,
                                muse::mtrc("iex_encore",
                                           "This file was created with a very old version of Encore (prior to 4.x) "
                                           "and uses an encrypted format that is not supported.\n"
                                           "Please open it in Encore 4.x or 5.x and re-save it as a regular .enc file."));
            }
        }
    }

    Err err = importEncore(score, path.toQString());
    return make_ret(err, path);
}
