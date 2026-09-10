/**
 * @file     : AttitudeIndicator.qml
 * @brief    : Renders the drone pitch and roll indicator.
 * @details  : Provides a primary flight display attitude visualization.
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
    Primary flight display attitude indicator (artificial horizon).
    Positive pitch = nose up, positive roll = right wing down.
*/
Item {
    id: adi

    /** Live aircraft attitude; positive pitch is nose-up and positive roll is right-wing-down. */
    property real pitch: 0
    property real roll: 0
    property real pixelsPerDegree: height / 55
    property color skyColor: "#12496b"
    property color groundColor: "#4a3320"

    implicitWidth: 200
    implicitHeight: 200

    Item {
        id: horizonLayer
        anchors.fill: parent
        visible: false
        layer.enabled: true
        layer.smooth: true

        Item {
            anchors.fill: parent

            // Translate pitch first and rotate the complete horizon layer for roll.
            transform: [
                Translate { y: adi.pitch * adi.pixelsPerDegree },
                Rotation {
                    origin.x: adi.width / 2
                    origin.y: adi.height / 2
                    angle: -adi.roll
                }
            ]

            Rectangle {
                x: -adi.width
                width: adi.width * 3
                y: -adi.height * 2
                height: adi.height * 2 + adi.height / 2
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.darker(adi.skyColor, 1.8) }
                    GradientStop { position: 1.0; color: adi.skyColor }
                }
            }

            Rectangle {
                x: -adi.width
                width: adi.width * 3
                y: adi.height / 2
                height: adi.height * 2.5
                gradient: Gradient {
                    GradientStop { position: 0.0; color: adi.groundColor }
                    GradientStop { position: 1.0; color: Qt.darker(adi.groundColor, 2.0) }
                }
            }

            // Horizon line
            Rectangle {
                x: -adi.width
                width: adi.width * 3
                y: adi.height / 2 - 1
                height: 2
                color: "#e8f6ff"
            }

            // Pitch ladder
            Repeater {
                model: [-30, -20, -10, 10, 20, 30]

                Item {
                    required property int modelData
                    width: adi.width
                    height: 1
                    y: adi.height / 2 - modelData * adi.pixelsPerDegree
                    x: 0

                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.abs(parent.modelData) === 10 ? adi.width * 0.34 : adi.width * 0.22
                        height: 1.5
                        color: "#CCE8F6FF"
                    }

                    Text {
                        anchors.right: parent.horizontalCenter
                        anchors.rightMargin: adi.width * 0.2
                        anchors.verticalCenter: parent.verticalCenter
                        text: Math.abs(parent.modelData)
                        color: "#CCE8F6FF"
                        font.family: Theme.monoFamily
                        font.pixelSize: 11
                    }
                }
            }
        }
    }

    Rectangle {
        id: circleMask
        anchors.fill: parent
        radius: width / 2
        color: "black"
        visible: false
        layer.enabled: true
        layer.smooth: true
    }

    // Crops the larger, rotating horizon texture to an instrument circle.
    MultiEffect {
        anchors.fill: parent
        source: horizonLayer
        maskEnabled: true
        maskSource: circleMask
    }

    // Roll scale (fixed)
    Repeater {
        model: [-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60]

        Item {
            required property int modelData
            anchors.fill: parent
            rotation: modelData

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                y: 3
                width: modelData % 30 === 0 ? 2 : 1
                height: modelData % 30 === 0 ? 9 : 5
                color: Theme.textPrimary
                opacity: 0.85
            }
        }
    }

    // Roll pointer
    Canvas {
        id: rollPointer
        anchors.fill: parent
        rotation: -adi.roll

        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            const cx = width / 2;
            ctx.beginPath();
            ctx.moveTo(cx, 14);
            ctx.lineTo(cx - 7, 25);
            ctx.lineTo(cx + 7, 25);
            ctx.closePath();
            ctx.fillStyle = Qt.rgba(0, 0.9, 1, 1);
            ctx.fill();
        }
    }

    // Fixed aircraft reference symbol
    Item {
        anchors.fill: parent

        Rectangle {
            x: parent.width / 2 - parent.width * 0.34
            y: parent.height / 2 - 1.5
            width: parent.width * 0.18
            height: 3
            color: Theme.accent
        }
        Rectangle {
            x: parent.width / 2 + parent.width * 0.16
            y: parent.height / 2 - 1.5
            width: parent.width * 0.18
            height: 3
            color: Theme.accent
        }
        Rectangle {
            anchors.centerIn: parent
            width: 6
            height: 6
            radius: 3
            color: "transparent"
            border.width: 2
            border.color: Theme.accent
        }
    }

    // Bezel
    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "transparent"
        border.width: 1
        border.color: Theme.glassBorderStrong
    }
}
