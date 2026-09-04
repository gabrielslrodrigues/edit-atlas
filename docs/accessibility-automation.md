# Accessibility automation contract

Edit Atlas exposes stable, nonlocalized identifiers for assistive technology
and black-box desktop automation. User-facing names and descriptions remain
localized; automation must select objects by identifier instead of visible
text.

Architectural ownership and toolkit-specific implementation rules are
documented in the [Qt Quick](api/qt-quick-frontend.md) and
[Qt Widgets](api/qt-widgets-frontend.md) frontend guides.

Qt widgets and dialogs use `accessibleIdentifier`. `QAction` does not provide
that property, so actions use the same value for `objectName` and a dynamic
`accessibleIdentifier` property. Qt Quick views use the same `objectName` for
in-process lookup and bind it to `Accessible.id`, which Qt's accessibility
bridges expose as the native automation identifier. Platform adapters may use
the action name exposed by the Qt accessibility bridge.

Identifiers are unique within a simultaneously displayed window or dialog.
Common modal controls such as `closeDialogButton` may be reused across dialogs
because only one of those modal scopes is active at a time.

## Invocable actions

An identifier lets automation find a control; an action lets it operate one.
Assistive technology and platform adapters prefer actions. Declaring a role
does not guarantee that Qt advertises or implements the complete action, so
each control must expose the behavior its semantic operation requires.

Every control this project owns exposes an action where the platform
accessibility contract can represent the complete interaction:

- Controls derived from `AbstractButton` keep the press action provided by Qt.
- A combo box declares a press action that opens and closes its popup.
- A combo box option declares a press action that emits the delegate's
  `clicked` signal, selecting the option and closing the popup.
- An event projection row declares a toggle action for including and
  excluding the column, for the same reason, and a press action that makes
  the row current. Selecting a row and making it current are different
  states, because the buttons beside the list act on the current one. The
  Windows adapter clicks Widgets rows at their reported bounds because UIA
  does not expose the current row separately and can report a checked row as
  selected without making it current. It then confirms the clicked row is the
  one Qt reports as focused, which is the current row: the movement buttons
  are enabled for any current row above the first, so their state cannot
  distinguish a click that landed one row off from one that landed correctly.
- A timeline column header declares a press action that sorts the column.

Five cases use pointer input, and all five are deliberate:

- Qt's built-in file chooser delegates, which this project does not own and
  which expose no action.
- The event projection drag handle, whose `Button` role has no press
  behaviour to offer: reordering is a drag, and its keyboard equivalent
  belongs to the row rather than the handle.
- Windows Widgets event projection rows, where a bounds-derived click makes
  the row current before a shared movement button is used, and the adapter
  verifies which row it made current.
- Windows menu openers whose accepted action does not provide the complete
  interaction. Qt Quick QML menu bar items retain their implemented press path;
  Qt Widgets `QAction` menu titles use a bounds-derived click, and the template
  actions button falls back to one when needed.
- Windows menu leaf actions. Their press triggers the action while leaving the
  menu open; a click both triggers and dismisses it, which is the complete
  interaction.

Adapters verify the state produced by an action rather than treating provider
acceptance as success. Qt Quick Test asserts the effect of declared actions.

## Persistent application surface

| Area | Identifiers |
| --- | --- |
| Window and menus | `mainWindow`, `applicationMenuBar`, `fileMenu`, `recentFilesMenu`, `helpMenu`, `languageSelector`, `appearanceSelector` |
| File actions | `openDocumentAction`, `rememberRecentFilesAction`, `exportAction`, `exitAction`, `recentFileActionN` |
| Help actions | `exportDiagnosticLogsAction`, `aboutAction` |
| Appearance actions | `systemAppearanceAction`, `lightAppearanceAction`, `darkAppearanceAction` |
| Document state | `applicationShell`, `documentStack`, `emptyOpenButton`, `loadingLabel`, `timelineTitleLabel`, `timelineSummary`, `failureDescriptionLabel`, `failureOpenButton` |
| Results | `filterResultLabel`, `eventTable`, `diagnosticsTree`, `timelineExportButton` |
| Templates | `templateSelector`, `templatePrimaryButton`, `templateActionsButton`, `templateActionsMenu`, `saveTemplateAction`, `editExportColumnsAction`, `renameTemplateAction`, `duplicateTemplateAction`, `deleteTemplateAction` |
| Filters | `timelineFilter`, `filterCombination`, `addFilterConditionButton`, `clearFiltersButton`, `filterConditionsScrollArea`, `filterErrorLabel` |

Dynamic filter rows use their current zero-based order:

```text
filterConditionN
filterConditionNField
filterConditionNText
filterConditionNTrackKind
filterConditionNEditType
filterConditionNMatchCase
filterConditionNMatchWholeWord
filterConditionNRegularExpression
filterConditionNRemove
```

Identifiers are renumbered after a row is removed. An adapter should reacquire
a row after operations that add, remove, clear, or apply filter conditions.

## Projection and spreadsheet options

| Dialog or control | Identifier |
| --- | --- |
| Template projection dialog | `eventProjectionDialog` |
| Spreadsheet options dialog | `spreadsheetOptionsDialog` |
| Projection controls | `eventProjectionWidget`, `eventColumnsList`, `moveColumnUpButton`, `moveColumnDownButton`, `columnSelectionErrorLabel` |
| Projection dialog buttons | `saveProjectionButton`, `cancelProjectionButton` |
| Workbook options | `workbookLanguageSelector`, `includeTimelineSheetCheckBox`, `includeDiagnosticsSheetCheckBox` |
| Spreadsheet video input | `renderedVideoGroup`, `renderedVideoPathField`, `browseRenderedVideoButton` |
| Spreadsheet option buttons | `continueSpreadsheetExportButton`, `cancelSpreadsheetExportButton` |
| Initial-frame export progress | `spreadsheetExportProgressDialog`, `cancelFrameExtractionButton` |

The Widgets projection editor exposes one selected row and shared
`moveColumnUpButton` and `moveColumnDownButton` actions. Qt Quick exposes the
equivalent controls on every projected row as
`eventColumnNMoveUpButton`, `eventColumnNMoveDownButton`, and
`eventColumnNCheckBox`. Its projection dialog applies changes immediately and
uses `closeProjectionButton`. Semantic E2E operations accommodate both forms
without selecting controls by translated text.

## Workflow dialogs

File selection uses `timelineOpenFileDialog`, `spreadsheetSaveFileDialog`, and
`supportBundleSaveFileDialog` when Qt exposes the dialog widget. Native file
dialog contents remain platform-owned and must be automated through their
native accessibility roles.

The remaining workflow identifiers are:

- `frameRateDialog` with `frameRateSelector`, `acceptFrameRateButton`, and
  `cancelFrameRateButton`;
- `templateNameDialog` with `templateNameEditor`,
  `acceptTemplateNameButton`, and `cancelTemplateNameButton`;
- `deleteTemplateDialog` with `confirmDeleteTemplateButton` and
  `cancelDeleteTemplateButton`;
- `invalidTemplatesDialog`, `invalidTemplateNameDialog`,
  `invalidTemplateFilterDialog`, and `templateFailureDialog`, each with
  `closeDialogButton`;
- `spreadsheetExporterUnavailableDialog` with `closeDialogButton`;
- `replaceSpreadsheetDialog` with `replaceSpreadsheetButton` and
  `cancelReplaceSpreadsheetButton`;
- `spreadsheetExportResultDialog` with `revealSpreadsheetButton` and
  `closeDialogButton`;
- `spreadsheetExportFailureDialog` and `revealSpreadsheetFailureDialog`, each
  with `closeDialogButton`;
- `supportBundleDisclosureDialog` with `continueSupportBundleButton` and
  `cancelSupportBundleButton`;
- `replaceSupportBundleDialog` with `replaceSupportBundleButton` and
  `cancelReplaceSupportBundleButton`;
- `supportBundleResultDialog` with `revealSupportBundleButton` and
  `closeDialogButton`;
- `supportBundleFailureDialog` and `revealSupportBundleFailureDialog`, each
  with `closeDialogButton`;
- `aboutDialog` with `closeDialogButton`.

Integration tests enforce required coverage, uniqueness, dynamic-row
renumbering, and stability across application translation changes. Qt Quick
Test additionally enforces QML bindings, accessible names and roles, keyboard
focus, projected controls, and dialog cancellation against the real shared
ViewModels.
