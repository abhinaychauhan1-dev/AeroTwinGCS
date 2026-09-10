/**
 * @file     : StatusChip.qml
 * @brief    : Defines a compact labeled status indicator.
 * @details  : Displays health and state information with color emphasis.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

import QtQuick
import AeroTwinGCS

/*!
    Compact status pill used across the header bar.
    Optionally pulses (MAVLink heartbeat / link health).
*/
Item {
    id: chip

    /** Caption and value supplied by the owning status rail. */
    property string label: ""
    property string value: ""
    /** Semantic color and animation state supplied by telemetry health. */
    property color accentColor: Theme.accent
    property bool pulsing: false
    property bool emphasized: false

    implicitHeight: 40
    implicitWidth: row.implicitWidth + 30

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: chip.emphasized ? Qt.rgba(chip.accentColor.r, chip.accentColor.g, chip.accentColor.b, 0.14)
                               : "#1412202B"
        border.width: 1
        border.color: chip.emphasized
                      ? Qt.rgba(chip.accentColor.r, chip.accentColor.g, chip.accentColor.b, 0.55)
                      : Theme.glassBorder

        // Smoothly reflects changes such as link degradation or arming.
        Behavior on color { ColorAnimation { duration: Theme.durationBase } }
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 8

        Rectangle {
            id: dot
            anchors.verticalCenter: parent.verticalCenter
            width: 10
            height: 10
            radius: 5
            color: chip.accentColor

            SequentialAnimation on opacity {
                running: chip.pulsing
                loops: Animation.Infinite
                alwaysRunToEnd: true
                NumberAnimation { to: 0.25; duration: 480; easing.type: Easing.InOutQuad }
                NumberAnimation { to: 1.0; duration: 480; easing.type: Easing.InOutQuad }
            }
            onVisibleChanged: if (!chip.pulsing) opacity = 1.0
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: chip.label.length > 0
            text: chip.label
            color: Theme.textDim
            font.family: Theme.uiFamily
            font.pixelSize: Theme.fontLabel
            font.bold: true
            font.letterSpacing: 1.2
            font.capitalization: Font.AllUppercase
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: chip.value
            color: chip.emphasized ? chip.accentColor : Theme.textPrimary
            font.family: Theme.monoFamily
            font.pixelSize: Theme.fontBody
            font.bold: true
        }
    }
}
