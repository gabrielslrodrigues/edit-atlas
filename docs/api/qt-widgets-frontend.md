# Qt Widgets frontend {#qt_widgets_frontend}

Qt Widgets is the maintained secondary Edit Atlas desktop frontend. It remains
independently buildable and tested for critical fixes, shared-presentation
compatibility, regression coverage, and emergency distribution rollback. It is
not included in ordinary production releases.

Shared MVVM responsibilities and the primary/secondary maintenance policy are
canonical in the
[repository architecture](https://github.com/gabrielslrodrigues/edit-atlas/blob/master/docs/architecture.md).
Qt Quick production conventions are documented on @ref qt_quick_frontend.

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

## Widget, controller, and style conventions

`MainWindow` composes the desktop shell. Focused Views present shared state,
while controllers translate widget signals and dialog results into shared
ViewModel commands. Add behavior according to ownership:

- reusable state, validation, and commands belong in Presentation;
- dialogs, confirmations, selection, focus, and Widget signal adaptation
  belong in the Widgets frontend;
- synchronous filesystem and format workflows belong in application services;
- domain and format behavior must remain independent from Qt.

`resources/styles/edit_atlas.qss` owns visual Widget rules and selectors. It
is a template rather than a finished stylesheet: it names shared appearance
tokens such as `@window@` and `@border@`, never literal colors, so light and
dark differ only by the palette substituted into it. Colors are owned by
`edit_atlas::presentation`, which both frontends read, so a new color is added
there and then referenced here. `application_style.cpp` substitutes the
palette, derives the semantic `QPalette` roles from the same table, and owns
non-visual application style behavior including the Fusion base style, Widget
animation settings, and minimum interface font size. An appearance change
reapplies both without restarting. Individual widgets continue to own their
layout, content, and interaction. Do not use QSS to encode state transitions
or application behavior.

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

## Verification and distribution boundary

Widgets packages dynamically deploy Qt Widgets, Qt Concurrent, Qt GUI, Qt
Core, platform plugins, and the shared non-Qt runtime. They receive the same
notices, corresponding-source material, private runtime boundaries, and
clean-installation and packaged-E2E verification as Qt Quick packages.

The permitted maintenance scope and rollback role are defined in the
[repository architecture](https://github.com/gabrielslrodrigues/edit-atlas/blob/master/docs/architecture.md).
The exact emergency release selection procedure is in
[Desktop packaging](https://github.com/gabrielslrodrigues/edit-atlas/blob/master/docs/packaging.md).
