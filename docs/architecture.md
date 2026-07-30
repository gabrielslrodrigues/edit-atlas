# Architecture

Edit Atlas separates format processing from application workflows and user
interfaces:

```text
Qt desktop frontend ─┐
future CLI frontend ─┼─> application services ─┬─> core
                     │                         └─> built-in formats ─> core
                     └─> diagnostic support
```

## Core

`EditAtlas::Core` owns the format-independent timeline model, handler registry,
and in-memory import and export pipeline. It has no filesystem or user-interface
dependency.

## Formats

Format targets implement importers and exporters against the core interfaces.
They receive byte spans and return domain objects or export artifacts. They do
not open files, display messages, or schedule work.

## Application services

`EditAtlas::Services` is the reusable application boundary. It composes the
built-in format registry and coordinates filesystem operations with the core
pipeline using standard C++ types. `DocumentImportService` imports local
documents. A successful request returns a `DocumentImportReceipt`;
failures preserve their stage, path, filesystem error, and import diagnostics.
Filesystem failures retain their native `std::error_code` instead of
frontend-specific text.

Document exports use the same boundary. `DocumentExportService` selects a
registered exporter, writes its artifact beside the destination, and atomically
commits the completed file. It never replaces an existing destination unless
the frontend explicitly authorizes replacement.

The services target has no Qt dependency. Any executable can use document
import, atomic export, and built-in format composition by linking
`EditAtlas::Services`.

## Frontends

Frontends adapt platform-specific values at the application-services boundary.
The Qt desktop frontend owns dialogs, background scheduling, models, widgets,
translations, recent-file settings, and other desktop integration. A future
CLI can provide terminal input and output while invoking the same services.

Dependencies point inward: frontends may depend on application services, which
may depend on core and format targets. Core and format targets never depend on
application services or a frontend.

## Diagnostic support

`EditAtlas::Support` owns bounded persistent logging and privacy-limited
support-bundle generation. It depends on neither Qt nor editorial formats and
accepts only explicit diagnostic metadata and log-directory paths. Frontends
provide platform paths and runtime metadata, disclose bundle contents, and
present localized results.
