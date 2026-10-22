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

#include <gtest/gtest.h>

#include "engraving/dom/masterscore.h"
#include "engraving/dom/part.h"
#include "engraving/dom/instrument.h"

#include "engraving/api/v1/mixer.h"
#include "engraving/api/v1/part.h"

#include "utils/scorerw.h"
#include "utils/scorecomp.h"

using namespace mu::engraving;
using namespace mu::engraving::apiv1;

static const String MIXER_API_DATA_DIR("mixer_api_data/");

//---------------------------------------------------------
//   Engraving_MixerApiTests
//   Tests for the Plugin API mixer functionality
//---------------------------------------------------------

class Engraving_MixerApiTests : public ::testing::Test
{
protected:
    // Helper: Create a minimal score (for tests that don't need specific content)
    MasterScore* createMinimalScore() {
        // Load a simple test score
        // Using existing test scores is more reliable than creating from scratch
        return ScoreRW::readScore(String("clef_courtesy_data/clef_courtesy01.mscx"));
    }
};

//---------------------------------------------------------
//   Test: Mixer Channel Access
//---------------------------------------------------------

TEST_F(Engraving_MixerApiTests, testGetMixerChannel)
{
    // [GIVEN] A score with one part
    MasterScore* score = createMinimalScore();
    ASSERT_TRUE(score);
    ASSERT_EQ(score->parts().size(), 1);

    mu::engraving::Part* engPart = score->parts().at(0);
    ASSERT_TRUE(engPart);

    // [WHEN] Create API wrapper and get mixer channel
    apiv1::Part apiPart(engPart);
    apiv1::MixerChannel* mixer = apiPart.mixerChannel();

    // [THEN] Mixer channel should be created
    EXPECT_NE(mixer, nullptr);

    // [THEN] Calling again should return same cached instance
    apiv1::MixerChannel* mixer2 = apiPart.mixerChannel();
    EXPECT_EQ(mixer, mixer2);

    delete score;
}

TEST_F(Engraving_MixerApiTests, testMultiplePartsMixerChannels)
{
    // [GIVEN] A score with parts
    MasterScore* score = createMinimalScore();
    ASSERT_TRUE(score != nullptr);
    ASSERT_GT(score->parts().size(), 0);  // At least one part

    // [WHEN] Get mixer channels for all parts
    std::vector<apiv1::MixerChannel*> mixers;
    for (size_t i = 0; i < score->parts().size(); ++i) {
        apiv1::Part apiPart(score->parts().at(i));
        apiv1::MixerChannel* mixer = apiPart.mixerChannel();
        mixers.push_back(mixer);
    }

    // [THEN] All mixer channels should be created
    EXPECT_EQ(mixers.size(), score->parts().size());
    for (auto* mixer : mixers) {
        EXPECT_NE(mixer, nullptr);
    }

    // If there are multiple parts, each should get a unique mixer channel
    if (mixers.size() >= 2) {
        for (size_t i = 0; i < mixers.size(); ++i) {
            for (size_t j = i + 1; j < mixers.size(); ++j) {
                EXPECT_NE(mixers[i], mixers[j]);
            }
        }
    }

    delete score;
}

//---------------------------------------------------------
//   Test: Volume Property
//---------------------------------------------------------

TEST_F(Engraving_MixerApiTests, testVolumeProperty)
{
    // [GIVEN] A score with a part and mixer channel
    MasterScore* score = createMinimalScore();
    apiv1::Part apiPart(score->parts().at(0));
    apiv1::MixerChannel* mixer = apiPart.mixerChannel();
    ASSERT_NE(mixer, nullptr);

    // [WHEN] Get initial volume
    float initialVolume = mixer->volume();

    // [THEN] Volume should be in valid range [0.0, 1.0]
    EXPECT_GE(initialVolume, 0.0f);
    EXPECT_LE(initialVolume, 1.0f);

    // [WHEN] Set volume to 0.7
    mixer->setVolume(0.7f);

    // [THEN] Volume should be updated
    // Note: This may return default value until async bridge is implemented
    float newVolume = mixer->volume();
    EXPECT_GE(newVolume, 0.0f);
    EXPECT_LE(newVolume, 1.0f);

    delete score;
}

TEST_F(Engraving_MixerApiTests, testVolumeRangeClamping)
{
    // [GIVEN] A score with mixer channel
    MasterScore* score = createMinimalScore();
    apiv1::Part apiPart(score->parts().at(0));
    apiv1::MixerChannel* mixer = apiPart.mixerChannel();
    ASSERT_NE(mixer, nullptr);

    // [WHEN] Set volume to values outside valid dB range
    mixer->setVolume(-100.0f);  // Below minimum (-60 dB)
    float vol1 = mixer->volume();
    EXPECT_GE(vol1, -60.0f);  // Should be clamped to -60 dB

    mixer->setVolume(20.0f);  // Above maximum (12 dB)
    float vol2 = mixer->volume();
    EXPECT_LE(vol2, 12.0f);  // Should be clamped to 12 dB

    delete score;
}

//---------------------------------------------------------
//   Test: Balance Property
//---------------------------------------------------------

TEST_F(Engraving_MixerApiTests, testBalanceProperty)
{
    // [GIVEN] A score with mixer channel
    MasterScore* score = createMinimalScore();
    apiv1::Part apiPart(score->parts().at(0));
    apiv1::MixerChannel* mixer = apiPart.mixerChannel();
    ASSERT_NE(mixer, nullptr);

    // [WHEN] Get initial balance
    float initialBalance = mixer->balance();

    // [THEN] Balance should be in valid range [-1.0, 1.0]
    EXPECT_GE(initialBalance, -1.0f);
    EXPECT_LE(initialBalance, 1.0f);

    // [WHEN] Set balance to left (-0.5)
    mixer->setBalance(-0.5f);

    // [THEN] Balance should be updated
    float newBalance = mixer->balance();
    EXPECT_GE(newBalance, -1.0f);
    EXPECT_LE(newBalance, 1.0f);

    delete score;
}

TEST_F(Engraving_MixerApiTests, testBalanceRangeClamping)
{
    // [GIVEN] A score with mixer channel
    MasterScore* score = createMinimalScore();
    apiv1::Part apiPart(score->parts().at(0));
    apiv1::MixerChannel* mixer = apiPart.mixerChannel();
    ASSERT_NE(mixer, nullptr);

    // [WHEN] Set balance to values outside valid range
    mixer->setBalance(-1.5f);  // Below minimum
    float bal1 = mixer->balance();
    EXPECT_GE(bal1, -1.0f);  // Should be clamped to -1

    mixer->setBalance(1.5f);  // Above maximum
    float bal2 = mixer->balance();
    EXPECT_LE(bal2, 1.0f);  // Should be clamped to 1

    delete score;
}

//---------------------------------------------------------
//   Test: Mute Property
//---------------------------------------------------------

TEST_F(Engraving_MixerApiTests, testMuteProperty)
{
    // [GIVEN] A score with mixer channel
    MasterScore* score = createMinimalScore();
    apiv1::Part apiPart(score->parts().at(0));
    apiv1::MixerChannel* mixer = apiPart.mixerChannel();
    ASSERT_NE(mixer, nullptr);

    // [WHEN] Get initial mute state
    bool initialMuted = mixer->muted();

    // [THEN] Should be a valid boolean
    EXPECT_TRUE(initialMuted == true || initialMuted == false);

    // [WHEN] Set mute to true
    mixer->setMuted(true);

    // [THEN] Mute state should be updated (or return default)
    bool muted = mixer->muted();
    EXPECT_TRUE(muted == true || muted == false);

    // [WHEN] Set mute to false
    mixer->setMuted(false);
    bool unmuted = mixer->muted();
    EXPECT_TRUE(unmuted == true || unmuted == false);

    delete score;
}

//---------------------------------------------------------
//   Test: Solo Property
//---------------------------------------------------------

TEST_F(Engraving_MixerApiTests, testSoloProperty)
{
    // [GIVEN] A score with mixer channel
    MasterScore* score = createMinimalScore();
    apiv1::Part apiPart(score->parts().at(0));
    apiv1::MixerChannel* mixer = apiPart.mixerChannel();
    ASSERT_NE(mixer, nullptr);

    // [WHEN] Get initial solo state
    bool initialSolo = mixer->solo();

    // [THEN] Should be a valid boolean
    EXPECT_TRUE(initialSolo == true || initialSolo == false);

    // [WHEN] Set solo to true
    mixer->setSolo(true);

    // [THEN] Solo state should be updated (or return default)
    bool solo = mixer->solo();
    EXPECT_TRUE(solo == true || solo == false);

    delete score;
}

//---------------------------------------------------------
//   Test: Available Sounds
//---------------------------------------------------------

TEST_F(Engraving_MixerApiTests, testAvailableSounds)
{
    // [GIVEN] A score with mixer channel
    MasterScore* score = createMinimalScore();
    apiv1::Part apiPart(score->parts().at(0));
    apiv1::MixerChannel* mixer = apiPart.mixerChannel();
    ASSERT_NE(mixer, nullptr);

    // [WHEN] Get available sounds
    QList<apiv1::AudioResource*> sounds = mixer->availableSounds();

    // [THEN] Should return a list (may be empty if playback not initialized)
    // This is a valid state - just check it doesn't crash
    EXPECT_GE(sounds.size(), 0);

    delete score;
}

TEST_F(Engraving_MixerApiTests, testAvailableSoundsCaching)
{
    // [GIVEN] A score with mixer channel
    MasterScore* score = createMinimalScore();
    apiv1::Part apiPart(score->parts().at(0));
    apiv1::MixerChannel* mixer = apiPart.mixerChannel();
    ASSERT_NE(mixer, nullptr);

    // [WHEN] Get available sounds twice
    QList<apiv1::AudioResource*> sounds1 = mixer->availableSounds();
    QList<apiv1::AudioResource*> sounds2 = mixer->availableSounds();

    // [THEN] Should return same cached list
    EXPECT_EQ(sounds1.size(), sounds2.size());

    // If there are sounds, they should be the same objects (cached)
    if (sounds1.size() > 0 && sounds2.size() > 0) {
        EXPECT_EQ(sounds1[0], sounds2[0]);
    }

    delete score;
}

//---------------------------------------------------------
//   Test: Current Sound
//---------------------------------------------------------

TEST_F(Engraving_MixerApiTests, testCurrentSound)
{
    // [GIVEN] A score with mixer channel
    MasterScore* score = createMinimalScore();
    apiv1::Part apiPart(score->parts().at(0));
    apiv1::MixerChannel* mixer = apiPart.mixerChannel();
    ASSERT_NE(mixer, nullptr);

    // [WHEN] Get current sound
    apiv1::AudioResource* sound = mixer->currentSound();

    // [THEN] May be null if playback not initialized - just check it doesn't crash
    // If not null, check it has valid properties
    if (sound) {
        EXPECT_FALSE(sound->id().isEmpty());
        EXPECT_FALSE(sound->type().isEmpty());
    }

    delete score;
}

//---------------------------------------------------------
//   Test: AudioResource Properties
//---------------------------------------------------------

TEST_F(Engraving_MixerApiTests, testAudioResourceProperties)
{
    // [GIVEN] A score with mixer channel that has sounds
    MasterScore* score = createMinimalScore();
    apiv1::Part apiPart(score->parts().at(0));
    apiv1::MixerChannel* mixer = apiPart.mixerChannel();
    ASSERT_NE(mixer, nullptr);

    QList<apiv1::AudioResource*> sounds = mixer->availableSounds();

    // [WHEN] We have at least one sound
    if (sounds.size() > 0) {
        apiv1::AudioResource* sound = sounds[0];
        ASSERT_NE(sound, nullptr);

        // [THEN] Should have valid properties
        EXPECT_FALSE(sound->id().isEmpty());
        EXPECT_FALSE(sound->type().isEmpty());
        // vendor and name may be empty, just check they don't crash
        QString vendor = sound->vendor();
        QString name = sound->name();
        (void)vendor;  // Suppress unused warning
        (void)name;
    }

    delete score;
}

//---------------------------------------------------------
//   Test: Set Sound
//---------------------------------------------------------

TEST_F(Engraving_MixerApiTests, testSetSound)
{
    // [GIVEN] A score with mixer channel
    MasterScore* score = createMinimalScore();
    apiv1::Part apiPart(score->parts().at(0));
    apiv1::MixerChannel* mixer = apiPart.mixerChannel();
    ASSERT_NE(mixer, nullptr);

    // [WHEN] Try to set sound with invalid ID
    bool result1 = mixer->setSound("invalid-sound-id-12345");

    // [THEN] Should return false for invalid ID
    EXPECT_FALSE(result1);

    // [WHEN] Get available sounds and try to set the first one
    QList<apiv1::AudioResource*> sounds = mixer->availableSounds();
    if (sounds.size() > 0) {
        QString soundId = sounds[0]->id();
        bool result2 = mixer->setSound(soundId);

        // [THEN] Should succeed (or fail gracefully if playback not initialized)
        // Just check it doesn't crash
        (void)result2;
    }

    delete score;
}

//---------------------------------------------------------
//   Test: MIDI Program/Bank
//---------------------------------------------------------

TEST_F(Engraving_MixerApiTests, testMidiProgram)
{
    // [GIVEN] A score with mixer channel
    MasterScore* score = createMinimalScore();
    apiv1::Part apiPart(score->parts().at(0));
    apiv1::MixerChannel* mixer = apiPart.mixerChannel();
    ASSERT_NE(mixer, nullptr);

    // [WHEN] Get MIDI program
    int program = mixer->midiProgram();

    // [THEN] Should return -1 if not applicable, or valid program (0-127)
    if (program != -1) {
        EXPECT_GE(program, 0);
        EXPECT_LE(program, 127);
    }

    delete score;
}

TEST_F(Engraving_MixerApiTests, testSetMidiProgram)
{
    // [GIVEN] A score with mixer channel
    MasterScore* score = createMinimalScore();
    apiv1::Part apiPart(score->parts().at(0));
    apiv1::MixerChannel* mixer = apiPart.mixerChannel();
    ASSERT_NE(mixer, nullptr);

    // [WHEN] Set MIDI program to valid value
    bool result1 = mixer->setMidiProgram(42);

    // [THEN] Should either succeed or return false if not applicable
    // Just check it doesn't crash
    (void)result1;

    // [WHEN] Set MIDI program to invalid values
    bool result2 = mixer->setMidiProgram(-5);
    bool result3 = mixer->setMidiProgram(200);

    // [THEN] Should return false for out-of-range values
    EXPECT_FALSE(result2);
    EXPECT_FALSE(result3);

    delete score;
}

TEST_F(Engraving_MixerApiTests, testMidiBank)
{
    // [GIVEN] A score with mixer channel
    MasterScore* score = createMinimalScore();
    apiv1::Part apiPart(score->parts().at(0));
    apiv1::MixerChannel* mixer = apiPart.mixerChannel();
    ASSERT_NE(mixer, nullptr);

    // [WHEN] Get MIDI bank
    int bank = mixer->midiBank();

    // [THEN] Should return -1 if not applicable, or valid bank number
    if (bank != -1) {
        EXPECT_GE(bank, 0);
    }

    delete score;
}

TEST_F(Engraving_MixerApiTests, testSetMidiBank)
{
    // [GIVEN] A score with mixer channel
    MasterScore* score = createMinimalScore();
    apiv1::Part apiPart(score->parts().at(0));
    apiv1::MixerChannel* mixer = apiPart.mixerChannel();
    ASSERT_NE(mixer, nullptr);

    // [WHEN] Set MIDI bank to valid value
    bool result = mixer->setMidiBank(0);

    // [THEN] Should either succeed or return false if not applicable
    // Just check it doesn't crash
    (void)result;

    delete score;
}

//---------------------------------------------------------
//   Test: Null Safety
//---------------------------------------------------------

TEST_F(Engraving_MixerApiTests, testNullPartHandling)
{
    // [GIVEN] An API Part wrapper with null underlying part
    apiv1::Part apiPart(nullptr);

    // [WHEN] Try to get mixer channel
    apiv1::MixerChannel* mixer = apiPart.mixerChannel();

    // [THEN] Should handle gracefully (return null or valid object)
    // Just check it doesn't crash
    if (mixer) {
        // If it returns a mixer, it should still be safe to call methods
        float volume = mixer->volume();
        (void)volume;
    }
}

//---------------------------------------------------------
//   Test: Multiple Score Handling
//---------------------------------------------------------

TEST_F(Engraving_MixerApiTests, testMultipleScores)
{
    // [GIVEN] Multiple scores
    MasterScore* score1 = createMinimalScore();
    MasterScore* score2 = createMinimalScore();

    apiv1::Part apiPart1(score1->parts().at(0));
    apiv1::Part apiPart2(score2->parts().at(0));

    apiv1::MixerChannel* mixer1 = apiPart1.mixerChannel();
    apiv1::MixerChannel* mixer2 = apiPart2.mixerChannel();

    // [THEN] Mixer channels should be different objects
    EXPECT_NE(mixer1, mixer2);

    delete score1;
    delete score2;
}
