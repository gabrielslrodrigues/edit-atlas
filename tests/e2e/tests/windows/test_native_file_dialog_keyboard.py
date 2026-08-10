from __future__ import annotations

from pathlib import Path
from typing import cast

from adapters.windows_uia import WindowsApplicationSession
from application.gui import EditAtlasApplication


def test_focused_native_dialog_accepts_keyboard_path(
    edit_atlas_application: EditAtlasApplication,
    fixture_directory: Path,
) -> None:
    application = edit_atlas_application
    application.switch_language("English")
    session = cast(WindowsApplicationSession, application.session)
    session.activate("fileMenu")
    session.activate("openDocumentAction")

    session.probe_file_dialog_keyboard(
        "timelineOpenFileDialog",
        fixture_directory / "mixed_tracks.edl",
    )
