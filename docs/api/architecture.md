# API architecture {#api_architecture}

The public API follows one inward dependency direction:

```text
frontend -> application services -> core
                              |----> built-in formats -> core
frontend -> diagnostic support
```

## Core model and pipeline

The `edit_atlas::core` namespace contains exact timecode types, the editorial
timeline model, format interfaces, `edit_atlas::core::FormatRegistry`, and the
in-memory `edit_atlas::core::DocumentPipeline`. Core code does not open files,
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
