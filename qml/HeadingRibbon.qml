/**
 * @file     : HeadingRibbon.qml
 * @brief    : Renders the live heading ribbon indicator.
 * @details  : Shows the drone compass direction and numerical heading.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

import QtQuick
import AeroTwinGCS

/*!
    Horizontal heading ribbon (compass strip) with a fixed centre lubber line.
*/
Item {
    id: ribbon

    /** True heading in degrees; the strip scrolls under the fixed center marker. */
    property real heading: 0            // 0..360, degrees true
    property real pixelsPerDegree: 3.2
    property int minorStep: 5
    property int majorStep: 15

    implicitWidth: 260
    implicitHeight: 64

    readonly property int tickSpan: 26

    // Normalizes wraparound values before selecting a cardinal or numeric label.
    function cardinal(deg) {
        const d = ((deg % 360) + 360) % 360;
        switch (d) {
        case 0: return "N";
        case 90: return "E";
        case 180: return "S";
        case 270: return "W";
        default: return d.toString().padStart(3, "0");
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusSmall
        color: "#8C0A1017"
        border.width: 1
        border.color: Theme.glassBorder
    }

    Item {
        id: strip
        anchors.fill: parent
        anchors.margins: 1
        anchors.bottomMargin: 28   // reserved for the readout band
        clip: true

        Item {
            height: parent.height
            x: parent.width / 2 - ribbon.heading * ribbon.pixelsPerDegree

            Repeater {
                model: 2 * ribbon.tickSpan + 1

                Item {
                    id: tick
                    required property int index

                    // Generates a bounded set of ticks centered on the live heading.
                    readonly property int baseValue: Math.floor(ribbon.heading / ribbon.minorStep) * ribbon.minorStep
                    readonly property int value: baseValue + (index - ribbon.tickSpan) * ribbon.minorStep
                    readonly property bool major: ((value % ribbon.majorStep) + ribbon.majorStep) % ribbon.majorStep === 0
                    readonly property bool isCardinal: ((value % 90) + 90) % 90 === 0

                    width: 1
                    height: strip.height
                    x: value * ribbon.pixelsPerDegree

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 5
                        width: 1
                        height: tick.major ? 11 : 6
                        color: tick.isCardinal ? Theme.accent : Theme.textSecondary
                        opacity: tick.major ? 0.9 : 0.4
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 18
                        visible: tick.major
                        text: ribbon.cardinal(tick.value)
                        color: tick.isCardinal ? Theme.accent : Theme.textSecondary
                        font.family: Theme.monoFamily
                        font.pixelSize: tick.isCardinal ? 14 : 11
                        font.bold: tick.isCardinal
                    }
                }
            }
        }
    }

    // Lubber line + readout
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 2
        width: 2
        height: 14
        color: Theme.accent
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 4
        width: 66
        height: 24
        radius: 4
        color: "#EE0F1822"
        border.width: 1
        border.color: Theme.accent

        Text {
            anchors.centerIn: parent
            text: (((Math.round(ribbon.heading) % 360) + 360) % 360).toString().padStart(3, "0") + "\u00B0"
            color: Theme.accent
            font.family: Theme.monoFamily
            font.pixelSize: Theme.fontBody
            font.bold: true
        }
    }
}
