import QtQuick
import QtQuick.Templates as T

T.MenuBar {
    id: control

    bottomPadding: 1
    spacing: 0

    implicitHeight: Math.max(implicitBackgroundHeight,
                             implicitContentHeight + topPadding
                             + bottomPadding)
    implicitWidth: Math.max(implicitBackgroundWidth,
                            implicitContentWidth + leftPadding
                            + rightPadding)

    delegate: MenuBarItem {}

    contentItem: Row {
        spacing: control.spacing

        Repeater {
            model: control.contentModel
        }
    }

    background: Rectangle {
        color: Theme.surface
        implicitHeight: DesignTokens.controlHeight

        Rectangle {
            anchors.bottom: parent.bottom
            color: Theme.border
            height: DesignTokens.borderWidth
            width: parent.width
        }
    }
}
