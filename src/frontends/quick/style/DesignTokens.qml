pragma Singleton

import QtQuick

QtObject {
    readonly property int bodyPointSize: 12
    readonly property int headingPointSize: 14
    readonly property int titlePointSize: 22

    readonly property int spacingExtraSmall: 4
    readonly property int spacingSmall: 8
    readonly property int spacingMedium: 12
    readonly property int spacingLarge: 16
    readonly property int spacingExtraLarge: 24

    readonly property int borderWidth: 1
    readonly property int controlHeight: 40
    readonly property int iconSmall: 16
    readonly property int iconMedium: 20
    readonly property int minimumButtonWidth: 112
    readonly property int radiusMedium: 8
    readonly property int radiusLarge: 12
    readonly property int textFieldWidth: 320

    readonly property int animationFast: 100
}
