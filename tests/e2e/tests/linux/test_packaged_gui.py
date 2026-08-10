from __future__ import annotations

from pathlib import Path

from application.gui import EditAtlasApplication
from inspectors.support_bundle import SupportBundle
from inspectors.xlsx import XlsxWorkbook


def test_startup_import_failure_recovery_and_preferences_persist(
    edit_atlas_application: EditAtlasApplication,
    fixture_directory: Path,
    output_directory: Path,
) -> None:
    app = edit_atlas_application
    assert "Abrir" in app.session.text("emptyOpenButton")

    app.switch_language("English")
    assert "Open Timeline" in app.session.text("emptyOpenButton")
    app.set_recent_files_enabled(True)
    app.open_timeline(fixture_directory / "mixed_tracks.edl", frame_rate="24 fps")

    assert "SYNTHETIC MIXED TRACKS" in app.session.text("timelineTitleLabel")
    assert "4 events" in app.session.text("timelineSummary")
    table_text = "\n".join(app.table_text())
    assert "001" in table_text
    assert "opening.mov" in table_text
    assert "SYNTHETIC AUDIO NOTE" in table_text
    assert not app.session.has_element("diagnosticsTree")

    invalid = output_directory / "invalid-encoding.edl"
    invalid.write_bytes(b"\xff\xfeA")
    app.open_timeline(invalid, expect_document=False)
    assert app.wait_import_failure()
    assert app.session.visible_text("diagnosticsTree")

    app.open_timeline(fixture_directory / "mixed_tracks.edl", frame_rate="24 fps")
    app.wait_event_count(4)
    app.restart()

    assert "Open Timeline" in app.session.text("emptyOpenButton")
    app.session.activate("fileMenu")
    assert app.session.is_checked("rememberRecentFilesAction")
    app.session.activate("recentFilesMenu")
    app.session.element("recentFileAction0")
    assert "mixed_tracks.edl" in app.session.element_name("recentFileAction0")


def test_filter_and_template_workflow_persists(
    edit_atlas_application: EditAtlasApplication,
    fixture_directory: Path,
) -> None:
    app = edit_atlas_application
    app.switch_language("English")
    app.open_timeline(fixture_directory / "mixed_tracks.edl", frame_rate="24 fps")

    app.set_filter_field(0, "Reel")
    app.set_filter_text(0, "BROLL")
    app.wait_event_count(2)
    app.add_filter_condition()
    app.set_filter_field(1, "Track type")
    app.set_filter_track_kind(1, "Video")
    app.wait_event_count(1)
    app.set_filter_combination("Any condition")
    app.wait_event_count(4)
    app.set_filter_combination("All conditions")
    app.wait_event_count(1)

    app.clear_filters()
    app.set_filter_field(0, "Comments")
    app.set_filter_text(0, "synthetic audio note")
    app.wait_event_count(1)
    app.set_filter_option(0, "MatchCase", True)
    app.wait_event_count(0)
    app.set_filter_option(0, "MatchCase", False)
    app.set_filter_option(0, "MatchWholeWord", True)
    app.wait_event_count(1)
    app.set_filter_option(0, "RegularExpression", True)
    app.set_filter_text(0, "SYNTHETIC.*NOTE")
    app.wait_event_count(1)
    app.set_filter_text(0, "[")
    assert app.session.text("filterErrorLabel")
    assert not app.session.is_sensitive("timelineExportButton")

    app.clear_filters()
    app.set_filter_field(0, "Reel")
    app.set_filter_text(0, "BROLL")
    app.wait_event_count(2)
    assert app.session.is_sensitive("timelineExportButton")
    app.edit_template_export_columns(
        {"Comments", "Event"}, ["Comments", "Event"]
    )
    app.save_template("B-roll")
    app.add_filter_condition()
    app.set_filter_field(1, "Track type")
    app.set_filter_track_kind(1, "Video")
    app.wait_event_count(1)
    app.update_template()
    app.rename_template("Primary")
    app.duplicate_template("Copy")
    app.delete_template()

    app.restart()
    app.open_timeline(fixture_directory / "mixed_tracks.edl", frame_rate="24 fps")
    app.select_template("Primary")
    app.wait_event_count(1)
    app.begin_spreadsheet_export()
    columns = app.session.list_items("eventColumnsList")
    assert columns[:2] == ["Comments", "Event"]
    assert app.session.is_list_item_checked("eventColumnsList", "Comments")
    assert app.session.is_list_item_checked("eventColumnsList", "Event")
    app.cancel_spreadsheet_options()


def test_filtered_spreadsheet_and_private_support_bundle(
    edit_atlas_application: EditAtlasApplication,
    fixture_directory: Path,
    output_directory: Path,
) -> None:
    app = edit_atlas_application
    app.switch_language("English")
    app.open_timeline(fixture_directory / "mixed_tracks.edl", frame_rate="24 fps")
    app.set_filter_field(0, "Reel")
    app.set_filter_text(0, "BROLL")
    app.wait_event_count(2)

    destination = output_directory / "linux-gui-filtered.xlsx"
    destination.unlink(missing_ok=True)
    app.begin_spreadsheet_export()
    app.set_export_columns({"Comments", "Event"}, ["Comments", "Event"])
    app.continue_spreadsheet_export(destination)
    app.finish_spreadsheet_export()

    workbook = XlsxWorkbook(destination)
    assert workbook.sheet_names() == ["Events"]
    assert workbook.event_row_count() == 2
    assert workbook.event_headers() == ["Comments", "Event"]
    assert "SYNTHETIC AUDIO NOTE" in workbook.shared_strings()

    original = destination.read_bytes()
    app.begin_spreadsheet_export()
    app.continue_spreadsheet_export(destination)
    app.cancel_spreadsheet_replacement()
    assert destination.read_bytes() == original

    app.begin_spreadsheet_export()
    app.continue_spreadsheet_export(destination)
    app.replace_spreadsheet()
    app.finish_spreadsheet_export()
    assert XlsxWorkbook(destination).event_row_count() == 2

    bundle_path = output_directory / "linux-gui-support.zip"
    bundle_path.unlink(missing_ok=True)
    app.export_support_bundle(bundle_path)
    bundle = SupportBundle(bundle_path)
    summary = bundle.environment_summary()
    for label in (
        "Application version",
        "Operating system",
        "Architecture",
        "Qt version",
        "Qt platform plugin",
        "cmx-3600",
        "xlsx",
    ):
        assert label in summary
    assert any(name.startswith("logs/") for name in bundle.entry_names())
    bundle.assert_private({"mixed_tracks.edl", destination.name})
