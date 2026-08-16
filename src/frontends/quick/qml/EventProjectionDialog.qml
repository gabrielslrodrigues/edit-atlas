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
    parent: Overlay.overlay
    standardButtons: Dialog.Close
    title: qsTr("Export Columns")
    width: Math.min(560, parent.width - Atlas.DesignTokens.spacingExtraLarge)

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
            Accessible.name: qsTr("Export event columns")
            clip: true
            model: root.configuration.eventProjectionModel
            spacing: Atlas.DesignTokens.spacingExtraSmall

            delegate: Rectangle {
                id: projectionRow

                required property int index
                required property var model

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
                            text: "↑"
                            onClicked: root.configuration.MoveEventProjectionUp(
                                           projectionRow.index)
                        }

                        ToolButton {
                            Accessible.description: qsTr("Move %1 down")
                                                        .arg(projectionRow.model.display)
                            enabled: projectionRow.index
                                     < projectionList.count - 1
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
