/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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
#include "vstsynthesiser.h"

#include <algorithm>
#include <cctype>

#include "pluginterfaces/vst/ivstchannelcontextinfo.h"
#include "pluginterfaces/vst/ivstnoteexpression.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/utility/stringconvert.h"

#include "log.h"

using namespace muse;
using namespace muse::vst;
using namespace muse::audio::synth;
using namespace muse::audio;
using namespace muse::audioplugins;

static const std::set<Steinberg::Vst::CtrlNumber> SUPPORTED_CONTROLLERS = {
    Steinberg::Vst::kCtrlVolume,
    Steinberg::Vst::kCtrlExpression,
    Steinberg::Vst::kCtrlSustainOnOff,
    Steinberg::Vst::kCtrlSustenutoOnOff,
    Steinberg::Vst::kPitchBend,
};

// Query IKeyswitchController from a VST3 plugin and build a keyswitch profile if the plugin
// supports it. Maps plugin keyswitch names to MuseScore articulation types using common
// naming patterns (Tremolo, Pizzicato, Harmonic, Mute, etc.).
static std::optional<VstKeyswitchProfile> queryKeyswitchProfile(const PluginControllerPtr& controller)
{
    using namespace Steinberg;
    using namespace Steinberg::Vst;

    if (!controller) {
        return std::nullopt;
    }

    // Query for IKeyswitchController interface
    FUnknownPtr<IKeyswitchController> keyswitchCtrl(controller);
    if (!keyswitchCtrl) {
        return std::nullopt; // Plugin does not support keyswitches
    }

    // Query keyswitches for event bus 0, channel 0
    const int32 busIndex = 0;
    const int16 channel = 0;
    int32 count = keyswitchCtrl->getKeyswitchCount(busIndex, channel);

    if (count <= 0) {
        return std::nullopt; // No keyswitches defined
    }

    VstKeyswitchProfile profile;

    for (int32 i = 0; i < count; ++i) {
        KeyswitchInfo info;
        if (keyswitchCtrl->getKeyswitchInfo(busIndex, channel, i, info) != kResultTrue) {
            continue;
        }

        // Convert UTF-16 title to std::string (case-insensitive matching)
        std::string title = VST3::StringConvert::convert(info.title);
        std::transform(title.begin(), title.end(), title.begin(), ::tolower);

        // Map keyswitch titles to MuseScore ArticulationTypes using common naming patterns.
        // Plugins can use variations like "Tremolo", "Trem", "Pizzicato", "Pizz", etc.
        
        if (title.find("trem") != std::string::npos) {
            // Map all tremolo types to the same keyswitch
            profile.keyswitches[mpe::ArticulationType::Tremolo8th] = info.keyswitchMin;
            profile.keyswitches[mpe::ArticulationType::Tremolo16th] = info.keyswitchMin;
            profile.keyswitches[mpe::ArticulationType::Tremolo32nd] = info.keyswitchMin;
            profile.keyswitches[mpe::ArticulationType::Tremolo64th] = info.keyswitchMin;
        } else if (title.find("pizz") != std::string::npos) {
            profile.keyswitches[mpe::ArticulationType::Pizzicato] = info.keyswitchMin;
            profile.keyswitches[mpe::ArticulationType::SnapPizzicato] = info.keyswitchMin;
        } else if (title.find("harm") != std::string::npos) {
            profile.keyswitches[mpe::ArticulationType::Harmonic] = info.keyswitchMin;
        } else if (title.find("mute") != std::string::npos || title.find("palm") != std::string::npos) {
            profile.keyswitches[mpe::ArticulationType::PalmMute] = info.keyswitchMin;
            profile.keyswitches[mpe::ArticulationType::Mute] = info.keyswitchMin;
        } else if (title.find("staccato") != std::string::npos || title.find("stacc") != std::string::npos) {
            profile.keyswitches[mpe::ArticulationType::Staccato] = info.keyswitchMin;
            profile.keyswitches[mpe::ArticulationType::Staccatissimo] = info.keyswitchMin;
        } else if (title.find("legato") != std::string::npos) {
            profile.keyswitches[mpe::ArticulationType::Legato] = info.keyswitchMin;
        } else if (title.find("tenuto") != std::string::npos) {
            profile.keyswitches[mpe::ArticulationType::Tenuto] = info.keyswitchMin;
        } else if (title.find("marcato") != std::string::npos || title.find("accent") != std::string::npos) {
            profile.keyswitches[mpe::ArticulationType::Marcato] = info.keyswitchMin;
            profile.keyswitches[mpe::ArticulationType::Accent] = info.keyswitchMin;
        } else if (title.find("col legno") != std::string::npos || title.find("collegno") != std::string::npos) {
            profile.keyswitches[mpe::ArticulationType::ColLegno] = info.keyswitchMin;
        } else if (title.find("sul pont") != std::string::npos || title.find("sulpont") != std::string::npos) {
            profile.keyswitches[mpe::ArticulationType::SulPont] = info.keyswitchMin;
        } else if (title.find("sul tasto") != std::string::npos || title.find("sultasto") != std::string::npos) {
            profile.keyswitches[mpe::ArticulationType::SulTasto] = info.keyswitchMin;
        } else if (title.find("vibrato") != std::string::npos) {
            profile.keyswitches[mpe::ArticulationType::Vibrato] = info.keyswitchMin;
        } else if (title.find("distortion") != std::string::npos || title.find("overdrive") != std::string::npos) {
            profile.keyswitches[mpe::ArticulationType::Distortion] = info.keyswitchMin;
        } else if (title.find("standard") != std::string::npos || title.find("normal") != std::string::npos 
                   || title.find("natural") != std::string::npos || title.find("pick") != std::string::npos) {
            profile.keyswitches[mpe::ArticulationType::Standard] = info.keyswitchMin;
        }
    }

    if (profile.keyswitches.empty()) {
        return std::nullopt;
    }

    return profile;
}


VstSynthesiser::VstSynthesiser(const TrackId trackId, const muse::audio::AudioInputParams& params,
                               const modularity::ContextPtr& iocCtx)
    : AbstractSynthesizer(params, iocCtx),
    m_vstAudioClient(std::make_unique<VstAudioClient>(iocCtx)),
    m_trackId(trackId)
{
}

VstSynthesiser::~VstSynthesiser()
{
    instancesRegister()->unregisterInstrPlugin(m_params.resourceMeta.id, m_trackId);
}

void VstSynthesiser::init(const OutputSpec& spec)
{
    IF_ASSERT_FAILED(spec.isValid()) {
        return;
    }

    m_outputSpec = spec;

    m_pluginPtr = instancesRegister()->makeAndRegisterInstrPlugin(m_params.resourceMeta.id, m_trackId);

    m_vstAudioClient->init(AudioPluginType::Instrument, m_pluginPtr);

    auto onPluginLoaded = [this]() {
        m_pluginPtr->updatePluginConfig(m_params.configuration);
        m_vstAudioClient->setOutputSpec(m_outputSpec);
        m_vstAudioClient->loadSupportedParams();
        
        // Query keyswitch profile directly from the plugin via IKeyswitchController interface.
        const std::optional<VstKeyswitchProfile> keyswitchProfile = queryKeyswitchProfile(m_pluginPtr->controller());
        
        m_sequencer.init(m_vstAudioClient->paramsMapping(SUPPORTED_CONTROLLERS), m_useDynamicEvents, keyswitchProfile);
        m_inited = true;
        sendChannelContext();
    };

    if (m_pluginPtr->isLoaded()) {
        onPluginLoaded();
    } else {
        m_pluginPtr->loadingCompleted().onNotify(this, onPluginLoaded);
    }

    m_pluginPtr->pluginSettingsChanged().onReceive(this, [this](const muse::audio::AudioUnitConfig& newConfig) {
        if (m_params.configuration == newConfig) {
            return;
        }

        m_params.configuration = newConfig;
        m_paramsChanges.send(m_params);
    });

    m_sequencer.setOnOffStreamFlushed([this]() {
        m_vstAudioClient->flushSound();
    });
}

void VstSynthesiser::updateRenderingMode(const RenderMode mode)
{
    if (mode == RenderMode::OfflineMode) {
        m_vstAudioClient->setProcessMode(VstProcessMode::kOffline);
    } else {
        m_vstAudioClient->setProcessMode(VstProcessMode::kRealtime);
    }
}

void VstSynthesiser::toggleVolumeGain(const bool isActive)
{
    static constexpr muse::audio::gain_t NON_ACTIVE_GAIN = 0.5f;

    if (isActive) {
        m_vstAudioClient->setVolumeGain(m_sequencer.currentGain());
    } else {
        m_vstAudioClient->setVolumeGain(NON_ACTIVE_GAIN);
    }
}

bool VstSynthesiser::isValid() const
{
    if (!m_pluginPtr) {
        return false;
    }

    return m_pluginPtr->isLoaded();
}

muse::audio::AudioSourceType VstSynthesiser::type() const
{
    return m_params.type();
}

std::string VstSynthesiser::name() const
{
    if (!m_pluginPtr) {
        return std::string();
    }

    return m_pluginPtr->name();
}

void VstSynthesiser::setHostTrackName(const std::string& name)
{
    m_hostTrackName = name;
    if (m_inited) {
        sendChannelContext();
    }
}

void VstSynthesiser::sendChannelContext()
{
    if (m_hostTrackName.empty() || !m_pluginPtr) {
        return;
    }

    PluginControllerPtr controller = m_pluginPtr->controller();
    if (!controller) {
        return;
    }

    Steinberg::FUnknownPtr<Steinberg::Vst::ChannelContext::IInfoListener> infoListener(controller);
    if (!infoListener) {
        return; // the plugin does not use channel context
    }

    Steinberg::IPtr<Steinberg::Vst::IAttributeList> list = Steinberg::Vst::HostAttributeList::make();
    Steinberg::Vst::String128 name128 = {};
    Steinberg::Vst::StringConvert::convert(m_hostTrackName, name128);
    list->setString(Steinberg::Vst::ChannelContext::kChannelNameKey, name128);
    infoListener->setChannelContextInfos(list);
}

void VstSynthesiser::flushSound()
{
    m_sequencer.flushOffstream();
    m_vstAudioClient->flushSound();
}

void VstSynthesiser::setupSound(const mpe::PlaybackSetupData& setupData)
{
    m_useDynamicEvents = setupData.supportsSingleNoteDynamics;
}

void VstSynthesiser::setupEvents(const mpe::PlaybackData& playbackData)
{
    m_sequencer.load(playbackData);
}

const mpe::PlaybackData& VstSynthesiser::playbackData() const
{
    return m_sequencer.playbackData();
}

bool VstSynthesiser::isActive() const
{
    return m_sequencer.isActive();
}

void VstSynthesiser::setIsActive(const bool isActive)
{
    if (m_sequencer.isActive() == isActive) {
        return;
    }

    m_sequencer.setActive(isActive);
    toggleVolumeGain(isActive);
    m_vstAudioClient->setIsPlaying(isActive);
    m_vstAudioClient->setIsActive(isActive);
}

muse::audio::msecs_t VstSynthesiser::playbackPosition() const
{
    return m_sequencer.playbackPosition();
}

void VstSynthesiser::setPlaybackPosition(const muse::audio::msecs_t newPosition)
{
    m_sequencer.setPlaybackPosition(newPosition);
    m_currentPositionSamples = microSecsToSamples(newPosition, m_outputSpec.sampleRate);

    if (isActive()) {
        m_vstAudioClient->setVolumeGain(m_sequencer.currentGain());
    }
}

void VstSynthesiser::setOutputSpec(const audio::OutputSpec& spec)
{
    m_outputSpec = spec;
    m_currentPositionSamples = microSecsToSamples(m_sequencer.playbackPosition(), m_outputSpec.sampleRate);

    if (m_inited) {
        m_vstAudioClient->setOutputSpec(spec);
    }
}

unsigned int VstSynthesiser::audioChannelsCount() const
{
    return m_outputSpec.audioChannelCount;
}

async::Channel<unsigned int> VstSynthesiser::audioChannelsCountChanged() const
{
    return m_streamsCountChanged;
}

samples_t VstSynthesiser::process(float* buffer, samples_t samplesPerChannel)
{
    if (!buffer) {
        return 0;
    }

    const msecs_t nextMsecs = samplesToMsecs(samplesPerChannel, m_outputSpec.sampleRate);
    const VstSequencer::EventSequenceMap sequences = m_sequencer.movePlaybackForward(nextMsecs);
    const bool active = m_sequencer.isActive();

    samples_t sampleOffset = 0;
    samples_t processedSamples = 0;

    for (auto it = sequences.cbegin(); it != sequences.cend(); ++it) {
        samples_t durationInSamples = samplesPerChannel - sampleOffset;

        auto nextIt = std::next(it);
        if (nextIt != sequences.cend()) {
            msecs_t duration = nextIt->first - it->first;
            durationInSamples = microSecsToSamples(duration, m_outputSpec.sampleRate);
        }

        IF_ASSERT_FAILED(sampleOffset + durationInSamples <= samplesPerChannel) {
            break;
        }

        processedSamples += processSequence(it->second, durationInSamples, buffer + sampleOffset * m_outputSpec.audioChannelCount, sampleOffset);
        sampleOffset += durationInSamples;

        if (active) {
            m_currentPositionSamples += durationInSamples;
        }
    }

    return processedSamples;
}

samples_t VstSynthesiser::processSequence(const VstSequencer::EventSequence& sequence, const samples_t samples, float* buffer, samples_t bufferOffset)
{
    for (const VstSequencer::EventType& event : sequence) {
        if (std::holds_alternative<VstEvent>(event)) {
            VstEvent evt = std::get<VstEvent>(event);
            evt.sampleOffset = bufferOffset;
            m_vstAudioClient->handleEvent(evt);
        } else if (std::holds_alternative<ParamChangeEvent>(event)) {
            m_vstAudioClient->handleParamChange(std::get<ParamChangeEvent>(event));
        } else {
            muse::audio::gain_t newGain = std::get<muse::audio::gain_t>(event);
            m_vstAudioClient->setVolumeGain(newGain);
        }
    }

    if (samples == 0) {
        return 0;
    }

    return m_vstAudioClient->process(buffer, samples, m_currentPositionSamples);
}
