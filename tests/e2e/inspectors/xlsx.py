"""Inspect XLSX ZIP/XML structures without a spreadsheet runtime."""

from __future__ import annotations

from hashlib import sha256
from pathlib import Path
import posixpath
from xml.etree import ElementTree
from zipfile import BadZipFile, ZipFile


MAIN_NAMESPACE = "http://schemas.openxmlformats.org/spreadsheetml/2006/main"
DRAWING_NAMESPACE = (
    "http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing"
)
RELATIONSHIP_NAMESPACE = (
    "http://schemas.openxmlformats.org/package/2006/relationships"
)
DRAWING_RELATIONSHIP_TYPE = (
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/drawing"
)
IMAGE_RELATIONSHIP_TYPE = (
    "http://schemas.openxmlformats.org/officeDocument/2006/relationships/image"
)


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

    def event_headers(self) -> list[str]:
        root = self._xml("xl/worksheets/sheet1.xml")
        row = root.find(f".//{{{MAIN_NAMESPACE}}}row")
        if row is None:
            return []
        shared = self.shared_strings()
        headers: list[str] = []
        for cell in row.findall(f"{{{MAIN_NAMESPACE}}}c"):
            value = cell.find(f"{{{MAIN_NAMESPACE}}}v")
            if value is None or value.text is None:
                inline = cell.find(f"{{{MAIN_NAMESPACE}}}is")
                headers.append(
                    ""
                    if inline is None
                    else "".join(
                        text.text or ""
                        for text in inline.iter(f"{{{MAIN_NAMESPACE}}}t")
                    )
                )
            elif cell.attrib.get("t") == "s":
                headers.append(shared[int(value.text)])
            else:
                headers.append(value.text)
        return headers

    def event_image_entries(self) -> list[str]:
        return sorted(
            name
            for name in self.entry_names()
            if name.startswith("xl/media/image") and name.endswith(".png")
        )

    def event_image_hashes(self) -> list[str]:
        with ZipFile(self.path) as archive:
            return [
                sha256(archive.read(entry)).hexdigest()
                for entry in self.event_image_entries()
            ]

    def event_images_are_png(self) -> bool:
        signature = b"\x89PNG\r\n\x1a\n"
        with ZipFile(self.path) as archive:
            return all(
                archive.read(entry).startswith(signature)
                for entry in self.event_image_entries()
            )

    def event_image_relationship_targets(self) -> list[str]:
        root = self._xml("xl/drawings/_rels/drawing1.xml.rels")
        targets = []
        for relationship in root.findall(
            f"{{{RELATIONSHIP_NAMESPACE}}}Relationship"
        ):
            if relationship.attrib.get("Type") != IMAGE_RELATIONSHIP_TYPE:
                continue
            target = relationship.attrib.get("Target")
            if target is not None:
                targets.append(
                    posixpath.normpath(posixpath.join("xl/drawings", target))
                )
        return sorted(targets)

    def event_image_anchors(self) -> list[tuple[int, int]]:
        root = self._xml("xl/drawings/drawing1.xml")
        anchors = []
        for origin in root.findall(f".//{{{DRAWING_NAMESPACE}}}from"):
            column = origin.find(f"{{{DRAWING_NAMESPACE}}}col")
            row = origin.find(f"{{{DRAWING_NAMESPACE}}}row")
            if column is not None and row is not None:
                anchors.append((int(column.text or "0"), int(row.text or "0")))
        return anchors

    def has_event_drawing_relationship(self) -> bool:
        root = self._xml("xl/worksheets/_rels/sheet1.xml.rels")
        return any(
            relationship.attrib.get("Type") == DRAWING_RELATIONSHIP_TYPE
            and relationship.attrib.get("Target") == "../drawings/drawing1.xml"
            for relationship in root.findall(
                f"{{{RELATIONSHIP_NAMESPACE}}}Relationship"
            )
        )

    def _xml(self, entry: str) -> ElementTree.Element:
        try:
            with ZipFile(self.path) as archive:
                return ElementTree.fromstring(archive.read(entry))
        except (BadZipFile, KeyError, ElementTree.ParseError, OSError) as error:
            raise WorkbookInspectionError(
                f"cannot parse {entry} in {self.path}: {error}"
            ) from error
