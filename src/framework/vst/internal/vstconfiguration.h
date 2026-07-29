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

#include <vector>

#include <QByteArray>

#include "ivstconfiguration.h"

#include "global/modularity/ioc.h"
#include "global/iglobalconfiguration.h"
#include "global/io/ifilesystem.h"

namespace muse::vst {
class VstConfiguration : public IVstConfiguration, public Contextable
{
    GlobalInject<IGlobalConfiguration> globalConfiguration;
    GlobalInject<io::IFileSystem> fileSystem;

public:
    VstConfiguration(const modularity::ContextPtr& iocCtx)
        : Contextable(iocCtx) {}

    void init();

    io::paths_t userVstDirectories() const override;
    void setUserVstDirectories(const io::paths_t& paths) override;
    async::Channel<io::paths_t> userVstDirectoriesChanged() const override;

    std::optional<VstKeyswitchProfile> keyswitchProfileForPlugin(const std::string& pluginName,
                                                                 const std::string& resourceId,
                                                                 const std::string& vendor) const override;

    // dev
    std::string usedVstView() const override;
    void setUsedVstView(const std::string& code) override;

private:
    struct KeyswitchProfileEntry {
        std::string matchName;
        std::string matchId;
        std::string matchVendor;
        VstKeyswitchProfile profile;
    };

    void ensureKeyswitchProfilesLoaded() const;
    void parseKeyswitchProfiles(const QByteArray& json) const;

    async::Channel<io::paths_t> m_userVstDirsChanged;

    mutable bool m_keyswitchProfilesLoaded = false;
    mutable std::vector<KeyswitchProfileEntry> m_keyswitchProfiles;
};
}
