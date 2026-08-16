import EditAtlasStyle as Atlas
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
    id: window

    required property ApplicationShell applicationShell
    property bool frameRatePromptPending: false
    property url pendingTimelineUrl: ""

    function presentPendingFrameRateDialog() {
        if (!frameRatePromptPending || openDialog.visible
                || frameRateDialog.visible) {
            return
        }
        frameRatePromptPending = false
        frameRateDialog.open()
    }

    function requestFrameRateDialog() {
        if (frameRatePromptPending || frameRateDialog.visible) {
            return
        }
        frameRatePromptPending = true
        Qt.callLater(() => window.presentPendingFrameRateDialog())
    }

    function startPendingTimelineImport() {
        if (openDialog.visible || pendingTimelineUrl.toString().length === 0) {
            return
        }
        const selectedUrl = pendingTimelineUrl
        pendingTimelineUrl = ""
        frameRatePromptPending = false
        applicationShell.OpenUrl(selectedUrl)
    }

    height: 700
    minimumHeight: 500
    minimumWidth: 760
    title: qsTr("Edit Atlas")
    visible: true
    width: 1100
    color: Atlas.Theme.window
    font.pointSize: Atlas.DesignTokens.bodyPointSize

    palette.accent: Atlas.Theme.accent
    palette.alternateBase: Atlas.Theme.surface
    palette.base: Atlas.Theme.control
    palette.brightText: Atlas.Theme.onAccent
    palette.button: Atlas.Theme.control
    palette.buttonText: Atlas.Theme.textPrimary
    palette.dark: Atlas.Theme.window
    palette.highlight: Atlas.Theme.accent
    palette.highlightedText: Atlas.Theme.onAccent
    palette.light: Atlas.Theme.controlHovered
    palette.link: Atlas.Theme.accent
    palette.linkVisited: Atlas.Theme.accentPressed
    palette.mid: Atlas.Theme.border
    palette.midlight: Atlas.Theme.controlHovered
    palette.placeholderText: Atlas.Theme.textSecondary
    palette.shadow: Atlas.Theme.window
    palette.text: Atlas.Theme.textPrimary
    palette.toolTipBase: Atlas.Theme.surface
    palette.toolTipText: Atlas.Theme.textPrimary
    palette.window: Atlas.Theme.window
    palette.windowText: Atlas.Theme.textPrimary

    onClosing: close => {
        if (!applicationShell.RequestClose()) {
            close.accepted = false
            operationInProgressDialog.open()
        }
    }

    menuBar: MenuBar {
        Accessible.name: qsTr("Application menu")

        Menu {
            title: qsTr("&File")

            Action {
                id: openAction

                enabled: !applicationShell.busy
                shortcut: StandardKey.Open
                text: qsTr("&Open Timeline…")
                onTriggered: openDialog.open()
            }

            Menu {
                id: recentFilesMenu

                enabled: !applicationShell.busy
                title: qsTr("Open &Recent")

                Instantiator {
                    model: applicationShell.recentFiles

                    delegate: MenuItem {
                        required property string modelData

                        text: applicationShell.FileName(modelData)
                        ToolTip.text: modelData
                        ToolTip.visible: hovered
                        onTriggered: applicationShell.OpenPath(modelData)
                    }

                    onObjectAdded: (index, object) => {
                        recentFilesMenu.insertItem(index, object)
                    }
                    onObjectRemoved: (index, object) => {
                        recentFilesMenu.removeItem(object)
                    }
                }

                MenuItem {
                    enabled: false
                    text: applicationShell.rememberRecentFiles
                          ? qsTr("No recent files")
                          : qsTr("History disabled")
                    visible: applicationShell.recentFiles.length === 0
                }
            }

            MenuItem {
                checkable: true
                checked: applicationShell.rememberRecentFiles
                text: qsTr("Remember Recent Files")
                Accessible.description: qsTr("When enabled, Edit Atlas stores only the paths of opened files.")
                onTriggered: {
                    applicationShell.rememberRecentFiles = checked
                }
            }

            MenuSeparator {}

            Action {
                shortcut: StandardKey.Quit
                text: qsTr("E&xit")
                onTriggered: window.close()
            }
        }

        Menu {
            title: qsTr("&Language")

            MenuItem {
                checkable: true
                checked: applicationShell.languageCode === "pt_BR"
                text: "Português (Brasil)"
                onTriggered: applicationShell.languageCode = "pt_BR"
            }

            MenuItem {
                checkable: true
                checked: applicationShell.languageCode === "en"
                text: "English"
                onTriggered: applicationShell.languageCode = "en"
            }
        }
    }

    header: Rectangle {
        color: Atlas.Theme.surface
        implicitHeight: headerLayout.implicitHeight
                        + Atlas.DesignTokens.spacingLarge * 2

        RowLayout {
            id: headerLayout

            anchors.fill: parent
            anchors.leftMargin: Atlas.DesignTokens.spacingExtraLarge
            anchors.rightMargin: Atlas.DesignTokens.spacingExtraLarge
            anchors.topMargin: Atlas.DesignTokens.spacingLarge
            anchors.bottomMargin: Atlas.DesignTokens.spacingLarge
            spacing: Atlas.DesignTokens.spacingLarge

            ColumnLayout {
                spacing: Atlas.DesignTokens.spacingExtraSmall

                Label {
                    color: Atlas.Theme.textPrimary
                    font.pointSize: Atlas.DesignTokens.headingPointSize
                    font.weight: Font.DemiBold
                    text: qsTr("Edit Atlas")
                }

                Label {
                    color: Atlas.Theme.textSecondary
                    text: qsTr("Editorial timeline workspace")
                }
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                Accessible.description: qsTr("Choose a local timeline file to inspect")
                enabled: !applicationShell.busy
                highlighted: true
                text: qsTr("Open Timeline")
                visible: !applicationShell.empty
                onClicked: openDialog.open()
            }
        }
    }

    footer: Rectangle {
        border.color: Atlas.Theme.border
        border.width: Atlas.DesignTokens.borderWidth
        color: Atlas.Theme.surface
        implicitHeight: statusLabel.implicitHeight
                        + Atlas.DesignTokens.spacingMedium * 2

        Label {
            id: statusLabel

            anchors.fill: parent
            anchors.leftMargin: Atlas.DesignTokens.spacingLarge
            anchors.rightMargin: Atlas.DesignTokens.spacingLarge
            color: Atlas.Theme.textSecondary
            elide: Text.ElideMiddle
            text: applicationShell.statusText
            verticalAlignment: Text.AlignVCenter

            Accessible.name: qsTr("Status")
            Accessible.description: text
        }
    }

    DropArea {
        anchors.fill: parent
        enabled: !applicationShell.busy
        onEntered: drag => {
            drag.accepted = drag.hasUrls
        }
        onDropped: drop => {
            if (drop.hasUrls && drop.urls.length > 0) {
                applicationShell.OpenUrl(drop.urls[0])
                drop.acceptProposedAction()
            }
        }
    }

    StackLayout {
        anchors.fill: parent
        anchors.margins: Atlas.DesignTokens.spacingExtraLarge
        currentIndex: applicationShell.documentState

        Item {
            Accessible.name: qsTr("No timeline open")

            Atlas.Surface {
                anchors.centerIn: parent
                height: emptyLayout.implicitHeight
                        + Atlas.DesignTokens.spacingExtraLarge * 2
                width: Math.min(parent.width, 580)

                ColumnLayout {
                    id: emptyLayout

                    anchors.fill: parent
                    anchors.margins: Atlas.DesignTokens.spacingExtraLarge
                    spacing: Atlas.DesignTokens.spacingLarge

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        color: Atlas.Theme.textPrimary
                        font.pointSize: Atlas.DesignTokens.titlePointSize
                        font.weight: Font.DemiBold
                        text: qsTr("Open an editorial timeline")
                    }

                    Label {
                        Layout.fillWidth: true
                        color: Atlas.Theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        text: qsTr("Choose a supported timeline file or drop it anywhere in this window.")
                        wrapMode: Text.WordWrap
                    }

                    Button {
                        Layout.alignment: Qt.AlignHCenter
                        Accessible.description: qsTr("Choose a local timeline file to inspect")
                        highlighted: true
                        text: qsTr("Open Timeline")
                        onClicked: openDialog.open()
                    }
                }
            }
        }

        Item {
            Accessible.name: qsTr("Opening timeline")

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Atlas.DesignTokens.spacingLarge

                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    running: true
                }

                Label {
                    color: Atlas.Theme.textPrimary
                    font.pointSize: Atlas.DesignTokens.headingPointSize
                    text: qsTr("Opening %1…").arg(applicationShell.sourceFileName)
                }
            }
        }

        Item {
            Accessible.name: qsTr("Timeline ready")

            ColumnLayout {
                anchors.fill: parent
                spacing: Atlas.DesignTokens.spacingMedium

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Atlas.DesignTokens.spacingLarge

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Atlas.DesignTokens.spacingExtraSmall

                        Label {
                            Layout.fillWidth: true
                            color: Atlas.Theme.textPrimary
                            elide: Text.ElideMiddle
                            font.pointSize: Atlas.DesignTokens.titlePointSize
                            font.weight: Font.DemiBold
                            text: applicationShell.timelineTitle
                        }

                        Label {
                            color: Atlas.Theme.textSecondary
                            text: applicationShell.timelineSummaryText
                        }
                    }

                    Label {
                        color: Atlas.Theme.textSecondary
                        text: qsTr("Showing %1 of %2 events")
                                  .arg(applicationShell.visibleEventCount)
                                  .arg(applicationShell.eventCount)
                    }
                }

                TimelineConfigurationPanel {
                    Layout.fillWidth: true
                    availableHeight: window.height
                    configuration: applicationShell.timelineConfiguration
                }

                TimelineTable {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    applicationShell: window.applicationShell
                }

                DiagnosticsPanel {
                    Layout.fillWidth: true
                    Layout.maximumHeight: 240
                    Layout.minimumHeight: 160
                    diagnosticCount: applicationShell.diagnosticCount
                    diagnosticsModel: applicationShell.diagnosticsModel
                    visible: applicationShell.diagnosticCount > 0
                }
            }
        }

        Item {
            Accessible.name: qsTr("Timeline import failed")

            ColumnLayout {
                anchors.fill: parent
                spacing: Atlas.DesignTokens.spacingMedium

                Atlas.Surface {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.fillWidth: true
                    Layout.maximumWidth: 680
                    implicitHeight: errorLayout.implicitHeight
                                    + Atlas.DesignTokens.spacingExtraLarge * 2

                    ColumnLayout {
                        id: errorLayout

                        anchors.fill: parent
                        anchors.margins: Atlas.DesignTokens.spacingExtraLarge
                        spacing: Atlas.DesignTokens.spacingLarge

                        Label {
                            Layout.fillWidth: true
                            color: Atlas.Theme.textPrimary
                            font.pointSize: Atlas.DesignTokens.titlePointSize
                            font.weight: Font.DemiBold
                            text: qsTr("Could not open %1")
                                      .arg(applicationShell.sourceFileName)
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            Layout.fillWidth: true
                            color: Atlas.Theme.textSecondary
                            text: applicationShell.errorText
                            wrapMode: Text.WordWrap
                        }

                        Button {
                            Accessible.description: qsTr("Choose a different local timeline file")
                            highlighted: true
                            text: qsTr("Open Another Timeline")
                            onClicked: openDialog.open()
                        }
                    }
                }

                DiagnosticsPanel {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                    diagnosticCount: applicationShell.diagnosticCount
                    diagnosticsModel: applicationShell.diagnosticsModel
                    visible: applicationShell.diagnosticCount > 0
                }
            }
        }
    }

    FileDialog {
        id: openDialog

        fileMode: FileDialog.OpenFile
        nameFilters: [
            qsTr("Supported timeline files (%1)")
                .arg(applicationShell.importFilePatterns.length === 0
                     ? "*" : applicationShell.importFilePatterns.join(" ")),
            qsTr("All files (*)")
        ]
        title: qsTr("Open Timeline")
        onAccepted: {
            window.pendingTimelineUrl = selectedFile
            Qt.callLater(() => window.startPendingTimelineImport())
        }
        onVisibleChanged: {
            if (!visible) {
                Qt.callLater(() => window.startPendingTimelineImport())
                Qt.callLater(() => window.presentPendingFrameRateDialog())
            }
        }
    }

    Dialog {
        id: frameRateDialog

        anchors.centerIn: parent
        closePolicy: Popup.NoAutoClose
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        title: qsTr("Select Frame Rate")

        readonly property var frameRateValues: [
            "24000/1001", "24", "25", "30000/1001",
            "30", "50", "60000/1001", "60"
        ]

        ColumnLayout {
            spacing: Atlas.DesignTokens.spacingMedium

            Label {
                Layout.maximumWidth: 400
                text: qsTr("This non-drop-frame EDL does not declare its frame rate.")
                wrapMode: Text.WordWrap
            }

            ComboBox {
                id: frameRateSelector

                Accessible.name: qsTr("Frame rate")
                currentIndex: 1
                model: [
                    qsTr("23.976 fps"), qsTr("24 fps"),
                    qsTr("25 fps"), qsTr("29.97 fps"),
                    qsTr("30 fps"), qsTr("50 fps"),
                    qsTr("59.94 fps"), qsTr("60 fps")
                ]
            }
        }

        onAccepted: {
            applicationShell.RetryWithFrameRate(
                frameRateValues[frameRateSelector.currentIndex])
        }
    }

    Dialog {
        id: operationInProgressDialog

        anchors.centerIn: parent
        modal: true
        standardButtons: Dialog.Ok
        title: qsTr("Operation in Progress")

        Label {
            text: qsTr("Wait for the current operation to finish before closing Edit Atlas.")
            wrapMode: Text.WordWrap
        }
    }

    Connections {
        target: applicationShell

        function onFrameRateRequired() {
            window.requestFrameRateDialog()
        }
    }
}
