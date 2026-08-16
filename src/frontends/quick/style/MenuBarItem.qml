import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T

T.MenuBarItem {
    id: control

    Accessible.id: objectName
    Accessible.name: text
    Accessible.role: Accessible.MenuItem
    bottomPadding: DesignTokens.spacingSmall
    leftPadding: DesignTokens.spacingMedium
    rightPadding: DesignTokens.spacingMedium
    topPadding: DesignTokens.spacingSmall

    implicitHeight: Math.max(DesignTokens.controlHeight,
                             contentItem.implicitHeight + topPadding
                             + bottomPadding)
    implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding
    objectName: menu !== null ? menu.objectName : ""

    contentItem: MnemonicLabel {
        color: control.enabled ? Theme.textPrimary : Theme.disabled
        elide: Text.ElideRight
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        text: control.text
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        color: control.down || control.highlighted
               ? Theme.controlHovered : "transparent"
        radius: DesignTokens.radiusMedium
    }
}
