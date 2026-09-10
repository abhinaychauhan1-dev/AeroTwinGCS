/**
 * @file     : AltitudeTape.qml
 * @brief    : Renders the live altitude tape indicator.
 * @details  : Shows the drone altitude above ground level.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

import QtQuick
import AeroTwinGCS

/*!
    Vertical altitude-above-ground-level tape with a rolling readout box.
*/
Item {
    id: tape

    /** Live altitude above ground level and display-scale configuration. */
    property real altitude: 0
    property real pixelsPerMeter: 7
    property int minorStep: 2
    property int majorStep: 10
    property string units: "m"

    implicitWidth: 94
    implicitHeight: 220

    // Fixed tick budget keeps the scrollable scale stable at all altitudes.
    readonly property int tickSpan: 30

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSmall
        color: "#8C0A1017"
        border.width: 1
        border.color: Theme.glassBorder
    }

    Item {
        id: clipArea
        anchors.fill: parent
        anchors.margins: 1
        clip: true

        Item {
            width: parent.width
            y: parent.height / 2 + tape.altitude * tape.pixelsPerMeter

            Repeater {
                model: 2 * tape.tickSpan + 1

                Item {
                    id: tick
                    required property int index

                    // Re-anchor ticks to the current altitude to create a rolling scale.
                    readonly property int baseValue: Math.floor(tape.altitude / tape.minorStep) * tape.minorStep
                    readonly property int value: baseValue + (index - tape.tickSpan) * tape.minorStep
                    readonly property bool major: value % tape.majorStep === 0

                    width: clipArea.width
                    height: 1
                    y: -value * tape.pixelsPerMeter
                    visible: value >= 0

                    Rectangle {
                        anchors.right: parent.right
                        anchors.rightMargin: 7
                        width: tick.major ? 16 : 8
                        height: 1
                        color: Theme.textSecondary
                        opacity: tick.major ? 0.9 : 0.45
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.rightMargin: 28
                        anchors.verticalCenter: parent.verticalCenter
                        visible: tick.major
                        text: tick.value
                        color: Theme.textSecondary
                        font.family: Theme.monoFamily
                        font.pixelSize: 12
                    }
                }
            }
        }

        // Ground shading below 0 m AGL
        Rectangle {
            width: parent.width
            height: Math.max(0, parent.height / 2 + tape.altitude * tape.pixelsPerMeter)
            y: parent.height - height
            visible: height > 0 && height < parent.height
            color: "#33ff4d5e"
        }
    }

    // Current value box
    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: -6
        width: parent.width * 0.8
        height: 32
        radius: 4
        color: "#EE0F1822"
        border.width: 1
        border.color: Theme.accent

        Text {
            anchors.centerIn: parent
            text: tape.altitude.toFixed(1)
            color: Theme.accent
            font.family: Theme.monoFamily
            font.pixelSize: Theme.fontTitle + 2
            font.bold: true
        }
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 5
        text: "AGL " + tape.units
        color: Theme.textDim
        font.family: Theme.monoFamily
        font.pixelSize: 11
        font.letterSpacing: 1
    }
}
