# Changelog

This file records user-facing Edit Atlas changes beginning with version 0.2.0.

## [0.2.0]

### Added

- A command-line frontend for importing CMX 3600 EDL files and exporting XLSX
  workbooks.
- Field-specific timeline filters with literal, case-sensitive, whole-word,
  regular-expression, track-kind, edit-type, timecode, and duration conditions.
- XLSX export of only the currently filtered timeline events.
- Selection and ordering of the event columns included in XLSX workbooks.
- Reusable templates that preserve filter conditions and export-column choices.

### Changed

- The filter interface now scrolls when many conditions are present and scales
  with the application window.
- Filter, export, and template responsibilities are separated from the desktop
  interface so they can be shared by other frontends.

[0.2.0]: https://github.com/gabrielslrodrigues/edit-atlas/compare/v0.1.2...v0.2.0
