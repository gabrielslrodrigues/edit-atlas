# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Edit Atlas is a privacy-first desktop application for inspecting editorial
timeline interchange files (CMX 3600 EDL) and exporting structured XLSX
reports. Two Qt desktop frontends (Qt Quick primary, Qt Widgets secondary) and
a Qt-free CLI share the same C++23 application core. No telemetry or network
behavior; conversion and diagnostics are local filesystem operations only.

## Build, test, run

Bootstrap once after cloning (vcpkg is a submodule):

```sh
git submodule update --init --recursive
./vcpkg/bootstrap-vcpkg.sh -disableMetrics   # .bat on Windows
```

Configure and build with a platform preset (`debug-x64-linux`,
`debug-arm64-osx`, `debug-x64-osx`, `debug-universal-osx`,
`debug-x64-windows`, and `release-*` equivalents). vcpkg installs
`vcpkg.json` dependencies as part of configure:

```sh
cmake --preset debug-x64-linux
cmake --build --preset debug-x64-linux
```

Run the apps from a build tree:

```sh
./build/debug-x64-linux/src/frontends/quick/edit-atlas            # primary GUI
./build/debug-x64-linux/src/frontends/widgets/edit-atlas-widgets   # secondary GUI
./build/debug-x64-linux/src/frontends/cli/edit-atlas-cli convert --fps 24 timeline.edl report.xlsx
```

Run tests (GoogleTest via CTest; every test has a `unit` or `integration`
label):

```sh
ctest --preset debug-x64-linux                              # both layers
ctest --preset debug-x64-linux --label-regex '^unit$'
ctest --preset debug-x64-linux --label-regex '^integration$'
ctest --preset debug-x64-linux --tests-regex '^edit_atlas_quick_qml_tests$'  # Qt Quick Test only
```

Lint QML (must be run from a configured build tree):

```sh
cmake --build --preset debug-x64-linux --target all_qmllint
```

Other useful configure flags:

```sh
cmake --preset debug-x64-linux -DBUILD_TESTING=OFF
cmake --preset debug-x64-linux -DEDIT_ATLAS_WARNINGS_AS_ERRORS=ON
```

`EDIT_ATLAS_DEFAULT_FRONTEND` (`quick`|`widgets`) selects the frontend aliased
by `EditAtlas::Application`; `EDIT_ATLAS_BUILD_FRONTENDS` (`all`|`quick`|`widgets`)
controls which graphical frontend(s) a build includes. Generic `debug-`/
`release-` presets build both graphical frontends plus the CLI;
`release-widgets-*`/`release-quick-*` presets build only the named frontend.

E2E tests (pytest, black-box, drive installed packages via accessibility
APIs) are intentionally not registered with CTest — see
`tests/e2e/README.md` and `docs/accessibility-automation.md` before touching
them; they require building fixtures and installing a package first.

Style: LLVM-based `.clang-format` (4-space indent, 80 cols, no tabs). No
separate lint command beyond `all_qmllint` and compiler warnings
(`EDIT_ATLAS_WARNINGS_AS_ERRORS`).

## Architecture

Strict inward dependency direction — nothing inward ever depends on
presentation or a frontend:

```text
Qt Widgets / Qt Quick ─> presentation ─┬─> application services ─┬─> core
                                       │                          ├─> formats ─> core
                                       │                          └─> storage
                                       └─> diagnostic support ─────> storage
CLI ───────────────────────────────────> application services
```

- **`src/core`** (`edit_atlas::core`) — format-independent timeline model,
  `Importer`/`Exporter` interfaces, `FormatRegistry`, in-memory
  `TimelineDocumentPipeline`. No filesystem or UI dependency.
- **`src/formats`** — built-in importer/exporter implementations
  (`edit_atlas::formats`, e.g. `cmx3600`, `xlsx`) against the core
  interfaces. They exchange byte spans/domain objects only; they never open
  files or display messages.
- **`src/services`** (`EditAtlas::Services`, `edit_atlas::services`) — the
  reusable, Qt-free application boundary. Composes the format registry with
  local filesystem operations: `TimelineDocumentImportService`,
  `TimelineDocumentExportService` (atomic commit, never overwrites without
  explicit authorization), `TimelineFilterQuery`/filtering, ordered
  `TimelineEventField` projections, `TimelineTemplateService` (CRUD +
  schema-versioned JSON persistence), and the rendered-video pipeline
  (`TimelineVideoInspectionService` → `TimelineFrameExtractionService` →
  `TimelineRenderedVideoExportService`) for embedding initial-frame images in
  XLSX exports. Any executable can link `EditAtlas::Services` directly (the
  CLI does).
- **`src/storage`** (`EditAtlas::Storage`) — shared complete-file reads and
  atomic local-file writes, used by services and diagnostic support.
- **`src/media`** (`edit_atlas::media`) — presentation-neutral video
  decoding boundary; private FFmpeg implementation is hidden behind public
  metadata/decoder/RGB24-frame types.
- **`src/support`** (`EditAtlas::Support`, `edit_atlas::support`) — bounded
  persistent logging and privacy-limited support-bundle generation. No Qt,
  no editorial-format knowledge.
- **`src/presentation`** (`EditAtlas::Presentation`, `edit_atlas::presentation`)
  — shared Qt-facing MVVM boundary (Qt Core only, no Widgets/Quick).
  `TimelineDocumentViewModel`, `TimelineTemplateViewModel`,
  `SupportBundleViewModel` expose state/commands/signals and schedule
  synchronous services via Qt Concurrent. Also owns application
  paths/settings, translation loading, localized diagnostic text, and the
  timeline item model shared by both graphical frontends.
- **`src/frontends`** — concrete Views, one directory per frontend, mirrored
  by namespace and by `tests/{unit,integration}/frontends/<name>`:
  - `quick/` — **primary, production** frontend
    (`EditAtlas::QuickFrontend`/`edit_atlas::frontends::quick`). QML views
    under `quick/views`; the separate `EditAtlasStyle` QML module
    (`quick/style`) supplies design tokens, theme, and Qt Quick Controls
    built on the Basic fallback style. See `docs/api/qt-quick-frontend.md`.
  - `widgets/` — **maintained secondary** frontend
    (`EditAtlas::WidgetsFrontend`/`edit_atlas::frontends::widgets`), scoped
    to critical fixes, Presentation-compatibility changes, regression
    coverage, and emergency rollback — not ordinary new features or visual
    parity. See `docs/api/qt-widgets-frontend.md`.
  - `cli/` — `EditAtlas::CliFrontend`/`edit_atlas::frontends::cli`, Qt-free,
    calls application services directly, stable process exit codes (see
    `docs/cli.md`).
  - `resources/` — product icons/desktop metadata shared by both graphical
    frontends.

  `EditAtlas::Application` aliases whichever graphical frontend
  `EDIT_ATLAS_DEFAULT_FRONTEND` selects (currently `quick`). Both graphical
  executables share the same application/organization identifiers and
  presentation-owned settings, so switching frontends needs no data
  migration. Release builds additionally produce isolated
  `WidgetsPackageApplication`/`QuickPackageApplication` targets that reuse
  the same compiled libraries under separate product-named install
  components.

Full architectural detail and the frontend maintenance policy are canonical
in `docs/architecture.md` and `docs/api/architecture.md` — read those before
proposing structural changes.

## Cross-cutting conventions worth knowing before editing

- **Accessibility identifiers are a stable automation contract.**
  Interactive/semantically meaningful controls expose a nonlocalized
  `objectName`/`Accessible.id` (Quick) or `accessibleIdentifier` (Widgets).
  Never derive an identifier from translated text or rename one casually —
  it's load-bearing for `tests/e2e` and the indexed dynamic-row conventions
  (`filterConditionN...`, `eventColumnN...`) documented in
  `docs/accessibility-automation.md`. Both frontends must expose the same
  semantic user workflow even when their concrete control structure differs.
- **Localization**: English is the source language; Brazilian Portuguese
  (`src/presentation/translations/edit_atlas_pt_BR.ts`) is the default for a
  new profile. QML uses `qsTr()`, Widgets/QObject uses `tr()`. Never persist
  translated text or use it as a format key, template value, or automation
  identifier.
- **Format/projection fields use stable, language-independent identifiers**
  (`TimelineEventField`); frontends localize labels, services validate
  projections against those stable IDs.
- **Filter conditions are strongly typed** — only free-text fields support
  literal/RE2 matching (with case/whole-word options); track kind, edit
  type, timecode, and duration conditions compare exact typed values.
- Native integration tests must set `EDIT_ATLAS_TEST_STATE_ROOT` before
  constructing application services (redirects QSettings, recent files,
  templates, logs) — never point it at a real developer profile.
- Public C++ API docs use LLVM-style `///` comments (first sentence = brief
  summary); generate the full reference with
  `cmake --workflow --preset documentation` (requires Doxygen 1.9.8+ and
  Graphviz, not needed for normal builds).
