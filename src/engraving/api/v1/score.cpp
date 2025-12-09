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

#include "score.h"

#include "compat/midi/compatmidirender.h"
#include "io/file.h"
#include "dom/excerpt.h"
#include "dom/factory.h"
#include "dom/instrtemplate.h"
#include "dom/measure.h"
#include "dom/part.h"
#include "dom/score.h"
#include "dom/masterscore.h"
#include "dom/segment.h"
#include "dom/staff.h"
#include "dom/stafftype.h"
#include "dom/text.h"
#include "editing/editpart.h"
#include "editing/editsystemlocks.h"
#include "editing/undo.h"
#include "types/typesconv.h"

#include "notation/imasternotation.h"

// api
#include "apistructs.h"
#include "cursor.h"
#include "elements.h"
#include "excerpt.h"
#include "apitypes.h"

using namespace mu::engraving::apiv1;

Cursor* Score::newCursor()
{
    return new Cursor(score());
}

void Score::addText(const QString& type, const QString& txt)
{
    static const QMetaEnum meta = QMetaEnum::fromType<enums::TextStyleType>();

    mu::engraving::TextStyleType tid;
    const std::string key = type.toStdString();
    bool ok = false;
    int val = meta.keyToValue(key.c_str(), &ok);
    if (ok) {
        tid = static_cast<mu::engraving::TextStyleType>(val);
    } else {
        LOGE() << "Please use engraving::TextStyleType enum, the use of Xml tags is deprecated.";
        AsciiStringView t(key);
        tid = mu::engraving::TConv::fromXml(t, mu::engraving::TextStyleType::DEFAULT);
    }

    mu::engraving::MeasureBase* mb = score()->first();
    if (!mb || !mb->isVBox()) {
        score()->insertBox(ElementType::VBOX, mb);
        mb = score()->first();
    }

    mu::engraving::Text* text = mu::engraving::Factory::createText(mb, tid);
    text->setParent(mb);
    text->setXmlText(txt);
    score()->undoAddElement(text);
}

//---------------------------------------------------------
//   defaultInstrTemplate
//---------------------------------------------------------

static const mu::engraving::InstrumentTemplate* defaultInstrTemplate()
{
    static mu::engraving::InstrumentTemplate defaultInstrument;
    if (defaultInstrument.channel.empty()) {
        mu::engraving::InstrChannel a;
        a.setChorus(0);
        a.setReverb(0);
        a.setName(muse::String::fromUtf8(mu::engraving::InstrChannel::DEFAULT_NAME));
        a.setBank(0);
        a.setVolume(90);
        a.setPan(0);
        defaultInstrument.channel.push_back(a);
    }
    return &defaultInstrument;
}

//---------------------------------------------------------
//   instrTemplateFromName
//---------------------------------------------------------

const mu::engraving::InstrumentTemplate* Score::instrTemplateFromName(const QString& name)
{
    const InstrumentTemplate* t = searchTemplate(name);
    if (!t) {
        LOGW("<%s> not found", qPrintable(name));
        t = defaultInstrTemplate();
    }
    return t;
}

mu::notation::INotationPtr Score::notation() const
{
    return context()->currentNotation();
}

mu::notation::INotationUndoStackPtr Score::undoStack() const
{
    return notation() ? notation()->undoStack() : nullptr;
}

//---------------------------------------------------------
//   Score::appendPart
//---------------------------------------------------------

void Score::appendPart(const QString& instrumentId)
{
    const InstrumentTemplate* t = searchTemplate(instrumentId);

    if (!t) {
        LOGW("appendPart: <%s> not found", qPrintable(instrumentId));
        t = defaultInstrTemplate();
    }

    score()->appendPart(t);
}

//---------------------------------------------------------
//   Score::appendPartByMusicXmlId
//---------------------------------------------------------

void Score::appendPartByMusicXmlId(const QString& instrumentMusicXmlId)
{
    const InstrumentTemplate* t = searchTemplateForMusicXmlId(instrumentMusicXmlId);

    if (!t) {
        LOGW("appendPart: <%s> not found", qPrintable(instrumentMusicXmlId));
        t = defaultInstrTemplate();
    }

    score()->appendPart(t);
}

/** APIDOC
* Replaces the instrument for a given part with a new instrument.
* This changes the instrument definition including its name, clef, and sound.
* @method
* @param {Part} part - The Part object whose instrument should be replaced.
* @param {string} instrumentId - ID of the new instrument from instruments.xml.
*/
void Score::replaceInstrument(apiv1::Part* part, const QString& instrumentId)
{
    if (!part) {
        LOGW("replaceInstrument: part is null");
        return;
    }

    const InstrumentTemplate* t = searchTemplate(instrumentId);
    if (!t) {
        LOGW("replaceInstrument: <%s> not found", qPrintable(instrumentId));
        return;
    }

    mu::engraving::Part* domPart = part->part();
    mu::engraving::Instrument newInstrument = mu::engraving::Instrument::fromTemplate(t);
    String newPartName = t->trackName;

    score()->undo(new ChangePart(domPart, new mu::engraving::Instrument(newInstrument), newPartName));
}

//---------------------------------------------------------
//   Score::firstSegment
//---------------------------------------------------------

Segment* Score::firstSegment(int segmentType)
{
    return wrap<Segment>(score()->firstSegment(engraving::SegmentType(segmentType)), Ownership::SCORE);
}

Measure* Score::tick2measure(Fraction* f)
{
    const mu::engraving::Fraction tick = f->fraction();
    if (!tick.isValid() || tick.negative()) {
        return nullptr;
    }
    return wrap<Measure>(score()->tick2measure(tick));
}

Segment* Score::findSegmentAtTick(int segmentTypes, Fraction* f)
{
    const mu::engraving::Fraction tick = f->fraction();
    if (!tick.isValid() || tick.negative()) {
        return nullptr;
    }
    mu::engraving::Measure* measure = score()->tick2measure(tick);
    mu::engraving::Segment* segment = measure->findSegment(engraving::SegmentType(segmentTypes), tick);
    return segment ? wrap<Segment>(segment, Ownership::SCORE) : nullptr;
}

//---------------------------------------------------------
//   Score::lastSegment
//---------------------------------------------------------

Segment* Score::lastSegment()
{
    return wrap<Segment>(score()->lastSegment(), Ownership::SCORE);
}

//---------------------------------------------------------
//   Score::firstMeasure
//---------------------------------------------------------

Measure* Score::firstMeasure()
{
    return wrap<Measure>(score()->firstMeasure(), Ownership::SCORE);
}

//---------------------------------------------------------
//   Score::firstMeasureMM
//---------------------------------------------------------

Measure* Score::firstMeasureMM()
{
    return wrap<Measure>(score()->firstMeasureMM(), Ownership::SCORE);
}

//---------------------------------------------------------
//   Score::lastMeasure
//---------------------------------------------------------

Measure* Score::lastMeasure()
{
    return wrap<Measure>(score()->lastMeasure(), Ownership::SCORE);
}

//---------------------------------------------------------
//   Score::firstMeasureMM
//---------------------------------------------------------

Measure* Score::lastMeasureMM()
{
    return wrap<Measure>(score()->lastMeasureMM(), Ownership::SCORE);
}

QString Score::name() const
{
    return score()->masterScore()->name();
}

void Score::setName(const QString& /*name*/)
{
    NOT_IMPLEMENTED;
}

void Score::createPlayEvents()
{
    mu::engraving::CompatMidiRender::createPlayEvents(score());
}

QQmlListProperty<Staff> Score::staves() const
{
    return wrapContainerProperty<Staff>(this, score()->staves());
}

QQmlListProperty<Part> Score::parts() const
{
    return wrapContainerProperty<Part>(this, score()->parts());
}

QQmlListProperty<Excerpt> Score::excerpts() const
{
    return wrapExcerptsContainerProperty<Excerpt>(this, score()->masterScore()->excerpts());
}

QQmlListProperty<Page> Score::pages() const
{
    return wrapContainerProperty<Page>(this, score()->pages());
}

QQmlListProperty<System> Score::systems() const
{
    return wrapContainerProperty<System>(this, score()->systems());
}

bool Score::hasLyrics() const
{
    return score()->hasLyrics();
}

int Score::lyricCount() const
{
    return score()->lyricCount();
}

QQmlListProperty<Lyrics> Score::lyrics() const
{
    static std::vector<engraving::Lyrics*> list;
    list = score()->lyrics();
    return wrapContainerProperty<Lyrics>(this, list);
}

QString Score::extractLyrics() const
{
    return score()->extractLyrics();
}

QQmlListProperty<Spanner> Score::spanners()
{
    static std::vector<mu::engraving::Spanner*> spannerList;
    spannerList = score()->spannerList();
    return wrapContainerProperty<Spanner>(this, spannerList);
}

//---------------------------------------------------------
//   Score::startCmd
//---------------------------------------------------------

void Score::startCmd(const QString& qActionName)
{
    IF_ASSERT_FAILED(undoStack()) {
        return;
    }

    muse::TranslatableString actionName = qActionName.isEmpty()
                                          ? TranslatableString("undoableAction", "Plugin edit")
                                          : TranslatableString::untranslatable(qActionName);

    undoStack()->prepareChanges(actionName);
    // Lock the undo stack, so that all changes made by the plugin,
    // including PluginAPI::cmd(), are committed as a single command.
    undoStack()->lock();
}

void Score::endCmd(bool rollback)
{
    IF_ASSERT_FAILED(undoStack()) {
        return;
    }

    undoStack()->unlock();

    if (rollback) {
        undoStack()->rollbackChanges();
    } else {
        undoStack()->commitChanges();
    }

    notation()->notationChanged().notify();
}

void Score::doLayout(Fraction* startTick, Fraction* endTick)
{
    score()->doLayoutRange(startTick->fraction(), endTick->fraction());
}

void Score::addRemoveSystemLocks(int interval, bool lock)
{
    EditSystemLocks::addRemoveSystemLocks(score(), interval, lock);
}

void Score::makeIntoSystem(apiv1::MeasureBase* first, apiv1::MeasureBase* last)
{
    EditSystemLocks::makeIntoSystem(score(), first->measureBase(), last->measureBase());
}

void Score::showElementInScore(apiv1::EngravingItem* wrappedElement, int staffIdx)
{
    if (!wrappedElement->element()) {
        return;
    }
    notation()->interaction()->showItem(wrappedElement->element(), staffIdx);
}

//---------------------------------------------------------
//   Score::loadStyle
//---------------------------------------------------------

bool Score::loadStyle(const QString& filePath, bool allowAnyVersion)
{
    muse::io::File styleFile(filePath);
    if (!styleFile.open(muse::io::IODevice::ReadOnly)) {
        LOGW("loadStyle: cannot open <%s>", qPrintable(filePath));
        return false;
    }

    return score()->loadStyle(styleFile, allowAnyVersion);
}

//---------------------------------------------------------
//   Score::addLinkedStaff
//---------------------------------------------------------

bool Score::addLinkedStaff(apiv1::Staff* staffWrapper, const QString& staffTypeId)
{
    if (!staffWrapper) {
        LOGW("addLinkedStaff: staff is null");
        return false;
    }

    mu::engraving::Staff* sourceStaff = staffWrapper->staff();
    if (!sourceStaff) {
        LOGW("addLinkedStaff: source staff is null");
        return false;
    }

    mu::engraving::Part* part = sourceStaff->part();
    if (!part) {
        LOGW("addLinkedStaff: part is null");
        return false;
    }

    // Get the staff type preset
    const mu::engraving::StaffType* staffTypePreset = mu::engraving::StaffType::presetFromXmlName(staffTypeId);
    if (!staffTypePreset) {
        LOGW("addLinkedStaff: staff type <%s> not found", qPrintable(staffTypeId));
        return false;
    }

    // Create a copy of the staff type to modify
    mu::engraving::StaffType staffType = *staffTypePreset;

    // For tablature, use circled frets for half notes
    if (staffType.group() == mu::engraving::StaffGroup::TAB) {
        staffType.setMinimStyle(mu::engraving::TablatureMinimStyle::CIRCLED);
    }

    // Create the new linked staff
    mu::engraving::Staff* linkedStaff = mu::engraving::Factory::createStaff(part);
    linkedStaff->setScore(score());
    linkedStaff->setPart(part);

    // Set the staff type
    linkedStaff->setStaffType(mu::engraving::Fraction(0, 1), staffType);

    // Calculate local index within the part (part-relative, not global)
    // Insert after the source staff within the part
    staff_idx_t sourceLocalIdx = sourceStaff->idx() - part->staff(0)->idx();
    staff_idx_t insertLocalIdx = sourceLocalIdx + 1;

    // Insert the staff
    score()->undoInsertStaff(linkedStaff, insertLocalIdx, false);

    // Clone/link the content
    mu::engraving::Excerpt::cloneStaff(sourceStaff, linkedStaff);

    return true;
}

bool Score::resetExcerpt(apiv1::Excerpt* excerptWrapper)
{
    if (!excerptWrapper) {
        LOGW("resetExcerpt: excerpt is null");
        return false;
    }

    mu::engraving::Excerpt* targetExcerpt = excerptWrapper->excerpt();
    if (!targetExcerpt) {
        LOGW("resetExcerpt: underlying excerpt is null");
        return false;
    }

    // Get the master notation from context
    auto masterNotation = context()->currentMasterNotation();
    if (!masterNotation) {
        LOGW("resetExcerpt: master notation is null");
        return false;
    }

    // Find the matching IExcerptNotationPtr by comparing excerpt scores
    const auto& excerptNotations = masterNotation->excerpts();
    mu::notation::IExcerptNotationPtr matchingExcerpt;

    mu::engraving::Score* targetScore = targetExcerpt->excerptScore();
    for (const auto& excerptNotation : excerptNotations) {
        if (excerptNotation->notation() && excerptNotation->notation()->elements()) {
            if (excerptNotation->notation()->elements()->msScore() == targetScore) {
                matchingExcerpt = excerptNotation;
                break;
            }
        }
    }

    if (!matchingExcerpt) {
        LOGW("resetExcerpt: could not find matching excerpt notation");
        return false;
    }

    masterNotation->resetExcerpt(matchingExcerpt);
    return true;
}

Excerpt* Score::createExcerptFromPart(Part* partWrapper, const QString& name)
{
    if (!partWrapper) {
        LOGW("createExcerptFromPart: part is null");
        return nullptr;
    }

    mu::engraving::Part* part = partWrapper->part();
    if (!part) {
        LOGW("createExcerptFromPart: underlying part is null");
        return nullptr;
    }

    mu::engraving::MasterScore* ms = score()->masterScore();
    if (!ms) {
        LOGW("createExcerptFromPart: master score is null");
        return nullptr;
    }

    // Check if an excerpt already exists for this part
    for (mu::engraving::Excerpt* ex : ms->excerpts()) {
        if (ex->containsPart(part)) {
            LOGW("createExcerptFromPart: excerpt already exists for this part");
            return nullptr;
        }
    }

    // Create the excerpt using the internal API
    std::vector<mu::engraving::Part*> parts = { part };
    std::vector<mu::engraving::Excerpt*> newExcerpts = mu::engraving::Excerpt::createExcerptsFromParts(parts, ms);

    if (newExcerpts.empty()) {
        LOGW("createExcerptFromPart: failed to create excerpt");
        return nullptr;
    }

    mu::engraving::Excerpt* newExcerpt = newExcerpts.front();

    // Set custom name if provided
    if (!name.isEmpty()) {
        newExcerpt->setName(name);
    }

    // Initialize and add the excerpt to the master score
    ms->initAndAddExcerpt(newExcerpt, false);
    ms->setExcerptsChanged(true);

    // Return wrapped excerpt
    return excerptWrap(newExcerpt);
}

Excerpt* Score::duplicateExcerpt(Excerpt* excerptWrapper, const QString& name)
{
    if (!excerptWrapper) {
        LOGW("duplicateExcerpt: excerpt is null");
        return nullptr;
    }

    mu::engraving::Excerpt* sourceExcerpt = excerptWrapper->excerpt();
    if (!sourceExcerpt) {
        LOGW("duplicateExcerpt: underlying excerpt is null");
        return nullptr;
    }

    mu::engraving::MasterScore* ms = score()->masterScore();
    if (!ms) {
        LOGW("duplicateExcerpt: master score is null");
        return nullptr;
    }

    // Create a copy of the excerpt using copy constructor
    mu::engraving::Excerpt* newExcerpt = new mu::engraving::Excerpt(*sourceExcerpt);
    newExcerpt->markAsCustom();

    // Set the new name
    if (!name.isEmpty()) {
        newExcerpt->setName(name);
    }

    // Initialize and add the excerpt to the master score
    ms->initAndAddExcerpt(newExcerpt, false);
    ms->setExcerptsChanged(true);

    // Return wrapped excerpt
    return excerptWrap(newExcerpt);
}

bool Score::openExcerpt(Excerpt* excerptWrapper, bool setAsCurrent)
{
    if (!excerptWrapper) {
        LOGW("openExcerpt: excerpt is null");
        return false;
    }

    mu::engraving::Excerpt* targetExcerpt = excerptWrapper->excerpt();
    if (!targetExcerpt) {
        LOGW("openExcerpt: underlying excerpt is null");
        return false;
    }

    // Get the master notation from context
    auto masterNotation = context()->currentMasterNotation();
    if (!masterNotation) {
        LOGW("openExcerpt: master notation is null");
        return false;
    }

    // Find the matching IExcerptNotationPtr by comparing excerpt scores
    const auto& excerptNotations = masterNotation->excerpts();
    mu::notation::IExcerptNotationPtr matchingExcerpt;

    mu::engraving::Score* targetScore = targetExcerpt->excerptScore();
    for (const auto& excerptNotation : excerptNotations) {
        if (excerptNotation->notation() && excerptNotation->notation()->elements()) {
            if (excerptNotation->notation()->elements()->msScore() == targetScore) {
                matchingExcerpt = excerptNotation;
                break;
            }
        }
    }

    if (!matchingExcerpt) {
        LOGW("openExcerpt: could not find matching excerpt notation");
        return false;
    }

    // Open the excerpt in tabs
    masterNotation->setExcerptIsOpen(matchingExcerpt->notation(), true);

    // Optionally set as current view
    if (setAsCurrent && matchingExcerpt->notation()) {
        context()->setCurrentNotation(matchingExcerpt->notation());
    }

    return true;
}

void Score::resetTextStyleOverrides()
{
    score()->cmdResetTextStyleOverrides();
}
