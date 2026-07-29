/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
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

#include <optional>
#include <string>

#include "modularity/imoduleinterface.h"

#include "io/path.h"
#include "async/channel.h"

#include "vstkeyswitchprofile.h"

namespace muse::vst {
class IVstConfiguration : MODULE_GLOBAL_INTERFACE
{
    INTERFACE_ID(IVstConfiguration)

public:
    virtual ~IVstConfiguration() = default;

    virtual io::paths_t userVstDirectories() const = 0;
    virtual void setUserVstDirectories(const io::paths_t& paths) = 0;
    virtual async::Channel<io::paths_t> userVstDirectoriesChanged() const = 0;

    // Keyswitch profile for the given plugin, from vst_keyswitches.json (bundled default plus a
    // user override in the user data dir). Returns nullopt when the plugin is not listed, which
    // is the signal to send plain notes (no articulation keyswitches) to that instrument.
    virtual std::optional<VstKeyswitchProfile> keyswitchProfileForPlugin(const std::string& pluginName,
                                                                         const std::string& resourceId,
                                                                         const std::string& vendor) const = 0;

    // dev
    virtual std::string usedVstView() const = 0;
    virtual void setUsedVstView(const std::string& code) = 0;
};
}
