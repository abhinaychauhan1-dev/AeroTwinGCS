/**
 * @file     : CameraFrustum.qml
 * @brief    : Visualizes the drone camera viewing frustum.
 * @details  : Renders the camera beam, footprint, and scanning plane.
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
    Camera frustum visualiser.

    Parent it to the gimbal Node - the geometry apex sits at the local origin
    and opens along -Z, matching the Qt Quick 3D camera convention, so the
    frustum inherits the gimbal's pan/tilt for free:

        Node {                       // airframe
            Node {                   // gimbal
                eulerRotation.x: -35
                CameraFrustum { horizontalFov: 84; projectionRange: 45 }
            }
        }

    For the neon look, use ExtendedSceneEnvironment with glowEnabled: true -
    the emissive line colour then blooms into the surrounding pixels.
*/
Node {
    id: frustum

    /** Camera optics define the shared wireframe, beam, footprint, and scan-plane shape. */
    property real horizontalFov: 78
    property real aspectRatio: 16 / 9
    property real projectionRange: 40
    property real nearPlane: 0.4

    /** Visibility and opacity controls let the operator reduce 3D overlay clutter. */
    property color beamColor: "#00e5ff"
    property real beamOpacity: 0.16
    property real lineOpacity: 0.95
    property bool beamVisible: true
    property bool footprintVisible: true
    property bool scanning: true

    // Derived footprint extents keep the projected rectangle aligned with the frustum geometry.
    readonly property real verticalFov: wireGeometry.verticalFov
    readonly property real farHalfWidth: Math.tan(horizontalFov * Math.PI / 360) * projectionRange
    readonly property real farHalfHeight: farHalfWidth / aspectRatio

    // --- Wireframe pyramid -------------------------------------------------
    Model {
        // Custom C++ geometry exposes the camera-local line topology and depth alpha.
        geometry: FrustumGeometry {
            id: wireGeometry
            style: FrustumGeometry.Wireframe
            horizontalFov: frustum.horizontalFov
            aspectRatio: frustum.aspectRatio
            projectionRange: frustum.projectionRange
            nearPlane: frustum.nearPlane
            farAlpha: 0.35
        }

        materials: PrincipledMaterial {
            lighting: PrincipledMaterial.NoLighting
            baseColor: frustum.beamColor
            emissiveFactor: Qt.vector3d(frustum.beamColor.r,
                                        frustum.beamColor.g,
                                        frustum.beamColor.b)
            vertexColorsEnabled: true          // multiplies in the depth-fade alpha ramp
            alphaMode: PrincipledMaterial.Blend
            opacity: frustum.lineOpacity
            cullMode: Material.NoCulling
            lineWidth: 2.0                     // honoured only where the RHI backend supports it
        }
    }

    // --- Translucent raycast beam -----------------------------------------
    Model {
        visible: frustum.beamVisible

        // The same solver switches to indexed triangles for the translucent volume.
        geometry: FrustumGeometry {
            style: FrustumGeometry.Beam
            horizontalFov: frustum.horizontalFov
            aspectRatio: frustum.aspectRatio
            projectionRange: frustum.projectionRange
            farAlpha: 0.0                      // dissolves completely at the far plane
        }

        materials: PrincipledMaterial {
            lighting: PrincipledMaterial.NoLighting
            baseColor: frustum.beamColor
            vertexColorsEnabled: true
            alphaMode: PrincipledMaterial.Blend
            blendMode: PrincipledMaterial.Screen
            opacity: frustum.beamOpacity
            cullMode: Material.NoCulling
            depthDrawMode: Material.NeverDepthDraw   // never occludes the asset behind it
        }
    }

    // --- Coverage footprint on the target surface -------------------------
    Model {
        source: "#Rectangle"
        visible: frustum.footprintVisible
        z: -frustum.projectionRange
        scale: Qt.vector3d(frustum.farHalfWidth * 2 / 100,
                           frustum.farHalfHeight * 2 / 100,
                           1)

        materials: PrincipledMaterial {
            lighting: PrincipledMaterial.NoLighting
            baseColor: frustum.beamColor
            alphaMode: PrincipledMaterial.Blend
            opacity: 0.10
            cullMode: Material.NoCulling
            depthDrawMode: Material.NeverDepthDraw
        }
    }

    // --- Sweeping scan plane ----------------------------------------------
    Model {
        id: scanPlane
        source: "#Rectangle"
        visible: frustum.scanning

        property real distance: frustum.nearPlane

        z: -distance
        // Recalculate the moving plane dimensions at its current distance from the apex.
        scale: {
            const halfWidth = Math.tan(frustum.horizontalFov * Math.PI / 360) * distance;
            return Qt.vector3d(halfWidth * 2 / 100,
                               halfWidth * 2 / frustum.aspectRatio / 100,
                               1);
        }

        materials: PrincipledMaterial {
            lighting: PrincipledMaterial.NoLighting
            baseColor: frustum.beamColor
            alphaMode: PrincipledMaterial.Blend
            opacity: 0.22 * (1.0 - scanPlane.distance / frustum.projectionRange)
            cullMode: Material.NoCulling
            depthDrawMode: Material.NeverDepthDraw
        }

        NumberAnimation on distance {
            running: frustum.scanning
            loops: Animation.Infinite
            from: frustum.nearPlane
            to: frustum.projectionRange
            duration: 2200
            easing.type: Easing.InOutSine
        }
    }
}
