/**
 * @file     : HeaderBar.qml
 * @brief    : Displays primary drone health and link status.
 * @details  : Shows vehicle identity, arming state, battery, GNSS, and link.
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
    Top status rail: identity, arm state, power, GNSS quality and link health.
*/
GlassPanel {
    id: header

    /** Live vehicle health values bound by Main.qml from live or demo telemetry. */
    property bool armed: false
    property string flightMode: "INSPECT"
    property int batteryPercent: 0
    property real batteryVolts: 0
    property string gpsFix: "No Fix"
    property int satellites: 0
    property int linkLatencyMs: 0
    property bool mavlinkAlive: false
    property string vehicleName: "AT-77 / AeroTwin"

    /** Requests an arming-state change from the owning application shell. */
    signal armToggleRequested(bool arm)

    // Convert telemetry thresholds into consistent visual health states.
    readonly property color batteryColor: batteryPercent > 45 ? Theme.good
                                        : batteryPercent > 20 ? Theme.warn : Theme.bad
    readonly property color gpsColor: gpsFix === "RTK Fixed" ? Theme.good
                                    : gpsFix === "RTK Float" ? Theme.warn : Theme.bad
    readonly property color linkColor: linkLatencyMs < 60 ? Theme.good
                                     : linkLatencyMs < 150 ? Theme.warn : Theme.bad

    cornerRadius: Theme.radius
    frosted: false
    tint: "transparent"
    contentMargins: Theme.spacing

    RowLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        // --- Identity -----------------------------------------------------
        RowLayout {
            spacing: 10
            Layout.alignment: Qt.AlignVCenter

            Rectangle {
                width: 38
                height: 38
                radius: 10
                color: "transparent"
                border.width: 1.5
                border.color: Theme.accent

                Text {
                    anchors.centerIn: parent
                    text: "\u25B2"
                    color: Theme.accent
                    font.pixelSize: 16
                }
            }

            ColumnLayout {
                spacing: 1
                Text {
                    text: header.vehicleName
                    color: Theme.textPrimary
                    font.family: Theme.uiFamily
                    font.pixelSize: Theme.fontTitle
                    font.bold: true
                    font.letterSpacing: 1.0
                }
                Text {
                    text: header.flightMode
                    color: Theme.accentDim
                    font.family: Theme.monoFamily
                    font.pixelSize: Theme.fontLabel
                    font.letterSpacing: 1.4
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            Layout.topMargin: 6
            Layout.bottomMargin: 6
            color: Theme.glassBorder
        }

        // --- Arm state ----------------------------------------------------
        StatusChip {
            Layout.alignment: Qt.AlignVCenter
            label: "State"
            value: header.armed ? "ARMED" : "DISARMED"
            accentColor: header.armed ? Theme.bad : Theme.textSecondary
            emphasized: header.armed
            pulsing: header.armed

            // Keep the command at the shell boundary so the header stays presentation-only.
            TapHandler {
                onTapped: header.armToggleRequested(!header.armed)
            }
        }

        StatusChip {
            Layout.alignment: Qt.AlignVCenter
            label: "Batt"
            value: header.batteryPercent + "%  " + header.batteryVolts.toFixed(1) + "V"
            accentColor: header.batteryColor
            emphasized: header.batteryPercent <= 20
        }

        StatusChip {
            Layout.alignment: Qt.AlignVCenter
            label: "GNSS"
            value: header.gpsFix + "  " + header.satellites + " SV"
            accentColor: header.gpsColor
            emphasized: header.gpsFix.indexOf("RTK") === 0
        }

        StatusChip {
            Layout.alignment: Qt.AlignVCenter
            label: "Link"
            value: header.linkLatencyMs + " ms"
            accentColor: header.linkColor
        }

        Item { Layout.fillWidth: true }

        // --- MAVLink heartbeat -------------------------------------------
        RowLayout {
            spacing: 10
            Layout.alignment: Qt.AlignVCenter
            visible: header.width > 1120

            SectionLabel { text: "MAVLink" }

            Row {
                spacing: 4
                Repeater {
                    model: 5
                    Rectangle {
                        required property int index
                        width: 4
                        height: 18
                        radius: 2
                        color: header.mavlinkAlive ? Theme.accent : Theme.textDim

                        SequentialAnimation on opacity {
                            running: header.mavlinkAlive
                            loops: Animation.Infinite
                            PauseAnimation { duration: index * 90 }
                            NumberAnimation { to: 1.0; duration: 180 }
                            NumberAnimation { to: 0.2; duration: 320 }
                            PauseAnimation { duration: (5 - index) * 90 }
                        }
                    }
                }
            }

            Text {
                text: header.mavlinkAlive ? "HEARTBEAT" : "LOST"
                color: header.mavlinkAlive ? Theme.good : Theme.bad
                font.family: Theme.monoFamily
                font.pixelSize: Theme.fontLabel
                font.bold: true
                font.letterSpacing: 1.2
            }        }
    }
}
