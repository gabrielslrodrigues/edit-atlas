import EditAtlasStyle as Atlas
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Dialog {
    id: root

    required property ApplicationInformation information

    property string externalFailureText: ""
    property bool showVideoBuildDetails: false

    function openExternal(action, failureText) {
        if (!action()) {
            externalFailureText = failureText
            externalFailureDialog.open()
        }
    }

    anchors.centerIn: parent
    height: Math.min(680, parent.height - Atlas.DesignTokens.spacingExtraLarge)
    modal: true
    parent: Overlay.overlay
    standardButtons: Dialog.Close
    title: qsTr("About Edit Atlas")
    width: Math.min(660, parent.width - Atlas.DesignTokens.spacingExtraLarge)

    ScrollView {
        anchors.fill: parent
        clip: true

        ColumnLayout {
            width: root.availableWidth
            spacing: Atlas.DesignTokens.spacingMedium

            Image {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredHeight: Atlas.DesignTokens.applicationIconSize
                Layout.preferredWidth: Atlas.DesignTokens.applicationIconSize
                Accessible.name: qsTr("Edit Atlas application icon")
                fillMode: Image.PreserveAspectFit
                smooth: true
                source: "qrc:/icons/edit_atlas.png"
                sourceSize.height: Math.ceil(
                    Atlas.DesignTokens.applicationIconSize
                    * Screen.devicePixelRatio)
                sourceSize.width: Math.ceil(
                    Atlas.DesignTokens.applicationIconSize
                    * Screen.devicePixelRatio)
            }

            Label {
                Layout.fillWidth: true
                color: Atlas.Theme.textPrimary
                font.pointSize: Atlas.DesignTokens.titlePointSize
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Edit Atlas %1")
                          .arg(root.information.applicationVersion)
            }

            Label {
                Layout.fillWidth: true
                color: Atlas.Theme.textSecondary
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Inspect editorial timelines and export structured reports.")
                wrapMode: Text.WordWrap
            }

            Label {
                color: Atlas.Theme.textPrimary
                font.weight: Font.DemiBold
                text: qsTr("Runtime")
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Atlas.DesignTokens.spacingMedium
                rowSpacing: Atlas.DesignTokens.spacingExtraSmall

                Label { text: qsTr("Operating system") }
                Label {
                    Layout.fillWidth: true
                    color: Atlas.Theme.textSecondary
                    text: root.information.operatingSystem
                    wrapMode: Text.WrapAnywhere
                }
                Label { text: qsTr("Architecture") }
                Label {
                    color: Atlas.Theme.textSecondary
                    text: root.information.architecture
                }
                Label { text: qsTr("Qt") }
                Label {
                    color: Atlas.Theme.textSecondary
                    text: root.information.qtVersion
                }
                Label { text: qsTr("Platform plugin") }
                Label {
                    color: Atlas.Theme.textSecondary
                    text: root.information.platformPlugin
                }
                Label { text: qsTr("Video backend") }
                Label {
                    color: Atlas.Theme.textSecondary
                    text: "%1 %2".arg(root.information.videoBackendName)
                                  .arg(root.information.videoBackendVersion)
                }
            }

            Label {
                color: Atlas.Theme.textPrimary
                font.weight: Font.DemiBold
                text: qsTr("Registered formats")
            }

            Label {
                Layout.fillWidth: true
                color: Atlas.Theme.textSecondary
                text: qsTr("Import: %1")
                          .arg(root.information.importFormats.join(", "))
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                color: Atlas.Theme.textSecondary
                text: qsTr("Export: %1")
                          .arg(root.information.exportFormats.join(", "))
                wrapMode: Text.WordWrap
            }

            Label {
                color: Atlas.Theme.textPrimary
                font.weight: Font.DemiBold
                text: qsTr("Diagnostics")
            }

            Label {
                Layout.fillWidth: true
                color: Atlas.Theme.textSecondary
                text: root.information.logDirectory
                wrapMode: Text.WrapAnywhere
            }

            Button {
                text: qsTr("Open Log Folder")
                onClicked: root.openExternal(
                               () => root.information.OpenLogDirectory(),
                               qsTr("The application log directory could not be opened."))
            }

            Label {
                color: Atlas.Theme.textPrimary
                font.weight: Font.DemiBold
                text: qsTr("Licensing")
            }

            Label {
                Layout.fillWidth: true
                color: Atlas.Theme.textSecondary
                text: qsTr("Edit Atlas is licensed under the Apache License 2.0.")
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                color: Atlas.Theme.textSecondary
                text: qsTr("Qt %1 is dynamically linked and used under the GNU Lesser General Public License version 3.")
                          .arg(root.information.qtVersion)
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                color: Atlas.Theme.textSecondary
                text: qsTr("%1 reports its license as: %2")
                          .arg(root.information.videoBackendName)
                          .arg(root.information.videoBackendLicense)
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true

                Button {
                    text: qsTr("Project Website")
                    onClicked: root.openExternal(
                                   () => root.information.OpenProjectWebsite(),
                                   qsTr("The project website could not be opened."))
                }

                Button {
                    text: qsTr("Qt Licensing")
                    onClicked: root.openExternal(
                                   () => root.information.OpenQtLicensingInformation(),
                                   qsTr("Qt licensing information could not be opened."))
                }

                Button {
                    text: qsTr("FFmpeg Legal Information")
                    onClicked: root.openExternal(
                                   () => root.information.OpenFfmpegLegalInformation(),
                                   qsTr("FFmpeg legal information could not be opened."))
                }
            }

            Button {
                text: root.showVideoBuildDetails
                      ? qsTr("Hide FFmpeg Build Details")
                      : qsTr("Show FFmpeg Build Details")
                onClicked: root.showVideoBuildDetails
                           = !root.showVideoBuildDetails
            }

            TextArea {
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                Accessible.name: qsTr("FFmpeg build configuration")
                readOnly: true
                text: root.information.videoBackendConfiguration
                visible: root.showVideoBuildDetails
                wrapMode: TextEdit.WrapAnywhere
            }
        }
    }

    Dialog {
        id: externalFailureDialog

        anchors.centerIn: parent
        modal: true
        parent: Overlay.overlay
        standardButtons: Dialog.Close
        title: qsTr("Could Not Open Link")
        width: Math.min(560,
                        parent.width - Atlas.DesignTokens.spacingExtraLarge)

        Label {
            text: root.externalFailureText
            width: externalFailureDialog.availableWidth
            wrapMode: Text.WordWrap
        }
    }
}
