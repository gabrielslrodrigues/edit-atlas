# Changelog

This file records user-facing Edit Atlas changes beginning with version 0.2.0.

## [Unreleased]

### Added

- A System, Light, and Dark appearance preference shared by both desktop
  frontends, defaulting to following the operating system and applied without
  restarting the application.

### Changed

- The production desktop application now uses the redesigned Qt Quick
  interface while preserving existing language, recent-file, template, and
  export settings.

## [0.3.0]

### Added

- A formatted event-duration column using `HH:MM:SS:FF`, alongside the existing
  duration-in-frames column.
- Optional rendered-video input for spreadsheet exports from MOV, MP4, and MXF
  files with compatible embedded timecode metadata.
- An Initial Frame event column that embeds the frame at each event's record-in
  timecode as a PNG thumbnail in the exported XLSX workbook.
- Validation that the rendered video's frame rate, starting timecode, and
  coverage match the imported timeline before frame extraction begins.
- Cancellable rendered-video frame extraction with partial-workbook cleanup and
  actionable diagnostics for missing or incompatible timecode metadata.

### Changed

- Packaged distributions now dynamically deploy the required FFmpeg libraries
  and include their license notices, corresponding source, and replacement
  instructions.
- Accessibility metadata and desktop automation coverage now include the
  rendered-video export workflow on required Linux and Windows runners.
- Native integration and packaged end-to-end tests now validate application,
  command-line, installer, accessibility, and media workflows separately from
  unit tests.

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

[Unreleased]: https://github.com/gabrielslrodrigues/edit-atlas/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/gabrielslrodrigues/edit-atlas/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/gabrielslrodrigues/edit-atlas/compare/v0.1.2...v0.2.0
