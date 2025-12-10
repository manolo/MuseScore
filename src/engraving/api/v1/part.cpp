/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited
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

#include "part.h"

#include "engraving/dom/harppedaldiagram.h"

// api
#include "apistructs.h"
#include "elements.h"
#include "instrument.h"
#include "mixer.h"

#include "log.h"

using namespace mu::engraving::apiv1;

InstrumentListProperty::InstrumentListProperty(Part* p)
    : QQmlListProperty<Instrument>(p, p, &count, &at) {}

//---------------------------------------------------------
//   InstrumentListProperty::count
//---------------------------------------------------------

qsizetype InstrumentListProperty::count(QQmlListProperty<Instrument>* l)
{
    return static_cast<qsizetype>(static_cast<Part*>(l->data)->part()->instruments().size());
}

//---------------------------------------------------------
//   InstrumentListProperty::at
//---------------------------------------------------------

Instrument* InstrumentListProperty::at(QQmlListProperty<Instrument>* l, qsizetype i)
{
    Part* part = static_cast<Part*>(l->data);
    const mu::engraving::InstrumentList& il = part->part()->instruments();

    if (i < 0 || i >= int(il.size())) {
        return nullptr;
    }

    mu::engraving::Instrument* instr = std::next(il.begin(), i)->second;

    return customWrap<Instrument>(instr, part->part());
}

//---------------------------------------------------------
//   Part::instruments
//---------------------------------------------------------

InstrumentListProperty Part::instruments()
{
    return InstrumentListProperty(this);
}

//---------------------------------------------------------
//   Part::instrumentAtTick
//---------------------------------------------------------

Instrument* Part::instrumentAtTick(int tick)
{
    return customWrap<Instrument>(part()->instrument(mu::engraving::Fraction::fromTicks(tick)), part());
}

Instrument* Part::instrumentAtTick(Fraction* tick)
{
    return customWrap<Instrument>(part()->instrument(tick->fraction()), part());
}

QQmlListProperty<Staff> Part::staves()
{
    return wrapContainerProperty<Staff>(this, part()->staves());
}

QString Part::longNameAtTick(Fraction* tick)
{
    return part()->longName(tick->fraction());
}

QString Part::shortNameAtTick(Fraction* tick)
{
    return part()->shortName(tick->fraction());
}

QString Part::instrumentNameAtTick(Fraction* tick)
{
    return part()->instrumentName(tick->fraction());
}

QString Part::instrumentIdAtTick(Fraction* tick)
{
    return part()->instrumentId(tick->fraction());
}

EngravingItem* Part::currentHarpDiagramAtTick(Fraction* tick)
{
    return wrap(part()->currentHarpDiagram(tick->fraction()));
}

EngravingItem* Part::nextHarpDiagramFromTick(Fraction* tick)
{
    return wrap(part()->nextHarpDiagram(tick->fraction()));
}

EngravingItem* Part::prevHarpDiagramFromTick(Fraction* tick)
{
    return wrap(part()->prevHarpDiagram(tick->fraction()));
}

Fraction* Part::tickOfCurrentHarpDiagram(Fraction* tick)
{
    return wrap(part()->currentHarpDiagramTick(tick->fraction()));
}

//---------------------------------------------------------
//   Part::mixerChannel
//---------------------------------------------------------

MixerChannel* Part::mixerChannel()
{
    // Check local cache first (for same Part wrapper instance)
    if (m_mixerChannel) {
        LOGD() << "Returning locally cached MixerChannel: " << m_mixerChannel;
        return m_mixerChannel;
    }

    // Check if part is valid
    if (!part()) {
        LOGW() << "Part is null, cannot create MixerChannel";
        return nullptr;
    }

    // Use partId as the cache key (instrumentId can vary, causing cache misses)
    mu::engraving::ID partId = part()->id();

    // Create the InstrumentTrackId for MixerChannel constructor (still needs full trackId)
    mu::engraving::InstrumentTrackId trackId;
    trackId.partId = partId;

    // Use the first instrument's ID (parts typically have one primary instrument)
    auto instrument = part()->instrument();
    if (instrument) {
        trackId.instrumentId = instrument->id();
    }

    // Check global cache using only partId (persists across Part wrapper lifecycles)
    if (MixerChannel::s_mixerChannelCache.contains(partId)) {
        m_mixerChannel = MixerChannel::s_mixerChannelCache[partId];
        LOGD() << "Reusing globally cached MixerChannel: " << m_mixerChannel << " for part: " << part()->partName().toStdString();
        return m_mixerChannel;
    }

    // Create new MixerChannel and add to both caches
    LOGD() << "Creating new MixerChannel for part: " << part()->partName().toStdString();
    m_mixerChannel = new MixerChannel(trackId, nullptr);  // nullptr parent - owned by global cache
    MixerChannel::s_mixerChannelCache[partId] = m_mixerChannel;
    LOGD() << "Created and globally cached MixerChannel: " << m_mixerChannel;

    return m_mixerChannel;
}
