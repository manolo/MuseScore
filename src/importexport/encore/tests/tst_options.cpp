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

#include <gtest/gtest.h>

#include "engraving/compat/scoreaccess.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/layoutbreak.h"
#include "engraving/dom/spanner.h"
#include "engraving/dom/volta.h"
#include "engraving/dom/rest.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/instrument.h"
#include "engraving/dom/part.h"
#include "engraving/dom/staff.h"
#include "engraving/dom/stafftext.h"
#include "engraving/dom/stafftype.h"
#include "engraving/dom/tempotext.h"
#include "engraving/style/style.h"
#include "engraving/types/fraction.h"

#include "../internal/importer/import-options.h"

#include "testbase.h"

static const QString ENC_DIR(QString(iex_encore_tests_DATA_ROOT) + "/data/");

using namespace mu::engraving;
using namespace mu::iex::enc;

class Tst_Options : public ::testing::Test, public MTest
{
protected:
    void SetUp() override { setRootDir(ENC_DIR); }
};

// ===========================================================================
// importStaffSize
// All test files in data/ have scoreSize=3, which maps to MAG 1.00 (100%).
// ===========================================================================

TEST_F(Tst_Options, importStaffSize_true_applies_encore_scale)
{
    MasterScore* score = readEncoreScore("bazo.enc");
    ASSERT_NE(score, nullptr);
    // scoreSize=3 → kScaleBySize[2] = 1.00 (100%)
    const double mag = score->staff(0)->staffType(Fraction(0, 1))->userMag();
    EXPECT_DOUBLE_EQ(mag, 1.00)
        << "importStaffSize=true (default) must apply Encore scoreSize=3 → MAG 1.00";
    delete score;
}

TEST_F(Tst_Options, importStaffSize_false_keeps_unit_scale)
{
    EncImportOptions opts;
    opts.importStaffSize = false;
    MasterScore* score = readEncoreScoreWithOpts("bazo.enc", opts);
    ASSERT_NE(score, nullptr);
    const double mag = score->staff(0)->staffType(Fraction(0, 1))->userMag();
    EXPECT_DOUBLE_EQ(mag, 1.0)
        << "importStaffSize=false must leave staff MAG at the MuseScore default (1.0)";
    delete score;
}
