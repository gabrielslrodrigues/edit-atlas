# API architecture {#api_architecture}

The public API follows one inward dependency direction:

```text
frontend -> application services -> core
                              |----> built-in formats -> core
                              |----> media decoding backend -> FFmpeg
frontend -> diagnostic support -> local storage
```

## Core model and pipeline

The `edit_atlas::core` namespace contains exact timecode types, the editorial
timeline model, format interfaces, `edit_atlas::core::FormatRegistry`, and the
in-memory `edit_atlas::core::TimelineDocumentPipeline`. Core code does not open files,
schedule work, or depend on a user-interface framework.

## Format extensions

An importer implements `edit_atlas::core::Importer`; an exporter implements
`edit_atlas::core::Exporter`. Handlers exchange byte spans, domain objects,
export artifacts, and structured diagnostics. Built-in implementations live in
the `edit_atlas::formats` namespace and obey the same interfaces available to
future formats.

## Application services

The `edit_atlas::services` namespace adapts local filesystem operations to the
in-memory pipeline. Services return presentation-neutral receipts or failures,
leaving scheduling, overwrite confirmation, localization, and user feedback to
the caller. The desktop frontend and CLI therefore share the same
workflows. Timeline filter queries and event selections also live here so a
frontend can present and export the same result set without modifying its
imported document. Free-text conditions support literal or RE2 matching with
optional case and Unicode whole-word constraints. Categorical, timecode, and
duration conditions carry typed exact values, and invalid expressions produce
structured validation failures.

Ordered event projections use `edit_atlas::core::TimelineEventField`. Stable
identifiers are independent from translated labels, and the default projection
reproduces the complete event report. `TimelineDocumentExportService` rejects empty or
duplicate projections before invoking an exporter or touching its destination.
Named `edit_atlas::services::TimelineTemplate` values combine these filter and
projection types. `edit_atlas::services::TimelineTemplateService` owns their
catalog and persistence-neutral CRUD behavior. Its private store writes
independent schema-versioned local files and reports unsupported entries
without discarding valid ones.

`edit_atlas::services::TimelineVideoInspectionService` validates a rendered
video's embedded starting timecode, constant frame rate, and duration against
the imported record timeline. A one-frame duration difference is tolerated;
larger differences reject the video rather than guessing an alignment.
Successful inspection retains the opened decoder and an exact frame mapping
for later extraction without coupling a frontend to the media backend.
`edit_atlas::services::TimelineFrameExtractionService` consumes that validated
decoder, maps each event's Record In to an exact video frame, and produces
owned RGB frame images at caller-selected output dimensions. Duplicate frame
mappings share immutable image ownership, while cancellation and failures
return no partial collection. Thumbnail dimensions, encoding, and placement
remain policies of the consuming exporter.

Export requests may associate immutable RGB images with event indices. The
XLSX exporter exposes `Initial Frame` as an optional projection field, encodes
the corresponding images as PNG, and owns their workbook column width, row
height, placement, and localized header. Projections that omit the field do not
alter existing workbooks.

`TimelineRenderedVideoExportService` is the shared frontend orchestration for
image-bearing exports. It validates video timing against the complete imported
timeline, extracts only the possibly filtered export events, reindexes their
images against the export document, and then delegates to the ordinary document
export service. This keeps desktop and CLI validation behavior identical while
preserving correct frame alignment after filtering.

The `edit_atlas::storage` namespace provides shared complete-file reads and
atomic local-file writes for services and diagnostic support.

## Media decoding

The `edit_atlas::media` namespace owns the presentation-neutral video boundary.
Its public metadata, failure, decoder, and RGB24 frame types do not expose
FFmpeg declarations. The private FFmpeg implementation opens MOV, MP4, and MXF
containers, applies the documented codec policy, and returns structured errors
to its caller. Frame-index seeking and timestamp conversion stay inside this
boundary. The caller may request bounded output dimensions, which the backend
applies during RGB conversion without assigning presentation semantics to the
result. Timeline validation, event mapping, cancellation, and progress belong
to application services; thumbnail policy, user interaction, and spreadsheet
embedding remain responsibilities of frontends and format adapters.

## Diagnostic support

The `edit_atlas::support` namespace owns bounded application logging and
privacy-limited support bundles. Callers provide explicit paths and diagnostic
metadata; support code does not inspect user documents or arbitrary environment
state.

## Frontend boundary

Qt desktop classes are adapters rather than public application services. The
`edit_atlas::cli` namespace provides the corresponding terminal adapter and
stable process outcomes. Callers should integrate reusable workflows through
the standard C++ domain, format, service, and support interfaces.
