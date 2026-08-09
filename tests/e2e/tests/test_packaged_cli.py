from __future__ import annotations

from pathlib import Path

from application.cli import InstalledCli
from inspectors.xlsx import XlsxWorkbook


def test_installed_cli_reports_its_version(installed_cli: InstalledCli) -> None:
    result = installed_cli.invoke(("--version",))

    assert result.exit_code == 0
    assert result.standard_output.startswith("Edit Atlas ")
    assert result.standard_error == ""


def test_installed_cli_converts_and_inspects_workbook(
    installed_cli: InstalledCli,
    fixture_directory: Path,
    output_directory: Path,
) -> None:
    destination = output_directory / "packaged-cli.xlsx"
    destination.unlink(missing_ok=True)

    result = installed_cli.convert(
        fixture_directory / "mixed_tracks.edl",
        destination,
        "--fps=24",
        "--language=en",
        "--no-timeline-sheet",
        "--no-diagnostics-sheet",
        "--columns=comments,event,duration-frames",
    )

    assert result.exit_code == 0
    assert "Converted 4 event(s)" in result.standard_output
    assert result.standard_error == ""
    workbook = XlsxWorkbook(destination)
    assert workbook.sheet_names() == ["Events"]
    assert workbook.event_row_count() == 4
    strings = workbook.shared_strings()
    assert "Comments" in strings
    assert "Event" in strings
    assert "Duration Frames" in strings
    assert "Reel" not in strings


def test_installed_cli_reports_warnings_and_writes_output(
    installed_cli: InstalledCli, output_directory: Path
) -> None:
    source = output_directory / "warning.edl"
    source.write_text(
        "TITLE: WARNING\nFCM: NON-DROP FRAME\nUNRECOGNIZED CONTENT\n",
        encoding="utf-8",
    )
    destination = output_directory / "warning.xlsx"
    destination.unlink(missing_ok=True)

    result = installed_cli.convert(source, destination, "--fps=24")

    assert result.exit_code == 1
    assert "cmx3600.unknown_content" in result.standard_error
    assert destination.is_file()
    assert XlsxWorkbook(destination).event_row_count() == 0


def test_installed_cli_preserves_unicode_paths(
    installed_cli: InstalledCli, output_directory: Path
) -> None:
    source = output_directory / "edição.edl"
    source.write_text(
        "TITLE: EDIÇÃO\nFCM: NON-DROP FRAME\n",
        encoding="utf-8",
    )
    destination = output_directory / "relatório.xlsx"
    destination.unlink(missing_ok=True)

    result = installed_cli.convert(source, destination, "--fps=24")

    assert result.exit_code == 0
    assert str(source) in result.standard_output
    assert str(destination) in result.standard_output
    assert XlsxWorkbook(destination).event_row_count() == 0


def test_installed_cli_preserves_then_replaces_existing_output(
    installed_cli: InstalledCli,
    fixture_directory: Path,
    output_directory: Path,
) -> None:
    destination = output_directory / "existing.xlsx"
    destination.write_bytes(b"original")
    source = fixture_directory / "mixed_tracks.edl"

    refused = installed_cli.convert(source, destination, "--fps=24")
    assert refused.exit_code == 4
    assert "cli.output.destination_exists" in refused.standard_error
    assert destination.read_bytes() == b"original"

    replaced = installed_cli.convert(source, destination, "--fps=24", "--force")
    assert replaced.exit_code == 0
    assert XlsxWorkbook(destination).event_row_count() == 4


def test_installed_cli_reports_input_and_usage_errors(
    installed_cli: InstalledCli,
    fixture_directory: Path,
    output_directory: Path,
) -> None:
    destination = output_directory / "invalid.xlsx"
    destination.unlink(missing_ok=True)

    invalid = installed_cli.convert(
        fixture_directory / "mixed_tracks.edl", destination
    )
    assert invalid.exit_code == 3
    assert "cmx3600.missing_frame_rate" in invalid.standard_error
    assert not destination.exists()

    usage = installed_cli.invoke(
        ("convert", "--language=es", "input.edl", destination)
    )
    assert usage.exit_code == 2
    assert "--language" in usage.standard_error

    missing = output_directory / "missing.edl"
    missing.unlink(missing_ok=True)
    operational = installed_cli.convert(missing, destination)
    assert operational.exit_code == 5
    assert "cli.input.open_failed" in operational.standard_error
    assert not destination.exists()
