# Edit Atlas C++ API {#mainpage}

Edit Atlas provides a UI-independent C++23 foundation for importing editorial
timeline formats and exporting structured reports. The documented API covers
the domain model, format extension points, application services, built-in
formats, and privacy-limited diagnostic support.

## Start here

- @ref api_architecture "Architecture and dependency direction"
- `edit_atlas::core::TimelineDocument` is the shared editorial model.
- `edit_atlas::core::Importer` and `edit_atlas::core::Exporter` are the format
  extension points.
- `edit_atlas::services::DocumentImportService` and
  `edit_atlas::services::DocumentExportService` provide filesystem workflows
  that do not depend on a frontend.
- `edit_atlas::services::CreateBuiltInFormatRegistry()` composes the handlers
  shipped with the application.

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
