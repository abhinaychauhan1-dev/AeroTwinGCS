/**
 * @file     : CoverageDock.qml
 * @brief    : Displays inspection coverage and uninspected hotspot status.
 * @details  : Provides coverage progress and automatic rerouting controls.
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
    Bottom dock: inspection coverage, uninspected hotspots, reroute action.
*/
GlassPanel {
    id: dock

    /** Coverage progress, remaining inspection zones, and reroute state from the backend. */
    property real coverage: 0            // 0.0 - 1.0
    property int hotspotCount: 0
    property bool rerouting: false
    property string assetName: ""

    /** Exposes the primary reroute control to integration and automated UI code. */
    property alias rerouteButton: rerouteButton

    /** Requests emitted when the operator focuses hotspots or starts automatic rerouting. */
    signal rerouteRequested()
    signal hotspotsFocusRequested()

    frosted: false
    tint: "transparent"
    contentMargins: Theme.spacing

    RowLayout {
        anchors.fill: parent
        spacing: Theme.spacing * 2

        // --- Coverage -----------------------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 260
            Layout.alignment: Qt.AlignVCenter
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                SectionLabel { text: "Surface Coverage" }

                Text {
                    visible: dock.assetName.length > 0
                    text: "\u00B7 " + dock.assetName
                    color: Theme.textSecondary
                    font.family: Theme.uiFamily
                    font.pixelSize: Theme.fontBody
                    elide: Text.ElideRight
                    Layout.maximumWidth: 220
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: (dock.coverage * 100).toFixed(1) + "%"
                    color: Theme.accent
                    font.family: Theme.monoFamily
                    font.pixelSize: Theme.fontValue
                    font.bold: true
                }
            }

            // Progress track
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 12
                radius: 6
                color: "#0B1219"
                border.width: 1
                border.color: Theme.glassBorder

                Rectangle {
                    id: fill
                    height: parent.height - 2
                    y: 1
                    x: 1
                    // Clamp external coverage data before converting it to progress-bar geometry.
                    width: Math.max(0, (parent.width - 2) * Math.max(0, Math.min(1, dock.coverage)))
                    radius: 4
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: Theme.accentDim }
                        GradientStop { position: 1.0; color: Theme.accent }
                    }

                    Behavior on width {
                        NumberAnimation { duration: Theme.durationSlow; easing.type: Theme.easing }
                    }
                }

                // Sweep highlight while rerouting
                Rectangle {
                    visible: dock.rerouting
                    width: 60
                    height: parent.height - 2
                    y: 1
                    radius: 4
                    opacity: 0.35
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 0.5; color: "#FFFFFFFF" }
                        GradientStop { position: 1.0; color: "transparent" }
                    }

                    NumberAnimation on x {
                        running: dock.rerouting
                        loops: Animation.Infinite
                        from: -60
                        to: fill.parent.width
                        duration: 1200
                    }
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            Layout.topMargin: 8
            Layout.bottomMargin: 8
            color: Theme.glassBorder
        }

        // --- Hotspots -----------------------------------------------------
        Item {
            Layout.preferredWidth: hotspotRow.implicitWidth
            Layout.fillHeight: true

            RowLayout {
                id: hotspotRow
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10

                Rectangle {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    radius: 24
                    color: dock.hotspotCount > 0 ? "#26FF7043" : "#1A3FDD85"
                    border.width: 1
                    border.color: dock.hotspotCount > 0 ? Theme.hotspot : Theme.good

                    Text {
                        anchors.centerIn: parent
                        text: dock.hotspotCount
                        color: dock.hotspotCount > 0 ? Theme.hotspot : Theme.good
                        font.family: Theme.monoFamily
                        font.pixelSize: Theme.fontValue
                        font.bold: true
                    }

                    SequentialAnimation on scale {
                        running: dock.hotspotCount > 0
                        loops: Animation.Infinite
                        NumberAnimation { to: 1.06; duration: 700; easing.type: Easing.InOutQuad }
                        NumberAnimation { to: 1.0; duration: 700; easing.type: Easing.InOutQuad }
                    }
                }

                ColumnLayout {
                    spacing: 0
                    SectionLabel { text: "Uninspected" }
                    Text {
                        text: dock.hotspotCount === 1 ? "hotspot zone" : "hotspot zones"
                        color: Theme.textSecondary
                        font.family: Theme.uiFamily
                        font.pixelSize: Theme.fontBody
                    }
                }
            }

            // The parent decides how a hotspot focus request changes the 3D camera.
            TapHandler {
                onTapped: dock.hotspotsFocusRequested()
            }
        }

        HudButton {
            id: rerouteButton
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: Math.max(300, implicitWidth)
            primary: true
            // Prevent duplicate reroute jobs and hide the action when no work remains.
            enabled: !dock.rerouting && dock.hotspotCount > 0
            glyph: dock.rerouting ? "\u25CF" : "\u21BB"
            text: dock.rerouting ? "Rerouting\u2026" : "Auto Reroute Missing Zones"
            onClicked: dock.rerouteRequested()
        }
    }
}
