/**
 * @file     : Theme.qml
 * @brief    : Defines shared application colors, typography, and spacing.
 * @details  : Provides visual constants for the AeroTwin user interface.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

pragma Singleton

import QtQuick

QtObject {
    id: theme

    /** Shared immutable visual tokens consumed by all AeroTwin QML components. */
    // --- Surfaces ---------------------------------------------------------
    readonly property color background: "#0d1117"
    readonly property color backgroundDeep: "#070b10"
    readonly property color glass: "#B3101720"          // 70% dark, frosted overlay tint
    readonly property color glassRaised: "#CC141C26"
    readonly property color glassBorder: "#00e5ff33"
    readonly property color glassBorderStrong: "#00e5ff66"
    readonly property color scrim: "#99000000"

    // --- Accents ----------------------------------------------------------
    readonly property color accent: "#00e5ff"
    readonly property color accentDim: "#0097a7"
    readonly property color good: "#3fdd85"
    readonly property color warn: "#ffb454"
    readonly property color bad: "#ff4d5e"
    readonly property color hotspot: "#ff7043"

    // --- Text -------------------------------------------------------------
    readonly property color textPrimary: "#e6edf3"
    readonly property color textSecondary: "#9fb0c0"
    readonly property color textDim: "#5d6b7a"

    // --- Typography -------------------------------------------------------
    readonly property string uiFamily: "Segoe UI"
    readonly property string monoFamily: "Consolas"
    readonly property int fontLabel: 12
    readonly property int fontBody: 14
    readonly property int fontTitle: 16
    readonly property int fontValue: 20
    readonly property int fontHero: 26

    // --- Metrics ----------------------------------------------------------
    readonly property int radius: 14
    readonly property int radiusSmall: 8
    readonly property int spacing: 12
    readonly property int spacingSmall: 8
    readonly property int panelMargin: 16
    readonly property int headerHeight: 74
    readonly property int dockHeight: 116
    readonly property int controlHeight: 40
    readonly property int rowHeight: 36

    // --- Motion -----------------------------------------------------------
    readonly property int durationFast: 120
    readonly property int durationBase: 220
    readonly property int durationSlow: 380
    readonly property int easing: Easing.OutCubic

    // Converts boolean health and warning state to the standard dashboard palette.
    function statusColor(ok, warning) {
        return ok ? theme.good : (warning ? theme.warn : theme.bad);
    }
}
