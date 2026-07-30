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
#include <string>
#include <unordered_map>

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
// supports it. The plugin advertises each keyswitch with the exact MuseScore articulation name
// (the mpe::ArticulationType spelling), so titles are matched exactly and case-insensitively
// against the curated vocabulary below. Naming the keyswitches canonically is the plugin's
// responsibility; the host does no fuzzy guessing.
//
// KeyswitchInfo: only keyswitchMin on bus 0 / channel 0 is read. This is a deliberate design
// choice for a keyswitch SENDER, not a gap to close later:
//  - keyswitchMin already selects the articulation. keyswitchMax only widens the set of notes that
//    would also select it, so min is always a valid choice.
//  - typeId is effectively kNoteOnKeyswitchTypeID here: we send the keyswitch note on before the
//    note, which is what almost every instrument expects. On the fly keyswitches are rare.
//  - channel and unitId have no equivalent in MuseScore: playback sends every note on channel 0
//    (see buildEvent) and mpe has no per articulation channel or unit concept. The whole keyswitch
//    feature lives only in this fork, so there is no host machinery being wasted; honoring these
//    fields would mean building new host support with no benefit for the current instruments.
// If a future plugin genuinely needs ranges, another channel or on the fly keyswitches, that is
// new, separately justified host work, not a defect in this code.
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

    // Curated vocabulary: exact MuseScore articulation names (lowercased) mapped to their type.
    // Limited to what these plucked-string instruments support; the plugin must use these exact
    // names. Close relatives are filled in by the alias pass below.
    static const std::unordered_map<std::string, mpe::ArticulationType> NAME_TO_TYPE = {
        { "standard", mpe::ArticulationType::Standard },
        { "pizzicato", mpe::ArticulationType::Pizzicato },
        { "snappizzicato", mpe::ArticulationType::SnapPizzicato },
        { "randompizzicato", mpe::ArticulationType::RandomPizzicato },
        { "harmonic", mpe::ArticulationType::Harmonic },
        { "mute", mpe::ArticulationType::Mute },
        { "palmmute", mpe::ArticulationType::PalmMute },
        { "tremolo8th", mpe::ArticulationType::Tremolo8th },
        { "tremolo16th", mpe::ArticulationType::Tremolo16th },
        { "tremolo32nd", mpe::ArticulationType::Tremolo32nd },
        { "tremolo64th", mpe::ArticulationType::Tremolo64th },
    };

    VstKeyswitchProfile profile;

    for (int32 i = 0; i < count; ++i) {
        KeyswitchInfo info;
        if (keyswitchCtrl->getKeyswitchInfo(busIndex, channel, i, info) != kResultTrue) {
            continue;
        }

        // Lowercase the title and match it exactly against the canonical vocabulary.
        std::string title = VST3::StringConvert::convert(info.title);
        std::transform(title.begin(), title.end(), title.begin(), ::tolower);

        auto it = NAME_TO_TYPE.find(title);
        if (it != NAME_TO_TYPE.cend()) {
            profile.keyswitches[it->second] = info.keyswitchMin;
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
