import EditAtlasStyle as Atlas
import QtQuick
import QtQuick.Controls

ApplicationWindow {
    height: 640
    minimumHeight: 480
    minimumWidth: 640
    title: qsTr("Edit Atlas Quick")
    visible: true
    width: 960
    color: Atlas.Theme.window

    Column {
        anchors.fill: parent
        anchors.margins: Atlas.DesignTokens.spacingExtraLarge
        spacing: Atlas.DesignTokens.spacingLarge

        Label {
            color: Atlas.Theme.textPrimary
            font.pointSize: Atlas.DesignTokens.titlePointSize
            font.weight: Font.DemiBold
            text: qsTr("Edit Atlas design system")
        }

        Label {
            color: Atlas.Theme.textSecondary
            font.pointSize: Atlas.DesignTokens.bodyPointSize
            text: qsTr("Reusable controls follow the active theme and font scale.")
        }

        Atlas.Surface {
            height: previewContent.implicitHeight
                    + Atlas.DesignTokens.spacingExtraLarge * 2
            width: parent.width

            Column {
                id: previewContent

                anchors.left: parent.left
                anchors.margins: Atlas.DesignTokens.spacingExtraLarge
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: Atlas.DesignTokens.spacingLarge

                Label {
                    color: Atlas.Theme.textPrimary
                    font.pointSize: Atlas.DesignTokens.headingPointSize
                    font.weight: Font.DemiBold
                    text: qsTr("Control preview")
                }

                TextField {
                    placeholderText: qsTr("Timeline title")
                    width: Math.min(parent.width,
                                    Atlas.DesignTokens.textFieldWidth)
                }

                Row {
                    spacing: Atlas.DesignTokens.spacingMedium

                    Button {
                        highlighted: true
                        text: qsTr("Primary action")
                    }

                    Button {
                        text: Atlas.Theme.dark
                              ? qsTr("Use light theme")
                              : qsTr("Use dark theme")
                        onClicked: Atlas.Theme.dark = !Atlas.Theme.dark
                    }
                }
            }
        }
    }
}
