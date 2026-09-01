pragma Singleton

import QtQuick

// Colors come from edit_atlas::presentation, so both frontends present the
// same palette and a change reaches every binding through `Appearance`.
QtObject {
    readonly property color accent: Appearance.palette.accent
    readonly property color accentHovered: Appearance.palette.accentHovered
    readonly property color accentPressed: Appearance.palette.accentPressed
    readonly property color border: Appearance.palette.border
    readonly property color control: Appearance.palette.control
    readonly property color controlHovered: Appearance.palette.controlHovered
    readonly property color controlPressed: Appearance.palette.controlPressed
    readonly property color danger: Appearance.palette.danger
    readonly property color disabled: Appearance.palette.disabled
    readonly property color focus: Appearance.palette.focus
    readonly property color onAccent: Appearance.palette.onAccent
    readonly property color surface: Appearance.palette.surface
    readonly property color surfaceAlternate:
        Appearance.palette.surfaceAlternate
    readonly property color textPrimary: Appearance.palette.textPrimary
    readonly property color textSecondary: Appearance.palette.textSecondary
    readonly property color tooltipSurface: Appearance.palette.tooltipSurface
    readonly property color tooltipText: Appearance.palette.tooltipText
    readonly property color warning: Appearance.palette.warning
    readonly property color window: Appearance.palette.window
}
