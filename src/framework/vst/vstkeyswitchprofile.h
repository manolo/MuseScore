/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
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

#include <map>

#include "mpe/mpetypes.h"

namespace muse::vst {
// Per-plugin keyswitch profile: how a VST instrument that selects articulations by keyswitch
// wants each articulation delivered. Loaded from vst_keyswitches.json (see IVstConfiguration).
struct VstKeyswitchProfile {
    // Articulation -> keyswitch note (reserved low range, typically 0..11). A note without a
    // mapped articulation falls back to the Standard entry, or 0 if it is absent.
    std::map<mpe::ArticulationType, int> keyswitches;

    // When true, a notated tremolo (rendered upstream as repeated sub-notes) is collapsed into
    // a single sustained note plus the tremolo keyswitch, so the instrument's tremolo sample
    // plays once. Set false for instruments that expect the repeated-note tremolo simulation.
    bool collapseTremolo = true;
};
}
