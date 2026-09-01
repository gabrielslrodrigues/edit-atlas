pragma Singleton

import QtQuick

QtObject {
    // Typography comes from edit_atlas::presentation so both frontends
    // render the same hierarchy; only dimensions below are Quick's own.
    readonly property int bodyPointSize: Typography.bodyPointSize
    readonly property int headingPointSize: Typography.headingPointSize
    readonly property int titlePointSize: Typography.titlePointSize
    readonly property int bodyWeight: Typography.bodyWeight
    readonly property int headingWeight: Typography.headingWeight
    readonly property int titleWeight: Typography.titleWeight

    readonly property int spacingExtraSmall: 4
    readonly property int spacingSmall: 8
    readonly property int spacingMedium: 12
    readonly property int spacingLarge: 16
    readonly property int spacingExtraLarge: 24

    readonly property int borderWidth: 1
    readonly property int controlHeight: 40
    readonly property int iconSmall: 16
    readonly property int iconMedium: 20
    readonly property int applicationIconSize: 128
    readonly property int minimumButtonWidth: 112
    readonly property int radiusMedium: 8
    readonly property int radiusLarge: 12
    readonly property int textFieldWidth: 320

    readonly property int animationFast: 100
}
