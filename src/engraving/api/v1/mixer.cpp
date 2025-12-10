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
#include "modularity/ioc.h"
#include "context/iglobalcontext.h"
#include "playback/iplaybackcontroller.h"
#include "audio/main/iplayback.h"
#include "notation/inotationparts.h"
#include "engraving/dom/part.h"

#include "log.h"

using namespace mu::engraving::apiv1;
using namespace muse;

// Initialize static members
QList<AudioResource*> MixerChannel::s_cachedAvailableSounds;
bool MixerChannel::s_availableSoundsCacheValid = false;
QMap<mu::engraving::ID, MixerChannel*> MixerChannel::s_mixerChannelCache;

//---------------------------------------------------------
//   AudioResource
//---------------------------------------------------------

AudioResource::AudioResource(const muse::audio::AudioResourceMeta& meta, QObject* parent)
    : QObject(parent), m_meta(meta)
{
}

QString AudioResource::type() const
{
    using namespace muse::audio;
    switch (m_meta.type) {
    case AudioResourceType::FluidSoundfont:
        return "FluidSoundfont";
    case AudioResourceType::VstPlugin:
        return "VstPlugin";
    case AudioResourceType::MuseSamplerSoundPack:
        return "MuseSampler";
    default:
        return "Unknown";
    }
}

QString AudioResource::name() const
{
    // Try to get a human-readable name from attributes
    auto it = m_meta.attributes.find(muse::String("name"));
    if (it != m_meta.attributes.end()) {
        return QString::fromStdString(it->second.toStdString());
    }

    // Fall back to ID or vendor
    if (!m_meta.id.empty()) {
        return QString::fromStdString(m_meta.id);
    }

    return QString::fromStdString(m_meta.vendor);
}

//---------------------------------------------------------
//   MixerChannel
//---------------------------------------------------------

MixerChannel::MixerChannel(const mu::engraving::InstrumentTrackId& trackId,
                           QObject* parent)
    : QObject(parent), m_trackId(trackId)
{
    // Initialize with default values
    m_cachedOutputParams.volume = 1.0f;
    m_cachedOutputParams.balance = 0.0f;
    m_cachedOutputParams.muted = false;
    m_cachedOutputParams.solo = false;

    // Load initial parameters and subscribe to changes (async)
    loadInitialParams();
    subscribeToParamChanges();
}

MixerChannel::~MixerChannel()
{
    // Clean up per-channel cached resources
    if (m_cachedCurrentSound) {
        delete m_cachedCurrentSound;
        m_cachedCurrentSound = nullptr;
    }

    // Note: s_cachedAvailableSounds is static and shared, so we don't delete it here
}

mu::playback::IPlaybackController* MixerChannel::playbackController() const
{
    return muse::modularity::globalIoc()->resolve<mu::playback::IPlaybackController>("playback").get();
}

muse::audio::IPlayback* MixerChannel::playback() const
{
    return muse::modularity::globalIoc()->resolve<muse::audio::IPlayback>("audio").get();
}

muse::audio::TrackSequenceId MixerChannel::currentSequenceId() const
{
    auto controller = playbackController();
    if (!controller) {
        return muse::audio::TrackSequenceId(-1);
    }

    // Get the current track sequence ID from playback controller
    return controller->currentTrackSequenceId();
}

muse::audio::TrackId MixerChannel::audioTrackId() const
{
    auto controller = playbackController();
    if (!controller) {
        return muse::audio::TrackId();
    }

    // Get the audio track ID from the instrument track ID
    const auto& trackIdMap = controller->instrumentTrackIdMap();
    auto it = trackIdMap.find(m_trackId);

    if (it == trackIdMap.end()) {
        return muse::audio::TrackId();
    }

    return it->second;
}

void MixerChannel::invalidateCache()
{
    s_availableSoundsCacheValid = false;  // Invalidate global cache
    m_currentSoundCacheValid = false;
}

void MixerChannel::loadInitialParams()
{
    auto pb = playback();
    if (!pb) {
        return;
    }

    auto sequenceId = currentSequenceId();
    auto trackId = audioTrackId();

    // Skip loading if sequence or track is invalid (e.g., plugin opened before playback initialized)
    if (sequenceId == -1 || trackId == -1) {
        return;
    }

    // Load output parameters (volume, balance, mute, solo) - async
    pb->outputParams(sequenceId, trackId)
        .onResolve(this, [this](const muse::audio::AudioOutputParams& params) {
        m_cachedOutputParams = params;
        m_outputParamsValid = true;
    })
        .onReject(this, [](int code, const std::string& msg) {
        LOGW() << "Failed to load output params: " << code << " - " << msg;
    });

    // Load input parameters (sound selection) - async
    pb->inputParams(sequenceId, trackId)
        .onResolve(this, [this](const muse::audio::AudioInputParams& params) {
        m_cachedInputParams = params;
        m_inputParamsValid = true;
        // Invalidate sound caches when input changes
        invalidateCache();
    })
        .onReject(this, [](int code, const std::string& msg) {
        LOGW() << "Failed to load input params: " << code << " - " << msg;
    });
}

void MixerChannel::subscribeToParamChanges()
{
    auto pb = playback();
    if (!pb) {
        return;
    }

    auto sequenceId = currentSequenceId();
    auto trackId = audioTrackId();

    // Skip subscription if sequence or track is invalid
    if (sequenceId == -1 || trackId == -1) {
        return;
    }

    // Subscribe to output parameter changes (volume, balance, mute, solo)
    pb->outputParamsChanged().onReceive(
        this, [this, trackId](const muse::audio::TrackSequenceId&, const muse::audio::TrackId& changedTrackId,
                              const muse::audio::AudioOutputParams& params) {
        if (changedTrackId == trackId) {
            m_cachedOutputParams = params;
            m_outputParamsValid = true;
        }
    });

    // Subscribe to input parameter changes (sound selection)
    pb->inputParamsChanged().onReceive(
        this, [this, trackId](const muse::audio::TrackSequenceId&, const muse::audio::TrackId& changedTrackId,
                              const muse::audio::AudioInputParams& params) {
        if (changedTrackId == trackId) {
            m_cachedInputParams = params;
            m_inputParamsValid = true;
            // Invalidate sound caches when input changes
            invalidateCache();
        }
    });
}

float MixerChannel::volume() const
{
    // Return cached value (synchronous)
    if (m_outputParamsValid) {
        return m_cachedOutputParams.volume;  // Implicit conversion from volume_db_t to float
    }
    return 1.0f;
}

void MixerChannel::setVolume(float volume)
{
    // Clamp volume to valid dB range (-60 to +12 dB)
    volume = std::max(-60.0f, std::min(12.0f, volume));

    // Update the notation solo/mute state (persists the volume for excerpts)
    auto controller = playbackController();
    if (controller) {
        mu::notation::INotationSoloMuteState::SoloMuteState state = controller->trackSoloMuteState(m_trackId);
        state.volumeDb = volume;
        state.hasCustomVolume = true;
        controller->setTrackSoloMuteState(m_trackId, state);
    }

    // Update local cache and send to audio system
    m_cachedOutputParams.volume = volume;
    m_outputParamsValid = true;

    auto pb = playback();
    if (!pb) {
        return;
    }

    auto sequenceId = currentSequenceId();
    auto trackId = audioTrackId();
    if (trackId != -1) {
        pb->setOutputParams(sequenceId, trackId, m_cachedOutputParams);
    }
}

float MixerChannel::balance() const
{
    // Return cached value (synchronous)
    return m_outputParamsValid ? m_cachedOutputParams.balance : 0.0f;
}

void MixerChannel::setBalance(float balance)
{
    // Clamp balance to valid range
    balance = std::max(-1.0f, std::min(1.0f, balance));

    // Update cache immediately (responsive UI)
    m_cachedOutputParams.balance = balance;
    m_outputParamsValid = true;

    // Send to audio system (async, fire & forget)
    auto pb = playback();
    if (!pb) {
        return;
    }

    auto sequenceId = currentSequenceId();
    auto trackId = audioTrackId();
    if (trackId == -1) {
        return;
    }

    pb->setOutputParams(sequenceId, trackId, m_cachedOutputParams);
}

bool MixerChannel::muted() const
{
    // Return cached value (synchronous)
    return m_outputParamsValid ? m_cachedOutputParams.muted : false;
}

void MixerChannel::setMuted(bool muted)
{
    // Update the notation solo/mute state (persists the mute for excerpts)
    auto controller = playbackController();
    if (controller) {
        mu::notation::INotationSoloMuteState::SoloMuteState state = controller->trackSoloMuteState(m_trackId);
        state.mute = muted;
        controller->setTrackSoloMuteState(m_trackId, state);
    }

    // Update local cache and send to audio system
    m_cachedOutputParams.muted = muted;
    m_outputParamsValid = true;

    auto pb = playback();
    if (!pb) {
        return;
    }

    auto sequenceId = currentSequenceId();
    auto trackId = audioTrackId();
    if (trackId != -1) {
        pb->setOutputParams(sequenceId, trackId, m_cachedOutputParams);
    }
}

bool MixerChannel::solo() const
{
    // Return cached value (synchronous)
    return m_outputParamsValid ? m_cachedOutputParams.solo : false;
}

void MixerChannel::setSolo(bool solo)
{
    auto pb = playback();
    if (!pb) {
        return;
    }

    auto sequenceId = currentSequenceId();
    auto trackId = audioTrackId();
    if (trackId == -1) {
        return;
    }

    if (!m_outputParamsValid) {
        // Need to load params first, then set solo
        pb->outputParams(sequenceId, trackId)
            .onResolve(this, [this, pb, sequenceId, trackId, solo](const muse::audio::AudioOutputParams& params) {
            m_cachedOutputParams = params;
            m_cachedOutputParams.solo = solo;
            m_outputParamsValid = true;
            pb->setOutputParams(sequenceId, trackId, m_cachedOutputParams);
        })
            .onReject(this, [](int code, const std::string& msg) {
            LOGW() << "setSolo failed to load params: " << code << " - " << msg;
        });
        return;
    }

    // Cache is valid, update and send immediately
    m_cachedOutputParams.solo = solo;
    pb->setOutputParams(sequenceId, trackId, m_cachedOutputParams);
}

QList<AudioResource*> MixerChannel::availableSounds()
{
    // Use global cache (shared across all MixerChannel instances)
    if (s_availableSoundsCacheValid) {
        return s_cachedAvailableSounds;
    }

    // Clean up old global cache
    qDeleteAll(s_cachedAvailableSounds);
    s_cachedAvailableSounds.clear();

    auto pb = playback();
    if (!pb) {
        s_availableSoundsCacheValid = true;
        return s_cachedAvailableSounds;
    }

    // Load available resources async
    pb->availableInputResources()
        .onResolve(this, [](const muse::audio::AudioResourceMetaList& resources) {
        qDeleteAll(s_cachedAvailableSounds);
        s_cachedAvailableSounds.clear();

        for (const auto& meta : resources) {
            s_cachedAvailableSounds.append(new AudioResource(meta, nullptr));
        }
        s_availableSoundsCacheValid = true;
    })
        .onReject(this, [](int code, const std::string& msg) {
        LOGW() << "Failed to load available sounds: " << code << " - " << msg;
    });

    return s_cachedAvailableSounds;
}

AudioResource* MixerChannel::currentSound()
{
    if (m_currentSoundCacheValid && m_cachedCurrentSound) {
        return m_cachedCurrentSound;
    }

    // Clean up old cache
    if (m_cachedCurrentSound) {
        delete m_cachedCurrentSound;
        m_cachedCurrentSound = nullptr;
    }

    if (!m_inputParamsValid) {
        return nullptr;
    }

    // Create AudioResource from cached input params
    m_cachedCurrentSound = new AudioResource(m_cachedInputParams.resourceMeta, this);
    m_currentSoundCacheValid = true;

    return m_cachedCurrentSound;
}

bool MixerChannel::setSound(const QString& resourceId)
{
    auto pb = playback();
    if (!pb) {
        return false;
    }

    auto sequenceId = currentSequenceId();
    auto trackId = audioTrackId();
    if (trackId == -1) {
        return false;
    }

    // Find resource in available sounds (following MuseScore async pattern)
    pb->availableInputResources()
        .onResolve(this, [this, pb, sequenceId, trackId, resourceId](const muse::audio::AudioResourceMetaList& resources) {
        muse::audio::AudioResourceMeta targetMeta;
        bool found = false;

        for (const auto& meta : resources) {
            if (QString::fromStdString(meta.id) == resourceId) {
                targetMeta = meta;
                found = true;
                break;
            }
        }

        if (!found) {
            LOGW() << "Sound resource not found: " << resourceId;
            return;
        }

        // Update cached input params
        m_cachedInputParams.resourceMeta = targetMeta;
        m_inputParamsValid = true;

        // Invalidate sound caches
        invalidateCache();

        // Send to audio system
        pb->setInputParams(sequenceId, trackId, m_cachedInputParams);
    })
        .onReject(this, [resourceId](int code, const std::string& msg) {
        LOGW() << "Failed to set sound " << resourceId << ": " << code << " - " << msg;
    });

    return true;  // Operation initiated (will complete async)
}

int MixerChannel::midiProgram() const
{
    if (!m_inputParamsValid) {
        return -1;
    }

    // MIDI program is stored in configuration (map<string, string>)
    auto it = m_cachedInputParams.configuration.find("midiProgram");
    if (it != m_cachedInputParams.configuration.end()) {
        return std::stoi(it->second);
    }

    return -1;
}

bool MixerChannel::setMidiProgram(int program)
{
    if (program < 0 || program > 127) {
        LOGE() << "Invalid MIDI program: " << program;
        return false;
    }

    // Update cached input params (configuration is map<string, string>)
    m_cachedInputParams.configuration["midiProgram"] = std::to_string(program);
    m_inputParamsValid = true;

    // Send to audio system (async, fire & forget)
    auto pb = playback();
    if (!pb) {
        return false;
    }

    auto sequenceId = currentSequenceId();
    auto trackId = audioTrackId();
    if (trackId == -1) {
        return false;
    }

    pb->setInputParams(sequenceId, trackId, m_cachedInputParams);
    return true;
}

int MixerChannel::midiBank() const
{
    if (!m_inputParamsValid) {
        return -1;
    }

    // MIDI bank is stored in configuration (map<string, string>)
    auto it = m_cachedInputParams.configuration.find("midiBank");
    if (it != m_cachedInputParams.configuration.end()) {
        return std::stoi(it->second);
    }

    return -1;
}

bool MixerChannel::setMidiBank(int bank)
{
    // Update cached input params (configuration is map<string, string>)
    m_cachedInputParams.configuration["midiBank"] = std::to_string(bank);
    m_inputParamsValid = true;

    // Send to audio system (async, fire & forget)
    auto pb = playback();
    if (!pb) {
        return false;
    }

    auto sequenceId = currentSequenceId();
    auto trackId = audioTrackId();
    if (trackId == -1) {
        return false;
    }

    pb->setInputParams(sequenceId, trackId, m_cachedInputParams);
    return true;
}

void MixerChannel::resetToMaster()
{
    auto globalContext = muse::modularity::globalIoc()->resolve<mu::context::IGlobalContext>("context");
    if (!globalContext) {
        return;
    }

    auto masterNotation = globalContext->currentMasterNotation();
    auto currentNotation = globalContext->currentNotation();

    if (!masterNotation || !currentNotation) {
        return;
    }

    // Only works in an excerpt (part), not the master score
    if (currentNotation == masterNotation->notation()) {
        return;
    }

    auto excerptSoloMuteState = currentNotation->soloMuteState();
    if (!excerptSoloMuteState) {
        return;
    }

    // Clear all custom states for this excerpt (like Parts dialog Reset does)
    excerptSoloMuteState->clearAllStates();

    // Re-initialize with correct mute states (replicates MasterNotation::initNotationSoloMuteState):
    // - Parts that exist in the excerpt: mute=false
    // - Parts that don't exist in the excerpt: mute=true
    auto masterParts = masterNotation->notation()->parts();
    auto excerptParts = currentNotation->parts();

    if (!masterParts || !excerptParts) {
        return;
    }

    for (const mu::engraving::Part* masterPart : masterParts->partList()) {
        const mu::engraving::Part* excerptPart = excerptParts->part(masterPart->id());
        const bool shouldMute = !excerptPart || !excerptPart->isVisible();

        mu::notation::INotationSoloMuteState::SoloMuteState state;
        state.mute = shouldMute;
        state.solo = false;

        for (const mu::engraving::InstrumentTrackId& trackId : masterPart->instrumentTrackIdSet()) {
            excerptSoloMuteState->setTrackSoloMuteState(trackId, state);
        }
    }

    // Invalidate local cache so next read gets fresh values
    m_outputParamsValid = false;
}
