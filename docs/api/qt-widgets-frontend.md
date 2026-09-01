# Qt Widgets frontend {#qt_widgets_frontend}

Qt Widgets is the maintained secondary Edit Atlas desktop frontend. It remains
independently buildable and tested for critical fixes, shared-presentation
compatibility, regression coverage, and emergency distribution rollback. It is
not included in ordinary production releases.

Shared MVVM responsibilities and the primary/secondary maintenance policy are
canonical on @ref api_architecture. Qt Quick production conventions are
documented on @ref qt_quick_frontend.

## Architectural boundary

Qt Widgets implements the View and adapts shared Presentation ViewModels to
widgets, actions, native dialogs, and focused controllers. It owns window
composition, interaction, focus, confirmations, and translated feedback. It
must not own format parsing, persistence formats, filesystem transactions,
domain rules, or duplicated service workflows.

The frontend is exposed through `EditAtlas::WidgetsFrontend` and the
`edit_atlas::frontends::widgets` namespace. `EditAtlas::WidgetsApplication`
identifies its concrete executable independently from the product-selected
`EditAtlas::Application` alias.

## Source layout

The implementation lives under `src/frontends/widgets`:

```text
widgets/
├── include/edit_atlas/frontends/widgets/   public adapter declarations
├── resources/styles/                       application QSS
├── main.cpp                                executable composition root
├── main_window.cpp                         desktop shell composition
├── application_menu_bar.cpp                actions and recent-file UI
├── timeline_document_view.cpp              document-state presentation
└── *_controller.cpp                        dialog and command adapters
```

Public declarations stay in the project include hierarchy. Private widgets,
controllers, and implementation headers remain beside their source files.
Shared icons and native desktop metadata stay under `src/frontends/resources`.

## Configure, build, and run

Generic presets build Qt Quick, Qt Widgets, and the CLI. Configure and build a
Linux debug tree with:

```sh
cmake --preset debug-x64-linux
cmake --build --preset debug-x64-linux
```

Run the secondary Widgets application:

```sh
./build/debug-x64-linux/src/frontends/widgets/edit-atlas-widgets
```

Use the corresponding `debug-arm64-osx`, `debug-x64-osx`, or
`debug-x64-windows` preset on another supported host. Focused Release presets
named `release-widgets-<platform>` exclude the Qt Quick implementation from
the default build and produce the Widgets application as `edit-atlas`.

Create a focused local package where a workflow preset exists with:

```sh
cmake --workflow --preset create-widgets-package-x64-linux
```

Use `create-widgets-package-x64-windows` on Windows. Universal macOS package
assembly remains automated in CI from native ARM64 and x64 stages.

## Widget, controller, and style conventions

`MainWindow` composes the desktop shell. Focused Views present shared state,
while controllers translate widget signals and dialog results into shared
ViewModel commands. Add behavior according to ownership:

- reusable state, validation, and commands belong in Presentation;
- dialogs, confirmations, selection, focus, and Widget signal adaptation
  belong in the Widgets frontend;
- synchronous filesystem and format workflows belong in application services;
- domain and format behavior must remain independent from Qt.

`resources/styles/edit_atlas.qss` owns visual Widget rules and selectors.
`application_style.cpp` owns non-visual application style behavior, including
the Fusion base style, Widget animation settings, minimum interface font size,
and semantic `QPalette` roles. Individual widgets continue to own their layout,
content, and interaction. Do not use QSS to encode state transitions or
application behavior.

Object names used by QSS are also part of the maintenance surface. Do not
rename one without reviewing style selectors, accessibility identifiers, and
automation coverage.

## Localization

User-facing QObject and Widget text uses `tr()` in the owning translation
context. English remains the source language and Brazilian Portuguese remains
the initial language for a new profile. Do not persist translated labels or
use them as format keys, template values, or automation identifiers.

Both graphical executables use the same application and organization
identifiers, translation catalog, recent-file history, template directory, and
export preferences. Switching to Widgets for validation or rollback requires
no settings migration.

## Accessibility and automation

Widgets and dialogs use `accessibleIdentifier`. Actions use the same stable
value for `objectName` and their dynamic `accessibleIdentifier` property.
Accessible names and descriptions remain localized; identifiers remain stable
and nonlocalized.

Preserve keyboard navigation, modal cancellation, semantic roles, and focus
restoration when maintaining the secondary frontend. Dynamic filter and
projection controls must continue following the indexed identifier contract in
`docs/accessibility-automation.md` so the frontend-neutral E2E façade can drive
the same user workflow.

## Tests

Run the normal unit and integration layers with:

```sh
ctest --preset debug-x64-linux --label-regex '^unit$'
ctest --preset debug-x64-linux --label-regex '^integration$'
```

The `edit_atlas_widgets_frontend_tests` GoogleTest target covers isolated
frontend behavior. `edit_atlas_widgets_frontend_integration_tests` covers the
real application shell, persistent-state integration, and accessibility
contract. Package verification installs and launches the Widgets artifacts on
every supported platform even though production releases select Qt Quick.

The semantic pytest E2E façade retains Widgets support, and the daily
scheduled CI run exercises it: packaged E2E installs the package of every
frontend that is not the production one — currently Widgets — and drives the
platform suites on Linux and Windows. A failure opens or updates a
tracking issue rather than resting in a workflow run list. A rollback must
select the Widgets package for packaged E2E and pass every required platform
gate before a release tag is created, and the scheduled run is the standing
evidence that the path still works.

## Deployment, licensing, and maintenance

Widgets packages dynamically deploy Qt Widgets, Qt Concurrent, Qt GUI, Qt
Core, platform plugins, and the shared non-Qt runtime. They receive the same
notices, corresponding-source material, private runtime boundaries, and
clean-installation verification as Qt Quick packages.

The permitted maintenance scope and rollback role are defined on
@ref api_architecture rather than independently by this toolkit guide. The
exact emergency release selection procedure is documented in
`docs/packaging.md`.
