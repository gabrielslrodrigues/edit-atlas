import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T

T.Menu {
    id: control

    bottomPadding: DesignTokens.spacingExtraSmall
    leftPadding: DesignTokens.spacingExtraSmall
    margins: DesignTokens.spacingSmall
    overlap: 0
    rightPadding: DesignTokens.spacingExtraSmall
    topPadding: DesignTokens.spacingExtraSmall

    implicitHeight: Math.max(implicitBackgroundHeight,
                             implicitContentHeight + topPadding
                             + bottomPadding)
    implicitWidth: Math.max(implicitBackgroundWidth,
                            implicitContentWidth + leftPadding
                            + rightPadding)

    delegate: MenuItem {}

    contentItem: ListView {
        clip: true
        currentIndex: control.currentIndex
        implicitHeight: contentHeight
        model: control.contentModel

        ScrollIndicator.vertical: ScrollIndicator {}
    }

    background: Rectangle {
        border.color: Theme.border
        border.width: DesignTokens.borderWidth
        color: Theme.surface
        implicitHeight: DesignTokens.controlHeight
        implicitWidth: 280
        radius: DesignTokens.radiusMedium
    }
}
