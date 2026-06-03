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
#ifndef MU_IMPORTEXPORT_ENC_IMPORT_RESOLVERS_H
#define MU_IMPORTEXPORT_ENC_IMPORT_RESOLVERS_H

#include "ctx.h"

namespace mu::iex::encore {
void resolveAll(BuildCtx& ctx);
void resolveSlurs(BuildCtx& ctx);
void resolveHairpins(BuildCtx& ctx);
void resolveOrnaments(BuildCtx& ctx);
void resolveFingeringAndBowing(BuildCtx& ctx);
} // namespace mu::iex::encore

#endif // MU_IMPORTEXPORT_ENC_IMPORT_RESOLVERS_H
