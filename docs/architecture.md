# Architecture

Edit Atlas separates format processing from application workflows and user
interfaces:

```text
Qt desktop frontend ─┬─> application services ─┬─> core
                     │                         └─> built-in formats ─> core
                     └─> diagnostic support
CLI frontend ───────────> application services
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
the frontend explicitly authorizes replacement. Format-specific export options
use stable metadata keys and values. For example, the XLSX language option uses
IETF language tags and is resolved by the frontend before the service call, so
the exporter remains independent from Qt and the active interface language.

The services target has no Qt dependency. Any executable can use document
import, atomic export, and built-in format composition by linking
`EditAtlas::Services`.

Timeline filtering is also an application service. A `TimelineFilterQuery`
combines presentation-independent field conditions and produces ordered source
event indices. Frontends use the same selection for presentation and create a
filtered document copy for export, leaving the imported domain object intact.
Conditions are strongly typed: only free-text fields expose literal or RE2
matching, while track kind, edit type, timecode, and duration conditions compare
domain values exactly. The service prepares each regular expression once per
evaluation and reports invalid expressions without exposing UI types. Case and
Unicode whole-word matching remain explicit text-condition properties so future
frontends and saved filter templates share the same semantics.

## Frontends

Frontends adapt platform-specific values at the application-services boundary.
The Qt desktop frontend separates its shell, presentation, and asynchronous
workflow adapters:

- `MainWindow` composes the desktop UI and handles top-level navigation,
  language changes, and status presentation.
- `ApplicationMenuBar` owns actions, language selection, and recent-file
  settings.
- `DocumentView` presents empty, loading, timeline, and import-failure states.
- `DocumentController` and `SupportBundleController` coordinate localized
  dialogs and translate user actions into workflow requests.
- `DocumentWorkflow` and `SupportBundleWorkflow` schedule UI-independent
  services and report completion through Qt signals.

This keeps widget construction, document presentation, persistent desktop
settings, workflow policy, and background execution out of the top-level
window.

The command-line frontend links `EditAtlas::Services` without Qt. It adapts
UTF-8 command arguments, local paths, stable process exit codes, and terminal
diagnostics to the same synchronous document import and export services used
by the desktop workflow. Format serialization remains owned by the registered
format implementations rather than either frontend.

Dependencies point inward: frontends may depend on application services, which
may depend on core and format targets. Core and format targets never depend on
application services or a frontend.

## Diagnostic support

`EditAtlas::Support` owns bounded persistent logging and privacy-limited
support-bundle generation. It depends on neither Qt nor editorial formats and
accepts only explicit diagnostic metadata and log-directory paths. Frontends
provide platform paths and runtime metadata, disclose bundle contents, and
present localized results.
