import EditAtlas.Frontends.Quick
import EditAtlasStyle as Atlas
import QtQuick
import QtTest

TestCase {
    id: testCase

    name: "ApplicationShellView"
    when: windowShown

    property var applicationWindow: null
    property var temporaryFilterPanel: null
    property var temporaryTimelineTable: null

    Component {
        id: applicationComponent

        Main {
            applicationShell: testApplicationShell
            visible: true
        }
    }

    Component {
        id: timelineTableComponent

        TimelineTable {
            applicationShell: testApplicationShell
        }
    }

    Component {
        id: timelineConfigurationComponent

        TimelineConfigurationPanel {
            availableHeight: applicationWindow.height
            configuration: testApplicationShell.timelineConfiguration
            expanded: true
        }
    }

    function initTestCase() {
        applicationWindow = applicationComponent.createObject(null)
        verify(applicationWindow !== null)
        tryCompare(applicationWindow, "visible", true)
    }

    function cleanupTestCase() {
        applicationWindow.destroy()
        applicationWindow = null
    }

    function cleanup() {
        if (temporaryFilterPanel !== null) {
            temporaryFilterPanel.destroy()
            temporaryFilterPanel = null
        }
        if (temporaryTimelineTable !== null) {
            temporaryTimelineTable.destroy()
            temporaryTimelineTable = null
        }
    }

    // The column headers carry no identifier of their own, so they are
    // reached through the header view that owns them. Searching the item tree
    // instead finds delegates the view has stopped using, which are still
    // children of it and still answer for their column, but acting on one has
    // no effect.
    function findHeaderView(item) {
        const children = item.children === undefined ? [] : item.children
        for (let index = 0; index < children.length; ++index) {
            const child = children[index]
            if (child.itemAtCell !== undefined) {
                return child
            }
            const found = findHeaderView(child)
            if (found !== null) {
                return found
            }
        }
        return null
    }

    function findObject(identifier) {
        if (applicationWindow.objectName === identifier) {
            return applicationWindow
        }
        const visualObject = findChild(applicationWindow.contentItem,
                                       identifier)
        return visualObject !== null
                ? visualObject
                : findChild(applicationWindow, identifier)
    }

    function test_persistentAccessibilityIdentifiers() {
        const identifiers = [
            "mainWindow",
            "applicationMenuBar",
            "fileMenu",
            "recentFilesMenu",
            "languageSelector",
            "appearanceSelector",
            "helpMenu",
            "openDocumentAction",
            "rememberRecentFilesAction",
            "exportAction",
            "exportDiagnosticLogsAction",
            "aboutAction",
            "documentStack",
            "emptyOpenButton",
            "statusLabel",
            "timelineFilter",
            "eventTable"
        ]
        for (const identifier of identifiers) {
            verify(findObject(identifier) !== null,
                   "Missing objectName: " + identifier)
        }
    }

    function test_contentItemDoesNotDuplicateApplicationRole() {
        verify(applicationWindow.contentItem.Accessible.role
               !== Accessible.Application)
    }

    function test_emptyPageKeyboardFocus() {
        const openButton = findObject("emptyOpenButton")
        verify(openButton !== null)
        compare(openButton.Accessible.id, "emptyOpenButton")
        compare(openButton.Accessible.role, Accessible.Button)
        verify(openButton.Accessible.name.length > 0)
        verify(openButton.activeFocusOnTab)
        openButton.forceActiveFocus()
        tryVerify(() => openButton.activeFocus)
    }

    function test_frameRateOptionsExposeAccessibleListItems() {
        const dialog = findObject("frameRateDialog")
        const selector = findObject("frameRateSelector")
        verify(dialog !== null)
        verify(selector !== null)

        dialog.open()
        tryCompare(dialog, "visible", true)
        selector.popup.open()
        tryCompare(selector.popup, "visible", true)
        tryVerify(() => selector.popup.contentItem.itemAtIndex(1) !== null)
        selector.currentIndex = 1

        const option = selector.popup.contentItem.itemAtIndex(1)
        compare(option.Accessible.name, "24 fps")
        compare(option.Accessible.role, Accessible.ListItem)
        verify(option.Accessible.selectable)
        verify(option.Accessible.selected)
        compare(selector.contentItem.Accessible.name, selector.displayText)
        compare(selector.contentItem.Accessible.role, Accessible.StaticText)

        selector.popup.close()
        dialog.close()
    }

    function test_projectionDialogExposesAccessibleControls() {
        const dialog = findObject("eventProjectionDialog")
        verify(dialog !== null)
        dialog.open()
        tryCompare(dialog, "visible", true)

        const list = findChild(dialog, "eventColumnsList")
        verify(list !== null)
        verify(list.Accessible.name.length > 0)
        tryVerify(() => findChild(list.contentItem,
                                 "eventColumn0CheckBox") !== null)
        const firstRow = findChild(list.contentItem, "eventColumn0")
        const moveDown = findChild(list.contentItem,
                                   "eventColumn0MoveDownButton")
        verify(firstRow !== null)
        compare(firstRow.Accessible.id, "eventColumn0")
        compare(firstRow.Accessible.role, Accessible.ListItem)
        verify(moveDown !== null)
        compare(moveDown.Accessible.id, "eventColumn0MoveDownButton")

        dialog.reject()
        tryCompare(dialog, "visible", false)
    }

    function test_spreadsheetOptionsExposeBindingsAndCancel() {
        const dialog = findObject("spreadsheetOptionsDialog")
        verify(dialog !== null)
        dialog.openForExport()
        tryCompare(dialog, "visible", true)

        const language = findChild(dialog, "workbookLanguageSelector")
        const includeTimeline = findChild(
                    dialog, "includeTimelineSheetCheckBox")
        const includeDiagnostics = findChild(
                    dialog, "includeDiagnosticsSheetCheckBox")
        const continueButton = findChild(
                    dialog, "continueSpreadsheetExportButton")
        verify(language !== null)
        verify(includeTimeline !== null)
        verify(includeDiagnostics !== null)
        verify(continueButton !== null)
        verify(language.Accessible.name.length > 0)
        compare(includeTimeline.checked, true)
        compare(includeDiagnostics.checked, true)
        compare(continueButton.enabled, true)

        dialog.reject()
        tryCompare(dialog, "visible", false)
    }

    function test_templateNameDialogFocusAndCancellation() {
        const dialog = findObject("templateNameDialog")
        verify(dialog !== null)
        dialog.openFor(0)
        tryCompare(dialog, "visible", true)

        const editor = findChild(dialog, "templateNameEditor")
        verify(editor !== null)
        tryVerify(() => editor.activeFocus)
        verify(editor.Accessible.name.length > 0)

        dialog.reject()
        tryCompare(dialog, "visible", false)
    }

    function test_languageSelectionUsesStableIdentifiers() {
        const english = findObject("englishLanguageAction")
        const portuguese = findObject("brazilianPortugueseLanguageAction")
        verify(english !== null)
        verify(portuguese !== null)
        compare(english.checked, true)

        testApplicationShell.languageCode = "pt_BR"
        tryCompare(portuguese, "checked", true)
        compare(english.checked, false)

        testApplicationShell.languageCode = "en"
        tryCompare(english, "checked", true)
    }

    function test_appearanceSelectionUsesStableIdentifiers() {
        const system = findObject("systemAppearanceAction")
        const light = findObject("lightAppearanceAction")
        const dark = findObject("darkAppearanceAction")
        verify(system !== null)
        verify(light !== null)
        verify(dark !== null)

        const initial = Atlas.Appearance.appearanceCode

        Atlas.Appearance.appearanceCode = "dark"
        tryCompare(dark, "checked", true)
        compare(light.checked, false)
        compare(system.checked, false)

        light.triggered()
        tryCompare(light, "checked", true)
        compare(Atlas.Appearance.appearanceCode, "light")

        Atlas.Appearance.appearanceCode = initial
    }

    function test_paletteExposesEveryColorToQml() {
        // A palette member reading as undefined means QML has no type
        // information for the value type, which leaves every theme color
        // invalid rather than wrong.
        const palette = Atlas.Appearance.palette
        const names = ["accent", "border", "control", "surface",
                       "textPrimary", "window"]
        for (const name of names) {
            verify(palette[name] !== undefined, name + " is undefined")
            compare(palette[name].length, 7, name + " is not #rrggbb")
        }
    }

    function test_themeFollowsTheSelectedAppearance() {
        Atlas.Appearance.appearanceCode = "dark"
        const darkWindow = Atlas.Theme.window.toString()
        compare(darkWindow,
                Qt.color(Atlas.Appearance.palette.window).toString())

        Atlas.Appearance.appearanceCode = "light"
        tryVerify(() => Atlas.Theme.window.toString() !== darkWindow)
        compare(Atlas.Theme.window.toString(),
                Qt.color(Atlas.Appearance.palette.window).toString())

        Atlas.Appearance.appearanceCode = "dark"
        tryVerify(() => Atlas.Theme.window.toString() === darkWindow)
    }

    function test_recentFilesAccessibilityActionUpdatesApplicationState() {
        const action = findObject("rememberRecentFilesAction")
        verify(action !== null)
        const original = testApplicationShell.rememberRecentFiles

        action.Accessible.pressAction()
        tryCompare(testApplicationShell, "rememberRecentFiles", !original)

        action.Accessible.pressAction()
        tryCompare(testApplicationShell, "rememberRecentFiles", original)
    }

    function test_comboBoxAccessibilityActionsOpenAndSelect() {
        const dialog = findObject("frameRateDialog")
        const selector = findObject("frameRateSelector")
        verify(dialog !== null)
        verify(selector !== null)

        dialog.open()
        tryCompare(dialog, "visible", true)
        selector.currentIndex = 0
        compare(selector.popup.visible, false)

        // Neither the ComboBox role nor the ListItem role its options declare
        // advertises an action, so both of these are the project's own.
        selector.Accessible.pressAction()
        tryCompare(selector.popup, "visible", true)
        tryVerify(() => selector.popup.contentItem.itemAtIndex(1) !== null)

        const option = selector.popup.contentItem.itemAtIndex(1)
        option.Accessible.pressAction()
        tryCompare(selector, "currentIndex", 1)
        tryCompare(selector.popup, "visible", false)

        dialog.close()
        tryCompare(dialog, "visible", false)
    }

    function test_columnHeaderAccessibilityActionSortsTheColumn() {
        temporaryTimelineTable = timelineTableComponent.createObject(
            applicationWindow.contentItem,
            {
                "height": applicationWindow.height,
                "width": applicationWindow.width
            })
        verify(temporaryTimelineTable !== null)
        const headerView = findHeaderView(temporaryTimelineTable)
        verify(headerView !== null)
        tryVerify(() => headerView.itemAtCell(0, 0) !== null)
        const header = headerView.itemAtCell(0, 0)

        // A plain Item advertises the Button role's press action whether or
        // not anything implements it, so this asserts the effect rather than
        // the action being offered.
        header.Accessible.pressAction()
        tryCompare(testApplicationShell, "eventSortColumn", header.column)
        const ascending = testApplicationShell.eventSortAscending

        header.Accessible.pressAction()
        tryCompare(testApplicationShell, "eventSortAscending", !ascending)
        compare(testApplicationShell.eventSortColumn, header.column)
    }

    function test_aboutDialogCanBeCancelled() {
        const dialog = findObject("aboutDialog")
        verify(dialog !== null)
        dialog.open()
        tryCompare(dialog, "visible", true)
        const logButton = findChild(dialog, "openLogFolderButton")
        verify(logButton !== null)
        verify(logButton.activeFocusOnTab)
        logButton.forceActiveFocus()
        tryVerify(() => logButton.activeFocus)
        keyClick(Qt.Key_Tab)
        tryVerify(() => applicationWindow.activeFocusItem !== null
                  && applicationWindow.activeFocusItem !== logButton)
        dialog.reject()
        tryCompare(dialog, "visible", false)
    }

    function test_supportDisclosureCanBeCancelled() {
        const dialog = findObject("supportBundleDisclosureDialog")
        verify(dialog !== null)
        dialog.openForExport()
        tryCompare(dialog, "visible", true)
        keyClick(Qt.Key_Escape)
        tryCompare(dialog, "visible", false)
    }

    function test_dynamicFilterRowsHaveUniqueIdentifiers() {
        const configuration = testApplicationShell.timelineConfiguration
        temporaryFilterPanel = timelineConfigurationComponent.createObject(
            applicationWindow.contentItem,
            {
                "height": applicationWindow.height,
                "width": applicationWindow.width
            })
        const filter = temporaryFilterPanel
        verify(filter !== null)
        tryCompare(filter, "visible", true)
        const list = findChild(filter, "filterConditionsScrollArea")
        verify(list !== null)
        tryVerify(() => findChild(list.contentItem,
                                  "filterCondition0") !== null)

        configuration.AddFilterCondition()
        tryVerify(() => findChild(list.contentItem,
                                  "filterCondition1") !== null)
        verify(findChild(list.contentItem,
                         "filterCondition0Field") !== null)
        verify(findChild(list.contentItem,
                         "filterCondition1Field") !== null)

        configuration.ClearFilter()
        tryVerify(() => findChild(list.contentItem,
                                  "filterCondition1") === null)
    }

    function test_programmaticFilterTextUpdatesTheFilterModel() {
        const configuration = testApplicationShell.timelineConfiguration
        configuration.ClearFilter()
        temporaryFilterPanel = timelineConfigurationComponent.createObject(
            applicationWindow.contentItem,
            {
                "height": applicationWindow.height,
                "width": applicationWindow.width
            })
        verify(temporaryFilterPanel !== null)
        const firstList = findChild(temporaryFilterPanel,
                                    "filterConditionsScrollArea")
        verify(firstList !== null)
        tryVerify(() => findChild(firstList.contentItem,
                                  "filterCondition0Text") !== null)
        const firstEditor = findChild(firstList.contentItem,
                                      "filterCondition0Text")
        firstEditor.text = "BROLL"

        temporaryFilterPanel.destroy()
        temporaryFilterPanel = timelineConfigurationComponent.createObject(
            applicationWindow.contentItem,
            {
                "height": applicationWindow.height,
                "width": applicationWindow.width
            })
        verify(temporaryFilterPanel !== null)
        const secondList = findChild(temporaryFilterPanel,
                                     "filterConditionsScrollArea")
        verify(secondList !== null)
        tryVerify(() => findChild(secondList.contentItem,
                                  "filterCondition0Text") !== null)
        const secondEditor = findChild(secondList.contentItem,
                                       "filterCondition0Text")
        compare(secondEditor.text, "BROLL")
        configuration.ClearFilter()
    }
}
