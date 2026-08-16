import EditAtlasStyle as Atlas
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    required property SupportBundle workflow

    property bool destinationPending: false
    property bool failurePending: false
    property url pendingDestination: ""
    property bool progressClosing: false
    property bool replacementPending: false
    property bool successPending: false

    signal destinationRequested(url suggestedDestination)

    function openForExport() {
        workflow.ClearResult()
        destinationPending = false
        failurePending = false
        pendingDestination = ""
        progressClosing = false
        replacementPending = false
        successPending = false
        open()
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
        if (destinationPending) {
            return
        }
        destinationPending = true
        close()
    }

    function startExport(destination) {
        if (workflow.DestinationExists(destination)) {
            pendingDestination = destination
            replaceDialog.open()
        } else {
            workflow.Start(destination, false)
        }
    }

    anchors.centerIn: parent
    height: Math.min(implicitHeight,
                     parent.height - Atlas.DesignTokens.spacingExtraLarge)
    modal: true
    objectName: "supportBundleDisclosureDialog"
    parent: Overlay.overlay
    title: qsTr("Export Diagnostic Logs")
    width: Math.min(560, parent.width - Atlas.DesignTokens.spacingExtraLarge)

    ColumnLayout {
        anchors.fill: parent
        spacing: Atlas.DesignTokens.spacingMedium

        Label {
            Layout.fillWidth: true
            color: Atlas.Theme.textPrimary
            font.weight: Font.DemiBold
            text: qsTr("Review what the support bundle contains")
            wrapMode: Text.WordWrap
        }

        Label {
            Layout.fillWidth: true
            color: Atlas.Theme.textSecondary
            text: qsTr("The support bundle will contain recent Edit Atlas application logs and a summary of the application version, operating system, architecture, Qt version, platform plugin, and registered formats.")
            wrapMode: Text.WordWrap
        }

        Atlas.Surface {
            Layout.fillWidth: true
            implicitHeight: excludedLayout.implicitHeight
                            + Atlas.DesignTokens.spacingMedium * 2

            ColumnLayout {
                id: excludedLayout

                anchors.fill: parent
                anchors.margins: Atlas.DesignTokens.spacingMedium
                spacing: Atlas.DesignTokens.spacingExtraSmall

                Label {
                    color: Atlas.Theme.textPrimary
                    font.weight: Font.DemiBold
                    text: qsTr("Not included automatically")
                }

                Label {
                    Layout.fillWidth: true
                    color: Atlas.Theme.textSecondary
                    text: qsTr("Timelines, spreadsheets, media, environment variables, and secrets.")
                    wrapMode: Text.WordWrap
                }
            }
        }
    }

    footer: DialogButtonBox {
        onAccepted: root.requestDestination()
        onRejected: root.reject()

        Button {
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            objectName: "cancelSupportBundleButton"
            text: qsTr("Cancel")
        }

        Button {
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            highlighted: true
            objectName: "continueSupportBundleButton"
            text: qsTr("Continue")
            onClicked: root.requestDestination()
        }
    }

    onClosed: {
        if (destinationPending) {
            destinationPending = false
            Qt.callLater(() => destinationRequested(
                             workflow.suggestedDestinationUrl))
        }
    }

    Dialog {
        id: replaceDialog

        anchors.centerIn: parent
        modal: true
        objectName: "replaceSupportBundleDialog"
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
                objectName: "cancelReplaceSupportBundleButton"
                text: qsTr("Cancel")
            }

            Button {
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
                highlighted: true
                objectName: "replaceSupportBundleButton"
                text: qsTr("Replace")
                onClicked: {
                    root.replacementPending = true
                    replaceDialog.close()
                }
            }
        }

        onClosed: {
            if (root.replacementPending) {
                root.replacementPending = false
                const destination = root.pendingDestination
                root.pendingDestination = ""
                Qt.callLater(() => root.workflow.Start(destination, true))
            } else {
                root.pendingDestination = ""
            }
        }
    }

    Dialog {
        id: progressDialog

        anchors.centerIn: parent
        closePolicy: Popup.NoAutoClose
        modal: true
        objectName: "supportBundleProgressDialog"
        parent: Overlay.overlay
        title: qsTr("Export Diagnostic Logs")

        ColumnLayout {
            spacing: Atlas.DesignTokens.spacingMedium

            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                Accessible.name: qsTr("Creating diagnostic support bundle")
                running: progressDialog.visible
            }

            Label {
                text: qsTr("Creating diagnostic support bundle…")
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
        objectName: "supportBundleResultDialog"
        parent: Overlay.overlay
        title: qsTr("Diagnostic Logs Exported")

        ColumnLayout {
            spacing: Atlas.DesignTokens.spacingMedium

            Label {
                Layout.maximumWidth: 560
                text: qsTr("The support bundle was saved to:\n%1")
                          .arg(root.workflow.resultPath)
                wrapMode: Text.WrapAnywhere
            }

            Label {
                color: Atlas.Theme.textSecondary
                text: qsTr("Recent application log files included: %1")
                          .arg(root.workflow.includedLogFileCount)
            }
        }

        footer: DialogButtonBox {
            onRejected: resultDialog.close()

            Button {
                DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
                objectName: "revealSupportBundleButton"
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
        objectName: "supportBundleFailureDialog"
        parent: Overlay.overlay
        standardButtons: Dialog.Close
        title: qsTr("Could Not Export Diagnostic Logs")
        width: Math.min(560,
                        parent.width - Atlas.DesignTokens.spacingExtraLarge)

        Component.onCompleted: {
            standardButton(Dialog.Close).objectName = "closeDialogButton"
        }

        Label {
            text: root.workflow.errorText
            width: failureDialog.availableWidth
            wrapMode: Text.WordWrap
        }

        onClosed: root.workflow.ClearResult()
    }

    Dialog {
        id: revealFailureDialog

        anchors.centerIn: parent
        modal: true
        objectName: "revealSupportBundleFailureDialog"
        parent: Overlay.overlay
        standardButtons: Dialog.Close
        title: qsTr("Could Not Reveal File")
        width: Math.min(560,
                        parent.width - Atlas.DesignTokens.spacingExtraLarge)

        Component.onCompleted: {
            standardButton(Dialog.Close).objectName = "closeDialogButton"
        }

        Label {
            text: qsTr("The support bundle was saved, but its location could not be opened.")
            width: Math.min(520, implicitWidth)
            wrapMode: Text.WordWrap
        }
    }

    Connections {
        target: root.workflow

        function onBusyChanged() {
            if (root.workflow.busy) {
                root.progressClosing = false
                progressDialog.open()
            } else if (progressDialog.visible) {
                root.progressClosing = true
                progressDialog.close()
            } else {
                Qt.callLater(() => root.presentPendingResult())
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
