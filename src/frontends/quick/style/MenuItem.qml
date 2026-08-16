import QtQuick
import QtQuick.Controls.impl
import QtQuick.Templates as T

T.MenuItem {
    id: control

    bottomPadding: DesignTokens.spacingSmall
    leftPadding: DesignTokens.spacingMedium
    rightPadding: DesignTokens.spacingMedium
    spacing: DesignTokens.spacingSmall
    topPadding: DesignTokens.spacingSmall

    implicitHeight: Math.max(DesignTokens.controlHeight,
                             contentItem.implicitHeight + topPadding
                             + bottomPadding)
    implicitWidth: Math.max(272, contentItem.implicitWidth + leftPadding
                            + rightPadding)

    contentItem: IconLabel {
        readonly property real arrowPadding: control.subMenu
                                              ? DesignTokens.iconSmall
                                                + control.spacing : 0
        readonly property real indicatorPadding: control.checkable
                                                  ? DesignTokens.iconSmall
                                                    + control.spacing : 0

        alignment: Qt.AlignLeft
        color: control.enabled ? Theme.textPrimary : Theme.disabled
        display: control.display
        font: control.font
        icon: control.icon
        leftPadding: control.mirrored ? arrowPadding : indicatorPadding
        mirrored: control.mirrored
        rightPadding: control.mirrored ? indicatorPadding : arrowPadding
        spacing: control.spacing
        text: control.text
    }

    indicator: Text {
        color: control.enabled ? Theme.accent : Theme.disabled
        font.bold: true
        font.pointSize: DesignTokens.bodyPointSize
        height: DesignTokens.iconSmall
        horizontalAlignment: Text.AlignHCenter
        text: control.checked ? "✓" : ""
        verticalAlignment: Text.AlignVCenter
        visible: control.checkable
        width: DesignTokens.iconSmall
        x: control.mirrored
           ? control.width - width - control.rightPadding
           : control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
    }

    arrow: Text {
        color: control.enabled ? Theme.textSecondary : Theme.disabled
        font.pointSize: DesignTokens.bodyPointSize
        height: DesignTokens.iconSmall
        horizontalAlignment: Text.AlignHCenter
        text: control.subMenu ? "›" : ""
        verticalAlignment: Text.AlignVCenter
        visible: control.subMenu
        width: DesignTokens.iconSmall
        x: control.mirrored
           ? control.leftPadding
           : control.width - width - control.rightPadding
        y: control.topPadding + (control.availableHeight - height) / 2
    }

    background: Rectangle {
        color: {
            if (control.down) {
                return Theme.controlPressed
            }
            return control.highlighted ? Theme.controlHovered : "transparent"
        }
        radius: DesignTokens.radiusMedium
    }
}
