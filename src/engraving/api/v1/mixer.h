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

#ifndef MU_ENGRAVING_APIV1_MIXER_H
#define MU_ENGRAVING_APIV1_MIXER_H

#include <QObject>
#include <QQmlListProperty>
#include <memory>

#include "engraving/types/types.h"
#include "async/asyncable.h"
#include "audio/main/iplayback.h"

namespace mu::context {
class IGlobalContext;
}

namespace mu::playback {
class IPlaybackController;
}

namespace muse::audio {
class IPlayback;
}

namespace mu::engraving::apiv1 {
class AudioResource;
class MixerChannel;

//---------------------------------------------------------
//   AudioResource
///   \brief Represents an audio resource (sound library)
///   that can be assigned to a mixer channel.
///   \details Audio resources include Muse Sounds,
///   SoundFonts, and VST instruments.
///   \since MuseScore 4.7
//---------------------------------------------------------

class AudioResource : public QObject
{
    Q_OBJECT

    /// Unique identifier for this resource
    Q_PROPERTY(QString id READ id CONSTANT)
    /// Resource vendor/provider name
    Q_PROPERTY(QString vendor READ vendor CONSTANT)
    /// Resource type (e.g., "MuseSampler", "FluidSoundfont", "VstPlugin")
    Q_PROPERTY(QString type READ type CONSTANT)
    /// Human-readable name of the resource
    Q_PROPERTY(QString name READ name CONSTANT)

public:
    /// \cond MS_INTERNAL
    AudioResource(const muse::audio::AudioResourceMeta& meta, QObject* parent = nullptr);

    QString id() const { return QString::fromStdString(m_meta.id); }
    QString vendor() const { return QString::fromStdString(m_meta.vendor); }
    QString type() const;
    QString name() const;
    /// \endcond

    /// \cond MS_INTERNAL
    const muse::audio::AudioResourceMeta& resourceMeta() const { return m_meta; }
    /// \endcond

private:
    muse::audio::AudioResourceMeta m_meta;
};

//---------------------------------------------------------
//   MixerChannel
///   \brief Represents a mixer channel for an instrument
///   in the score.
///   \details Provides access to volume, balance, mute,
///   solo controls, and sound selection for a part's
///   instrument.
///   \since MuseScore 4.7
//---------------------------------------------------------

class MixerChannel : public QObject, public muse::async::Asyncable
{
    Q_OBJECT

    /// Volume level in dB (0.0 = nominal, positive = amplification, negative = attenuation)
    /// Note: Stored as volume_db_t (db_t), values are in decibels directly, not linear scale
    /// Example: 4.0 = +4dB, -6.0 = -6dB
    Q_PROPERTY(float volume READ volume WRITE setVolume)
    /// Balance/pan (-1.0 = full left, 0.0 = center, 1.0 = full right)
    Q_PROPERTY(float balance READ balance WRITE setBalance)
    /// Whether this channel is muted
    Q_PROPERTY(bool muted READ muted WRITE setMuted)
    /// Whether this channel is soloed
    Q_PROPERTY(bool solo READ solo WRITE setSolo)
    /// Currently assigned audio resource (sound)
    Q_PROPERTY(apiv1::AudioResource * currentSound READ currentSound)

public:
    /// \cond MS_INTERNAL
    MixerChannel(const mu::engraving::InstrumentTrackId& trackId,
                 QObject* parent = nullptr);
    ~MixerChannel();

    // Global cache for MixerChannel instances (shared across Part wrapper lifecycles)
    // Maps InstrumentTrackId -> MixerChannel to persist state across plugin re-runs
    static QMap<mu::engraving::InstrumentTrackId, MixerChannel*> s_mixerChannelCache;
    /// \endcond

    /// Returns the current volume level in dB
    /// \since MuseScore 4.7
    float volume() const;

    /// Sets the volume level in dB
    /// \param volume Volume level in decibels (0.0 = nominal, positive = amplification)
    /// Example: 4.0 sets volume to +4dB, -6.0 sets to -6dB
    /// \since MuseScore 4.7
    void setVolume(float volume);

    /// Returns the current balance/pan (-1.0 to 1.0)
    /// \since MuseScore 4.7
    float balance() const;

    /// Sets the balance/pan
    /// \param balance Pan position between -1.0 (full left), 0.0 (center), and 1.0 (full right)
    /// \since MuseScore 4.7
    void setBalance(float balance);

    /// Returns whether this channel is muted
    /// \since MuseScore 4.7
    bool muted() const;

    /// Sets the mute state
    /// \param muted True to mute, false to unmute
    /// \since MuseScore 4.7
    void setMuted(bool muted);

    /// Returns whether this channel is soloed
    /// \since MuseScore 4.7
    bool solo() const;

    /// Sets the solo state
    /// \param solo True to solo, false to unsolo
    /// \since MuseScore 4.7
    void setSolo(bool solo);

    /// Returns a list of all available audio resources (sounds)
    /// that can be assigned to this channel
    /// \since MuseScore 4.7
    Q_INVOKABLE QList<apiv1::AudioResource*> availableSounds();

    /// Returns the currently assigned audio resource (sound)
    /// \since MuseScore 4.7
    AudioResource* currentSound();

    /// Sets the audio resource (sound) for this channel
    /// \param resourceId The unique ID of the resource to assign
    /// \returns True if successful, false if resource not found or invalid
    /// \since MuseScore 4.7
    Q_INVOKABLE bool setSound(const QString& resourceId);

    /// Returns the current MIDI program number (for SoundFonts)
    /// \returns MIDI program number (0-127), or -1 if not applicable
    /// \since MuseScore 4.7
    Q_INVOKABLE int midiProgram() const;

    /// Sets the MIDI program number (for SoundFonts)
    /// \param program MIDI program number (0-127)
    /// \returns True if successful, false if not applicable to current sound
    /// \since MuseScore 4.7
    Q_INVOKABLE bool setMidiProgram(int program);

    /// Returns the current MIDI bank number (for SoundFonts)
    /// \returns MIDI bank number, or -1 if not applicable
    /// \since MuseScore 4.7
    Q_INVOKABLE int midiBank() const;

    /// Sets the MIDI bank number (for SoundFonts)
    /// \param bank MIDI bank number
    /// \returns True if successful, false if not applicable to current sound
    /// \since MuseScore 4.7
    Q_INVOKABLE bool setMidiBank(int bank);

private:
    mu::engraving::InstrumentTrackId m_trackId;

    // Cached audio parameters (following MixerChannelItem pattern)
    mutable muse::audio::AudioOutputParams m_cachedOutputParams;
    mutable muse::audio::AudioInputParams m_cachedInputParams;
    mutable bool m_outputParamsValid = false;
    mutable bool m_inputParamsValid = false;

    // Cached current sound (per-channel)
    mutable apiv1::AudioResource* m_cachedCurrentSound = nullptr;
    mutable bool m_currentSoundCacheValid = false;

    // Global cache for available sounds (shared across all MixerChannel instances)
    static QList<apiv1::AudioResource*> s_cachedAvailableSounds;
    static bool s_availableSoundsCacheValid;

    // Helper methods
    mu::playback::IPlaybackController* playbackController() const;
    muse::audio::IPlayback* playback() const;
    muse::audio::TrackSequenceId currentSequenceId() const;
    muse::audio::TrackId audioTrackId() const;
    void invalidateCache();

    // Load and subscribe to parameter changes (async)
    void loadInitialParams();
    void subscribeToParamChanges();
};
} // namespace mu::engraving::apiv1

#endif // MU_ENGRAVING_APIV1_MIXER_H
