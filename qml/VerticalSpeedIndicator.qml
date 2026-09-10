/**
 * @file     : VerticalSpeedIndicator.qml
 * @brief    : Renders the vertical speed indicator.
 * @details  : Shows whether the drone is climbing or descending.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

import QtQuick
import AeroTwinGCS

/*!
    Vertical speed indicator: centre-zero bar, climb up / sink down.
*/
Item {
    id: vsi

    /** Vertical velocity in metres per second; positive values represent climb. */
    property real verticalSpeed: 0      // m/s, positive = climb
    property real range: 5.0

    implicitWidth: 54
    implicitHeight: 220

    // Limits bar geometry while preserving the true numeric readout below.
    readonly property real clamped: Math.max(-range, Math.min(range, verticalSpeed))
    readonly property color barColor: verticalSpeed >= 0 ? Theme.good : Theme.warn

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSmall
        color: "#8C0A1017"
        border.width: 1
        border.color: Theme.glassBorder
    }

    Item {
        id: track
        anchors.fill: parent
        anchors.margins: 8
        anchors.topMargin: 22
        anchors.bottomMargin: 26

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 1
            height: parent.height
            color: Theme.glassBorder
        }

        Repeater {
            model: 11
            Rectangle {
                required property int index
                anchors.horizontalCenter: parent.horizontalCenter
                width: index % 5 === 0 ? 16 : 8
                height: 1
                y: index * (track.height / 10)
                color: Theme.textDim
            }
        }

        // Zero reference
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: track.height / 2
            width: 22
            height: 1.5
            color: Theme.textSecondary
        }

        // Moving bar
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 7
            radius: 3
            color: vsi.barColor
            y: vsi.clamped >= 0
               ? track.height / 2 - height
               : track.height / 2
            height: Math.abs(vsi.clamped) / vsi.range * (track.height / 2)

            Behavior on height { NumberAnimation { duration: Theme.durationFast } }
        }
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 5
        text: "V/S"
        color: Theme.textDim
        font.family: Theme.monoFamily
        font.pixelSize: 11
        font.letterSpacing: 1
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 6
        text: (vsi.verticalSpeed >= 0 ? "+" : "") + vsi.verticalSpeed.toFixed(1)
        color: vsi.barColor
        font.family: Theme.monoFamily
        font.pixelSize: Theme.fontBody
        font.bold: true
    }
}
