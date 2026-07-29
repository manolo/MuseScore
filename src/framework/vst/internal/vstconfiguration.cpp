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

#include "vstconfiguration.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "settings.h"
#include "mpe/internal/articulationstringutils.h"
#include "log.h"

using namespace muse;
using namespace muse::vst;

static const std::string module_name("vst");

static const io::path_t KEYSWITCH_RESOURCE_PATH(":/vst/resources/vst_keyswitches.json");

static const Settings::Key USER_VST_PATHS = Settings::Key(module_name, "application/paths/myVSTs");
static const Settings::Key USED_VST_VIEW = Settings::Key(module_name, "application/vst/view");

void VstConfiguration::init()
{
    settings()->setDefaultValue(USED_VST_VIEW, Val("newview"));
    settings()->setDefaultValue(USER_VST_PATHS, Val(""));
    settings()->valueChanged(USER_VST_PATHS).onReceive(nullptr, [this](const Val&) {
        m_userVstDirsChanged.send(userVstDirectories());
    });
}

io::paths_t VstConfiguration::userVstDirectories() const
{
    std::string pathsStr = settings()->value(USER_VST_PATHS).toString();
    return io::pathsFromString(pathsStr);
}

void VstConfiguration::setUserVstDirectories(const io::paths_t& paths)
{
    settings()->setSharedValue(USER_VST_PATHS, Val(io::pathsToString(paths)));
}

async::Channel<io::paths_t> VstConfiguration::userVstDirectoriesChanged() const
{
    return m_userVstDirsChanged;
}

static bool fieldMatches(const std::string& pattern, const std::string& value)
{
    if (pattern.empty()) {
        return true; // wildcard: this match field is not constrained
    }
    // Case-insensitive substring, so "Bandurria" matches a host-decorated "Bandurria (Pulso y Pua)".
    const QString v = QString::fromStdString(value).toLower();
    const QString p = QString::fromStdString(pattern).toLower();
    return v.contains(p);
}

std::optional<VstKeyswitchProfile> VstConfiguration::keyswitchProfileForPlugin(const std::string& pluginName,
                                                                               const std::string& resourceId,
                                                                               const std::string& vendor) const
{
    ensureKeyswitchProfilesLoaded();

    for (const KeyswitchProfileEntry& entry : m_keyswitchProfiles) {
        if (fieldMatches(entry.matchName, pluginName)
            && fieldMatches(entry.matchId, resourceId)
            && fieldMatches(entry.matchVendor, vendor)) {
            return entry.profile;
        }
    }

    return std::nullopt;
}

void VstConfiguration::ensureKeyswitchProfilesLoaded() const
{
    if (m_keyswitchProfilesLoaded) {
        return;
    }
    m_keyswitchProfilesLoaded = true;

    // User override first so its entries win over the bundled defaults (first match wins).
    const io::path_t userFile = globalConfiguration()->userDataPath() + "/vst_keyswitches.json";
    if (fileSystem()->exists(userFile)) {
        RetVal<ByteArray> rv = fileSystem()->readFile(userFile);
        if (rv.ret) {
            parseKeyswitchProfiles(rv.val.toQByteArrayNoCopy());
        }
    }

    RetVal<ByteArray> bundled = fileSystem()->readFile(KEYSWITCH_RESOURCE_PATH);
    if (bundled.ret) {
        parseKeyswitchProfiles(bundled.val.toQByteArrayNoCopy());
    }
}

void VstConfiguration::parseKeyswitchProfiles(const QByteArray& json) const
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        LOGE() << "Failed to parse vst_keyswitches.json: " << err.errorString();
        return;
    }

    const QJsonArray profiles = doc.object().value("profiles").toArray();
    for (const QJsonValue& pv : profiles) {
        const QJsonObject po = pv.toObject();
        const QJsonObject match = po.value("match").toObject();

        KeyswitchProfileEntry entry;
        entry.matchName = match.value("name").toString().toStdString();
        entry.matchId = match.value("id").toString().toStdString();
        entry.matchVendor = match.value("vendor").toString().toStdString();
        entry.profile.collapseTremolo = po.value("collapseTremolo").toBool(true);

        const QJsonObject ks = po.value("keyswitches").toObject();
        for (const QString& artName : ks.keys()) {
            const mpe::ArticulationType type = mpe::articulationTypeFromString(artName);
            if (type == mpe::ArticulationType::Undefined) {
                LOGW() << "Unknown articulation in vst_keyswitches.json: " << artName;
                continue;
            }
            entry.profile.keyswitches[type] = ks.value(artName).toInt();
        }

        m_keyswitchProfiles.push_back(std::move(entry));
    }
}

std::string VstConfiguration::usedVstView() const
{
    return settings()->value(USED_VST_VIEW).toString();
}

void VstConfiguration::setUsedVstView(const std::string& code)
{
    settings()->setSharedValue(USED_VST_VIEW, Val(code));
}
