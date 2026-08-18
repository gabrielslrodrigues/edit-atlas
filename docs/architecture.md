# Architecture

Edit Atlas separates format processing, presentation state, and concrete user
interfaces:

```text
Qt Widgets / Qt Quick ─> presentation ─┬─> application services ─┬─> core
                                      │                         ├─> formats ─> core
                                      │                         └─> storage
                                      └─> diagnostic support ─────> storage
CLI ──────────────────────────────────> application services
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
pipeline using standard C++ types. `TimelineDocumentImportService` imports local
documents. A successful request returns a `TimelineDocumentImportReceipt`;
failures preserve their stage, path, filesystem error, and import diagnostics.
Filesystem failures retain their native `std::error_code` instead of
frontend-specific text.

Timeline exports use the same boundary. `TimelineDocumentExportService` selects a
registered exporter, writes its artifact beside the destination, and atomically
commits the completed file. It never replaces an existing destination unless
the frontend explicitly authorizes replacement. Format-specific export options
use stable metadata keys and values. For example, the XLSX language option uses
IETF language tags and is resolved by the frontend before the service call, so
the exporter remains independent from Qt and the active interface language.
Event-column selection crosses the same boundary as an ordered
`TimelineEventField` projection. Fields have stable, language-independent
identifiers; frontends localize their labels, services validate the projection,
and tabular exporters render only the selected fields in the requested order.

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

`TimelineTemplate` combines a filter query with an ordered event projection
using stable, nonlocalized identifiers. `TimelineTemplateService` owns the
catalog and provides frontend-independent create, update, rename, duplicate,
and remove operations. Its private JSON store persists each template as an
independent schema-versioned file. A bad or newer file is reported and skipped
without preventing valid templates from loading. The shared template ViewModel
owns active selection and dirty-state comparison. Concrete Views own prompts,
confirmations, and localization; non-Qt frontends can use the service directly.

`EditAtlas::Storage` centralizes complete binary reads and atomic local-file
writes. Timeline import/export, template persistence, and diagnostic bundle
creation share this boundary rather than implementing platform-specific commit
logic separately.

## Presentation

`EditAtlas::Presentation` is the shared Qt-facing MVVM boundary. Its public
contract is based on Qt Core and depends on neither Qt Widgets nor Qt Quick. Its
ViewModels expose state, commands, structured results, item models, and change
signals without creating windows or dialogs:

- `TimelineDocumentViewModel` owns one imported timeline, filtering, event
  projection, import/export state, and asynchronous results.
- `TimelineTemplateViewModel` owns the persisted template catalog, active
  selection, editable filter/projection state, and modification detection.
- `SupportBundleViewModel` owns asynchronous diagnostic-bundle export state and
  its structured receipt or failure.

Presentation workflows schedule synchronous services with Qt Concurrent. The
same target also owns application paths and settings, translation loading,
localized diagnostic text, the timeline item model, and desktop integration
shared by concrete graphical Views.

## Frontends

Frontends adapt platform-specific values at the application-services boundary.
Concrete implementations live under `src/frontends`, with matching unit and
integration tests under each test suite's `frontends` directory. Their
namespaces mirror that hierarchy.

The Qt desktop frontends follow Model-View-ViewModel: core and services are the
Model, `EditAtlas::Presentation` supplies the ViewModels, and Qt Widgets or Qt
Quick implements the View. Concrete Views own dialogs, confirmations,
navigation, translated feedback, and toolkit-specific interaction.

The existing Qt Widgets frontend is exposed as `EditAtlas::WidgetsFrontend` in
the `edit_atlas::frontends::widgets` namespace. It separates its shell and
focused adapters:

- `MainWindow` composes the desktop UI and handles top-level navigation,
  language changes, and status presentation.
- `ApplicationMenuBar` owns actions, language selection, and recent-file
  settings.
- `TimelineDocumentView` presents empty, loading, timeline, and import-failure
  states.
- Focused controllers coordinate localized dialogs and translate Widget signals
  into shared ViewModel commands.

This keeps Widget construction and interaction policy out of shared
presentation state, while keeping application services independent from both
graphical toolkits.

Concrete application targets identify their frontend explicitly:
`EditAtlas::WidgetsApplication`, `EditAtlas::CliApplication`, and
`EditAtlas::QuickApplication`. `EditAtlas::Application` aliases the current
default desktop frontend selected by `EDIT_ATLAS_DEFAULT_FRONTEND`, which is
currently Qt Quick. Generic development presets build both graphical frontends,
while frontend-specific presets use `EDIT_ATLAS_BUILD_FRONTENDS` to build only
their named implementation. The CLI remains independently available. The
selected target receives the product name, platform metadata, installation
rules, and Qt deployment. Its output keeps the product name `edit-atlas`.
Shared product icons and desktop metadata live under `src/frontends/resources`;
frontend-specific styling remains with its concrete frontend.

Release configurations additionally define
`EditAtlas::WidgetsPackageApplication` and
`EditAtlas::QuickPackageApplication`. They reuse the same compiled frontend
libraries but link into isolated product-named outputs. Separate
`WidgetsRuntime` and `QuickRuntime` install components own their application
and Qt deployment rules, while the shared `Runtime` component supplies the
CLI, licensing material, desktop metadata, and common runtime libraries.
This allows one generic Release build to produce either frontend package
without changing the default developer application or recompiling shared
code. Production packaging selects `QuickRuntime`; `WidgetsRuntime` remains
available for clean-machine verification and emergency rollback packages.
Both executables retain the same application and organization identifiers and
consume the same presentation-owned settings and application-data paths, so
selecting either frontend does not create or migrate frontend-specific user
state.

The primary Qt Quick frontend compiles its application QML into
`EditAtlas::QuickFrontend`. `EditAtlas::QuickStyle` provides the separate
`EditAtlasStyle` QML module: shared design tokens, light and dark theme colors,
surfaces, icons, and template-based Qt Quick Controls. The custom style uses
Basic as its fallback, so controls can be introduced incrementally without
depending on platform-native styles.

The command-line frontend is exposed as `EditAtlas::CliFrontend` in the
`edit_atlas::frontends::cli` namespace. It links `EditAtlas::Services` without
Qt and adapts UTF-8 command arguments, local paths, stable process exit codes,
and terminal diagnostics to the same synchronous document import and export
services used by the desktop workflow. Format serialization remains owned by
the registered format implementations rather than either frontend.

Dependencies point inward: graphical frontends may depend on Presentation,
which may depend on application services, support, and core. The CLI depends on
application services directly. Core, formats, services, storage, and support
never depend on Presentation or a frontend.

## Diagnostic support

`EditAtlas::Support` owns bounded persistent logging and privacy-limited
support-bundle generation. It depends on neither Qt nor editorial formats and
accepts only explicit diagnostic metadata and log-directory paths. Frontends
provide platform paths and runtime metadata, disclose bundle contents, and
present localized results.
