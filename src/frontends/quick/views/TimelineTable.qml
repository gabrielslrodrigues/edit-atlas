import EditAtlasStyle as Atlas
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models

Atlas.Surface {
    id: root

    required property ApplicationShell applicationShell

    readonly property var columnWidths: [
        84, 112, 112, 100, 180, 124,
        124, 124, 124, 124, 140, 280
    ]

    Accessible.name: qsTr("Timeline edit events")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Atlas.DesignTokens.borderWidth
        spacing: 0

        HorizontalHeaderView {
            Layout.fillWidth: true
            clip: true
            syncView: eventTable

            delegate: Rectangle {
                id: headerButton

                required property int column
                required property var display

                Accessible.name: display
                Accessible.description: qsTr("Sort timeline events by %1")
                                            .arg(display)
                Accessible.role: Accessible.Button
                activeFocusOnTab: true
                border.color: activeFocus
                              ? Atlas.Theme.focus : Atlas.Theme.border
                border.width: Atlas.DesignTokens.borderWidth
                color: headerTap.pressed
                       ? Atlas.Theme.controlPressed
                       : headerHover.hovered
                         ? Atlas.Theme.controlHovered
                         : Atlas.Theme.control
                implicitHeight: Atlas.DesignTokens.controlHeight

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Atlas.DesignTokens.spacingMedium
                    anchors.rightMargin: Atlas.DesignTokens.spacingMedium
                    spacing: Atlas.DesignTokens.spacingSmall

                    Label {
                        Layout.fillWidth: true
                        color: Atlas.Theme.textPrimary
                        elide: Text.ElideRight
                        font.weight: Font.DemiBold
                        text: headerButton.display
                    }

                    Label {
                        color: Atlas.Theme.accent
                        text: {
                            if (root.applicationShell.eventSortColumn
                                    !== headerButton.column) {
                                return ""
                            }
                            return root.applicationShell.eventSortAscending
                                   ? "▲" : "▼"
                        }
                        visible: text.length !== 0
                    }
                }

                HoverHandler {
                    id: headerHover
                }

                TapHandler {
                    id: headerTap

                    onTapped: root.applicationShell.ToggleEventSort(
                                  headerButton.column)
                }

                Keys.onEnterPressed: root.applicationShell.ToggleEventSort(
                                         headerButton.column)
                Keys.onReturnPressed: root.applicationShell.ToggleEventSort(
                                          headerButton.column)
                Keys.onSpacePressed: root.applicationShell.ToggleEventSort(
                                         headerButton.column)
            }
        }

        TableView {
            id: eventTable

            Layout.fillHeight: true
            Layout.fillWidth: true
            Accessible.name: qsTr("Timeline edit events")
            activeFocusOnTab: true
            clip: true
            columnSpacing: Atlas.DesignTokens.borderWidth
            keyNavigationEnabled: true
            model: root.applicationShell.eventModel
            pointerNavigationEnabled: true
            reuseItems: true
            rowSpacing: Atlas.DesignTokens.borderWidth
            selectionBehavior: TableView.SelectRows
            selectionMode: TableView.SingleSelection
            selectionModel: ItemSelectionModel {
                model: root.applicationShell.eventModel
            }

            columnWidthProvider: column => root.columnWidths[column]
            rowHeightProvider: () => Atlas.DesignTokens.controlHeight

            delegate: Rectangle {
                id: eventCell

                required property int column
                required property bool current
                required property var display
                required property int row
                required property bool selected

                color: selected || current
                       ? Atlas.Theme.controlHovered
                       : row % 2 === 0
                         ? Atlas.Theme.window : Atlas.Theme.surface

                Label {
                    id: eventText

                    anchors.fill: parent
                    anchors.leftMargin: Atlas.DesignTokens.spacingMedium
                    anchors.rightMargin: Atlas.DesignTokens.spacingMedium
                    color: Atlas.Theme.textPrimary
                    elide: Text.ElideRight
                    text: eventCell.display
                    verticalAlignment: Text.AlignVCenter
                }

                HoverHandler {
                    id: eventHover
                }

                ToolTip.text: eventText.text
                ToolTip.visible: eventHover.hovered && eventText.truncated
            }

            ScrollBar.horizontal: ScrollBar {}
            ScrollBar.vertical: ScrollBar {}
        }
    }
}
