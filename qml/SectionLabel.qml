/**
 * @file     : SectionLabel.qml
 * @brief    : Defines a reusable section label component.
 * @details  : Provides consistent headings for dashboard sections.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

import QtQuick
import AeroTwinGCS

/** Shared uppercase section heading used by dashboard panels. */
Text {
    text: ""
    color: Theme.textDim
    font.family: Theme.uiFamily
    font.pixelSize: Theme.fontLabel
    font.bold: true
    font.letterSpacing: 1.6
    font.capitalization: Font.AllUppercase
}
