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
    Accessible.id: objectName
    objectName: "timelineFilter"

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
                objectName: "templateSelector"
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
                objectName: "toggleFiltersButton"
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
                objectName: "templatePrimaryButton"
                text: qsTr("Save New")
                onClicked: templateNameDialog.openFor(0)
            }

            Button {
                enabled: root.configuration.hasActiveTemplate
                         && root.configuration.templateModified
                         && root.configuration.filterValid
                         && root.configuration.eventProjectionValid
                objectName: "updateTemplateButton"
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
                objectName: "templateActionsButton"
                text: qsTr("Template Actions")
                onClicked: templateActionsMenu.open()

                Menu {
                    id: templateActionsMenu

                    objectName: "templateActionsMenu"
                    y: templateActionsButton.height

                    MenuItem {
                        objectName: "renameTemplateAction"
                        text: qsTr("Rename")
                        onTriggered: templateNameDialog.openFor(1)
                    }

                    MenuItem {
                        objectName: "duplicateTemplateAction"
                        text: qsTr("Duplicate")
                        onTriggered: templateNameDialog.openFor(2)
                    }

                    MenuSeparator {}

                    MenuItem {
                        objectName: "deleteTemplateAction"
                        text: qsTr("Delete")
                        onTriggered: deleteTemplateDialog.open()
                    }
                }
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                objectName: "editExportColumnsAction"
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
                objectName: "filterCombination"
                onActivated: index => root.configuration.filterCombination
                                      = index
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                objectName: "clearFiltersButton"
                text: qsTr("Clear")
                onClicked: root.configuration.ClearFilter()
            }

            Button {
                highlighted: true
                objectName: "addFilterConditionButton"
                text: qsTr("Add Condition")
                onClicked: root.configuration.AddFilterCondition()
            }
        }

        ListView {
            id: filterList

            Layout.fillWidth: true
            Layout.minimumHeight: count > 0
                                  ? Atlas.DesignTokens.controlHeight
                                  : 0
            Layout.preferredHeight: Math.min(contentHeight,
                                             root.maximumFilterHeight)
            Accessible.id: objectName
            Accessible.name: qsTr("Timeline filters")
            clip: true
            model: root.configuration.filterModel
            objectName: "filterConditionsScrollArea"
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
            Accessible.id: objectName
            color: Atlas.Theme.accent
            objectName: "filterErrorLabel"
            text: root.configuration.filterErrorText
            visible: root.expanded && text.length > 0
            wrapMode: Text.WordWrap
        }
    }

    EventProjectionDialog {
        id: eventProjectionDialog

        configuration: root.configuration
        objectName: "eventProjectionDialog"
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
        objectName: "templateNameDialog"
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
                objectName: "templateNameEditor"
            }
        }

        Component.onCompleted: {
            standardButton(Dialog.Save).objectName = "acceptTemplateNameButton"
            standardButton(Dialog.Cancel).objectName = "cancelTemplateNameButton"
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
        objectName: "deleteTemplateDialog"
        parent: Overlay.overlay
        standardButtons: Dialog.Yes | Dialog.No
        title: qsTr("Delete Template?")

        Component.onCompleted: {
            standardButton(Dialog.Yes).objectName = "confirmDeleteTemplateButton"
            standardButton(Dialog.No).objectName = "cancelDeleteTemplateButton"
        }

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
