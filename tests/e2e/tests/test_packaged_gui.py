from __future__ import annotations

from pathlib import Path

import pytest

from application.gui import EditAtlasApplication
from inspectors.support_bundle import SupportBundle
from inspectors.xlsx import XlsxWorkbook


def test_about_dialog_exposes_application_information(
    edit_atlas_application: EditAtlasApplication,
) -> None:
    app = edit_atlas_application
    app.switch_language("English")
    information = app.about_information()

    for expected in ("Edit Atlas", "structured reports"):
        assert any(expected in text for text in information)


def test_startup_import_failure_recovery_and_preferences_persist(
    edit_atlas_application: EditAtlasApplication,
    fixture_directory: Path,
    output_directory: Path,
) -> None:
    app = edit_atlas_application
    app.session.wait_text_contains("emptyOpenButton", "Abrir")

    app.switch_language("English")
    app.session.wait_text_contains("emptyOpenButton", "Open Timeline")
    app.set_recent_files_enabled(True)
    app.open_timeline(fixture_directory / "mixed_tracks.edl", frame_rate="24 fps")

    app.session.wait_text_contains("timelineTitleLabel", "SYNTHETIC MIXED TRACKS")
    app.session.wait_text_contains("timelineSummary", "4 events")
    table_text = "\n".join(app.table_text())
    assert "001" in table_text
    assert "opening.mov" in table_text
    assert "SYNTHETIC AUDIO NOTE" in table_text
    app.session.wait_absent("diagnosticsTree")

    invalid = output_directory / "invalid-encoding.edl"
    invalid.write_bytes(b"\xff\xfeA")
    app.open_timeline(invalid, expect_document=False)
    assert app.wait_import_failure()
    assert app.session.visible_text("diagnosticsTree")

    app.open_timeline(fixture_directory / "mixed_tracks.edl", frame_rate="24 fps")
    app.wait_event_count(4)
    app.restart()

    app.session.wait_text_contains("emptyOpenButton", "Open Timeline")
    app.session.activate("fileMenu")
    assert app.session.is_checked("rememberRecentFilesAction")
    app.session.activate("recentFilesMenu")
    app.session.element("recentFileAction0")
    app.session.wait_name_contains("recentFileAction0", "mixed_tracks.edl")


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
    app.session.wait_text_nonempty("filterErrorLabel")
    app.session.wait_sensitive("timelineExportButton", False)

    app.clear_filters()
    app.set_filter_field(0, "Reel")
    app.set_filter_text(0, "BROLL")
    app.wait_event_count(2)
    app.session.wait_sensitive("timelineExportButton", True)
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

    destination = output_directory / "gui-filtered.xlsx"
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

    bundle_path = output_directory / "gui-support.zip"
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


def test_rendered_video_export_embeds_matching_event_frames(
    edit_atlas_application: EditAtlasApplication,
    fixture_directory: Path,
    media_fixture_directory: Path,
    output_directory: Path,
) -> None:
    app = edit_atlas_application
    app.switch_language("English")
    app.open_timeline(
        fixture_directory / "mixed_tracks.edl", frame_rate="24 fps"
    )

    destination = output_directory / "gui-rendered-video.xlsx"
    destination.unlink(missing_ok=True)
    app.begin_spreadsheet_export()
    app.set_export_columns(
        {"Initial frame", "Event"}, ["Initial frame", "Event"]
    )
    app.session.element("renderedVideoGroup")
    app.session.wait_sensitive("continueSpreadsheetExportButton", False)
    app.select_rendered_video(media_fixture_directory / "matching-render.mov")
    app.session.wait_sensitive("continueSpreadsheetExportButton", True)
    app.continue_spreadsheet_export(destination)
    app.session.element("spreadsheetExportResultDialog")
    assert any(
        "decoded 4 unique frame(s)" in text
        for text in app.session.visible_text("spreadsheetExportResultDialog")
    )
    app.finish_spreadsheet_export()

    workbook = XlsxWorkbook(destination)
    assert workbook.event_headers() == ["Initial Frame", "Event"]
    assert workbook.event_row_count() == 4
    assert workbook.event_image_entries() == [
        f"xl/media/image{index}.png" for index in range(1, 5)
    ]
    assert workbook.event_images_are_png()
    assert len(set(workbook.event_image_hashes())) == 4
    assert workbook.event_image_relationship_targets() == (
        workbook.event_image_entries()
    )
    assert workbook.event_image_anchors() == [
        (0, row) for row in range(1, 5)
    ]
    assert workbook.has_event_drawing_relationship()


def test_rendered_video_export_requires_a_video(
    edit_atlas_application: EditAtlasApplication,
    fixture_directory: Path,
) -> None:
    app = edit_atlas_application
    app.switch_language("English")
    app.open_timeline(
        fixture_directory / "mixed_tracks.edl", frame_rate="24 fps"
    )

    app.begin_spreadsheet_export()
    app.set_export_columns(
        {"Initial frame", "Event"}, ["Initial frame", "Event"]
    )
    app.session.element("renderedVideoGroup")
    app.session.wait_sensitive("continueSpreadsheetExportButton", False)
    app.cancel_spreadsheet_options()


@pytest.mark.parametrize(
    "video_name",
    ["missing-timecode.mov", "incompatible-timecode.mov"],
)
def test_rendered_video_export_rejects_unmatched_timecode(
    edit_atlas_application: EditAtlasApplication,
    fixture_directory: Path,
    media_fixture_directory: Path,
    output_directory: Path,
    video_name: str,
) -> None:
    app = edit_atlas_application
    app.switch_language("English")
    app.open_timeline(
        fixture_directory / "mixed_tracks.edl", frame_rate="24 fps"
    )

    destination = output_directory / f"gui-{Path(video_name).stem}.xlsx"
    destination.unlink(missing_ok=True)
    app.begin_spreadsheet_export()
    app.set_export_columns(
        {"Initial frame", "Event"}, ["Initial frame", "Event"]
    )
    app.select_rendered_video(media_fixture_directory / video_name)
    app.continue_spreadsheet_export(destination)
    description = app.finish_rendered_video_export_failure()

    assert any("could not be validated" in text for text in description)
    assert not destination.exists()


def test_rendered_video_export_cancellation_leaves_no_workbook(
    edit_atlas_application: EditAtlasApplication,
    media_fixture_directory: Path,
    output_directory: Path,
) -> None:
    app = edit_atlas_application
    app.switch_language("English")
    app.open_timeline(
        media_fixture_directory / "cancellation.edl", frame_rate="24 fps"
    )

    destination = output_directory / "gui-cancelled-rendered-video.xlsx"
    destination.unlink(missing_ok=True)
    app.begin_spreadsheet_export()
    app.set_export_columns(
        {"Initial frame", "Event"}, ["Initial frame", "Event"]
    )
    app.select_rendered_video(
        media_fixture_directory / "cancellation-render.mov"
    )
    app.continue_spreadsheet_export(destination)
    progress = app.cancel_rendered_video_export()

    assert any(
        "Validating rendered video" in text
        or "Extracting initial frames" in text
        for text in progress
    )
    assert not destination.exists()
    app.session.wait_sensitive("timelineExportButton", True)
