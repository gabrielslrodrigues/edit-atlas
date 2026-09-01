# Qt Quick frontend {#qt_quick_frontend}

Qt Quick is the primary Edit Atlas desktop frontend and the only graphical
frontend published in production releases.

This page defines the Qt Quick source conventions and development workflow.
Shared MVVM responsibilities and the primary/secondary maintenance policy are
canonical on @ref api_architecture. Qt Widgets conventions are documented on
@ref qt_widgets_frontend.

## Architectural boundary

Qt Quick implements the View and supplies QML-facing adapters around shared
Presentation ViewModels. It owns declarative layout, dialogs, focus,
navigation, translated feedback, and toolkit-specific interaction. It must not
own format parsing, persistence formats, filesystem transactions, domain
rules, or duplicated service workflows.

## Source layout

The relevant source directories are:

```text
src/
├── presentation/              shared Qt Core ViewModels and presentation state
└── frontends/
    ├── quick/
    │   ├── include/           public C++ Qt Quick adapter declarations
    │   ├── style/             EditAtlasStyle controls, theme, and tokens
    │   └── views/             application QML views and dialogs
    └── widgets/               maintained secondary desktop frontend
```

The `EditAtlas.Frontends.Quick` QML module contains application views and
QML-facing C++ types. `EditAtlasStyle` is a separate Qt Quick Controls style
module built on the Basic fallback style. Shared product icons and native
desktop metadata live under `src/frontends/resources` rather than either
frontend.

## Configure, build, and run

Generic presets build Qt Quick, Qt Widgets, and the CLI. Configure and build a
Linux debug tree with:

```sh
cmake --preset debug-x64-linux
cmake --build --preset debug-x64-linux
```

Run the primary Qt Quick application:

```sh
./build/debug-x64-linux/src/frontends/quick/edit-atlas
```

Run the secondary Widgets application from the same build:

```sh
./build/debug-x64-linux/src/frontends/widgets/edit-atlas-widgets
```

Use the corresponding `debug-arm64-osx`, `debug-x64-osx`, or
`debug-x64-windows` preset on another supported host. The product-named Qt
Quick executable is located under `src/frontends/quick`; the secondary
Widgets executable is located under `src/frontends/widgets` and retains the
`edit-atlas-widgets` name.

Release presets named `release-quick-<platform>` and
`release-widgets-<platform>` provide focused frontend builds and package
validation. Generic Release presets build both isolated package applications
from one shared Release tree.

## QML and design-system conventions

Application QML belongs in `src/frontends/quick/views`. Reusable visual
controls and styling belong in `src/frontends/quick/style`; they must not be
copied into individual views.

Follow these rules when changing the Qt Quick interface:

- import `EditAtlasStyle as Atlas` and use its controls where the style module
  provides a project component;
- use `Atlas.DesignTokens` for dimensions, spacing, typography, radii, icons,
  and animation durations instead of introducing unrelated literals;
- use `Atlas.Theme` and the application palette for colors instead of embedding
  colors in application views; `Atlas.Theme` presents the shared palette owned
  by `edit_atlas::presentation`, so a new color is added there rather than in
  the style module;
- keep view state declarative and expose reusable commands or state through a
  ViewModel rather than calling services from QML;
- keep dialogs, focus transitions, and native file-selection behavior in the
  concrete View layer;
- add QML files through `qt_add_qml_module` so compilation, type registration,
  linting, deployment, and translation discovery remain reproducible;
- keep the `EditAtlas.Frontends.Quick` and `EditAtlasStyle` module URIs stable;
- avoid loading QML or generated translation artifacts from undocumented local
  paths.

The custom style uses Qt Quick Controls Basic as its fallback. Fusion and
platform-native styles are not the visual contract for the production
frontend.

## Localization

English is the source language and Brazilian Portuguese is the initial
application language for a new profile. QML user-facing strings use `qsTr()`;
C++ QObject text uses the appropriate Qt translation context. Do not use
translated text as a persistent value, automation identifier, format key, or
template field.

Prefer complete translatable sentences with numbered placeholders over
concatenated fragments. When adding or changing text, keep the English source
and `src/presentation/translations/edit_atlas_pt_BR.ts` catalog consistent.
Translation compilation remains part of the CMake build; no manually generated
translation artifact is tracked.

Language selection, recent files, templates, and export preferences are shared
presentation state. Both graphical executables use the same application and
organization identifiers, so switching frontend requires no settings or user
data migration.

## Accessibility and automation

Every interactive or semantically meaningful QML object exposes a stable,
nonlocalized `objectName` and the corresponding `Accessible.id`. Provide an
appropriate accessible role, localized name, and description where the
control's visible text is insufficient. Keyboard focus and cancellation must
remain usable without a pointer.

Identifiers are an automation contract. Do not derive them from translated
text or change them as part of a visual refactor. Dynamic identifiers must
follow the indexed conventions in `docs/accessibility-automation.md`.
Frontend-specific control structure is acceptable, but semantic E2E operations
must expose the same user workflow through both adapters.

## Linting and tests

Lint every compiled QML module from an existing build tree with:

```sh
cmake --build --preset debug-x64-linux --target all_qmllint
```

Run the normal unit and integration layers with:

```sh
ctest --preset debug-x64-linux --label-regex '^unit$'
ctest --preset debug-x64-linux --label-regex '^integration$'
```

Run only the in-process Qt Quick view suite with:

```sh
ctest --preset debug-x64-linux \
  --tests-regex '^edit_atlas_quick_qml_tests$'
```

GoogleTest covers C++ ViewModel and adapter behavior. Qt Quick Test covers QML
bindings, focus, accessibility, dynamic controls, and dialog cancellation
against real shared ViewModels. Packaged pytest E2E drives the installed Qt
Quick production application on Linux and Windows through platform
accessibility APIs. The macOS implementation remains non-required because
GitHub-hosted runners cannot be reliably pre-authorized for Accessibility.

## Deployment and licensing

Production installers contain Qt Quick and do not install a second graphical
application. CI still creates both frontend package variants so each can be
installed and verified independently.

Qt is dynamically linked. Qt Quick packages deploy the required Qt QML import
tree, plugins, platform integration, and shared libraries. Package verification
checks the deployed QML modules and dynamic dependency closure on Linux,
macOS, and Windows. Tagged releases publish only the verified Qt Quick binary
packages together with the required notices and corresponding Qt and FFmpeg
source archives.

Do not add a Qt module or third-party QML dependency without updating vcpkg,
deployment verification, notices, corresponding-source coverage where
applicable, and every supported package path. The detailed requirements remain
in `docs/qt-lgpl-compliance.md`, `docs/ffmpeg-lgpl-compliance.md`, and
`docs/packaging.md`.
