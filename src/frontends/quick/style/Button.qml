import QtQuick
import QtQuick.Templates as T

T.Button {
    id: control

    Accessible.id: objectName
    Accessible.name: text
    Accessible.role: Accessible.Button
    bottomPadding: DesignTokens.spacingSmall
    font.pointSize: DesignTokens.bodyPointSize
    leftPadding: DesignTokens.spacingLarge
    rightPadding: DesignTokens.spacingLarge
    topPadding: DesignTokens.spacingSmall

    implicitHeight: Math.max(DesignTokens.controlHeight,
                             contentItem.implicitHeight + topPadding
                             + bottomPadding)
    implicitWidth: Math.max(DesignTokens.minimumButtonWidth,
                            contentItem.implicitWidth + leftPadding
                            + rightPadding)

    contentItem: Text {
        color: !control.enabled
               ? Theme.disabled
               : control.highlighted ? Theme.onAccent : Theme.textPrimary
        elide: Text.ElideRight
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        text: control.text
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        border.color: control.activeFocus ? Theme.focus : Theme.border
        border.width: DesignTokens.borderWidth
        color: {
            if (control.highlighted) {
                if (control.down) {
                    return Theme.accentPressed
                }
                return control.hovered ? Theme.accentHovered : Theme.accent
            }
            if (control.down) {
                return Theme.controlPressed
            }
            return control.hovered ? Theme.controlHovered : Theme.control
        }
        radius: DesignTokens.radiusMedium

        Behavior on color {
            ColorAnimation {
                duration: DesignTokens.animationFast
            }
        }
    }
}
