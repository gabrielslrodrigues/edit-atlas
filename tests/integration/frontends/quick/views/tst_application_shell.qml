import EditAtlas.Frontends.Quick
import QtQuick
import QtTest

TestCase {
    id: testCase

    name: "ApplicationShellView"
    when: windowShown

    property var applicationWindow: null

    Component {
        id: applicationComponent

        Main {
            applicationShell: testApplicationShell
            visible: true
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
        const filter = findObject("timelineFilter")
        verify(filter !== null)
        filter.expanded = true
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
}
