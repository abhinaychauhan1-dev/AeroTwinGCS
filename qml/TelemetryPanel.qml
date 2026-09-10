/**
 * @file     : TelemetryPanel.qml
 * @brief    : Displays live aircraft attitude and flight telemetry.
 * @details  : Presents attitude, altitude, speed, and heading information.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

import QtQuick
import QtQuick.Layouts
import AeroTwinGCS

/*!
    Right shelf: live PFD stack.
*/
GlassPanel {
    id: hud

    /** Live flight values consumed by the primary-flight-display instruments. */
    property real pitch: 0
    property real roll: 0
    property real heading: 0
    property real altitudeAgl: 0
    property real verticalSpeed: 0
    property real groundSpeed: 0

    /** Aliases expose individual instruments for automation or backend integration. */
    property alias attitudeIndicator: adi
    property alias altitudeTape: altTape
    property alias headingRibbon: compass
    property alias verticalSpeedIndicator: vsi

    frosted: false
    tint: "transparent"
    contentMargins: Theme.spacing

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        SectionLabel { text: "Live Telemetry" }

        // Bind pitch and roll into the artificial horizon.
        AttitudeIndicator {
            id: adi
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.min(parent.width, 210)
            Layout.preferredHeight: Layout.preferredWidth
            pitch: hud.pitch
            roll: hud.roll
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing

            Item { Layout.fillWidth: true }

            Text {
                text: "\u03A6 " + hud.roll.toFixed(1) + "\u00B0"
                color: Theme.textSecondary
                font.family: Theme.monoFamily
                font.pixelSize: Theme.fontBody
            }
            Text {
                text: "\u0398 " + hud.pitch.toFixed(1) + "\u00B0"
                color: Theme.textSecondary
                font.family: Theme.monoFamily
                font.pixelSize: Theme.fontBody
            }

            Item { Layout.fillWidth: true }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacingSmall

            // Keep detailed tape instruments and compact numeric readouts in sync.
            AltitudeTape {
                id: altTape
                Layout.fillHeight: true
                Layout.preferredWidth: 94
                Layout.minimumHeight: 170
                altitude: hud.altitudeAgl
            }

            VerticalSpeedIndicator {
                id: vsi
                Layout.fillHeight: true
                Layout.preferredWidth: 54
                Layout.minimumHeight: 170
                verticalSpeed: hud.verticalSpeed
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.spacingSmall

                Readout { label: "GS"; value: hud.groundSpeed.toFixed(1); unit: "m/s" }
                Readout { label: "VS"; value: hud.verticalSpeed.toFixed(1); unit: "m/s" }
                Readout { label: "HDG"; value: Math.round(hud.heading); unit: "\u00B0" }
                Item { Layout.fillHeight: true }
            }
        }

        HeadingRibbon {
            id: compass
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            heading: hud.heading
        }
    }

    component Readout: Rectangle {
        property string label: ""
        property string value: "--"
        property string unit: ""

        Layout.fillWidth: true
        Layout.preferredHeight: 56
        radius: Theme.radiusSmall
        color: "#8C0A1017"
        border.width: 1
        border.color: Theme.glassBorder

        Column {
            anchors.centerIn: parent
            spacing: 2

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: label
                color: Theme.textDim
                font.family: Theme.uiFamily
                font.pixelSize: Theme.fontLabel
                font.bold: true
                font.letterSpacing: 1.2
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: value + " " + unit
                color: Theme.textPrimary
                font.family: Theme.monoFamily
                font.pixelSize: Theme.fontValue
                font.bold: true
            }
        }
    }
}
