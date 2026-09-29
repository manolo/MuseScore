/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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

#include <variant>
#include <vector>

#include "mpe/events.h"
#include "mpe/tests/utils/articulationutils.h"

#include "vst/vsttypes.h"
#include "vst/internal/synth/vstsequencer.h"

using namespace muse;
using namespace muse::mpe;
using namespace muse::mpe::tests;
using namespace muse::vst;

namespace {
// A plain articulation pattern segment (no arrangement/pitch/dynamic modification), enough for the
// NoteEvent constructor to build its curves. Reused for every articulation in these tests.
ArticulationPattern standardScope()
{
    ArticulationPatternSegment seg;
    seg.arrangementPattern = createArrangementPattern(HUNDRED_PERCENT /*durationFactor*/, 0 /*timestampOffset*/);
    seg.pitchPattern = createSimplePitchPattern(0);
    seg.expressionPattern = createSimpleExpressionPattern(dynamicLevelFromType(DynamicType::Natural));

    ArticulationPattern scope;
    scope.emplace(0, seg);
    return scope;
}

struct Artic {
    ArticulationType type;
    timestamp_t timestamp;      // the articulation range start (== the note onset for a span start)
    duration_t overallDuration; // the articulation range length (a slur may span several notes)
};

// Build a note carrying a set of articulations, each with its own range meta.
NoteEvent makeNote(timestamp_t noteTimestamp, duration_t noteDuration, const std::vector<Artic>& artics)
{
    ArticulationMap map;
    for (const Artic& a : artics) {
        ArticulationMeta meta;
        meta.type = a.type;
        meta.pattern = standardScope();
        meta.timestamp = a.timestamp;
        meta.overallDuration = a.overallDuration;
        map.emplace(a.type, ArticulationAppliedData(std::move(meta), 0, HUNDRED_PERCENT));
    }
    map.preCalculateAverageData();

    return NoteEvent(noteTimestamp, noteDuration, 0 /*voiceIdx*/, 0 /*staffIdx*/,
                     pitchLevel(PitchClass::A, 4), dynamicLevelFromType(DynamicType::Natural),
                     std::move(map), 0);
}

struct Emitted {
    int64_t timestamp; // msecs (the sequencer's event-map key)
    int type;          // Steinberg::Vst::Event::EventTypes
    int pitch;         // keyswitch note (or the played note)
};

// Drive the sequencer over the whole timeline and collect every VST note-on/note-off it emitted.
std::vector<Emitted> collect(VstSequencer& seq)
{
    seq.setActive(true);
    seq.setPlaybackPosition(0);
    const auto out = seq.movePlaybackForward(1000000);

    std::vector<Emitted> result;
    for (const auto& seqPair : out) {
        for (const auto& variantEvent : seqPair.second) {
            if (!std::holds_alternative<VstEvent>(variantEvent)) {
                continue;
            }
            const VstEvent& e = std::get<VstEvent>(variantEvent);
            if (e.type == VstEvent::kNoteOnEvent) {
                result.push_back({ seqPair.first, e.type, e.noteOn.pitch });
            } else if (e.type == VstEvent::kNoteOffEvent) {
                result.push_back({ seqPair.first, e.type, e.noteOff.pitch });
            }
        }
    }
    return result;
}

bool has(const std::vector<Emitted>& events, int64_t timestamp, int type, int pitch)
{
    for (const Emitted& e : events) {
        if (e.timestamp == timestamp && e.type == type && e.pitch == pitch) {
            return true;
        }
    }
    return false;
}

// No event ever touches the given keyswitch pitch (on or off, at any time).
bool none(const std::vector<Emitted>& events, int pitch)
{
    for (const Emitted& e : events) {
        if (e.pitch == pitch) {
            return false;
        }
    }
    return true;
}

VstKeyswitchProfile makeProfile()
{
    VstKeyswitchProfile profile;
    profile.keyswitches[ArticulationType::Standard] = 0;
    profile.keyswitches[ArticulationType::Pizzicato] = 1;
    profile.keyswitches[ArticulationType::Tremolo16th] = 8;
    profile.keyswitches[ArticulationType::Legato] = 12;
    return profile;
}

constexpr int NOTE_ON = VstEvent::kNoteOnEvent;
constexpr int NOTE_OFF = VstEvent::kNoteOffEvent;
constexpr int PIZZICATO_KS = 1;
constexpr int TREMOLO_KS = 8;
constexpr int LEGATO_KS = 12;
}

// A tremolo under a slur must forward BOTH the tremolo keyswitch (the primary, unchanged) and the
// Legato modifier as a keyswitch span: pressed at the note onset, released at the range end. Legato
// (a ranged modifier) must never hijack the primary keyswitch.
TEST(VstSequencerKeyswitchTest, SlurredTremoloForwardsTremoloPlusLegatoSpan)
{
    VstSequencer seq;
    seq.init({}, false, makeProfile());

    PlaybackData data;
    data.originEvents[0].push_back(makeNote(0, 500, {
        { ArticulationType::Tremolo16th, 0, 500 },
        { ArticulationType::Legato, 0, 500 },
    }));
    seq.load(data);

    const auto events = collect(seq);
    EXPECT_TRUE(has(events, 0, NOTE_ON, TREMOLO_KS));    // tremolo keyswitch still selected
    EXPECT_TRUE(has(events, 0, NOTE_ON, LEGATO_KS));     // legato pressed at the note onset
    EXPECT_TRUE(has(events, 500, NOTE_OFF, LEGATO_KS));  // legato released at the range end
}

// The Legato keyswitch is pressed at EVERY note the slur covers, not once at the range start, so
// starting playback partway through the slur still engages it (a single start event would be
// dropped by a seek past it).
TEST(VstSequencerKeyswitchTest, LegatoIsPressedAtEveryCoveredNote)
{
    VstSequencer seq;
    seq.init({}, false, makeProfile());

    PlaybackData data;
    // Two tremolo notes under one slur: the Legato range (0..1000) covers both; each tremolo note
    // is its own span (distinct meta.timestamp), so both re-send the tremolo keyswitch.
    data.originEvents[0].push_back(makeNote(0, 500, {
        { ArticulationType::Tremolo16th, 0, 500 },
        { ArticulationType::Legato, 0, 1000 },
    }));
    data.originEvents[500].push_back(makeNote(500, 500, {
        { ArticulationType::Tremolo16th, 500, 500 },
        { ArticulationType::Legato, 0, 1000 },
    }));
    seq.load(data);

    const auto events = collect(seq);
    EXPECT_TRUE(has(events, 0, NOTE_ON, LEGATO_KS));    // pressed at the first covered note
    EXPECT_TRUE(has(events, 500, NOTE_ON, LEGATO_KS));  // and re-pressed at the next covered note
}

// A ranged modifier never wins the single primary keyswitch: a slurred pizzicato still selects the
// pizzicato timbre, with Legato forwarded alongside as its own span.
TEST(VstSequencerKeyswitchTest, RangedModifierDoesNotHijackTheTimbreKeyswitch)
{
    VstSequencer seq;
    seq.init({}, false, makeProfile());

    PlaybackData data;
    data.originEvents[0].push_back(makeNote(0, 500, {
        { ArticulationType::Pizzicato, 0, 500 },
        { ArticulationType::Legato, 0, 500 },
    }));
    seq.load(data);

    const auto events = collect(seq);
    EXPECT_TRUE(has(events, 0, NOTE_ON, PIZZICATO_KS)); // the timbre keyswitch is still selected
    EXPECT_TRUE(has(events, 0, NOTE_ON, LEGATO_KS));    // legato forwarded alongside, not instead
    EXPECT_TRUE(none(events, TREMOLO_KS));              // and no tremolo keyswitch leaks in
}

// A standalone tremolo (no slur) carries no Legato, so no legato keyswitch is emitted at all.
TEST(VstSequencerKeyswitchTest, StandaloneTremoloEmitsNoLegato)
{
    VstSequencer seq;
    seq.init({}, false, makeProfile());

    PlaybackData data;
    data.originEvents[0].push_back(makeNote(0, 500, {
        { ArticulationType::Tremolo16th, 0, 500 },
    }));
    seq.load(data);

    const auto events = collect(seq);
    EXPECT_TRUE(has(events, 0, NOTE_ON, TREMOLO_KS)); // the tremolo keyswitch is selected
    EXPECT_TRUE(none(events, LEGATO_KS));             // but nothing legato, since there is no slur
}
