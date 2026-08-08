# Accessibility automation contract

Edit Atlas exposes stable, nonlocalized identifiers for assistive technology
and black-box desktop automation. User-facing names and descriptions remain
localized; automation must select objects by identifier instead of visible
text.

Qt widgets and dialogs use `accessibleIdentifier`. `QAction` does not provide
that property, so actions use the same value for `objectName` and a dynamic
`accessibleIdentifier` property. Platform adapters may use the action name
exposed by the Qt accessibility bridge.

Identifiers are unique within a simultaneously displayed window or dialog.
Common modal controls such as `closeDialogButton` may be reused across dialogs
because only one of those modal scopes is active at a time.

## Persistent application surface

| Area | Identifiers |
| --- | --- |
| Window and menus | `mainWindow`, `applicationMenuBar`, `fileMenu`, `recentFilesMenu`, `helpMenu`, `languageSelector` |
| File actions | `openDocumentAction`, `rememberRecentFilesAction`, `exportAction`, `exitAction`, `recentFileActionN` |
| Help actions | `exportDiagnosticLogsAction`, `aboutAction` |
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
| Spreadsheet option buttons | `continueSpreadsheetExportButton`, `cancelSpreadsheetExportButton` |

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
renumbering, and stability across application translation changes.
