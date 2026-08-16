import EditAtlasStyle as Atlas
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Atlas.Surface {
    id: root

    required property real availableHeight
    required property TimelineConfiguration configuration

    property bool expanded: false

    readonly property real maximumFilterHeight: Math.max(
        Atlas.DesignTokens.controlHeight * 3,
        root.availableHeight * 0.3)

    implicitHeight: configurationLayout.implicitHeight
                    + Atlas.DesignTokens.spacingMedium * 2

    ColumnLayout {
        id: configurationLayout

        anchors.fill: parent
        anchors.margins: Atlas.DesignTokens.spacingMedium
        spacing: Atlas.DesignTokens.spacingMedium

        RowLayout {
            Layout.fillWidth: true
            spacing: Atlas.DesignTokens.spacingSmall

            Label {
                color: Atlas.Theme.textPrimary
                font.weight: Font.DemiBold
                text: qsTr("Timeline configuration")
            }

            ComboBox {
                id: templateSelector

                Layout.fillWidth: true
                Layout.maximumWidth: 420
                Accessible.name: qsTr("Saved template")
                currentIndex: root.configuration.activeTemplateRow
                model: root.configuration.templateModel
                textRole: "display"
                onActivated: index =>
                    root.configuration.SelectTemplateRow(index)
            }

            Label {
                color: Atlas.Theme.accent
                text: qsTr("Modified")
                visible: root.configuration.templateModified
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: root.expanded ? qsTr("Hide Filters")
                                    : qsTr("Show Filters")
                onClicked: root.expanded = !root.expanded
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Atlas.DesignTokens.spacingSmall

            Button {
                enabled: root.configuration.filterValid
                         && root.configuration.eventProjectionValid
                text: qsTr("Save New")
                onClicked: templateNameDialog.openFor(0)
            }

            Button {
                enabled: root.configuration.hasActiveTemplate
                         && root.configuration.templateModified
                         && root.configuration.filterValid
                         && root.configuration.eventProjectionValid
                text: qsTr("Update")
                onClicked: {
                    if (!root.configuration.UpdateActiveTemplate()) {
                        templateFailureDialog.open()
                    }
                }
            }

            Button {
                id: templateActionsButton

                enabled: root.configuration.hasActiveTemplate
                text: qsTr("Template Actions")
                onClicked: templateActionsMenu.open()

                Menu {
                    id: templateActionsMenu

                    y: templateActionsButton.height

                    MenuItem {
                        text: qsTr("Rename")
                        onTriggered: templateNameDialog.openFor(1)
                    }

                    MenuItem {
                        text: qsTr("Duplicate")
                        onTriggered: templateNameDialog.openFor(2)
                    }

                    MenuSeparator {}

                    MenuItem {
                        text: qsTr("Delete")
                        onTriggered: deleteTemplateDialog.open()
                    }
                }
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("Export Columns")
                onClicked: eventProjectionDialog.open()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            color: Atlas.Theme.border
            implicitHeight: Atlas.DesignTokens.borderWidth
            visible: root.expanded
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Atlas.DesignTokens.spacingSmall
            visible: root.expanded

            Label {
                color: Atlas.Theme.textSecondary
                text: qsTr("Match")
            }

            ComboBox {
                Layout.preferredWidth: 190
                Accessible.name: qsTr("Filter combination")
                currentIndex: root.configuration.filterCombination
                model: root.configuration.filterCombinationNames
                onActivated: index => root.configuration.filterCombination
                                      = index
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: qsTr("Clear")
                onClicked: root.configuration.ClearFilter()
            }

            Button {
                highlighted: true
                text: qsTr("Add Condition")
                onClicked: root.configuration.AddFilterCondition()
            }
        }

        ListView {
            id: filterList

            Layout.fillWidth: true
            Layout.minimumHeight: Math.min(contentHeight,
                                           Atlas.DesignTokens.controlHeight)
            Layout.preferredHeight: Math.min(contentHeight,
                                             root.maximumFilterHeight)
            Accessible.name: qsTr("Timeline filters")
            clip: true
            model: root.configuration.filterModel
            spacing: Atlas.DesignTokens.spacingSmall
            visible: root.expanded

            delegate: TimelineFilterCondition {
                required property int index
                required property var model

                conditionCount: filterList.count
                configuration: root.configuration
                row: index
                rowModel: model
                width: ListView.view.width
            }

            ScrollBar.vertical: ScrollBar {}
        }

        Label {
            Layout.fillWidth: true
            color: Atlas.Theme.accent
            text: root.configuration.filterErrorText
            visible: root.expanded && text.length > 0
            wrapMode: Text.WordWrap
        }
    }

    EventProjectionDialog {
        id: eventProjectionDialog

        configuration: root.configuration
    }

    Dialog {
        id: templateNameDialog

        property int operation: 0

        function openFor(requestedOperation) {
            operation = requestedOperation
            if (operation === 0) {
                title = qsTr("Save Template")
                templateNameEditor.text = ""
            } else if (operation === 1) {
                title = qsTr("Rename Template")
                templateNameEditor.text = root.configuration
                    .activeTemplateName
            } else {
                title = qsTr("Duplicate Template")
                templateNameEditor.text = qsTr("%1 copy")
                    .arg(root.configuration.activeTemplateName)
            }
            open()
            templateNameEditor.forceActiveFocus()
            templateNameEditor.selectAll()
        }

        anchors.centerIn: parent
        modal: true
        parent: Overlay.overlay
        standardButtons: Dialog.Save | Dialog.Cancel

        ColumnLayout {
            spacing: Atlas.DesignTokens.spacingSmall

            Label {
                text: qsTr("Template name")
            }

            TextField {
                id: templateNameEditor

                Accessible.name: qsTr("Template name")
                Layout.preferredWidth: Atlas.DesignTokens.textFieldWidth
            }
        }

        onAccepted: {
            let succeeded = false
            if (operation === 0) {
                succeeded = root.configuration.CreateTemplate(
                    templateNameEditor.text)
            } else if (operation === 1) {
                succeeded = root.configuration.RenameActiveTemplate(
                    templateNameEditor.text)
            } else {
                succeeded = root.configuration.DuplicateActiveTemplate(
                    templateNameEditor.text)
            }
            if (!succeeded) {
                Qt.callLater(() => templateFailureDialog.open())
            }
        }
    }

    Dialog {
        id: deleteTemplateDialog

        anchors.centerIn: parent
        modal: true
        parent: Overlay.overlay
        standardButtons: Dialog.Yes | Dialog.No
        title: qsTr("Delete Template?")

        Label {
            text: qsTr("Delete the template “%1”? This cannot be undone.")
                .arg(root.configuration.activeTemplateName)
            wrapMode: Text.WordWrap
        }

        onAccepted: {
            if (!root.configuration.RemoveActiveTemplate()) {
                Qt.callLater(() => templateFailureDialog.open())
            }
        }
    }

    Dialog {
        id: templateFailureDialog

        anchors.centerIn: parent
        modal: true
        parent: Overlay.overlay
        standardButtons: Dialog.Close
        title: qsTr("Could Not Change Template")

        Label {
            text: root.configuration.templateOperationErrorText
            wrapMode: Text.WordWrap
        }

        onClosed: root.configuration.ClearTemplateOperationError()
    }
}
