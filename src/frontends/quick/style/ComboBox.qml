pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Templates as T

T.ComboBox {
    id: control

    Accessible.id: objectName
    Accessible.role: Accessible.ComboBox
    bottomPadding: DesignTokens.spacingSmall
    leftPadding: DesignTokens.spacingMedium
    rightPadding: DesignTokens.spacingExtraLarge
    topPadding: DesignTokens.spacingSmall

    implicitHeight: Math.max(DesignTokens.controlHeight,
                             contentItem.implicitHeight + topPadding
                             + bottomPadding)
    implicitWidth: Math.max(240, contentItem.implicitWidth + leftPadding
                            + rightPadding)

    delegate: T.ItemDelegate {
        id: option

        required property int index
        required property var model

        Accessible.name: control.textAt(index)
        Accessible.role: Accessible.ListItem
        Accessible.selectable: true
        Accessible.selected: control.currentIndex === index
        bottomPadding: DesignTokens.spacingSmall
        highlighted: control.highlightedIndex === index
        hoverEnabled: control.hoverEnabled
        height: implicitHeight
        implicitHeight: Math.max(DesignTokens.controlHeight,
                                 contentItem.implicitHeight + topPadding
                                 + bottomPadding)
        leftPadding: DesignTokens.spacingMedium
        rightPadding: DesignTokens.spacingMedium
        topPadding: DesignTokens.spacingSmall
        width: ListView.view.width

        contentItem: Text {
            color: option.enabled ? Theme.textPrimary : Theme.disabled
            elide: Text.ElideRight
            font.pointSize: DesignTokens.bodyPointSize
            font.weight: control.currentIndex === option.index
                         ? Font.DemiBold : Font.Normal
            text: control.textAt(option.index)
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            color: option.highlighted ? Theme.controlHovered : "transparent"
            radius: DesignTokens.radiusMedium
        }
    }

    indicator: Text {
        color: control.enabled ? Theme.textSecondary : Theme.disabled
        font.pointSize: DesignTokens.bodyPointSize
        height: control.availableHeight
        horizontalAlignment: Text.AlignHCenter
        text: "▾"
        verticalAlignment: Text.AlignVCenter
        width: DesignTokens.iconMedium
        x: control.mirrored
           ? control.leftPadding
           : control.width - width - DesignTokens.spacingSmall
        y: control.topPadding
    }

    contentItem: Text {
        color: control.enabled ? Theme.textPrimary : Theme.disabled
        elide: Text.ElideRight
        font: control.font
        text: control.displayText
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        border.color: control.activeFocus ? Theme.focus : Theme.border
        border.width: DesignTokens.borderWidth
        color: control.down ? Theme.controlPressed : Theme.control
        radius: DesignTokens.radiusMedium
    }

    popup: T.Popup {
        bottomMargin: DesignTokens.spacingSmall
        height: Math.min(contentItem.implicitHeight + topPadding
                         + bottomPadding,
                         control.Window.height - topMargin - bottomMargin)
        leftPadding: DesignTokens.spacingExtraSmall
        rightPadding: DesignTokens.spacingExtraSmall
        topMargin: DesignTokens.spacingSmall
        topPadding: DesignTokens.spacingExtraSmall
        bottomPadding: DesignTokens.spacingExtraSmall
        width: control.width
        y: control.height + DesignTokens.spacingExtraSmall

        contentItem: ListView {
            clip: true
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0
            implicitHeight: contentHeight
            model: control.delegateModel
        }

        background: Rectangle {
            border.color: Theme.border
            border.width: DesignTokens.borderWidth
            color: Theme.surface
            radius: DesignTokens.radiusMedium
        }
    }
}
