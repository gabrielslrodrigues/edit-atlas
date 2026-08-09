"""Inspect diagnostic support bundles and their privacy boundary."""

from __future__ import annotations

from pathlib import Path
from zipfile import BadZipFile, ZipFile


class SupportBundleInspectionError(ValueError):
    pass


class SupportBundle:
    def __init__(self, path: Path) -> None:
        self.path = path
        try:
            with ZipFile(path) as archive:
                if "environment.txt" not in archive.namelist():
                    raise SupportBundleInspectionError(
                        "support bundle has no environment summary"
                    )
        except (BadZipFile, OSError) as error:
            raise SupportBundleInspectionError(
                f"cannot inspect {path}: {error}"
            ) from error

    def entry_names(self) -> set[str]:
        with ZipFile(self.path) as archive:
            return set(archive.namelist())

    def environment_summary(self) -> str:
        with ZipFile(self.path) as archive:
            return archive.read("environment.txt").decode("utf-8")

    def assert_private(self, forbidden_names: set[str] | None = None) -> None:
        names = self.entry_names()
        permitted = {"environment.txt"}
        permitted.update(name for name in names if name.startswith("logs/"))
        unexpected = names - permitted
        if unexpected:
            raise SupportBundleInspectionError(
                f"unexpected support-bundle entries: {sorted(unexpected)!r}"
            )
        forbidden_suffixes = {".edl", ".xlsx", ".env"}
        exposed = {
            name for name in names if Path(name).suffix.lower() in forbidden_suffixes
        }
        if forbidden_names:
            exposed.update(name for name in names if Path(name).name in forbidden_names)
        if exposed:
            raise SupportBundleInspectionError(
                f"private input appeared in support bundle: {sorted(exposed)!r}"
            )
