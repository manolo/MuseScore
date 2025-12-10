/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2024 MuseScore Limited
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
#include <vector>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "engraving/infrastructure/mscreader.h"

#include "notationsolomutestate.h"

using namespace muse;
using namespace mu::engraving;
using namespace mu::notation;

Ret NotationSoloMuteState::read(const engraving::MscReader& reader, const muse::io::path_t& pathPrefix)
{
    const ByteArray json = reader.readAudioSettingsJsonFile(pathPrefix);

    if (json.empty()) {
        return make_ret(Ret::Code::UnknownError);
    }

    const QJsonObject rootObj = QJsonDocument::fromJson(json.toQByteArrayNoCopy()).object();

    const QJsonArray tracksArray = rootObj.value("tracks").toArray();
    for (const QJsonValueConstRef track : tracksArray) {
        const QJsonObject trackObject = track.toObject();

        const InstrumentTrackId id = {
            trackObject.value("partId").toString(),
            trackObject.value("instrumentId").toString()
        };

        QJsonObject soloMuteObj = trackObject.value("soloMuteState").toObject();
        SoloMuteState soloMuteState;
        soloMuteState.mute = soloMuteObj.value("mute").toBool();
        soloMuteState.solo = soloMuteObj.value("solo").toBool();

        // Read volume/balance if present (new fields for per-excerpt mixer state)
        if (soloMuteObj.contains("volumeDb")) {
            soloMuteState.volumeDb = static_cast<float>(soloMuteObj.value("volumeDb").toDouble());
            soloMuteState.balance = static_cast<float>(soloMuteObj.value("balance").toDouble(0.0));
            soloMuteState.hasCustomVolume = true;
        }

        m_trackSoloMuteStatesMap.emplace(id, std::move(soloMuteState));
    }

    return make_ret(Ret::Code::Ok);
}

Ret NotationSoloMuteState::write(io::IODevice* out)
{
    QJsonObject rootObj;
    QJsonArray tracksArray;

    for (const auto& pair : m_trackSoloMuteStatesMap) {
        QJsonObject currentTrack;
        currentTrack["instrumentId"] = pair.first.instrumentId.toQString();
        currentTrack["partId"] = pair.first.partId.toQString();

        QJsonObject soloMuteStateObject;
        soloMuteStateObject["mute"] = pair.second.mute;
        soloMuteStateObject["solo"] = pair.second.solo;

        // Write volume/balance if this excerpt has custom values
        if (pair.second.hasCustomVolume) {
            soloMuteStateObject["volumeDb"] = static_cast<double>(pair.second.volumeDb);
            soloMuteStateObject["balance"] = static_cast<double>(pair.second.balance);
        }

        currentTrack["soloMuteState"] = soloMuteStateObject;
        tracksArray.append(currentTrack);
    }

    rootObj["tracks"] = tracksArray;

    out->write(QJsonDocument(rootObj).toJson());

    return make_ret(Ret::Code::Ok);
}

bool NotationSoloMuteState::trackSoloMuteStateExists(const engraving::InstrumentTrackId& partId) const
{
    auto search = m_trackSoloMuteStatesMap.find(partId);
    return search != m_trackSoloMuteStatesMap.end();
}

const INotationSoloMuteState::SoloMuteState& NotationSoloMuteState::trackSoloMuteState(const InstrumentTrackId& partId) const
{
    auto search = m_trackSoloMuteStatesMap.find(partId);

    if (search == m_trackSoloMuteStatesMap.end()) {
        static const SoloMuteState dummySoloMuteState;
        return dummySoloMuteState;
    }

    return search->second;
}

void NotationSoloMuteState::setTrackSoloMuteState(const InstrumentTrackId& partId, const SoloMuteState& state)
{
    auto it = m_trackSoloMuteStatesMap.find(partId);
    if (it != m_trackSoloMuteStatesMap.end() && it->second == state) {
        return;
    }

    // Check if only volume/balance changed (not mute/solo)
    // We only want to fire the signal when mute/solo actually changes
    // to avoid feedback loops when volume is adjusted
    // Exception: also fire when hasCustomVolume changes from false to true (first time)
    bool shouldSignal = true;
    if (it != m_trackSoloMuteStatesMap.end()) {
        const SoloMuteState& existing = it->second;
        bool hasCustomVolumeChanged = !existing.hasCustomVolume && state.hasCustomVolume;
        if (existing.mute == state.mute && existing.solo == state.solo && !hasCustomVolumeChanged) {
            // Only volume/balance changed (not first time), don't fire signal
            shouldSignal = false;
        }
    }

    m_trackSoloMuteStatesMap.insert_or_assign(partId, state);

    if (shouldSignal) {
        m_trackSoloMuteStateChanged.send(partId, state);
    }
}

void NotationSoloMuteState::removeTrackSoloMuteState(const engraving::InstrumentTrackId& trackId)
{
    auto soloMuteSearch = m_trackSoloMuteStatesMap.find(trackId);
    if (soloMuteSearch != m_trackSoloMuteStatesMap.end()) {
        m_trackSoloMuteStatesMap.erase(soloMuteSearch);
    }
}

void NotationSoloMuteState::clearAllStates()
{
    // Collect all track IDs before clearing
    std::vector<engraving::InstrumentTrackId> trackIds;
    for (const auto& pair : m_trackSoloMuteStatesMap) {
        trackIds.push_back(pair.first);
    }

    m_trackSoloMuteStatesMap.clear();

    // Notify listeners about each cleared track (with default/empty state)
    SoloMuteState emptyState;
    for (const auto& trackId : trackIds) {
        m_trackSoloMuteStateChanged.send(trackId, emptyState);
    }
}

muse::async::Channel<InstrumentTrackId, INotationSoloMuteState::SoloMuteState> NotationSoloMuteState::trackSoloMuteStateChanged() const
{
    return m_trackSoloMuteStateChanged;
}
