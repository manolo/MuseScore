/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited
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

#include "mixer.h"

#include "async/notifylist.h"
#include "engraving/dom/part.h"
#include "notation/imasternotation.h"
#include "notation/inotation.h"
#include "notation/inotationparts.h"

#include "log.h"

using namespace mu::engraving::apiv1;
using namespace muse;
using namespace muse::audio;

QList<AudioResource*> MixerChannel::s_availableSounds;
bool MixerChannel::s_availableSoundsValid = false;
QMap<mu::engraving::ID, MixerChannel*> MixerChannel::s_mixerChannelCache;

//---------------------------------------------------------
//   AudioResource
//---------------------------------------------------------

AudioResource::AudioResource(const AudioResourceMeta& meta, QObject* parent)
    : QObject(parent), m_meta(meta)
{
}

QString AudioResource::type() const
{
    return QString::fromStdString(m_meta.type);
}

QString AudioResource::name() const
{
    auto it = m_meta.attributes.find(muse::String("name"));
    if (it != m_meta.attributes.end()) {
        return it->second.toQString();
    }

    if (!m_meta.id.empty()) {
        return QString::fromStdString(m_meta.id);
    }

    return QString::fromStdString(m_meta.vendor);
}

//---------------------------------------------------------
//   MixerChannel
//---------------------------------------------------------

MixerChannel::MixerChannel(const mu::engraving::InstrumentTrackId& trackId, const muse::modularity::ContextPtr& iocCtx,
                           QObject* parent)
    : QObject(parent), muse::Contextable(iocCtx), m_trackId(trackId)
{
    loadParams();

    if (!playback()) {
        return;
    }

    // Follow changes made elsewhere, the mixer panel included
    playback()->controlParamsChanged().onReceive(this, [this](const TrackId trackId, const ControlParams& params) {
        if (trackId == audioTrackId()) {
            m_control = params;
            m_controlValid = true;
        }
    });

    playback()->sourceParamsChanged().onReceive(this, [this](const TrackId trackId, const AudioSourceParams& params) {
        if (trackId == audioTrackId()) {
            m_source = params;
            m_sourceValid = true;
            invalidateSoundCache();
        }
    });
}

MixerChannel::~MixerChannel()
{
    delete m_currentSound;
}

TrackId MixerChannel::audioTrackId() const
{
    if (!playbackController()) {
        return INVALID_TRACK_ID;
    }

    const auto& map = playbackController()->instrumentTrackIdMap();
    auto it = map.find(m_trackId);
    return it != map.end() ? it->second : INVALID_TRACK_ID;
}

void MixerChannel::loadParams()
{
    TrackId trackId = audioTrackId();
    if (!playback() || trackId == INVALID_TRACK_ID) {
        return;
    }

    playback()->params(trackId).onResolve(this, [this](const TrackParams& params) {
        m_control = params.control;
        m_controlValid = true;
        m_source = params.source;
        m_sourceValid = true;
        invalidateSoundCache();
    }).onReject(this, [](int code, const std::string& msg) {
        LOGW() << "unable to load mixer channel params: " << code << " " << msg;
    });
}

void MixerChannel::sendControl()
{
    TrackId trackId = audioTrackId();
    if (playback() && trackId != INVALID_TRACK_ID) {
        // The playback controller stores the result, per part when a part is open
        playback()->setControlParams(trackId, m_control);
    }
}

void MixerChannel::sendSource()
{
    TrackId trackId = audioTrackId();
    if (playback() && trackId != INVALID_TRACK_ID) {
        playback()->setSourceParams(trackId, m_source);
    }
}

void MixerChannel::invalidateSoundCache()
{
    delete m_currentSound;
    m_currentSound = nullptr;
}

float MixerChannel::volume() const
{
    if (!m_controlValid) {
        return 0.0f;
    }

    // An automated volume has no single value; report where it starts
    return m_control.volume.evaluateAt(secs_t(0.0), VOLUME_DB_MIN, VOLUME_DB_MAX).raw();
}

void MixerChannel::setVolume(float volume)
{
    volume = std::clamp(volume, VOLUME_DB_MIN.raw(), VOLUME_DB_MAX.raw());
    m_control.volume = AutomatableValue<volume_db_t>(volume_db_t::make(volume));
    m_controlValid = true;
    sendControl();
}

float MixerChannel::balance() const
{
    if (!m_controlValid) {
        return 0.0f;
    }

    return m_control.balance.evaluateAt(secs_t(0.0), balance_t(-1.f), balance_t(1.f)).raw();
}

void MixerChannel::setBalance(float balance)
{
    balance = std::clamp(balance, -1.0f, 1.0f);
    m_control.balance = AutomatableValue<balance_t>(balance_t::make(balance));
    m_controlValid = true;
    sendControl();
}

bool MixerChannel::hasSoloMuteState() const
{
    // The controller reads the state of the open notation and does not check for one
    return playbackController() && globalContext() && globalContext()->currentNotation();
}

bool MixerChannel::muted() const
{
    return hasSoloMuteState() ? playbackController()->trackSoloMuteState(m_trackId).mute : false;
}

void MixerChannel::setMuted(bool muted)
{
    // Same path as the mute button: the solo/mute state of the open notation
    if (!hasSoloMuteState()) {
        return;
    }

    auto state = playbackController()->trackSoloMuteState(m_trackId);
    state.mute = muted;
    playbackController()->setTrackSoloMuteState(m_trackId, state);
}

bool MixerChannel::solo() const
{
    return hasSoloMuteState() ? playbackController()->trackSoloMuteState(m_trackId).solo : false;
}

void MixerChannel::setSolo(bool solo)
{
    if (!hasSoloMuteState()) {
        return;
    }

    auto state = playbackController()->trackSoloMuteState(m_trackId);
    state.solo = solo;
    playbackController()->setTrackSoloMuteState(m_trackId, state);
}

QList<AudioResource*> MixerChannel::availableSounds()
{
    if (s_availableSoundsValid || !playback()) {
        return s_availableSounds;
    }

    playback()->availableInputResources().onResolve(this, [](const AudioResourceMetaList& resources) {
        qDeleteAll(s_availableSounds);
        s_availableSounds.clear();
        for (const AudioResourceMeta& meta : resources) {
            s_availableSounds.append(new AudioResource(meta, nullptr));
        }
        s_availableSoundsValid = true;
    }).onReject(this, [](int code, const std::string& msg) {
        LOGW() << "unable to load available sounds: " << code << " " << msg;
    });

    return s_availableSounds;
}

AudioResource* MixerChannel::currentSound()
{
    if (!m_sourceValid) {
        return nullptr;
    }

    if (!m_currentSound) {
        m_currentSound = new AudioResource(m_source.resourceMeta, this);
    }

    return m_currentSound;
}

bool MixerChannel::setSound(const QString& resourceId)
{
    if (!playback() || audioTrackId() == INVALID_TRACK_ID) {
        return false;
    }

    playback()->availableInputResources().onResolve(this, [this, resourceId](const AudioResourceMetaList& resources) {
        for (const AudioResourceMeta& meta : resources) {
            if (QString::fromStdString(meta.id) == resourceId) {
                m_source.resourceMeta = meta;
                m_sourceValid = true;
                invalidateSoundCache();
                sendSource();
                return;
            }
        }
        LOGW() << "sound resource not found: " << resourceId;
    }).onReject(this, [resourceId](int code, const std::string& msg) {
        LOGW() << "unable to set sound " << resourceId << ": " << code << " " << msg;
    });

    return true;
}

int MixerChannel::midiProgram() const
{
    auto it = m_source.configuration.find("midiProgram");
    return m_sourceValid && it != m_source.configuration.end() ? std::stoi(it->second) : -1;
}

bool MixerChannel::setMidiProgram(int program)
{
    if (program < 0 || program > 127 || audioTrackId() == INVALID_TRACK_ID) {
        return false;
    }

    m_source.configuration["midiProgram"] = std::to_string(program);
    sendSource();
    return true;
}

int MixerChannel::midiBank() const
{
    auto it = m_source.configuration.find("midiBank");
    return m_sourceValid && it != m_source.configuration.end() ? std::stoi(it->second) : -1;
}

bool MixerChannel::setMidiBank(int bank)
{
    if (bank < 0 || audioTrackId() == INVALID_TRACK_ID) {
        return false;
    }

    m_source.configuration["midiBank"] = std::to_string(bank);
    sendSource();
    return true;
}

void MixerChannel::resetToMaster()
{
    if (!globalContext()) {
        return;
    }

    auto masterNotation = globalContext()->currentMasterNotation();
    auto currentNotation = globalContext()->currentNotation();
    if (!masterNotation || !currentNotation || currentNotation == masterNotation->notation()) {
        return;
    }

    auto soloMuteState = currentNotation->soloMuteState();
    auto masterParts = masterNotation->notation()->parts();
    auto excerptParts = currentNotation->parts();
    if (!soloMuteState || !masterParts || !excerptParts) {
        return;
    }

    // What the Parts dialog does on Reset: forget the part's own mix, then
    // mute exactly the instruments the part does not show
    soloMuteState->clearAllStates();

    for (const mu::engraving::Part* masterPart : masterParts->partList()) {
        const mu::engraving::Part* excerptPart = excerptParts->part(masterPart->id());

        mu::notation::INotationSoloMuteState::SoloMuteState state;
        state.mute = !excerptPart || !excerptPart->isVisible();

        for (const mu::engraving::InstrumentTrackId& trackId : masterPart->instrumentTrackIdSet()) {
            soloMuteState->setTrackSoloMuteState(trackId, state);
        }
    }

    loadParams();
}
