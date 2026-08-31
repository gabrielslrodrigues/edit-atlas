import QtQuick
import QtQuick.Templates as T

T.CheckBox {
    id: control

    Accessible.id: objectName
    Accessible.name: text
    Accessible.role: Accessible.CheckBox
    spacing: DesignTokens.spacingSmall

    implicitHeight: Math.max(DesignTokens.controlHeight,
                             contentItem.implicitHeight + topPadding
                             + bottomPadding)
    implicitWidth: control.indicator.implicitWidth + spacing
                   + control.contentItem.implicitWidth
    leftPadding: 0

    indicator: Rectangle {
        implicitHeight: DesignTokens.iconMedium
        implicitWidth: DesignTokens.iconMedium
        x: control.leftPadding
        y: (control.height - height) / 2
        border.color: control.activeFocus ? Theme.focus : Theme.border
        border.width: DesignTokens.borderWidth
        color: control.checked ? Theme.accent : Theme.control
        radius: DesignTokens.spacingExtraSmall

        Text {
            anchors.centerIn: parent
            color: Theme.onAccent
            font.bold: true
            text: control.checked ? "✓" : ""
        }
    }

    contentItem: Text {
        color: control.enabled ? Theme.textPrimary : Theme.disabled
        font: control.font
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        verticalAlignment: Text.AlignVCenter
    }
}
