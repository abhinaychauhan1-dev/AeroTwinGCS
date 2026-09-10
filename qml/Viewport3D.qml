/**
 * @file     : Viewport3D.qml
 * @brief    : Renders the live three-dimensional drone inspection scene.
 * @details  : Displays the drone, inspected asset, camera view, and tracker.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

import QtQuick
import QtQuick3D
import QtQuick3D.Helpers
import AeroTwinGCS

/*!
    Placeholder inspection scene. Swap \l sceneRoot content for the real
    photogrammetry mesh / point cloud; the shell only needs this Item to keep
    filling the window so the glass panels have something to blur.
*/
Item {
    id: viewport

    /** Public scene aliases used by the application shell and external integrations. */
    property alias camera: cam
    property alias sceneRoot: sceneNode
    property alias droneNode: drone
    property alias gimbal: gimbal
    property alias cameraFrustum: frustum
    property alias cameraController: controller
    /** Selects live bridge data when available; otherwise keeps the demonstration aircraft visible. */
    readonly property bool liveTelemetry: DroneStateBridge.connected
    readonly property vector3d demoDronePosition: Qt.vector3d(0, 420, 300)
    /** Flight values forwarded from Main.qml to the persistent operator tracker. */
    property bool armed: false
    property string flightMode: "INSPECT"
    property real heading: 0
    property real altitudeAgl: 0
    property real groundSpeed: 0
    property real verticalSpeed: 0
    property real latitude: 0
    property real longitude: 0

    // Converts numeric motion state into an operator-readable activity label.
    readonly property bool moving: groundSpeed > 0.5
    readonly property string activity: !armed ? "ON STANDBY"
                                            : !moving ? "HOLDING POSITION"
                                            : verticalSpeed > 0.6 ? "CLIMBING"
                                            : verticalSpeed < -0.6 ? "DESCENDING"
                                            : "INSPECTING ROUTE"

    View3D {
        id: view
        anchors.fill: parent
        camera: cam
        // Scene environment supplies antialiasing and glow for the inspection overlays.
        environment: ExtendedSceneEnvironment {
            clearColor: Theme.background
            backgroundMode: SceneEnvironment.Color
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
            glowEnabled: true
            glowStrength: 0.9
            glowIntensity: 1.1
            glowBloom: 0.3
            glowQualityHigh: true
            glowUseBicubicUpscale: true
        }

        // Pose is written each frame by InspectionCameraController.
        PerspectiveCamera {
            id: cam
            clipFar: 8000
        }

        DirectionalLight {
            eulerRotation: Qt.vector3d(-40, -120, 0)
            brightness: 1.1
            color: "#cfe8ff"
        }

        DirectionalLight {
            eulerRotation: Qt.vector3d(20, 60, 0)
            brightness: 0.45
            color: Theme.accent
        }

        Node {
            id: sceneNode

            // Ground plane
            Model {
                source: "#Rectangle"
                scale: Qt.vector3d(30, 30, 1)
                eulerRotation.x: -90
                materials: PrincipledMaterial {
                    baseColor: "#0a1119"
                    roughness: 0.9
                    metalness: 0.0
                }
            }

            // Stand-in "Bridge Truss A" geometry
            Repeater3D {
                model: 9
                Model {
                    required property int index
                    source: "#Cube"
                    position: Qt.vector3d(-400 + index * 100, 60, 0)
                    scale: Qt.vector3d(0.2, 1.2, 0.2)
                    materials: PrincipledMaterial {
                        baseColor: "#243444"
                        roughness: 0.4
                        metalness: 0.8
                    }
                }
            }

            // Inspection target skinned with the live coverage material.
            Model {
                source: "#Cube"
                position: Qt.vector3d(0, 180, 0)
                scale: Qt.vector3d(9, 0.15, 0.6)
                materials: CoverageMaterial {
                    id: deckCoverage
                    coverageThreshold: 0.8
                    partialThreshold: 0.2
                    gridScale: 0.9
                    scanProgress: deckCoverage.coverage.coveredFraction
                }
            }

            // Stand-in for the coverage solver: sweeps a quality footprint
            // across the deck's UV space so the three states are visible.
            FrameAnimation {
                id: scanDriver
                running: true

                property real u: 0
                property int row: 0

                onTriggered: {
                    u += frameTime * 0.35;
                    if (u > 1.0) {
                        u = 0;
                        row = (row + 1) % 5;
                    }
                    const v = 0.1 + row * 0.2;
                    const quality = 0.35 + 0.65 * Math.abs(Math.sin(row * 1.3 + u * 2.0));
                    deckCoverage.coverage.splat(u, v, 0.09, quality);
                }
            }

            // Uninspected hotspot markers
            Repeater3D {
                model: 4
                Model {
                    required property int index
                    source: "#Sphere"
                    position: Qt.vector3d(-300 + index * 200, 130, 40)
                    scale: Qt.vector3d(0.35, 0.35, 0.35)
                    materials: PrincipledMaterial {
                        baseColor: Theme.hotspot
                        lighting: PrincipledMaterial.NoLighting
                        opacity: 0.85
                    }
                }
            }

            // --- Drone airframe -> gimbal -> camera frustum ----------------
            Node {
                id: drone
                // Live scene transforms are produced from telemetry ENU coordinates and attitude.
                position: viewport.liveTelemetry
                          ? DroneStateBridge.scenePosition
                          : viewport.demoDronePosition
                rotation: viewport.liveTelemetry
                         ? DroneStateBridge.sceneRotation
                         : Qt.quaternion(1, 0, 0, 0)

                Model {
                    source: "#Cube"
                    scale: Qt.vector3d(0.3, 0.08, 0.3)
                    materials: PrincipledMaterial {
                        baseColor: "#1b2734"
                        roughness: 0.5
                        metalness: 0.7
                    }
                }

                Node {
                    id: gimbal
                    y: -14
                    eulerRotation.x: -32   // tilt down onto the truss

                    CameraFrustum {
                        id: frustum
                        horizontalFov: 84
                        aspectRatio: 16 / 9
                        projectionRange: 420
                        nearPlane: 12
                        beamColor: Theme.accent
                    }
                }
            }
        }

        AxisHelper {
            enableXZGrid: true
            enableAxisLines: false
            gridOpacity: 0.12
            scale: Qt.vector3d(6, 6, 6)
        }
    }

    // Start in chase mode so operators immediately see the aircraft's real-time location.
    InspectionCameraController {
        id: controller
        anchors.fill: parent
        view: view
        camera: cam
        chaseTarget: drone

        mode: InspectionCameraController.DroneChase
        orbitCenter: viewport.demoDronePosition
        centerGoal: viewport.demoDronePosition
        azimuth: 35
        elevation: 18
        distance: 520
        chaseOffset: Qt.vector3d(0, 150, 420)
        minDistance: 40
        maxDistance: 4000
        flySpeed: 320
    }

    // Persistent overlay connects the 3D scene with readable flight intent and position data.
    Rectangle {
        id: tracker
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Theme.headerHeight + Theme.panelMargin * 2
        width: 318
        height: 128
        radius: Theme.radiusSmall
        color: "#DE0A1119"
        border.width: 1
        border.color: Theme.glassBorderStrong

        Column {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 7

            Row {
                width: parent.width
                spacing: 8

                Rectangle {
                    width: 10
                    height: 10
                    anchors.verticalCenter: parent.verticalCenter
                    radius: width / 2
                    color: viewport.liveTelemetry ? Theme.good : Theme.warn

                    // Armed status pulses independently of whether telemetry is live or demo data.
                    SequentialAnimation on opacity {
                        running: viewport.armed
                        loops: Animation.Infinite
                        NumberAnimation { to: 0.25; duration: 600 }
                        NumberAnimation { to: 1; duration: 600 }
                    }
                }

                Text {
                    text: viewport.liveTelemetry ? "LIVE DRONE POSITION" : "DEMO DRONE POSITION"
                    color: Theme.textPrimary
                    font.family: Theme.uiFamily
                    font.pixelSize: Theme.fontLabel
                    font.bold: true
                }

                Item { width: 1; height: 1 }

                Text {
                    text: viewport.activity
                    color: viewport.armed ? Theme.accent : Theme.textSecondary
                    font.family: Theme.monoFamily
                    font.pixelSize: Theme.fontLabel
                    font.bold: true
                }
            }

            Text {
                text: "HDG " + Math.round(viewport.heading) + " deg   ALT " + viewport.altitudeAgl.toFixed(1) + " m   GS " + viewport.groundSpeed.toFixed(1) + " m/s"
                color: Theme.textSecondary
                font.family: Theme.monoFamily
                font.pixelSize: Theme.fontBody
            }

            Text {
                text: viewport.liveTelemetry
                      ? "LAT " + viewport.latitude.toFixed(6) + "   LON " + viewport.longitude.toFixed(6)
                      : "MISSION " + viewport.flightMode + "   FOLLOW CAMERA ACTIVE"
                color: Theme.textDim
                font.family: Theme.monoFamily
                font.pixelSize: Theme.fontLabel
            }

            Row {
                spacing: 6

                // Explicit camera actions let operators recover a useful scene view quickly.
                TrackerButton {
                    label: "Follow drone"
                    active: controller.mode === InspectionCameraController.DroneChase
                    onClicked: controller.mode = InspectionCameraController.DroneChase
                }

                TrackerButton {
                    label: "Inspect asset"
                    active: controller.mode === InspectionCameraController.OrbitAsset
                    onClicked: controller.focusOn(Qt.vector3d(0, 150, 0), 900)
                }
            }
        }
    }

    /** Compact camera-mode control used by the tracker overlay. */
    component TrackerButton: Rectangle {
        property string label: ""
        property bool active: false
        signal clicked()

        width: label === "Follow drone" ? 116 : 108
        height: 27
        radius: 4
        color: active ? "#1A00E5FF" : "#111E2A"
        border.width: 1
        border.color: active ? Theme.accent : Theme.glassBorder

        Text {
            anchors.centerIn: parent
            text: parent.label
            color: parent.active ? Theme.accent : Theme.textSecondary
            font.family: Theme.uiFamily
            font.pixelSize: Theme.fontLabel
            font.bold: parent.active
        }

        // Delegate action ownership to the tracker button instance.
        TapHandler { onTapped: parent.clicked() }
    }

    // Vignette keeps the HUD readable over bright scene areas.
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#66000000" }
            GradientStop { position: 0.45; color: "#00000000" }
            GradientStop { position: 1.0; color: "#88000000" }
        }
    }
}
