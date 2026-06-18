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
import QtQuick

import Muse.UiComponents

BaseSection {
    id: root

    title: qsTrc("preferences", "Encore")

    // Layout group
    property alias importPageLayout: importPageLayoutBox.checked
    property alias importPageBreaks: importPageBreaksBox.checked
    property alias importStaffSize: importStaffSizeBox.checked

    // Text / content group
    property alias importTempoTextSemantic: importTempoTextSemanticBox.checked
    property alias importUnsupportedArticulationsAsText: importUnsupportedArticulationsAsTextBox.checked

    // Measure correction group
    property alias underfillStrategyModel: underfillStrategyBox.model
    property int currentUnderfillStrategy: 0

    property alias overfillStrategyModel: overfillStrategyBox.model
    property int currentOverfillStrategy: 0

    property alias firstMeasureIsPickup: firstMeasureIsPickupBox.checked

    signal importPageLayoutChangeRequested(bool value)
    signal importPageBreaksChangeRequested(bool value)
    signal importStaffSizeChangeRequested(bool value)
    signal importTempoTextSemanticChangeRequested(bool value)
    signal importUnsupportedArticulationsAsTextChangeRequested(bool value)
    signal underfillStrategyChangeRequested(int value)
    signal overfillStrategyChangeRequested(int value)
    signal firstMeasureIsPickupChangeRequested(bool value)

    CheckBox {
        id: importPageLayoutBox
        width: parent.width

        text: qsTrc("preferences", "Import page layout (margins and size)")

        navigation.name: "EncoreImportPageLayoutBox"
        navigation.panel: root.navigation
        navigation.row: 0

        onClicked: root.importPageLayoutChangeRequested(!checked)
    }

    CheckBox {
        id: importPageBreaksBox
        width: parent.width

        text: qsTrc("preferences", "Import page breaks")

        navigation.name: "EncoreImportPageBreaksBox"
        navigation.panel: root.navigation
        navigation.row: 1

        onClicked: root.importPageBreaksChangeRequested(!checked)
    }

    CheckBox {
        id: importStaffSizeBox
        width: parent.width

        text: qsTrc("preferences", "Import staff size")

        navigation.name: "EncoreImportStaffSizeBox"
        navigation.panel: root.navigation
        navigation.row: 2

        onClicked: root.importStaffSizeChangeRequested(!checked)
    }

    CheckBox {
        id: importTempoTextSemanticBox
        width: parent.width

        text: qsTrc("preferences", "Interpret Italian tempo markings as BPM")

        navigation.name: "EncoreImportTempoTextSemanticBox"
        navigation.panel: root.navigation
        navigation.row: 3

        onClicked: root.importTempoTextSemanticChangeRequested(!checked)
    }

    CheckBox {
        id: importUnsupportedArticulationsAsTextBox
        width: parent.width

        text: qsTrc("preferences", "Import unsupported articulations as text")

        navigation.name: "EncoreImportUnsupportedArticulationsAsTextBox"
        navigation.panel: root.navigation
        navigation.row: 4

        onClicked: root.importUnsupportedArticulationsAsTextChangeRequested(!checked)
    }

    ComboBoxWithTitle {
        id: underfillStrategyBox

        title: qsTrc("preferences", "Short measure handling")
        columnWidth: root.columnWidth

        currentIndex: indexOfValue(root.currentUnderfillStrategy)

        textRole: "title"
        valueRole: "value"

        navigationName: "EncoreUnderfillStrategyBox"
        navigationPanel: root.navigation
        navigationRow: 5

        onValueEdited: function(newIndex, newValue) {
            root.underfillStrategyChangeRequested(newValue)
        }
    }

    ComboBoxWithTitle {
        id: overfillStrategyBox

        title: qsTrc("preferences", "Long measure handling")
        columnWidth: root.columnWidth

        currentIndex: indexOfValue(root.currentOverfillStrategy)

        textRole: "title"
        valueRole: "value"

        navigationName: "EncoreOverfillStrategyBox"
        navigationPanel: root.navigation
        navigationRow: 6

        onValueEdited: function(newIndex, newValue) {
            root.overfillStrategyChangeRequested(newValue)
        }
    }

    CheckBox {
        id: firstMeasureIsPickupBox
        width: parent.width

        text: qsTrc("preferences", "Treat first measure as pickup (anacrusis)")

        navigation.name: "EncoreFirstMeasureIsPickupBox"
        navigation.panel: root.navigation
        navigation.row: 7

        onClicked: root.firstMeasureIsPickupChangeRequested(!checked)
    }
}
