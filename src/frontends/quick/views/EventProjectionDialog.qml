import EditAtlasStyle as Atlas
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    required property TimelineConfiguration configuration

    anchors.centerIn: parent
    height: Math.min(620, parent.height - Atlas.DesignTokens.spacingExtraLarge)
    modal: true
    objectName: "eventProjectionDialog"
    parent: Overlay.overlay
    standardButtons: Dialog.Close
    title: qsTr("Export Columns")
    width: Math.min(560, parent.width - Atlas.DesignTokens.spacingExtraLarge)

    Component.onCompleted: {
        standardButton(Dialog.Close).objectName = "closeProjectionButton"
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Atlas.DesignTokens.spacingMedium

        Label {
            Layout.fillWidth: true
            color: Atlas.Theme.textSecondary
            text: qsTr("Choose the event columns to export and arrange their order.")
            wrapMode: Text.WordWrap
        }

        ListView {
            id: projectionList

            Layout.fillHeight: true
            Layout.fillWidth: true
            Accessible.id: objectName
            Accessible.name: qsTr("Export event columns")
            clip: true
            model: root.configuration.eventProjectionModel
            objectName: "eventColumnsList"
            spacing: Atlas.DesignTokens.spacingExtraSmall

            delegate: Rectangle {
                id: projectionRow

                required property int index
                required property var model

                Accessible.checkable: true
                Accessible.checked: projectionRow.model.selected
                Accessible.id: objectName
                Accessible.name: projectionRow.model.display
                Accessible.role: Accessible.ListItem
                Accessible.onToggleAction: {
                    root.configuration.SetEventProjectionSelected(
                        projectionRow.index, !projectionRow.model.selected)
                }

                function reorderAt(position) {
                    const contentPosition = projectionList.contentItem
                        .mapFromItem(dragHandle, position.x, position.y)
                    const destinationRow = projectionList.indexAt(
                        contentPosition.x, contentPosition.y)
                    if (destinationRow >= 0
                            && destinationRow !== projectionRow.index) {
                        root.configuration.MoveEventProjection(
                            projectionRow.index, destinationRow)
                    }
                }

                border.color: reorderHandler.active
                              ? Atlas.Theme.accent : Atlas.Theme.border
                border.width: Atlas.DesignTokens.borderWidth
                color: reorderHandler.active
                       ? Atlas.Theme.controlHovered : index % 2 === 0
                       ? Atlas.Theme.control : Atlas.Theme.surface
                height: Atlas.DesignTokens.controlHeight
                radius: Atlas.DesignTokens.radiusMedium
                objectName: "eventColumn" + index
                width: ListView.view.width

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Atlas.DesignTokens.spacingMedium
                    anchors.rightMargin: Atlas.DesignTokens.spacingSmall
                    spacing: Atlas.DesignTokens.spacingSmall

                    Label {
                        id: dragHandle

                        Accessible.description: qsTr("Drag %1 to reorder")
                                                    .arg(projectionRow.model.display)
                        Accessible.role: Accessible.Button
                        Layout.alignment: Qt.AlignVCenter
                        color: reorderHandler.active
                               ? Atlas.Theme.accent
                               : Atlas.Theme.textSecondary
                        font.bold: true
                        text: "⋮⋮"

                        DragHandler {
                            id: reorderHandler

                            target: null
                            onCentroidChanged: {
                                if (active) {
                                    projectionRow.reorderAt(centroid.position)
                                }
                            }
                        }

                        HoverHandler {
                            cursorShape: reorderHandler.active
                                         ? Qt.ClosedHandCursor
                                         : Qt.OpenHandCursor
                        }
                    }

                    CheckBox {
                        Layout.fillWidth: true
                        checked: projectionRow.model.selected
                        objectName: "eventColumn" + projectionRow.index
                                    + "CheckBox"
                        text: projectionRow.model.display
                        onToggled: root.configuration.SetEventProjectionSelected(
                                       projectionRow.index, checked)
                    }

                    RowLayout {
                        spacing: Atlas.DesignTokens.spacingExtraSmall

                        ToolButton {
                            Accessible.description: qsTr("Move %1 up")
                                                        .arg(projectionRow.model.display)
                            enabled: projectionRow.index > 0
                            objectName: "eventColumn" + projectionRow.index
                                        + "MoveUpButton"
                            text: "↑"
                            onClicked: root.configuration.MoveEventProjectionUp(
                                           projectionRow.index)
                        }

                        ToolButton {
                            Accessible.description: qsTr("Move %1 down")
                                                        .arg(projectionRow.model.display)
                            enabled: projectionRow.index
                                     < projectionList.count - 1
                            objectName: "eventColumn" + projectionRow.index
                                        + "MoveDownButton"
                            text: "↓"
                            onClicked: root.configuration.MoveEventProjectionDown(
                                           projectionRow.index)
                        }
                    }
                }
            }

            ScrollBar.vertical: ScrollBar {}
        }

        Label {
            Layout.fillWidth: true
            color: Atlas.Theme.accent
            text: qsTr("Select at least one event column.")
            visible: !root.configuration.eventProjectionValid
        }
    }
}
