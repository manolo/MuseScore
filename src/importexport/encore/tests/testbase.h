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

#ifndef MU_IEX_ENCORE_TESTBASE_H
#define MU_IEX_ENCORE_TESTBASE_H

#include <QString>

namespace mu::engraving {
class MasterScore;
class Score;

class MTest
{
protected:
    QString root;

    MasterScore* readEncoreScore(const QString& name);
    bool saveScore(Score*, const QString& name) const;
    bool compareFiles(const QString& saveName, const QString& compareWith) const;
    bool saveCompareScore(Score*, const QString& saveName, const QString& compareWith) const;
    void setRootDir(const QString& root);

public:
    static bool compareFilesFromPaths(const QString& f1, const QString& f2);
};
} // namespace mu::engraving

#endif // MU_IEX_ENCORE_TESTBASE_H
