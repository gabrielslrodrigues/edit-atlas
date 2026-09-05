"""Semantic user operations for the Edit Atlas desktop application."""

from __future__ import annotations

from pathlib import Path
from typing import Callable

from adapters.desktop import DesktopSession
from application.polling import PollTimeoutError


SessionFactory = Callable[[str], DesktopSession]


class EditAtlasApplication:
    """Runner-independent façade over a platform accessibility session."""

    def __init__(self, session_factory: SessionFactory) -> None:
        self._session_factory = session_factory
        self._launch_count = 0
        self._session = self._launch()

    @property
    def session(self) -> DesktopSession:
        return self._session

    def restart(self) -> None:
        self._session.close()
        self._session = self._launch()

    def close(self) -> None:
        self._session.close()

    def capture_artifacts(self, stem: str) -> None:
        self._session.capture_artifacts(stem)

    def switch_language(self, language: str) -> None:
        actions = {
            "English": "englishLanguageAction",
            "Português (Brasil)": "brazilianPortugueseLanguageAction",
        }
        self._session.activate_menu_action(
            "languageSelector", actions[language]
        )

    APPEARANCE_ACTIONS = {
        "System": "systemAppearanceAction",
        "Light": "lightAppearanceAction",
        "Dark": "darkAppearanceAction",
    }

    def switch_appearance(self, appearance: str) -> None:
        self._session.activate_menu_action(
            "appearanceSelector", self.APPEARANCE_ACTIONS[appearance]
        )

    def selected_appearance(self) -> str:
        self._open_menu(
            "appearanceSelector", member=self.APPEARANCE_ACTIONS["System"]
        )
        selected = [
            name
            for name, identifier in self.APPEARANCE_ACTIONS.items()
            if self._session.is_checked(identifier)
        ]
        assert len(selected) == 1, f"expected one appearance, got {selected}"
        return selected[0]

    def set_recent_files_enabled(self, enabled: bool) -> None:
        self._open_menu("fileMenu")
        self._session.set_checked("rememberRecentFilesAction", enabled)

    def open_timeline(
        self,
        path: Path,
        *,
        frame_rate: str | None = None,
        expect_document: bool = True,
    ) -> None:
        self._session.activate_menu_action("fileMenu", "openDocumentAction")
        self._session.open_file_dialog("timelineOpenFileDialog", path)
        if frame_rate is not None:
            self._session.select_option("frameRateSelector", frame_rate)
            self._session.activate("acceptFrameRateButton")
        if expect_document:
            self._session.element("timelineTitleLabel")

    def wait_import_failure(self) -> str:
        return self._session.wait_text_nonempty("failureDescriptionLabel")

    def table_text(self) -> list[str]:
        return self._session.text_content("eventTable")

    def set_filter_field(self, index: int, field: str) -> None:
        self._ensure_filters_visible()
        self._session.select_option(f"filterCondition{index}Field", field)

    def set_filter_text(self, index: int, value: str) -> None:
        self._ensure_filters_visible()
        self._session.set_text(f"filterCondition{index}Text", value)

    def set_filter_track_kind(self, index: int, value: str) -> None:
        self._session.select_option(f"filterCondition{index}TrackKind", value)

    def set_filter_option(self, index: int, option: str, enabled: bool) -> None:
        self._session.set_checked(f"filterCondition{index}{option}", enabled)

    def set_filter_combination(self, value: str) -> None:
        self._session.select_option("filterCombination", value)

    def add_filter_condition(self) -> None:
        self._session.activate("addFilterConditionButton")

    def clear_filters(self) -> None:
        self._session.activate("clearFiltersButton")
        self._session.wait_text_contains(
            "filterResultLabel", "Showing 4 of 4 events"
        )

    def wait_event_count(self, visible: int, total: int = 4) -> str:
        return self._session.wait_text_contains(
            "filterResultLabel", f"Showing {visible} of {total} events"
        )

    def save_template(self, name: str) -> None:
        self._session.activate("templatePrimaryButton")
        self._complete_template_name(name)

    def edit_template_export_columns(
        self, checked: set[str], order: list[str]
    ) -> None:
        self._template_action("editExportColumnsAction")
        self._session.element("eventColumnsList")
        self.set_export_columns(checked, order)
        if self._session.has_element("closeProjectionButton"):
            self._session.activate("closeProjectionButton")
        else:
            self._session.activate("saveProjectionButton")
        self._session.wait_absent("eventColumnsList")

    def update_template(self) -> None:
        if self._session.has_element("updateTemplateButton"):
            self._session.activate("updateTemplateButton")
        else:
            self._session.activate("templatePrimaryButton")
            self._session.wait_name_contains("templatePrimaryButton", "Save as")

    def select_template(self, name: str) -> None:
        self._session.select_option("templateSelector", name)

    def rename_template(self, name: str) -> None:
        self._template_action("renameTemplateAction")
        self._complete_template_name(name)

    def duplicate_template(self, name: str) -> None:
        self._template_action("duplicateTemplateAction")
        self._complete_template_name(name)

    def delete_template(self) -> None:
        self._template_action("deleteTemplateAction")
        self._session.activate("confirmDeleteTemplateButton")
        self._session.wait_absent("deleteTemplateDialog")
        self._session.wait_selected_option("templateSelector", "No template")

    def set_export_columns(self, checked: set[str], order: list[str]) -> None:
        opened_projection = self._open_spreadsheet_export_columns()
        available = self._session.list_items("eventColumnsList")
        # Arrange rows before a selection can reveal conditional controls and
        # shrink the list's visible area.
        for target_index, name in enumerate(order):
            while available.index(name) > target_index:
                position = available.index(name)
                quick_move_button = f"eventColumn{position}MoveUpButton"
                if self._session.has_element(
                    quick_move_button, showing=False
                ):
                    # The row may be virtualized out of the visible
                    # viewport; bring it into view before clicking so the
                    # click actually lands on it.
                    self._session.focus(quick_move_button, showing=False)
                    self._session.activate(quick_move_button, showing=False)
                else:
                    # One operation on purpose: enumerating the list's
                    # accessible children resets the widget's current row,
                    # which is what the movement control acts on, so nothing
                    # may look an element up between the two steps.
                    self._session.move_list_item(
                        "eventColumnsList", name, "moveColumnUpButton"
                    )
                available = self._await_moved_column(name, available)
        # Apply the leading selections last so all remaining rows stay visible.
        for name in reversed(available):
            self._session.set_list_item_checked(
                "eventColumnsList", name, name in checked
            )
        if opened_projection:
            self._close_spreadsheet_export_columns()

    def _await_moved_column(
        self, name: str, before: list[str]
    ) -> list[str]:
        """Waits for the requested row, and only it, to move up one place.

        Comparing the whole list against a predicted order could only report
        that the prediction never arrived. A control that moved a different
        row is a distinct failure, and reconstructing which one from a
        screenshot afterwards is what this reports directly instead.
        """
        position = before.index(name)
        expected = list(before)
        expected[position - 1], expected[position] = (
            expected[position],
            expected[position - 1],
        )
        try:
            return self._session.wait_list_items("eventColumnsList", expected)
        except PollTimeoutError as error:
            observed = self._session.list_items("eventColumnsList")
            raise AssertionError(
                self._describe_move(name, position, before, observed)
            ) from error

    def _describe_move(
        self,
        name: str,
        position: int,
        before: list[str],
        observed: list[str],
    ) -> str:
        moved = [
            f"{item!r} from {index} to {observed.index(item)}"
            for index, item in enumerate(before)
            if item in observed and observed.index(item) != index
        ]
        return (
            f"moving {name!r} up from index {position} moved "
            + (", ".join(moved) if moved else "nothing")
            + f"; afterwards {self._describe_current_row()}"
        )

    def _describe_current_row(self) -> str:
        current = self._session.current_list_item("eventColumnsList")
        enabled = self._session.is_sensitive("moveColumnUpButton")
        return (
            f"the list reported {current!r} as its current row and "
            "moveColumnUpButton was "
            + ("enabled" if enabled else "disabled")
        )

    def spreadsheet_export_column_selection(
        self,
    ) -> tuple[list[str], set[str]]:
        opened_projection = self._open_spreadsheet_export_columns()
        columns = self._session.list_items("eventColumnsList")
        checked = {
            name
            for name in columns
            if self._session.is_list_item_checked("eventColumnsList", name)
        }
        if opened_projection:
            self._close_spreadsheet_export_columns()
        return (columns, checked)

    def begin_spreadsheet_export(
        self,
        *,
        workbook_language: str = "English",
        include_timeline: bool = False,
        include_diagnostics: bool = False,
    ) -> None:
        self._session.activate("timelineExportButton")
        self._session.element("spreadsheetOptionsDialog")
        self._session.select_option("workbookLanguageSelector", workbook_language)
        self._session.set_checked(
            "includeTimelineSheetCheckBox", include_timeline
        )
        self._session.set_checked(
            "includeDiagnosticsSheetCheckBox", include_diagnostics
        )

    def cancel_spreadsheet_options(self) -> None:
        self._session.activate("cancelSpreadsheetExportButton")
        self._session.wait_absent("spreadsheetOptionsDialog")

    def select_rendered_video(self, path: Path) -> None:
        self._session.activate("browseRenderedVideoButton")
        self._session.open_file_dialog("renderedVideoOpenFileDialog", path)

    def continue_spreadsheet_export(self, destination: Path) -> None:
        self._session.activate("continueSpreadsheetExportButton")
        self._session.open_file_dialog("spreadsheetSaveFileDialog", destination)

    def cancel_rendered_video_export(self) -> list[str]:
        self._session.element("spreadsheetExportProgressDialog")
        progress = self._session.visible_text(
            "spreadsheetExportProgressDialog"
        )
        self._session.activate("cancelFrameExtractionButton")
        self._session.wait_absent("spreadsheetExportProgressDialog")
        return progress

    def cancel_spreadsheet_replacement(self) -> None:
        self._session.activate("cancelReplaceSpreadsheetButton")
        self._session.wait_absent("replaceSpreadsheetDialog")

    def replace_spreadsheet(self) -> None:
        self._session.activate("replaceSpreadsheetButton")

    def finish_spreadsheet_export(self) -> None:
        self._session.element("spreadsheetExportResultDialog")
        self._session.activate("closeDialogButton")
        self._session.wait_absent("spreadsheetExportResultDialog")

    def finish_rendered_video_export_failure(self) -> list[str]:
        self._session.element("renderedVideoExportFailureDialog")
        description = self._session.visible_text(
            "renderedVideoExportFailureDialog"
        )
        self._session.activate("closeDialogButton")
        self._session.wait_absent("renderedVideoExportFailureDialog")
        return description

    def export_support_bundle(self, destination: Path) -> None:
        self._session.activate_menu_action(
            "helpMenu", "exportDiagnosticLogsAction"
        )
        self._session.element("supportBundleDisclosureDialog")
        self._session.activate("continueSupportBundleButton")
        self._session.open_file_dialog("supportBundleSaveFileDialog", destination)
        self._session.element("supportBundleResultDialog")
        self._session.activate("closeDialogButton")
        self._session.wait_absent("supportBundleResultDialog")

    def about_information(self) -> list[str]:
        self._session.activate_menu_action("helpMenu", "aboutAction")
        self._session.element("aboutDialog")
        information = self._session.visible_text("aboutDialog")
        self._session.activate("closeDialogButton")
        self._session.wait_absent("aboutDialog")
        return information

    def _ensure_filters_visible(self) -> None:
        if self._session.has_element("filterCondition0Field"):
            return
        if self._session.has_element("toggleFiltersButton"):
            self._session.activate("toggleFiltersButton")
        self._session.element("filterCondition0Field")

    def _open_spreadsheet_export_columns(self) -> bool:
        if self._session.has_element("eventColumnsList"):
            return False
        if self._session.has_element("editSpreadsheetColumnsButton"):
            self._session.activate("editSpreadsheetColumnsButton")
        self._session.element("eventColumnsList")
        return True

    def _close_spreadsheet_export_columns(self) -> None:
        self._session.activate("closeProjectionButton")
        self._session.wait_absent("eventColumnsList")

    def _complete_template_name(self, name: str) -> None:
        self._session.element("templateNameDialog")
        self._session.set_text("templateNameEditor", name)
        self._session.activate("acceptTemplateNameButton")
        self._session.wait_absent("templateNameDialog")
        self._session.wait_selected_option("templateSelector", name)

    def _template_action(self, identifier: str) -> None:
        # Always through the menu path, even when the item is already
        # showing: that path leaves an open menu alone, and it is what
        # dismisses the menu afterwards on Windows.
        self._session.activate_menu_action("templateActionsButton", identifier)

    def _open_menu(
        self, identifier: str, *, member: str | None = None
    ) -> None:
        # Activating an open menu closes it, so reopening one to read a second
        # item loses both. Callers that read several items pass a member to
        # probe, and an already-open menu is then left alone.
        if member is not None and self._session.has_element(member):
            return
        self._session.activate(identifier)

    def _launch(self) -> DesktopSession:
        self._launch_count += 1
        return self._session_factory(f"edit-atlas-{self._launch_count}")
