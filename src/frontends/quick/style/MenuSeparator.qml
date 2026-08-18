import QtQuick
import QtQuick.Templates as T

T.MenuSeparator {
    id: control

    bottomPadding: DesignTokens.spacingExtraSmall
    leftPadding: DesignTokens.spacingMedium
    rightPadding: DesignTokens.spacingMedium
    topPadding: DesignTokens.spacingExtraSmall

    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding

    contentItem: Rectangle {
        color: Theme.border
        implicitHeight: DesignTokens.borderWidth
        implicitWidth: 248
    }
}
