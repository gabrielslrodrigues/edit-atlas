import QtQuick
import QtQuick.Templates as T

T.TextField {
    id: control

    Accessible.id: objectName
    Accessible.role: Accessible.EditableText
    bottomPadding: DesignTokens.spacingSmall
    color: enabled ? Theme.textPrimary : Theme.disabled
    font.pointSize: DesignTokens.bodyPointSize
    leftPadding: DesignTokens.spacingMedium
    placeholderTextColor: Theme.textSecondary
    rightPadding: DesignTokens.spacingMedium
    selectedTextColor: Theme.onAccent
    selectionColor: Theme.accent
    topPadding: DesignTokens.spacingSmall

    implicitHeight: Math.max(DesignTokens.controlHeight,
                             contentHeight + topPadding + bottomPadding)
    implicitWidth: DesignTokens.textFieldWidth

    background: Rectangle {
        border.color: control.activeFocus ? Theme.focus : Theme.border
        border.width: DesignTokens.borderWidth
        color: Theme.control
        radius: DesignTokens.radiusMedium
    }
}
