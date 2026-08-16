import QtQuick
import QtQuick.Templates as T

T.ToolButton {
    id: control

    Accessible.id: objectName
    Accessible.name: text
    Accessible.role: Accessible.Button
    font.pointSize: DesignTokens.bodyPointSize
    padding: DesignTokens.spacingExtraSmall

    implicitHeight: DesignTokens.iconMedium + topPadding + bottomPadding
    implicitWidth: DesignTokens.iconMedium + leftPadding + rightPadding

    contentItem: Text {
        color: !control.enabled
               ? Theme.disabled : control.down
               ? Theme.textPrimary : Theme.textSecondary
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        text: control.text
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        border.color: control.activeFocus ? Theme.focus : "transparent"
        border.width: DesignTokens.borderWidth
        color: control.down
               ? Theme.controlPressed : control.hovered
               ? Theme.controlHovered : "transparent"
        radius: DesignTokens.radiusMedium

        Behavior on color {
            ColorAnimation {
                duration: DesignTokens.animationFast
            }
        }
    }
}
