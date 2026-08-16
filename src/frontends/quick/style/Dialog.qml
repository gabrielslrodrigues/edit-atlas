import QtQuick
import QtQuick.Templates as T

T.Dialog {
    id: control

    padding: DesignTokens.spacingMedium
    spacing: DesignTokens.spacingSmall

    Binding {
        property: "id"
        target: control.contentItem !== null
                ? control.contentItem.Accessible : null
        value: control.objectName
    }

    Binding {
        property: "name"
        target: control.contentItem !== null
                ? control.contentItem.Accessible : null
        value: control.title
    }

    Binding {
        property: "role"
        target: control.contentItem !== null
                ? control.contentItem.Accessible : null
        value: Accessible.Dialog
    }

    implicitHeight: Math.max(
                        implicitBackgroundHeight + topInset + bottomInset,
                        implicitContentHeight + topPadding + bottomPadding
                        + (implicitHeaderHeight > 0
                           ? implicitHeaderHeight + spacing : 0)
                        + (implicitFooterHeight > 0
                           ? implicitFooterHeight + spacing : 0))
    implicitWidth: Math.max(380,
                            implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding,
                            implicitHeaderWidth,
                            implicitFooterWidth)

    background: Rectangle {
        border.color: Theme.border
        border.width: DesignTokens.borderWidth
        color: Theme.surface
        radius: DesignTokens.radiusLarge
    }

    header: Text {
        bottomPadding: DesignTokens.spacingExtraSmall
        color: Theme.textPrimary
        elide: Text.ElideRight
        font.pointSize: DesignTokens.headingPointSize
        font.weight: Font.DemiBold
        leftPadding: DesignTokens.spacingMedium
        rightPadding: DesignTokens.spacingMedium
        text: control.title
        topPadding: DesignTokens.spacingMedium
        visible: control.title.length > 0
    }

    footer: DialogButtonBox {
        visible: count > 0
    }

    T.Overlay.modal: Rectangle {
        color: "#99000000"
    }

    T.Overlay.modeless: Rectangle {
        color: "#33000000"
    }
}
