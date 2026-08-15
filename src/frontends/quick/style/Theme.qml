pragma Singleton

import QtQuick

QtObject {
    property bool dark: true

    readonly property color accent: dark ? "#7aa2ff" : "#315fcb"
    readonly property color accentHovered: dark ? "#91b2ff" : "#254fae"
    readonly property color accentPressed: dark ? "#638ce8" : "#1d4193"
    readonly property color border: dark ? "#3b424d" : "#c9ced6"
    readonly property color control: dark ? "#262b33" : "#ffffff"
    readonly property color controlHovered: dark ? "#303741" : "#f1f3f6"
    readonly property color controlPressed: dark ? "#1f242b" : "#e4e7ec"
    readonly property color disabled: dark ? "#555c66" : "#9aa1ac"
    readonly property color focus: dark ? "#90b4ff" : "#315fcb"
    readonly property color onAccent: dark ? "#101319" : "#ffffff"
    readonly property color surface: dark ? "#1d2128" : "#ffffff"
    readonly property color textPrimary: dark ? "#f0f2f5" : "#20242b"
    readonly property color textSecondary: dark ? "#aeb5bf" : "#626a75"
    readonly property color window: dark ? "#14171c" : "#f4f5f7"
}
