# Edit Atlas C++ API {#mainpage}

Edit Atlas provides a UI-independent C++23 foundation for importing editorial
timeline formats and exporting structured reports. The documented API covers
the domain model, format extension points, application services, built-in
formats, media decoding, and privacy-limited diagnostic support.

## Start here

- @ref api_architecture "Architecture and dependency direction"
- `edit_atlas::core::TimelineDocument` is the shared editorial model.
- `edit_atlas::core::Importer` and `edit_atlas::core::Exporter` are the format
  extension points.
- `edit_atlas::core::TimelineEventField` provides stable event-column
  identifiers and the default report projection.
- `edit_atlas::core::RgbImage` provides presentation-neutral RGB24 ownership
  for event images passed through export requests.
- `edit_atlas::services::TimelineDocumentImportService` and
  `edit_atlas::services::TimelineDocumentExportService` provide filesystem workflows
  that do not depend on a frontend.
- `edit_atlas::services::TimelineTemplateService` manages and persists reusable
  filter and export-column templates without depending on a frontend.
- `edit_atlas::services::TimelineVideoInspectionService` validates rendered
  video timing against an imported record timeline.
- `edit_atlas::services::TimelineFrameExtractionService` extracts exact,
  deduplicated initial-frame images at caller-selected output dimensions.
- `edit_atlas::services::TimelineRenderedVideoExportService` coordinates
  validation, filtered frame extraction, and ordinary document export for all
  frontends.
- `edit_atlas::storage::ReadLocalFile()` and
  `edit_atlas::storage::WriteLocalFileAtomically()` provide the shared local
  storage boundary.
- `edit_atlas::services::CreateBuiltInFormatRegistry()` composes the handlers
  shipped with the application.
- `edit_atlas::media::VideoDecoder::Open()` opens supported MOV, MP4, and MXF
  inputs behind a frontend-independent decoding interface.

## API stability

Format identifiers, option keys, and diagnostic codes are stable integration
values. Other APIs currently follow the application version and may evolve
before Edit Atlas reaches version 1.0.

## Source documentation

Public declarations use LLVM-style `///` comments. A declaration's first
sentence is its brief summary. Document parameters with `\param`, return values
with `\returns`, invariants and ownership in the detailed description, and
enumeration values or public data members at their declarations.

Generated documentation is an engineering reference. It does not replace the
end-user application manual or describe private implementation details.
