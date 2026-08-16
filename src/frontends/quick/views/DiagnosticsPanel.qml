import EditAtlasStyle as Atlas
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models

Atlas.Surface {
    id: root

    required property int diagnosticCount
    required property var diagnosticsModel

    readonly property var columnWidths: [120, 80, 520]

    Accessible.name: qsTr("Import diagnostics")
    implicitHeight: Math.min(240, panelLayout.implicitHeight)

    ColumnLayout {
        id: panelLayout

        anchors.fill: parent
        anchors.margins: Atlas.DesignTokens.spacingMedium
        spacing: Atlas.DesignTokens.spacingSmall

        Label {
            color: Atlas.Theme.textPrimary
            font.weight: Font.DemiBold
            text: qsTr("Diagnostics (%1)").arg(root.diagnosticCount)
        }

        HorizontalHeaderView {
            Layout.fillWidth: true
            clip: true
            syncView: diagnosticsTable

            delegate: Rectangle {
                required property var display

                color: Atlas.Theme.control
                implicitHeight: Atlas.DesignTokens.controlHeight

                Label {
                    anchors.fill: parent
                    anchors.leftMargin: Atlas.DesignTokens.spacingMedium
                    anchors.rightMargin: Atlas.DesignTokens.spacingMedium
                    color: Atlas.Theme.textPrimary
                    elide: Text.ElideRight
                    font.weight: Font.DemiBold
                    text: parent.display
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        TableView {
            id: diagnosticsTable

            Layout.fillHeight: true
            Layout.fillWidth: true
            Accessible.name: qsTr("Import diagnostics")
            clip: true
            columnSpacing: Atlas.DesignTokens.borderWidth
            model: root.diagnosticsModel
            pointerNavigationEnabled: true
            reuseItems: true
            rowSpacing: Atlas.DesignTokens.borderWidth
            selectionBehavior: TableView.SelectRows
            selectionMode: TableView.SingleSelection
            selectionModel: ItemSelectionModel {
                model: root.diagnosticsModel
            }

            columnWidthProvider: column => root.columnWidths[column]
            rowHeightProvider: () => Atlas.DesignTokens.controlHeight

            delegate: Rectangle {
                id: diagnosticCell

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
                    id: diagnosticText

                    anchors.fill: parent
                    anchors.leftMargin: Atlas.DesignTokens.spacingMedium
                    anchors.rightMargin: Atlas.DesignTokens.spacingMedium
                    color: Atlas.Theme.textPrimary
                    elide: Text.ElideRight
                    text: diagnosticCell.display === undefined
                          || diagnosticCell.display === null
                          ? "" : diagnosticCell.display
                    verticalAlignment: Text.AlignVCenter
                }

                HoverHandler {
                    id: diagnosticHover
                }

                ToolTip.text: diagnosticText.text
                ToolTip.visible: diagnosticHover.hovered
                                     && diagnosticText.truncated
            }

            ScrollBar.horizontal: ScrollBar {}
            ScrollBar.vertical: ScrollBar {}
        }
    }
}
