"""Inspect XLSX ZIP/XML structures without a spreadsheet runtime."""

from __future__ import annotations

from pathlib import Path
from xml.etree import ElementTree
from zipfile import BadZipFile, ZipFile


MAIN_NAMESPACE = "http://schemas.openxmlformats.org/spreadsheetml/2006/main"


class WorkbookInspectionError(ValueError):
    pass


class XlsxWorkbook:
    def __init__(self, path: Path) -> None:
        self.path = path
        try:
            with ZipFile(path) as archive:
                names = set(archive.namelist())
                required = {"[Content_Types].xml", "xl/workbook.xml"}
                if not required.issubset(names):
                    raise WorkbookInspectionError(
                        f"{path} is missing required XLSX entries"
                    )
        except (BadZipFile, OSError) as error:
            raise WorkbookInspectionError(f"cannot inspect {path}: {error}") from error

    def entry_names(self) -> set[str]:
        with ZipFile(self.path) as archive:
            return set(archive.namelist())

    def sheet_names(self) -> list[str]:
        root = self._xml("xl/workbook.xml")
        return [
            sheet.attrib["name"]
            for sheet in root.findall(f".//{{{MAIN_NAMESPACE}}}sheet")
        ]

    def shared_strings(self) -> list[str]:
        if "xl/sharedStrings.xml" not in self.entry_names():
            return []
        root = self._xml("xl/sharedStrings.xml")
        return [
            "".join(text.text or "" for text in value.iter(f"{{{MAIN_NAMESPACE}}}t"))
            for value in root.findall(f"{{{MAIN_NAMESPACE}}}si")
        ]

    def event_row_count(self) -> int:
        root = self._xml("xl/worksheets/sheet1.xml")
        rows = root.findall(f".//{{{MAIN_NAMESPACE}}}row")
        return max(0, len(rows) - 1)

    def _xml(self, entry: str) -> ElementTree.Element:
        try:
            with ZipFile(self.path) as archive:
                return ElementTree.fromstring(archive.read(entry))
        except (BadZipFile, KeyError, ElementTree.ParseError, OSError) as error:
            raise WorkbookInspectionError(
                f"cannot parse {entry} in {self.path}: {error}"
            ) from error
