/**
 * @file     : Main.qml
 * @brief    : Defines the AeroTwin Ground Control Station application shell.
 * @details  : Composes the inspection viewport and operational control panels.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Window
import AeroTwinGCS

/*!
    AeroTwin GCS main shell.

    All backend-facing state lives on this root object, either as a plain
    property (status / telemetry) or as an alias to a child panel:

        auto *root = engine.rootObjects().constFirst();
        root->setProperty("batteryPercent", 63);
        QMetaObject::invokeMethod(root, "toggleLeftPanel");

    Properties are bound to the GcsBackend singleton by default; overwrite the
    binding to drive the shell from any other source.
*/
ApplicationWindow {
    id: shell

    width: 1680
    height: 1000
    minimumWidth: 1360
    minimumHeight: 820
    visible: true
    title: qsTr("AeroTwin \u00B7 Ground Control Station")
    color: Theme.background

    /**
        Top-level telemetry bindings. Each value favors DroneStateBridge while
        its MAVLink link is alive, then falls back to the demo backend.
    */
    property bool armed: DroneStateBridge.connected ? DroneStateBridge.armed : GcsBackend.armed
    property string flightMode: GcsBackend.flightMode
    property int batteryPercent: DroneStateBridge.connected ? DroneStateBridge.batteryPercent : GcsBackend.batteryPercent
    property real batteryVolts: DroneStateBridge.connected ? DroneStateBridge.batteryVolts : GcsBackend.batteryVolts
    property string gpsFix: DroneStateBridge.connected ? DroneStateBridge.gpsFix : GcsBackend.gpsFix
    property int satellites: DroneStateBridge.connected ? DroneStateBridge.satellites : GcsBackend.satellites
    property int linkLatencyMs: DroneStateBridge.connected ? DroneStateBridge.linkLatencyMs : GcsBackend.linkLatencyMs
    property bool mavlinkAlive: DroneStateBridge.connected

    property real pitch: DroneStateBridge.connected ? DroneStateBridge.pitch : GcsBackend.pitch
    property real roll: DroneStateBridge.connected ? DroneStateBridge.roll : GcsBackend.roll
    property real heading: DroneStateBridge.connected ? DroneStateBridge.yaw : GcsBackend.heading
    property real altitudeAgl: DroneStateBridge.connected
                                     ? DroneStateBridge.altitudeAboveOrigin
                                     : GcsBackend.altitudeAgl
    property real verticalSpeed: DroneStateBridge.connected ? DroneStateBridge.verticalSpeed : GcsBackend.verticalSpeed
    property real groundSpeed: DroneStateBridge.connected ? DroneStateBridge.groundSpeed : GcsBackend.groundSpeed

    property string activeAsset: GcsBackend.activeAsset
    property real coverage: GcsBackend.coverage
    property int hotspotCount: GcsBackend.hotspotCount
    property bool rerouting: GcsBackend.rerouting

    /** Aliases provide direct access to major QML surfaces and editable models. */
    property alias viewport3D: viewport
    property alias headerBar: header
    property alias missionPanel: leftPanel
    property alias telemetryPanel: rightPanel
    property alias coverageDock: dock
    property alias missionModel: leftPanel.missionModel
    property alias assetModel: leftPanel.assetModel

    // --- Shelf visibility -------------------------------------------------
    property bool leftPanelOpen: true
    property bool rightPanelOpen: true
    property bool dockOpen: true

    // Shelf handlers only transition visibility state; animations react to these bindings.
    function toggleLeftPanel() { leftPanelOpen = !leftPanelOpen; }
    function toggleRightPanel() { rightPanelOpen = !rightPanelOpen; }
    function toggleDock() { dockOpen = !dockOpen; }

    /** Application-level intents emitted after UI controls request an action. */
    signal armToggleRequested(bool arm)
    signal waypointCommand(string command)
    signal assetSelected(string asset)
    signal rerouteRequested()

    // Own the live UDP telemetry lifecycle with the QML application shell.
    Component.onCompleted: DroneStateBridge.startMavlink(14550)
    Component.onDestruction: DroneStateBridge.stop()

    readonly property int edge: Theme.panelMargin

    // ---------------------------------------------------------------------
    // Background: fullscreen 3D inspection viewport
    // ---------------------------------------------------------------------
    Viewport3D {
        id: viewport
        anchors.fill: parent
        // Forward the selected telemetry source into the 3D tracker overlay.
        armed: shell.armed
        flightMode: shell.flightMode
        heading: shell.heading
        altitudeAgl: shell.altitudeAgl
        groundSpeed: shell.groundSpeed
        verticalSpeed: shell.verticalSpeed
        latitude: DroneStateBridge.latitude
        longitude: DroneStateBridge.longitude
    }

    // ---------------------------------------------------------------------
    // Header
    // ---------------------------------------------------------------------
    HeaderBar {
        id: header
        blurSource: viewport

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: shell.edge
        height: Theme.headerHeight

        armed: shell.armed
        flightMode: shell.flightMode
        batteryPercent: shell.batteryPercent
        batteryVolts: shell.batteryVolts
        gpsFix: shell.gpsFix
        satellites: shell.satellites
        linkLatencyMs: shell.linkLatencyMs
        mavlinkAlive: shell.mavlinkAlive

        // Persist the request in the backend before notifying external integrations.
        onArmToggleRequested: function (arm) {
            GcsBackend.setArmed(arm);
            shell.armToggleRequested(arm);
        }
    }

    // ---------------------------------------------------------------------
    // Left shelf: mission
    // ---------------------------------------------------------------------
    MissionPanel {
        id: leftPanel
        blurSource: viewport

        width: 380
        x: shell.leftPanelOpen ? shell.edge : -(width + 4)
        y: header.y + header.height + Theme.spacing
        height: Math.max(0, dock.y - y - Theme.spacing)
        opacity: shell.leftPanelOpen ? 1 : 0
        visible: opacity > 0.01

        activeAsset: shell.activeAsset

        // Reflect mission-panel intents through the singleton and the public shell signal.
        onAssetSelected: function (asset) {
            GcsBackend.selectAsset(asset);
            shell.assetSelected(asset);
        }
        onWaypointCommand: function (command) {
            GcsBackend.sendWaypointCommand(command);
            shell.waypointCommand(command);
        }

        Behavior on x {
            NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easing }
        }
        Behavior on opacity {
            NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easing }
        }
    }

    // ---------------------------------------------------------------------
    // Right shelf: telemetry HUD
    // ---------------------------------------------------------------------
    TelemetryPanel {
        id: rightPanel
        blurSource: viewport

        width: 340
        x: shell.rightPanelOpen ? shell.width - width - shell.edge : shell.width + 4
        y: leftPanel.y
        height: leftPanel.height
        opacity: shell.rightPanelOpen ? 1 : 0
        visible: opacity > 0.01

        pitch: shell.pitch
        roll: shell.roll
        heading: shell.heading
        altitudeAgl: shell.altitudeAgl
        verticalSpeed: shell.verticalSpeed
        groundSpeed: shell.groundSpeed

        Behavior on x {
            NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easing }
        }
        Behavior on opacity {
            NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easing }
        }
    }

    // ---------------------------------------------------------------------
    // Bottom dock: coverage
    // ---------------------------------------------------------------------
    CoverageDock {
        id: dock
        blurSource: viewport

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: shell.edge
        anchors.rightMargin: shell.edge

        height: Theme.dockHeight
        y: shell.dockOpen ? shell.height - height - shell.edge : shell.height + 4
        opacity: shell.dockOpen ? 1 : 0
        visible: opacity > 0.01

        coverage: shell.coverage
        hotspotCount: shell.hotspotCount
        rerouting: shell.rerouting
        assetName: shell.activeAsset

        // The dock owns the request gesture; backend owns reroute state and progress.
        onRerouteRequested: {
            GcsBackend.autoRerouteMissingZones();
            shell.rerouteRequested();
        }

        Behavior on y {
            NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easing }
        }
        Behavior on opacity {
            NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easing }
        }
    }

    // ---------------------------------------------------------------------
    // Collapse handles
    // ---------------------------------------------------------------------
    ShelfHandle {
        glyph: shell.leftPanelOpen ? "\u2039" : "\u203A"
        tip: "Mission shelf  (F1)"
        x: shell.leftPanelOpen ? leftPanel.x + leftPanel.width + 6 : shell.edge
        y: leftPanel.y + 8
        onToggled: shell.toggleLeftPanel()

        Behavior on x {
            NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easing }
        }
    }

    ShelfHandle {
        glyph: shell.rightPanelOpen ? "\u203A" : "\u2039"
        tip: "Telemetry HUD  (F2)"
        x: shell.rightPanelOpen ? rightPanel.x - width - 6 : shell.width - width - shell.edge
        y: rightPanel.y + 8
        onToggled: shell.toggleRightPanel()

        Behavior on x {
            NumberAnimation { duration: Theme.durationBase; easing.type: Theme.easing }
        }
    }

    Shortcut { sequence: "F1"; onActivated: shell.toggleLeftPanel() }
    Shortcut { sequence: "F2"; onActivated: shell.toggleRightPanel() }
    Shortcut { sequence: "F3"; onActivated: shell.toggleDock() }
    Shortcut {
        sequence: "F11"
        onActivated: shell.visibility = shell.visibility === Window.FullScreen ? Window.Windowed
                                                                              : Window.FullScreen
    }

    component ShelfHandle: Rectangle {
        id: handle

        property string glyph: "\u2039"
        property string tip: ""
        signal toggled()

        width: 24
        height: 64
        radius: 6
        color: handleHover.hovered ? "#CC16222E" : "#99101820"
        border.width: 1
        border.color: handleHover.hovered ? Theme.glassBorderStrong : Theme.glassBorder

        HoverHandler { id: handleHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: handle.toggled() }
        ToolTip.visible: handleHover.hovered && handle.tip.length > 0
        ToolTip.text: handle.tip
        ToolTip.delay: 400

        Text {
            anchors.centerIn: parent
            text: handle.glyph
            color: Theme.accent
            font.pixelSize: 16
            font.bold: true
        }

        Behavior on color { ColorAnimation { duration: Theme.durationFast } }
    }
}
