/**
 * @file     : MissionPanel.qml
 * @brief    : Displays mission progress and waypoint controls.
 * @details  : Supports asset selection and live mission plan updates.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import AeroTwinGCS

/*!
    Left shelf: mission flight tree, asset selection and waypoint commands.
*/
GlassPanel {
    id: panel

    /** Active asset, editable mission model, and transient command feedback. */
    property string activeAsset: "Bridge Truss A"
    property alias missionModel: treeRepeater.model
    property alias assetModel: assetSelector.model
    property int currentLegIndex: 2
    property string commandStatus: ""

    /** Intent signals forwarded by Main.qml to the mission backend. */
    signal assetSelected(string asset)
    signal waypointCommand(string command)

    Timer {
        id: statusTimer
        interval: 2200
        onTriggered: panel.commandStatus = ""
    }

    // Displays short command feedback and restarts its automatic expiry timer.
    function announce(message) {
        panel.commandStatus = message
        statusTimer.restart()
    }

    // Inserts a pending waypoint with the same data schema as the static mission model.
    function insertWaypoint(index, title) {
        treeRepeater.model.insert(index, {
            title: title,
            depth: 2,
            kind: "wp",
            state_: "pending"
        })
        panel.currentLegIndex = index
    }

    // Owns local list transitions while the outer shell forwards the command to the backend.
    function handleWaypointCommand(command) {
        const model = treeRepeater.model
        const insertIndex = Math.min(model.count, panel.currentLegIndex + 1)

        switch (command) {
        case "add":
            insertWaypoint(model.count, "WP New - Standoff 3 m")
            announce("Waypoint added")
            break
        case "insert":
            insertWaypoint(insertIndex, "WP New - Inspection Point")
            announce("Waypoint inserted")
            break
        case "loiter":
            insertWaypoint(insertIndex, "LOITER - 30 seconds")
            announce("Loiter point added")
            break
        case "delete":
            if (model.count > 1 && panel.currentLegIndex >= 0
                    && panel.currentLegIndex < model.count) {
                model.remove(panel.currentLegIndex, 1)
                panel.currentLegIndex = Math.min(panel.currentLegIndex, model.count - 1)
                announce("Waypoint deleted")
            } else {
                announce("No waypoint selected")
            }
            break
        case "upload":
            announce("Mission upload queued")
            break
        default:
            announce("Unknown waypoint command")
        }
    }

    // Signal handler deliberately funnels every UI command through one state transition function.
    onWaypointCommand: function (command) {
        panel.handleWaypointCommand(command)
    }

    frosted: false
    tint: "transparent"
    contentMargins: Theme.spacing

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing

        // --- Asset selection ---------------------------------------------
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.spacingSmall

            SectionLabel { text: "Active Asset" }

            ComboBox {
                id: assetSelector
                Layout.fillWidth: true
                implicitHeight: Theme.controlHeight + 4
                model: ["Bridge Truss A", "Bridge Truss B", "Pier Cap 03", "Cable Stay North"]
                currentIndex: Math.max(0, model.indexOf ? model.indexOf(panel.activeAsset) : 0)
                // Emit only the selected label; Main.qml updates the authoritative backend asset.
                onActivated: panel.assetSelected(currentText)

                contentItem: Text {
                    leftPadding: 14
                    rightPadding: 36
                    text: assetSelector.displayText
                    color: Theme.accent
                    font.family: Theme.uiFamily
                    font.pixelSize: Theme.fontTitle
                    font.bold: true
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                indicator: Text {
                    x: assetSelector.width - width - 14
                    y: (assetSelector.height - height) / 2
                    text: "\u25BE"
                    color: Theme.accent
                    font.pixelSize: 14
                }

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: "#1a2430"
                    border.width: 1
                    border.color: assetSelector.hovered ? Theme.glassBorderStrong : Theme.glassBorder
                }

                popup: Popup {
                    y: assetSelector.height + 4
                    width: assetSelector.width
                    implicitHeight: Math.min(260, contentItem.implicitHeight + 8)
                    padding: 4

                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: assetSelector.popup.visible ? assetSelector.delegateModel : null
                        currentIndex: assetSelector.highlightedIndex
                        ScrollIndicator.vertical: ScrollIndicator {}
                    }

                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: "#F0121A24"
                        border.width: 1
                        border.color: Theme.glassBorder
                    }
                }

                delegate: ItemDelegate {
                    required property int index
                    required property var modelData
                    width: assetSelector.width - 8
                    height: 38
                    highlighted: assetSelector.highlightedIndex === index

                    contentItem: Text {
                        leftPadding: 10
                        text: modelData
                        color: parent.highlighted ? Theme.accent : Theme.textPrimary
                        font.family: Theme.uiFamily
                        font.pixelSize: Theme.fontBody
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    background: Rectangle {
                        radius: 6
                        color: parent.highlighted ? "#1A00E5FF" : "transparent"
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.glassBorder
        }

        // --- Mission flight tree -----------------------------------------
        SectionLabel { text: "Mission Flight Tree" }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            Column {
                width: parent.width
                spacing: 2

                Repeater {
                    id: treeRepeater

                    model: ListModel {
                        ListElement { title: "Inspection Plan \u2014 Truss A"; depth: 0; kind: "root"; state_: "running" }
                        ListElement { title: "Leg 01 \u00B7 North Face Sweep"; depth: 1; kind: "leg"; state_: "done" }
                        ListElement { title: "WP 1.1 \u00B7 Nadir 12 m"; depth: 2; kind: "wp"; state_: "done" }
                        ListElement { title: "WP 1.2 \u00B7 Oblique 30\u00B0"; depth: 2; kind: "wp"; state_: "done" }
                        ListElement { title: "Leg 02 \u00B7 Gusset Close-Up"; depth: 1; kind: "leg"; state_: "running" }
                        ListElement { title: "WP 2.1 \u00B7 Standoff 3 m"; depth: 2; kind: "wp"; state_: "running" }
                        ListElement { title: "WP 2.2 \u00B7 Standoff 3 m"; depth: 2; kind: "wp"; state_: "pending" }
                        ListElement { title: "Leg 03 \u00B7 Underdeck Pass"; depth: 1; kind: "leg"; state_: "pending" }
                        ListElement { title: "WP 3.1 \u00B7 Reverse Nadir"; depth: 2; kind: "wp"; state_: "pending" }
                        ListElement { title: "Leg 04 \u00B7 Return / Land"; depth: 1; kind: "leg"; state_: "pending" }
                    }

                    delegate: Rectangle {
                        id: node
                        required property int index
                        required property string title
                        required property int depth
                        required property string kind
                        required property string state_

                        // Mission-state values map directly to the tree's progress color language.
                        readonly property color stateColor: state_ === "done" ? Theme.good
                                                          : state_ === "running" ? Theme.accent
                                                          : Theme.textDim

                        width: parent ? parent.width : 0
                        height: Theme.rowHeight
                        radius: 6
                        color: nodeHover.hovered ? "#141E2A" : "transparent"
                        border.width: state_ === "running" ? 1 : 0
                        border.color: Theme.glassBorder

                        HoverHandler { id: nodeHover }

                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 10 + node.depth * 18
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            spacing: 10

                            Rectangle {
                                id: nodeMarker
                                anchors.verticalCenter: parent.verticalCenter
                                width: node.kind === "wp" ? 8 : 10
                                height: width
                                radius: node.kind === "wp" ? width / 2 : 2
                                color: node.stateColor

                                SequentialAnimation on opacity {
                                    running: node.state_ === "running"
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0.3; duration: 600 }
                                    NumberAnimation { to: 1.0; duration: 600 }
                                }
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: node.title
                                color: node.state_ === "pending" ? Theme.textSecondary : Theme.textPrimary
                                font.family: Theme.uiFamily
                                font.pixelSize: node.depth === 0 ? Theme.fontTitle : Theme.fontBody
                                font.bold: node.depth === 0
                                elide: Text.ElideRight
                                width: Math.max(0, parent.width - nodeMarker.width - parent.spacing)
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: Theme.glassBorder
        }

        // --- Waypoint commands -------------------------------------------
        SectionLabel { text: "Waypoint Commands" }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: Theme.spacingSmall
            rowSpacing: Theme.spacingSmall

            HudButton {
                Layout.fillWidth: true
                text: "Add WP"
                glyph: "+"
                onClicked: panel.waypointCommand("add")
            }
            HudButton {
                Layout.fillWidth: true
                text: "Insert"
                glyph: "\u21B3"
                onClicked: panel.waypointCommand("insert")
            }
            HudButton {
                Layout.fillWidth: true
                text: "Loiter"
                glyph: "\u21BB"
                onClicked: panel.waypointCommand("loiter")
            }
            HudButton {
                Layout.fillWidth: true
                text: "Delete"
                glyph: "\u2715"
                danger: true
                onClicked: panel.waypointCommand("delete")
            }
            HudButton {
                Layout.fillWidth: true
                Layout.columnSpan: 2
                text: "Upload Mission"
                glyph: "\u2191"
                primary: true
                onClicked: panel.waypointCommand("upload")
            }
        }

        Text {
            Layout.fillWidth: true
            text: panel.commandStatus
            color: Theme.good
            font.family: Theme.monoFamily
            font.pixelSize: Theme.fontLabel
            horizontalAlignment: Text.AlignHCenter
            visible: text.length > 0
        }
    }
}
