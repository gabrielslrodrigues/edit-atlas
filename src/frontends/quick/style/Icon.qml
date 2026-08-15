import QtQuick

Image {
    fillMode: Image.PreserveAspectFit
    opacity: enabled ? 1.0 : 0.4
    sourceSize.height: DesignTokens.iconMedium
    sourceSize.width: DesignTokens.iconMedium
}
