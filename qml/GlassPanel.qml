/**
 * @file     : GlassPanel.qml
 * @brief    : Defines the shared translucent panel surface.
 * @details  : Provides common styling for AeroTwin interface panels.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

import QtQuick
import QtQuick.Effects
import AeroTwinGCS

/*!
    Frosted-glass shelf.

    Samples whatever Item is assigned to \l blurSource (normally the 3D
    viewport underneath), blurs it with MultiEffect, masks it to a rounded
    rectangle and lays a translucent dark tint plus a cyan hairline on top.

    Children are declared directly:  GlassPanel { Text { ... } }
*/
Item {
    id: root

    /** Source item sampled to produce the optional frosted backdrop. */
    property Item blurSource: null
    /** Controls whether the live backdrop blur pipeline is active. */
    property bool frosted: true
    property real cornerRadius: Theme.radius
    property color tint: Theme.glass
    property color strokeColor: Theme.glassBorder
    property real strokeWidth: 1
    property real blurAmount: 1.0
    property real blurMax: 48
    property real contentMargins: Theme.spacing
    property bool glow: false

    /** Default child-content slot, inset by contentMargins. */
    default property alias content: contentItem.data

    // Rect of this panel expressed in blurSource coordinates.
    property rect captureRect: Qt.rect(0, 0, 0, 0)

    // Maps this moving panel into source coordinates before each blur capture.
    function refreshCapture() {
        if (!blurSource || width <= 0 || height <= 0) {
            captureRect = Qt.rect(0, 0, 0, 0);
            return;
        }
        const p = root.mapToItem(blurSource, 0, 0);
        captureRect = Qt.rect(p.x, p.y, root.width, root.height);
    }

    // Panels slide/fade, so the sampled rect has to follow them every frame.
    FrameAnimation {
        running: root.visible && root.frosted && root.blurSource !== null
        onTriggered: root.refreshCapture()
    }

    ShaderEffectSource {
        id: backdrop
        anchors.fill: parent
        visible: false
        live: true
        hideSource: false
        sourceItem: root.blurSource
        sourceRect: root.captureRect
        textureMirroring: ShaderEffectSource.NoMirroring
    }

    Rectangle {
        id: shapeMask
        anchors.fill: parent
        radius: root.cornerRadius
        color: "black"
        visible: false
        layer.enabled: true
        layer.smooth: true
    }

    MultiEffect {
        anchors.fill: parent
        visible: root.frosted && root.blurSource !== null
        source: backdrop
        blurEnabled: true
        blur: root.blurAmount
        blurMax: root.blurMax
        blurMultiplier: 1.0
        saturation: -0.25
        maskEnabled: true
        maskSource: shapeMask
    }

    Rectangle {
        id: sheet
        anchors.fill: parent
        radius: root.cornerRadius
        border.width: root.strokeWidth
        border.color: root.strokeColor
        gradient: Gradient {
            GradientStop { position: 0.0; color: root.tint }
            GradientStop { position: 1.0; color: Qt.darker(root.tint, 1.35) }
        }
    }

    // Specular top edge - sells the "pane of glass" read.
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: root.cornerRadius * 0.6
        height: 1
        visible: root.frosted
        color: "#1AFFFFFF"
    }

    Rectangle {
        anchors.fill: parent
        radius: root.cornerRadius
        color: "transparent"
        visible: root.glow
        border.width: 1
        border.color: Theme.glassBorderStrong
        opacity: 0.8
    }

    Item {
        id: contentItem
        anchors.fill: parent
        anchors.margins: root.contentMargins
        clip: true
    }
}
