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
#pragma once

// Page and printer geometry for the Encore importer: paper size/orientation/scale from the
// PREC block, margins from the WINI block, the SCO5 uniform-margin default, and the
// system-lock / page-break line layout derived from LINE blocks.

namespace mu::iex::enc {
struct BuildCtx;
struct EncPrintSetup;

// Resolve the page size (inches) from the PREC (DEVMODE) block: dmPaperSize enum, falling back
// to dmPaperLength/Width for custom sizes, with the landscape swap applied. Returns false when
// PREC has no usable size. Exposed for the import debug summary; applyPageSetup uses it too.
bool precPageSizeInches(const EncPrintSetup& pr, double& wIn, double& hIn);

// Apply Encore's page geometry to the score in buildScore's layout phase: page
// size/orientation/scale (PREC), margins (WINI), the SCO5 uniform-margin default, and the
// system-lock / page-break line layout. Each part is gated by its matching import option.
void applyPageSetup(BuildCtx& ctx);
} // namespace mu::iex::enc
