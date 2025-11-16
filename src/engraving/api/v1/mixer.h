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
#include <QList>
#include <QMap>

#include "engraving/types/types.h"
#include "async/asyncable.h"
#include "modularity/ioc.h"
#include "audio/common/audiotypes.h"
#include "audio/main/iplayback.h"
#include "playback/iplaybackcontroller.h"
#include "context/iglobalcontext.h"

namespace mu::engraving::apiv1 {
//---------------------------------------------------------
//   AudioResource
///  A sound that can be assigned to a mixer channel.
//---------------------------------------------------------

class AudioResource : public QObject
{
    Q_OBJECT

    /// Unique identifier for this resource
    Q_PROPERTY(QString id READ id CONSTANT)
    /// Resource vendor/provider name
    Q_PROPERTY(QString vendor READ vendor CONSTANT)
    /// Resource type, as the audio engine names it
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

    const muse::audio::AudioResourceMeta& resourceMeta() const { return m_meta; }
    /// \endcond

private:
    muse::audio::AudioResourceMeta m_meta;
};

//---------------------------------------------------------
//   MixerChannel
///  The mixer channel of a part: volume, balance, mute,
///  solo and sound, as the mixer panel shows them.
//---------------------------------------------------------

class MixerChannel : public QObject, public muse::async::Asyncable, public muse::Contextable
{
    Q_OBJECT

    /// Volume in dB, from -60 to +12 (0 = nominal)
    Q_PROPERTY(float volume READ volume WRITE setVolume)
    /// Balance, from -1.0 (full left) to 1.0 (full right)
    Q_PROPERTY(float balance READ balance WRITE setBalance)
    /// Whether this channel is muted
    Q_PROPERTY(bool muted READ muted WRITE setMuted)
    /// Whether this channel is soloed
    Q_PROPERTY(bool solo READ solo WRITE setSolo)
    /// Currently assigned audio resource (sound)
    Q_PROPERTY(apiv1::AudioResource * currentSound READ currentSound)

    muse::ContextInject<muse::audio::IPlayback> playback = { this };
    muse::ContextInject<mu::playback::IPlaybackController> playbackController = { this };
    muse::ContextInject<mu::context::IGlobalContext> globalContext = { this };

public:
    /// \cond MS_INTERNAL
    MixerChannel(const mu::engraving::InstrumentTrackId& trackId, const muse::modularity::ContextPtr& iocCtx,
                 QObject* parent = nullptr);
    ~MixerChannel() override;

    // Instances outlive the Part wrappers that hand them out, so a plugin that
    // runs again finds the values it left behind. Keyed by part.
    static QMap<mu::engraving::ID, MixerChannel*> s_mixerChannelCache;
    /// \endcond

    /// \since MuseScore 4.7
    float volume() const;
    /// \since MuseScore 4.7
    void setVolume(float volume);

    /// \since MuseScore 4.7
    float balance() const;
    /// \since MuseScore 4.7
    void setBalance(float balance);

    /// \since MuseScore 4.7
    bool muted() const;
    /// \since MuseScore 4.7
    void setMuted(bool muted);

    /// \since MuseScore 4.7
    bool solo() const;
    /// \since MuseScore 4.7
    void setSolo(bool solo);

    /// All audio resources (sounds) that can be assigned to this channel.
    /// Loaded asynchronously: the first call may return an empty list.
    /// \since MuseScore 4.7
    Q_INVOKABLE QList<apiv1::AudioResource*> availableSounds();

    /// \since MuseScore 4.7
    AudioResource* currentSound();

    /// Assigns a sound by its resource id.
    /// \returns true if the change was started; it completes asynchronously.
    /// \since MuseScore 4.7
    Q_INVOKABLE bool setSound(const QString& resourceId);

    /// MIDI program (0-127) for SoundFonts, or -1 if not applicable.
    /// \since MuseScore 4.7
    Q_INVOKABLE int midiProgram() const;
    /// \since MuseScore 4.7
    Q_INVOKABLE bool setMidiProgram(int program);

    /// MIDI bank for SoundFonts, or -1 if not applicable.
    /// \since MuseScore 4.7
    Q_INVOKABLE int midiBank() const;
    /// \since MuseScore 4.7
    Q_INVOKABLE bool setMidiBank(int bank);

    /// In a part (excerpt), drops the part's own mixer settings so it follows
    /// the main score again. Does nothing in the main score.
    /// \since MuseScore 4.7
    Q_INVOKABLE void resetToMaster();

private:
    muse::audio::TrackId audioTrackId() const;
    bool hasSoloMuteState() const;
    void loadParams();
    void sendControl();
    void sendSource();
    void invalidateSoundCache();

    mu::engraving::InstrumentTrackId m_trackId;

    muse::audio::ControlParams m_control;
    muse::audio::AudioSourceParams m_source;
    bool m_controlValid = false;
    bool m_sourceValid = false;

    AudioResource* m_currentSound = nullptr;

    static QList<AudioResource*> s_availableSounds;
    static bool s_availableSoundsValid;
};
}

#endif // MU_ENGRAVING_APIV1_MIXER_H
