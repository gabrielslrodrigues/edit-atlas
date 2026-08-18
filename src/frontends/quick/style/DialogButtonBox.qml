import QtQuick
import QtQuick.Templates as T

T.DialogButtonBox {
    id: control

    alignment: Qt.AlignRight
    bottomPadding: DesignTokens.spacingSmall
    leftPadding: DesignTokens.spacingMedium
    rightPadding: DesignTokens.spacingMedium
    spacing: DesignTokens.spacingSmall
    topPadding: DesignTokens.spacingSmall

    implicitHeight: Math.max(implicitBackgroundHeight,
                             implicitContentHeight + topPadding
                             + bottomPadding)
    implicitWidth: Math.max(implicitBackgroundWidth,
                            implicitContentWidth + leftPadding
                            + rightPadding)

    delegate: Button {}

    contentItem: ListView {
        boundsBehavior: Flickable.StopAtBounds
        implicitWidth: contentWidth
        model: control.contentModel
        orientation: ListView.Horizontal
        spacing: control.spacing
    }

    background: Item {
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            color: Theme.border
            height: DesignTokens.borderWidth
        }
    }
}
