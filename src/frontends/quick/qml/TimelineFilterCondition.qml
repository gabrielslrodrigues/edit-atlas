import EditAtlasStyle as Atlas
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    required property TimelineConfiguration configuration
    required property int conditionCount
    required property int row
    required property var rowModel

    implicitHeight: conditionLayout.implicitHeight

    ColumnLayout {
        id: conditionLayout

        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Atlas.DesignTokens.spacingSmall

        RowLayout {
            Layout.fillWidth: true
            spacing: Atlas.DesignTokens.spacingSmall

            ComboBox {
                Layout.preferredWidth: 190
                Accessible.name: qsTr("Filter field")
                currentIndex: root.rowModel.field
                model: root.configuration.filterFieldNames
                onActivated: index => root.rowModel.field = index
            }

            TextField {
                Layout.fillWidth: true
                Accessible.name: qsTr("Filter value")
                placeholderText: {
                    if (root.rowModel.editor
                            === root.configuration.timecodeFilterEditor) {
                        return qsTr("HH:MM:SS:FF")
                    }
                    if (root.rowModel.editor
                            === root.configuration.durationFilterEditor) {
                        return qsTr("Frames")
                    }
                    return qsTr("Text to match")
                }
                text: root.rowModel.conditionText
                validator: root.rowModel.editor
                           === root.configuration.durationFilterEditor
                           ? durationValidator : null
                visible: root.rowModel.editor
                         !== root.configuration.trackKindFilterEditor
                         && root.rowModel.editor
                            !== root.configuration.editTypeFilterEditor
                onTextEdited: root.rowModel.conditionText = text
            }

            ComboBox {
                Layout.fillWidth: true
                Accessible.name: qsTr("Filter value")
                currentIndex: root.rowModel.selection
                model: root.rowModel.editor
                       === root.configuration.trackKindFilterEditor
                       ? root.configuration.filterTrackKindNames
                       : root.configuration.filterEditTypeNames
                visible: root.rowModel.editor
                         === root.configuration.trackKindFilterEditor
                         || root.rowModel.editor
                            === root.configuration.editTypeFilterEditor
                onActivated: index => root.rowModel.selection = index
            }

            Button {
                Accessible.description: qsTr("Remove filter condition %1")
                                            .arg(root.row + 1)
                enabled: root.conditionCount > 1
                text: qsTr("Remove")
                onClicked: root.configuration.RemoveFilterCondition(root.row)
            }
        }

        RowLayout {
            Layout.leftMargin: 190 + Atlas.DesignTokens.spacingSmall
            spacing: Atlas.DesignTokens.spacingLarge
            visible: root.rowModel.editor
                     === root.configuration.textFilterEditor

            CheckBox {
                checked: root.rowModel.matchCase
                text: qsTr("Match case")
                onToggled: root.rowModel.matchCase = checked
            }

            CheckBox {
                checked: root.rowModel.matchWholeWord
                text: qsTr("Whole word")
                onToggled: root.rowModel.matchWholeWord = checked
            }

            CheckBox {
                checked: root.rowModel.regularExpression
                text: qsTr("Regular expression")
                onToggled: root.rowModel.regularExpression = checked
            }
        }

        Rectangle {
            Layout.fillWidth: true
            color: Atlas.Theme.border
            implicitHeight: Atlas.DesignTokens.borderWidth
        }
    }

    IntValidator {
        id: durationValidator

        bottom: 0
    }
}
