/**
 * @file     : InspectionCameraController.qml
 * @brief    : Controls navigation of the inspection scene camera.
 * @details  : Supports orbit, drone-follow, and free-roam camera modes.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

import QtQuick
import QtQuick3D
import AeroTwinGCS

/*!
    Inspection workstation camera controller.

    Drives an existing PerspectiveCamera; place it as an overlay over the
    View3D so it receives pointer and key input:

        View3D {
            id: view
            camera: cam
            PerspectiveCamera { id: cam }
        }
        InspectionCameraController {
            anchors.fill: parent
            view: view
            camera: cam
            chaseTarget: droneNode
        }

    Bindings:  LMB orbit - MMB pan - RMB free look - wheel zoom
               WASD + QE fly (Free Roam) - Shift boost
               click to pick, double click to focus the picked point

    Every mode writes the same rig state (centre, azimuth, elevation, distance,
    yaw, pitch), so switching modes never snaps: the smoother simply retargets.
*/
Item {
    id: root

    enum Mode {
        OrbitAsset,
        DroneChase,
        FreeRoam
    }

    /** Scene objects driven by this controller; chaseTarget is followed in DroneChase mode. */
    property View3D view: null
    property PerspectiveCamera camera: null
    property Node chaseTarget: null

    /** Current interaction state: asset orbit, moving drone chase, or free-flight camera. */
    property int mode: InspectionCameraController.OrbitAsset

    // --- Rig state (current, smoothed) ------------------------------------
    property vector3d orbitCenter: Qt.vector3d(0, 0, 0)
    property real azimuth: 35
    property real elevation: 22
    property real distance: 700

    // --- Limits -----------------------------------------------------------
    property real minDistance: 8
    property real maxDistance: 8000
    property real minElevation: -85
    property real maxElevation: 85

    // --- Feel -------------------------------------------------------------
    property real smoothing: 0.10        // response time constant, seconds
    property real inertia: 0.35          // momentum decay time constant, seconds
    property real orbitSensitivity: 0.35 // degrees per pixel
    property real lookSensitivity: 0.18
    property real zoomSensitivity: 0.0014
    property real flySpeed: 260          // scene units per second
    property real flyBoost: 4.0

    // --- Chase cam --------------------------------------------------------
    property vector3d chaseOffset: Qt.vector3d(0, 130, 340) // right / up / back, target-local
    property real chaseFrequency: 1.6    // spring frequency, Hz
    property real chaseDamping: 1.0      // 1.0 = critically damped

    readonly property bool interacting: dragButton !== Qt.NoButton

    /** Emits scene-picking results for tools that need the selected object or position. */
    signal picked(var result)
    signal surfacePicked(vector3d scenePosition, var model)

    // --- Goals and internals ---------------------------------------------
    property vector3d centerGoal: Qt.vector3d(0, 0, 0)
    property real azimuthGoal: 35
    property real elevationGoal: 22
    property real distanceGoal: 700

    property real yaw: 35
    property real pitch: -22
    property real yawGoal: 35
    property real pitchGoal: -22
    property vector3d positionGoal: Qt.vector3d(0, 0, 0)
    property vector3d position: Qt.vector3d(0, 0, 0)
    property vector3d chaseVelocity: Qt.vector3d(0, 0, 0)

    property real azimuthVelocity: 0
    property real elevationVelocity: 0

    property int dragButton: Qt.NoButton
    property real lastX: 0
    property real lastY: 0
    property real lastMoveMs: 0
    property bool dragged: false

    property var heldKeys: ({})

    focus: true

    // --- Public API -------------------------------------------------------
    // Updates orbit targets rather than camera values, allowing the frame loop to smooth motion.
    function setOrbit(az, el, dist) {
        azimuthGoal = az;
        elevationGoal = clamp(el, minElevation, maxElevation);
        distanceGoal = clamp(dist, minDistance, maxDistance);
        azimuthVelocity = 0;
        elevationVelocity = 0;
    }

    // Re-centers the orbit camera on a selected scene location.
    function focusOn(point, dist) {
        centerGoal = point;
        if (dist !== undefined)
            distanceGoal = clamp(dist, minDistance, maxDistance);
        mode = InspectionCameraController.OrbitAsset;
    }

    /*! Rebuilds the orbit parameters from wherever the camera currently is. */
    function syncOrbitFromCamera() {
        const offset = Qt.vector3d(position.x - orbitCenter.x,
                                   position.y - orbitCenter.y,
                                   position.z - orbitCenter.z);
        const radius = Math.max(offset.length(), minDistance);
        distanceGoal = clamp(radius, minDistance, maxDistance);
        azimuthGoal = Math.atan2(offset.x, offset.z) * 180 / Math.PI;
        elevationGoal = Math.asin(clamp(offset.y / radius, -1, 1)) * 180 / Math.PI;
        distance = radius;
        azimuth = azimuthGoal;
        elevation = elevationGoal;
    }

    function clamp(v, lo, hi) {
        return Math.max(lo, Math.min(hi, v));
    }

    // Frame-rate independent exponential approach; tau is the time constant.
    function approach(current, goal, tau, dt) {
        if (tau <= 0)
            return goal;
        return current + (goal - current) * (1 - Math.exp(-dt / tau));
    }

    function approachAngle(current, goal, tau, dt) {
        let delta = goal - current;
        while (delta > 180) delta -= 360;
        while (delta < -180) delta += 360;
        return approach(current, current + delta, tau, dt);
    }

    // Preserve the current view when transitioning between controller modes.
    onModeChanged: {
        if (mode === InspectionCameraController.OrbitAsset)
            syncOrbitFromCamera();
        else if (mode === InspectionCameraController.FreeRoam) {
            yawGoal = azimuth;
            pitchGoal = -elevation;
            positionGoal = position;
        }
        chaseVelocity = Qt.vector3d(0, 0, 0);
    }

    // Seed current and target rig state together to avoid an initial camera snap.
    Component.onCompleted: {
        centerGoal = orbitCenter;
        azimuthGoal = azimuth;
        elevationGoal = elevation;
        distanceGoal = distance;
        yaw = azimuth;
        pitch = -elevation;
        yawGoal = yaw;
        pitchGoal = pitch;
        applyOrbitRig();
    }

    // --- Update loop ------------------------------------------------------
    FrameAnimation {
        running: root.camera !== null
        onTriggered: root.step(Math.min(frameTime, 0.05)) // clamp after a stall
    }

    // Single dispatch point keeps all modes writing the same camera transform.
    function step(dt) {
        switch (mode) {
        case InspectionCameraController.DroneChase:
            stepChase(dt);
            break;
        case InspectionCameraController.FreeRoam:
            stepFreeRoam(dt);
            break;
        default:
            stepOrbit(dt);
            break;
        }

        camera.position = position;
        camera.eulerRotation = Qt.vector3d(pitch, yaw, 0);
    }

    function stepOrbit(dt) {
        // Momentum: the drag hands over its angular velocity on release.
        if (dragButton === Qt.NoButton && inertia > 0) {
            azimuthGoal += azimuthVelocity * dt;
            elevationGoal = clamp(elevationGoal + elevationVelocity * dt, minElevation, maxElevation);
            const decay = Math.exp(-dt / inertia);
            azimuthVelocity *= decay;
            elevationVelocity *= decay;
            if (Math.abs(azimuthVelocity) < 0.5) azimuthVelocity = 0;
            if (Math.abs(elevationVelocity) < 0.5) elevationVelocity = 0;
        }

        azimuth = approachAngle(azimuth, azimuthGoal, smoothing, dt);
        elevation = approach(elevation, elevationGoal, smoothing, dt);
        distance = approach(distance, distanceGoal, smoothing, dt);
        orbitCenter = Qt.vector3d(approach(orbitCenter.x, centerGoal.x, smoothing * 2, dt),
                                  approach(orbitCenter.y, centerGoal.y, smoothing * 2, dt),
                                  approach(orbitCenter.z, centerGoal.z, smoothing * 2, dt));

        applyOrbitRig();
    }

    function applyOrbitRig() {
        const a = azimuth * Math.PI / 180;
        const e = elevation * Math.PI / 180;
        const ce = Math.cos(e);

        position = Qt.vector3d(orbitCenter.x + distance * ce * Math.sin(a),
                               orbitCenter.y + distance * Math.sin(e),
                               orbitCenter.z + distance * ce * Math.cos(a));
        // Derived analytically from Qt's YXZ euler order: yaw = azimuth,
        // pitch = -elevation puts the camera's -Z straight through the centre.
        yaw = azimuth;
        pitch = -elevation;
    }

    function stepChase(dt) {
        if (!chaseTarget)
            return stepOrbit(dt);

        const t = chaseTarget.scenePosition;
        const heading = chaseTarget.eulerRotation.y * Math.PI / 180;
        const sin = Math.sin(heading);
        const cos = Math.cos(heading);

        // Offset is target-local: x right, y up, z back.
        positionGoal = Qt.vector3d(t.x + chaseOffset.x * cos + chaseOffset.z * sin,
                                   t.y + chaseOffset.y,
                                   t.z - chaseOffset.x * sin + chaseOffset.z * cos);

        // Critically damped spring, semi-implicit Euler: stable at any dt and
        // it overshoots far more gracefully than plain lerping when the drone
        // snaps direction.
        const omega = 2 * Math.PI * chaseFrequency;
        const k = omega * omega;
        const c = 2 * chaseDamping * omega;

        chaseVelocity = Qt.vector3d(
            chaseVelocity.x + ((positionGoal.x - position.x) * k - chaseVelocity.x * c) * dt,
            chaseVelocity.y + ((positionGoal.y - position.y) * k - chaseVelocity.y * c) * dt,
            chaseVelocity.z + ((positionGoal.z - position.z) * k - chaseVelocity.z * c) * dt);

        position = Qt.vector3d(position.x + chaseVelocity.x * dt,
                               position.y + chaseVelocity.y * dt,
                               position.z + chaseVelocity.z * dt);

        // Aim at the vehicle, not at the spring goal, so lag reads as camera
        // inertia rather than as the subject drifting off centre.
        const dx = t.x - position.x;
        const dy = t.y - position.y;
        const dz = t.z - position.z;
        const flat = Math.sqrt(dx * dx + dz * dz);

        yawGoal = Math.atan2(-dx, -dz) * 180 / Math.PI;
        pitchGoal = Math.atan2(dy, flat) * 180 / Math.PI;

        yaw = approachAngle(yaw, yawGoal, smoothing * 2.5, dt);
        pitch = approach(pitch, pitchGoal, smoothing * 2.5, dt);

        orbitCenter = t;
        centerGoal = t;
    }

    // Integrates keyboard input in local camera axes with optional shift boost.
    function stepFreeRoam(dt) {
        yaw = approachAngle(yaw, yawGoal, smoothing, dt);
        pitch = approach(pitch, pitchGoal, smoothing, dt);

        const y = yaw * Math.PI / 180;
        const p = pitch * Math.PI / 180;
        const cp = Math.cos(p);

        const forward = Qt.vector3d(-cp * Math.sin(y), Math.sin(p), -cp * Math.cos(y));
        const right = Qt.vector3d(Math.cos(y), 0, -Math.sin(y));

        let dx = 0, dy = 0, dz = 0;
        const step = flySpeed * (heldKeys[Qt.Key_Shift] ? flyBoost : 1);

        if (heldKeys[Qt.Key_W]) { dx += forward.x; dy += forward.y; dz += forward.z; }
        if (heldKeys[Qt.Key_S]) { dx -= forward.x; dy -= forward.y; dz -= forward.z; }
        if (heldKeys[Qt.Key_D]) { dx += right.x; dz += right.z; }
        if (heldKeys[Qt.Key_A]) { dx -= right.x; dz -= right.z; }
        if (heldKeys[Qt.Key_E]) dy += 1;
        if (heldKeys[Qt.Key_Q]) dy -= 1;

        const len = Math.sqrt(dx * dx + dy * dy + dz * dz);
        if (len > 0.0001) {
            positionGoal = Qt.vector3d(positionGoal.x + dx / len * step * dt,
                                       positionGoal.y + dy / len * step * dt,
                                       positionGoal.z + dz / len * step * dt);
        }

        position = Qt.vector3d(approach(position.x, positionGoal.x, smoothing, dt),
                               approach(position.y, positionGoal.y, smoothing, dt),
                               approach(position.z, positionGoal.z, smoothing, dt));
    }

    // --- Pointer input ----------------------------------------------------
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        cursorShape: root.dragButton === Qt.RightButton ? Qt.BlankCursor
                   : root.dragButton !== Qt.NoButton ? Qt.ClosedHandCursor
                   : Qt.ArrowCursor

        // Right-click transitions to free-look while retaining the current camera pose.
        onPressed: function (mouse) {
            root.forceActiveFocus();
            root.dragButton = mouse.button;
            root.lastX = mouse.x;
            root.lastY = mouse.y;
            root.lastMoveMs = Date.now();
            root.dragged = false;
            root.azimuthVelocity = 0;
            root.elevationVelocity = 0;

            if (mouse.button === Qt.RightButton && root.mode !== InspectionCameraController.FreeRoam) {
                root.yawGoal = root.yaw;
                root.pitchGoal = root.pitch;
                root.positionGoal = root.position;
                root.mode = InspectionCameraController.FreeRoam;
            }
        }

        onPositionChanged: function (mouse) {
            if (root.dragButton === Qt.NoButton)
                return;

            const dx = mouse.x - root.lastX;
            const dy = mouse.y - root.lastY;
            root.lastX = mouse.x;
            root.lastY = mouse.y;
            if (Math.abs(dx) + Math.abs(dy) > 2)
                root.dragged = true;

            // Real inter-event interval, so flick velocity is honest on any
            // pointer poll rate.
            const now = Date.now();
            const dt = Math.max(0.004, Math.min(0.05, (now - root.lastMoveMs) / 1000));
            root.lastMoveMs = now;

            if (root.dragButton === Qt.LeftButton) {
                if (root.mode === InspectionCameraController.DroneChase)
                    root.mode = InspectionCameraController.OrbitAsset;
                const dAz = -dx * root.orbitSensitivity;
                const dEl = dy * root.orbitSensitivity;
                root.azimuthGoal += dAz;
                root.elevationGoal = root.clamp(root.elevationGoal + dEl,
                                                root.minElevation, root.maxElevation);
                root.azimuthVelocity = dAz / dt;
                root.elevationVelocity = dEl / dt;
            } else if (root.dragButton === Qt.RightButton) {
                root.yawGoal -= dx * root.lookSensitivity;
                root.pitchGoal = root.clamp(root.pitchGoal - dy * root.lookSensitivity, -89, 89);
            } else if (root.dragButton === Qt.MiddleButton) {
                root.pan(dx, dy);
            }
        }

        // A click without meaningful drag is interpreted as a scene pick.
        onReleased: function (mouse) {
            const wasDrag = root.dragged;
            root.dragButton = Qt.NoButton;

            if (mouse.button === Qt.LeftButton && !wasDrag)
                root.pickAt(mouse.x, mouse.y, false);
        }

        onDoubleClicked: function (mouse) {
            if (mouse.button === Qt.LeftButton)
                root.pickAt(mouse.x, mouse.y, true);
        }
    }

    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: function (event) {
            // Exponential zoom: constant perceived speed at every scale, and it
            // can never step through the target.
            const notches = event.angleDelta.y !== 0 ? event.angleDelta.y : event.pixelDelta.y * 4;
            const factor = Math.exp(-notches * root.zoomSensitivity);

            if (root.mode === InspectionCameraController.FreeRoam) {
                root.flySpeed = root.clamp(root.flySpeed / factor, 5, 20000);
                return;
            }
            root.distanceGoal = root.clamp(root.distanceGoal * factor,
                                           root.minDistance, root.maxDistance);
        }
    }

    /*! Screen-space consistent pan: one pixel of drag moves one pixel of scene. */
    function pan(dx, dy) {
        if (!camera || height <= 0)
            return;

        const y = yaw * Math.PI / 180;
        const p = pitch * Math.PI / 180;
        const right = Qt.vector3d(Math.cos(y), 0, -Math.sin(y));
        const up = Qt.vector3d(Math.sin(p) * Math.sin(y), Math.cos(p), Math.sin(p) * Math.cos(y));

        const worldPerPixel = 2 * distance * Math.tan(camera.fieldOfView * Math.PI / 360) / height;
        const sx = -dx * worldPerPixel;
        const sy = dy * worldPerPixel;

        centerGoal = Qt.vector3d(centerGoal.x + right.x * sx + up.x * sy,
                                 centerGoal.y + right.y * sx + up.y * sy,
                                 centerGoal.z + right.z * sx + up.z * sy);
    }

    // Emits the picked object and optionally transitions the orbit target to that hit.
    function pickAt(x, y, focusResult) {
        if (!view)
            return;

        const result = view.pick(x, y);
        if (!result.objectHit)
            return;

        picked(result);
        surfacePicked(result.scenePosition, result.objectHit);

        if (focusResult) {
            centerGoal = result.scenePosition;
            distanceGoal = clamp(result.distance * 0.6, minDistance, maxDistance);
            mode = InspectionCameraController.OrbitAsset;
        }
    }

    // --- Keyboard ---------------------------------------------------------
    // F returns to asset orbit; C resumes chase only when a drone target exists.
    Keys.onPressed: function (event) {
        if (event.isAutoRepeat)
            return;
        heldKeys[event.key] = true;
        if (event.key === Qt.Key_F) {
            mode = InspectionCameraController.OrbitAsset;
            event.accepted = true;
        } else if (event.key === Qt.Key_C && chaseTarget) {
            mode = InspectionCameraController.DroneChase;
            event.accepted = true;
        }
    }

    Keys.onReleased: function (event) {
        if (!event.isAutoRepeat)
            heldKeys[event.key] = false;
    }
}
