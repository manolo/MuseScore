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
import QtQuick 2.15

import Muse.Ui 1.0
import Muse.UiComponents
import Muse.Audio 1.0

MixerPanelSection {
    id: root

    headerTitle: qsTrc("playback", "Pan")

    Item {
        id: content

        height: balanceKnob.height
        width: root.channelItemWidth

        property string accessibleName: (Boolean(root.needReadChannelName) ? channelItem.title + " " : "") + root.headerTitle
        property bool isEditing: false

        KnobControl {
            id: balanceKnob

            anchors.centerIn: parent
            visible: !content.isEditing

            radius: 18

            from: -100
            to: 100
            value: channelItem.balance
            stepSize: 1
            isBalanceKnob: true

            showValueLabel: true
            valueLabel: Math.round(channelItem.balance)
            handleLengthRatio: 0.5

            navigation.panel: channelItem.panel
            navigation.row: root.navigationRowStart
            navigation.accessible.name: content.accessibleName
            navigation.onActiveChanged: {
                if (navigation.active) {
                    root.navigateControlIndexChanged({row: navigation.row, column: navigation.column})
                }
            }

            onNewValueRequested: function(newValue) {
                channelItem.balance = newValue
            }

            onValueLabelClicked: {
                content.isEditing = true
                balanceInput.forceActiveFocus()
            }
        }

        TextInputField {
            id: balanceInput

            anchors.centerIn: parent
            visible: content.isEditing

            width: 40
            height: 20

            inputField.font.family: ui.theme.bodyFont.family
            inputField.font.pixelSize: ui.theme.bodyFont.pixelSize - 2

            currentText: Math.round(channelItem.balance)

            validator: IntValidator { bottom: -100; top: 100 }

            navigation.panel: channelItem.panel
            navigation.row: root.navigationRowStart + 1

            onTextEditingFinished: function(newTextValue) {
                var newVal = parseInt(newTextValue)
                if (!isNaN(newVal)) {
                    channelItem.balance = Math.max(-100, Math.min(100, newVal))
                }
                content.isEditing = false
            }

            Component.onCompleted: {
                balanceInput.selectAll()
            }
        }

        Connections {
            target: balanceInput
            function onActiveFocusChanged() {
                if (!balanceInput.activeFocus && content.isEditing) {
                    content.isEditing = false
                }
            }
        }
    }
}
