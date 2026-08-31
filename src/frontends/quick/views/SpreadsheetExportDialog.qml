import EditAtlasStyle as Atlas
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    required property string applicationLanguageCode
    required property TimelineConfiguration configuration
    required property SpreadsheetExport workflow

    property url renderedVideoUrl: ""
    property url pendingDestination: ""
    property bool replacementPending: false
    property bool failurePending: false
    property bool progressClosing: false
    property bool saveDialogPending: false
    property bool successPending: false

    signal destinationRequested(url suggestedDestination)
    signal videoSelectionRequested(url suggestedFolder)

    function openForExport() {
        workflow.ClearResult()
        workbookLanguage.currentIndex = 0
        includeTimeline.checked = true
        includeDiagnostics.checked = true
        renderedVideoUrl = ""
        pendingDestination = ""
        replacementPending = false
        failurePending = false
        progressClosing = false
        saveDialogPending = false
        successPending = false
        open()
    }

    function selectRenderedVideo(url) {
        renderedVideoUrl = url
    }

    function startExport(destination) {
        // Edit Atlas owns replacement confirmation rather than delegating
        // it to the file dialog, so an existing destination is only
        // replaced once the user authorizes it here.
        if (workflow.DestinationExists(destination)) {
            pendingDestination = destination
            replaceDialog.open()
        } else {
            beginExport(destination, false)
        }
    }

    function beginExport(destination, replaceExisting) {
        workflow.Start(destination, selectedWorkbookLanguage(),
                       includeTimeline.checked, includeDiagnostics.checked,
                       renderedVideoUrl, replaceExisting)
    }

    function presentPendingResult() {
        if (progressClosing || progressDialog.visible) {
            return
        }
        if (successPending) {
            successPending = false
            resultDialog.open()
        } else if (failurePending) {
            failurePending = false
            failureDialog.open()
        }
    }

    function requestDestination() {
        if (saveDialogPending) {
            return
        }
        saveDialogPending = true
        close()
    }

    function selectedWorkbookLanguage() {
        if (workbookLanguage.currentIndex === 1) {
            return "en"
        }
        if (workbookLanguage.currentIndex === 2) {
            return "pt-BR"
        }
        return applicationLanguageCode === "pt_BR" ? "pt-BR" : "en"
    }

    anchors.centerIn: parent
    height: Math.min(implicitHeight,
                     parent.height - Atlas.DesignTokens.spacingExtraLarge)
    modal: true
    objectName: "spreadsheetOptionsDialog"
    parent: Overlay.overlay
    title: qsTr("Spreadsheet Options")
    width: Math.min(560, parent.width - Atlas.DesignTokens.spacingExtraLarge)

    ColumnLayout {
        anchors.fill: parent
        spacing: Atlas.DesignTokens.spacingMedium

        Label {
            Layout.fillWidth: true
            color: Atlas.Theme.textSecondary
            text: qsTr("Choose the information to include in the workbook.")
            wrapMode: Text.WordWrap
        }

        Label {
            color: Atlas.Theme.textSecondary
            text: qsTr("Workbook language")
        }

        ComboBox {
            id: workbookLanguage

            Layout.fillWidth: true
            Accessible.name: qsTr("Workbook language")
            model: [
                qsTr("Same as application"),
                qsTr("English"),
                qsTr("Português (Brasil)")
            ]
            objectName: "workbookLanguageSelector"
        }

        CheckBox {
            id: includeTimeline

            checked: true
            objectName: "includeTimelineSheetCheckBox"
            text: qsTr("Include timeline summary")
        }

        CheckBox {
            id: includeDiagnostics

            checked: true
            objectName: "includeDiagnosticsSheetCheckBox"
            text: qsTr("Include diagnostics")
        }

        Atlas.Surface {
            Layout.fillWidth: true
            implicitHeight: videoLayout.implicitHeight
                            + Atlas.DesignTokens.spacingMedium * 2
            Accessible.id: objectName
            objectName: "renderedVideoGroup"
            visible: root.workflow.renderedVideoRequired

            ColumnLayout {
                id: videoLayout

                anchors.fill: parent
                anchors.margins: Atlas.DesignTokens.spacingMedium
                spacing: Atlas.DesignTokens.spacingSmall

                Label {
                    color: Atlas.Theme.textPrimary
                    font.weight: Font.DemiBold
                    text: qsTr("Rendered video")
                }

                Label {
                    Layout.fillWidth: true
                    color: Atlas.Theme.textSecondary
                    text: qsTr("Initial-frame images require a constant-frame-rate MOV, MP4, or MXF render with embedded starting timecode, frame rate, and duration matching the imported EDL.")
                    wrapMode: Text.WordWrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Atlas.DesignTokens.spacingSmall

                    Label {
                        Layout.fillWidth: true
                        Accessible.id: objectName
                        Accessible.name: qsTr("Rendered video path")
                        color: root.renderedVideoUrl.toString().length === 0
                               ? Atlas.Theme.textSecondary
                               : Atlas.Theme.textPrimary
                        elide: Text.ElideMiddle
                        objectName: "renderedVideoPathField"
                        text: root.renderedVideoUrl.toString().length === 0
                              ? qsTr("Select the matching rendered video")
                              : root.renderedVideoUrl.toString()
                    }

                    Button {
                        objectName: "browseRenderedVideoButton"
                        text: qsTr("Browse…")
                        onClicked: root.videoSelectionRequested(
                                       root.workflow.suggestedVideoFolderUrl)
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true

            Label {
                Layout.fillWidth: true
                color: Atlas.Theme.textSecondary
                text: qsTr("%1 event columns selected")
                          .arg(root.configuration
                               .eventProjectionSelectedCount)
            }

            Button {
                objectName: "editSpreadsheetColumnsButton"
                text: qsTr("Export Columns")
                onClicked: eventProjectionDialog.open()
            }
        }

        Label {
            Layout.fillWidth: true
            color: Atlas.Theme.accent
            text: qsTr("Select at least one event column.")
            visible: !root.configuration.eventProjectionValid
        }

        Item {
            Layout.fillHeight: true
        }
    }

    footer: DialogButtonBox {
        onAccepted: root.requestDestination()
        onRejected: root.reject()

        Button {
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            objectName: "cancelSpreadsheetExportButton"
            text: qsTr("Cancel")
        }

        Button {
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            enabled: root.configuration.eventProjectionValid
                     && (!root.workflow.renderedVideoRequired
                         || root.renderedVideoUrl.toString().length > 0)
            highlighted: true
            objectName: "continueSpreadsheetExportButton"
            text: qsTr("Continue")
            onClicked: root.requestDestination()
        }
    }

    onVisibleChanged: {
        if (!visible && saveDialogPending) {
            saveDialogPending = false
            destinationRequested(workflow.suggestedDestinationUrl)
        }
    }

    EventProjectionDialog {
        id: eventProjectionDialog

        configuration: root.configuration
        objectName: "spreadsheetEventProjectionDialog"
    }

    Dialog {
        id: replaceDialog

        anchors.centerIn: parent
        modal: true
        objectName: "replaceSpreadsheetDialog"
        parent: Overlay.overlay
        title: qsTr("Replace Existing File?")

        Label {
            text: qsTr("The selected file already exists. Do you want to replace it?")
            wrapMode: Text.WordWrap
        }

        footer: DialogButtonBox {
            onRejected: {
                root.replacementPending = false
                root.pendingDestination = ""
                replaceDialog.close()
            }

            Button {
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                objectName: "cancelReplaceSpreadsheetButton"
                text: qsTr("Cancel")
            }

            Button {
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                highlighted: true
                objectName: "replaceSpreadsheetButton"
                text: qsTr("Replace")
                onClicked: {
                    root.replacementPending = true
                    replaceDialog.close()
                }
            }
        }

        onClosed: {
            const destination = root.pendingDestination
            root.pendingDestination = ""
            if (root.replacementPending) {
                root.replacementPending = false
                Qt.callLater(() => root.beginExport(destination, true))
            }
        }
    }

    Dialog {
        id: progressDialog

        anchors.centerIn: parent
        closePolicy: Popup.NoAutoClose
        modal: true
        objectName: "spreadsheetExportProgressDialog"
        parent: Overlay.overlay
        title: qsTr("Export Spreadsheet")

        ColumnLayout {
            spacing: Atlas.DesignTokens.spacingMedium

            Label {
                Layout.fillWidth: true
                text: root.workflow.totalFrameCount === 0
                      ? qsTr("Preparing spreadsheet…")
                      : qsTr("Extracting initial frames: %1 of %2")
                            .arg(root.workflow.completedFrameCount)
                            .arg(root.workflow.totalFrameCount)
                wrapMode: Text.WordWrap
            }

            ProgressBar {
                Layout.fillWidth: true
                Accessible.description: root.workflow.totalFrameCount === 0
                                        ? qsTr("Preparing spreadsheet…")
                                        : qsTr("Extracting initial frames: %1 of %2")
                                              .arg(root.workflow.completedFrameCount)
                                              .arg(root.workflow.totalFrameCount)
                Accessible.name: qsTr("Spreadsheet export progress")
                from: 0
                indeterminate: root.workflow.totalFrameCount === 0
                to: Math.max(1, root.workflow.totalFrameCount)
                value: root.workflow.completedFrameCount
            }

            Button {
                Layout.alignment: Qt.AlignRight
                objectName: "cancelFrameExtractionButton"
                text: qsTr("Cancel")
                visible: root.workflow.renderedVideoRequired
                onClicked: root.workflow.Cancel()
            }
        }

        onClosed: {
            root.progressClosing = false
            Qt.callLater(() => root.presentPendingResult())
        }
    }

    Dialog {
        id: resultDialog

        anchors.centerIn: parent
        modal: true
        objectName: "spreadsheetExportResultDialog"
        parent: Overlay.overlay
        title: root.workflow.hasWarnings
               ? qsTr("Spreadsheet Exported with Warnings")
               : qsTr("Spreadsheet Exported")

        ColumnLayout {
            spacing: Atlas.DesignTokens.spacingMedium

            Label {
                Layout.maximumWidth: 560
                text: qsTr("The spreadsheet was saved to:\n%1")
                          .arg(root.workflow.resultPath)
                wrapMode: Text.WrapAnywhere
            }

            Label {
                Layout.maximumWidth: 560
                color: Atlas.Theme.textSecondary
                text: root.workflow.resultDetailsText
                visible: text.length > 0
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.maximumWidth: 560
                color: Atlas.Theme.accent
                text: root.workflow.warningText
                visible: text.length > 0
                wrapMode: Text.WordWrap
            }
        }

        footer: DialogButtonBox {
            onRejected: resultDialog.close()

            Button {
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                objectName: "revealSpreadsheetButton"
                text: qsTr("Reveal File")
                onClicked: {
                    if (!root.workflow.RevealResult()) {
                        revealFailureDialog.open()
                    } else {
                        resultDialog.close()
                    }
                }
            }

            Button {
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
                objectName: "closeDialogButton"
                text: qsTr("Close")
            }
        }

        onClosed: root.workflow.ClearResult()
    }

    Dialog {
        id: failureDialog

        anchors.centerIn: parent
        modal: true
        objectName: root.workflow.renderedVideoRequired
                    ? "renderedVideoExportFailureDialog"
                    : "spreadsheetExportFailureDialog"
        parent: Overlay.overlay
        standardButtons: Dialog.Close
        title: qsTr("Could Not Export Spreadsheet")

        Component.onCompleted: {
            standardButton(Dialog.Close).objectName = "closeDialogButton"
        }

        Label {
            Layout.maximumWidth: 560
            text: root.workflow.errorText
            wrapMode: Text.WordWrap
        }

        onClosed: root.workflow.ClearResult()
    }

    Dialog {
        id: revealFailureDialog

        anchors.centerIn: parent
        modal: true
        objectName: "revealSpreadsheetFailureDialog"
        parent: Overlay.overlay
        standardButtons: Dialog.Close
        title: qsTr("Could Not Reveal File")

        Component.onCompleted: {
            standardButton(Dialog.Close).objectName = "closeDialogButton"
        }

        Label {
            text: qsTr("The spreadsheet was saved, but its location could not be opened.")
            wrapMode: Text.WordWrap
        }
    }

    Connections {
        target: root.workflow

        function onBusyChanged() {
            if (root.workflow.busy) {
                root.progressClosing = false
                progressDialog.open()
            } else {
                if (progressDialog.visible) {
                    root.progressClosing = true
                    progressDialog.close()
                } else {
                    Qt.callLater(() => root.presentPendingResult())
                }
            }
        }

        function onExportSucceeded() {
            root.successPending = true
            Qt.callLater(() => root.presentPendingResult())
        }

        function onExportFailed() {
            root.failurePending = true
            Qt.callLater(() => root.presentPendingResult())
        }
    }
}
