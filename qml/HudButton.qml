/**
 * @file     : HudButton.qml
 * @brief    : Defines a reusable control button for the interface.
 * @details  : Supports standard, primary, and destructive button states.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

import QtQuick
import QtQuick.Controls.Basic
import AeroTwinGCS

Button {
    id: control

    /** Selects the visual importance and risk treatment of this command. */
    property bool primary: false
    property bool danger: false
    property string glyph: ""

    // Centralizes state colors so hover and press states preserve command meaning.
    property color baseColor: danger ? Theme.bad : (primary ? Theme.accent : "#1a2430")

    implicitHeight: primary ? 46 : 38
    implicitWidth: Math.max(120, contentRow.implicitWidth + 36)
    hoverEnabled: true
    padding: 0

    contentItem: Row {
        id: contentRow
        anchors.centerIn: parent
        spacing: 8

        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: control.glyph.length > 0
            text: control.glyph
            font.family: Theme.monoFamily
            font.pixelSize: Theme.fontTitle
            color: control.primary ? "#04141a" : Theme.accent
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            font.family: Theme.uiFamily
            font.pixelSize: control.primary ? Theme.fontTitle : Theme.fontBody
            font.bold: control.primary
            font.letterSpacing: 0.6
            color: control.primary ? "#04141a"
                                   : (control.enabled ? Theme.textPrimary : Theme.textDim)
        }
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: control.primary
               ? (control.down ? Qt.darker(control.baseColor, 1.2)
                               : (control.hovered ? Qt.lighter(control.baseColor, 1.1) : control.baseColor))
               : (control.down ? "#0d3742" : (control.hovered ? "#20303e" : control.baseColor))
        border.width: 1
        border.color: control.primary ? "transparent"
                                      : (control.hovered ? Theme.glassBorderStrong : Theme.glassBorder)
        opacity: control.enabled ? 1.0 : 0.45

        Behavior on color {
            ColorAnimation { duration: Theme.durationFast }
        }
    }
}
