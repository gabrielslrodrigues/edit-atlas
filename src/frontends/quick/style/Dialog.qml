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
    // The title's natural width is measured rather than read back from
    // implicitHeaderWidth. The header elides, and a dialog stretches its
    // header to its own width, so an elided header reports a width that
    // depends on the width it is given, which loops back into this
    // binding.
    implicitWidth: Math.max(380,
                            implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding,
                            titleMetrics.width + headerLabel.leftPadding
                            + headerLabel.rightPadding,
                            implicitFooterWidth)

    TextMetrics {
        id: titleMetrics

        font: headerLabel.font
        text: control.title
    }

    background: Rectangle {
        border.color: Theme.border
        border.width: DesignTokens.borderWidth
        color: Theme.surface
        radius: DesignTokens.radiusLarge
    }

    header: Text {
        id: headerLabel

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
