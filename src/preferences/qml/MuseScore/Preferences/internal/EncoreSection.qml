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
import QtQuick.Layouts

import Muse.Ui
import Muse.UiComponents

BaseSection {
    id: root

    // Suppress BaseSection default title; we render it ourselves in the header row.
    title: ""

    // Layout group
    property alias importPageLayout: importPageLayoutBox.checked
    property alias importPageBreaks: importPageBreaksBox.checked
    property alias importSystemLocks: importSystemLocksBox.checked
    property alias importStaffSize: importStaffSizeBox.checked

    // Text / content group
    property alias importTempoTextSemantic: importTempoTextSemanticBox.checked
    property alias importUnsupportedArticulationsAsText: importUnsupportedArticulationsAsTextBox.checked

    // Instrument search
    property alias instrumentSearchModeModel: instrumentSearchModeBox.model
    property int currentInstrumentSearchMode: 0

    // Measure correction group
    property alias underfillStrategyModel: underfillStrategyBox.model
    property int currentUnderfillStrategy: 0

    property alias overfillStrategyModel: overfillStrategyBox.model
    property int currentOverfillStrategy: 0

    property alias firstMeasureIsPickup: firstMeasureIsPickupBox.checked

    signal importPageLayoutChangeRequested(bool value)
    signal importPageBreaksChangeRequested(bool value)
    signal importSystemLocksChangeRequested(bool value)
    signal importStaffSizeChangeRequested(bool value)
    signal importTempoTextSemanticChangeRequested(bool value)
    signal importUnsupportedArticulationsAsTextChangeRequested(bool value)
    signal instrumentSearchModeChangeRequested(int value)
    signal underfillStrategyChangeRequested(int value)
    signal overfillStrategyChangeRequested(int value)
    signal firstMeasureIsPickupChangeRequested(bool value)
    signal resetToDefaultRequested()

    // Header: section title + reset button
    RowLayout {
        width: parent.width
        spacing: 6

        StyledTextLabel {
            text: qsTrc("preferences", "Encore")
            font: ui.theme.bodyBoldFont
            horizontalAlignment: Text.AlignLeft
            Layout.fillWidth: true
        }

        FlatButton {
            Layout.alignment: Qt.AlignVCenter
            width: 24
            height: 24

            icon: IconCode.UNDO
            toolTipTitle: qsTrc("preferences", "Reset to default")

            navigation.name: "EncoreResetToDefaultButton"
            navigation.panel: root.navigation
            navigation.row: 10

            onClicked: root.resetToDefaultRequested()
        }
    }

    // Instrument search — first option
    ComboBoxWithTitle {
        id: instrumentSearchModeBox

        title: qsTrc("preferences", "Instrument search")
        columnWidth: root.columnWidth

        currentIndex: indexOfValue(root.currentInstrumentSearchMode)

        textRole: "title"
        valueRole: "value"

        navigationName: "EncoreInstrumentSearchModeBox"
        navigationPanel: root.navigation
        navigationRow: 0

        onValueEdited: function(newIndex, newValue) {
            root.instrumentSearchModeChangeRequested(newValue)
        }
    }

    // Two-column grid for checkboxes
    GridLayout {
        width: parent.width
        columns: 2
        columnSpacing: 12
        rowSpacing: 0

        CheckBox {
            id: importPageLayoutBox
            Layout.fillWidth: true
            text: qsTrc("preferences", "Import page layout")
            navigation.name: "EncoreImportPageLayoutBox"
            navigation.panel: root.navigation
            navigation.row: 1
            onClicked: root.importPageLayoutChangeRequested(!checked)
        }

        CheckBox {
            id: importPageBreaksBox
            Layout.fillWidth: true
            text: qsTrc("preferences", "Import page breaks")
            navigation.name: "EncoreImportPageBreaksBox"
            navigation.panel: root.navigation
            navigation.row: 2
            onClicked: root.importPageBreaksChangeRequested(!checked)
        }

        CheckBox {
            id: importSystemLocksBox
            Layout.fillWidth: true
            text: qsTrc("preferences", "Lock systems to Encore layout")
            navigation.name: "EncoreImportSystemLocksBox"
            navigation.panel: root.navigation
            navigation.row: 3
            onClicked: root.importSystemLocksChangeRequested(!checked)
        }

        CheckBox {
            id: importStaffSizeBox
            Layout.fillWidth: true
            text: qsTrc("preferences", "Import staff size")
            navigation.name: "EncoreImportStaffSizeBox"
            navigation.panel: root.navigation
            navigation.row: 4
            onClicked: root.importStaffSizeChangeRequested(!checked)
        }

        CheckBox {
            id: importTempoTextSemanticBox
            Layout.fillWidth: true
            text: qsTrc("preferences", "Interpret Italian tempo as BPM")
            navigation.name: "EncoreImportTempoTextSemanticBox"
            navigation.panel: root.navigation
            navigation.row: 5
            onClicked: root.importTempoTextSemanticChangeRequested(!checked)
        }

        CheckBox {
            id: importUnsupportedArticulationsAsTextBox
            Layout.fillWidth: true
            text: qsTrc("preferences", "Articulations as text")
            navigation.name: "EncoreImportUnsupportedArticulationsAsTextBox"
            navigation.panel: root.navigation
            navigation.row: 6
            onClicked: root.importUnsupportedArticulationsAsTextChangeRequested(!checked)
        }
    }

    // First measure — before measure correction dropdowns
    CheckBox {
        id: firstMeasureIsPickupBox
        width: parent.width

        text: qsTrc("preferences", "Treat first measure as pickup (anacrusis)")

        navigation.name: "EncoreFirstMeasureIsPickupBox"
        navigation.panel: root.navigation
        navigation.row: 7

        onClicked: root.firstMeasureIsPickupChangeRequested(!checked)
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
        navigationRow: 8

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
        navigationRow: 9

        onValueEdited: function(newIndex, newValue) {
            root.overfillStrategyChangeRequested(newValue)
        }
    }
}
