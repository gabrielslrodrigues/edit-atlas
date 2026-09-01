"""Semantic operations for the installed Edit Atlas CLI."""

from __future__ import annotations

from itertools import count
import os
from pathlib import Path
import shlex
from typing import Iterable

from adapters.processes import CommandResult, ProcessRegistry


class InstalledCli:
    def __init__(
        self,
        executable: Path,
        processes: ProcessRegistry,
        state_root: Path,
        artifact_directory: Path,
        timeout: float,
        locale: str,
    ) -> None:
        self._executable = executable
        self._processes = processes
        self._state_root = state_root
        self._artifact_directory = artifact_directory
        self._timeout = timeout
        self._locale = locale
        self._invocations = count(1)

    @property
    def executable(self) -> Path:
        return self._executable

    def installed_license_directory(self) -> Path | None:
        """Locates the licensing material installed beside the application.

        The layout differs per platform: a macOS bundle keeps it under
        `Contents/Resources`, while Linux and Windows follow the data
        directory. Returns None when no candidate exists, so a test can
        report a missing directory rather than an attribute error.
        """
        prefix = self._executable.parent.parent
        candidates = (
            prefix / "share" / "licenses" / "edit-atlas",
            prefix / "Resources" / "licenses",
            self._executable.parent / "licenses",
        )
        for candidate in candidates:
            if candidate.is_dir():
                return candidate
        return None

    def invoke(self, arguments: Iterable[str | os.PathLike[str]]) -> CommandResult:
        command = (self._executable, *arguments)
        environment = os.environ.copy()
        environment["EDIT_ATLAS_TEST_STATE_ROOT"] = os.fspath(self._state_root)
        environment["LANG"] = self._locale
        environment["LC_ALL"] = self._locale
        result = self._processes.run(
            command,
            timeout=self._timeout,
            environment=environment,
        )
        self._record(result)
        return result

    def convert(
        self,
        source: Path,
        destination: Path,
        *options: str,
    ) -> CommandResult:
        return self.invoke(("convert", *options, source, destination))

    def _record(self, result: CommandResult) -> None:
        self._artifact_directory.mkdir(parents=True, exist_ok=True)
        index = next(self._invocations)
        command = shlex.join(result.arguments)
        content = (
            f"command: {command}\n"
            f"exit-code: {result.exit_code}\n"
            "\n[stdout]\n"
            f"{result.standard_output}"
            "\n[stderr]\n"
            f"{result.standard_error}"
        )
        (self._artifact_directory / f"cli-{index:02d}.txt").write_text(
            content, encoding="utf-8"
        )
