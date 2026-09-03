# Edit Atlas

[![CI](https://github.com/gabrielslrodrigues/edit-atlas/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/gabrielslrodrigues/edit-atlas/actions/workflows/ci.yml?query=branch%3Amaster)

Edit Atlas is a privacy-first desktop application for inspecting editorial
timeline interchange files and exporting structured reports. It currently
supports importing CMX 3600 EDL files and exporting Microsoft Excel workbooks.
The desktop interface can combine field-specific event filters, select and
order exported columns, and save those settings as reusable templates. A
command-line frontend provides the same import and spreadsheet-export workflow
for automation.

The project separates its UI-independent core, built-in formats, application
services, and frontend adapters. See [Architecture](docs/architecture.md) for
the dependency direction and extension points. Desktop contributors should
also read the [Qt Quick](docs/api/qt-quick-frontend.md) and
[Qt Widgets](docs/api/qt-widgets-frontend.md) frontend guides. See
[CHANGELOG.md](CHANGELOG.md) for release highlights, and
[CONTRIBUTING.md](CONTRIBUTING.md) for the conventions changes follow.

## Common requirements

- CMake 4.0 or newer
- A C++23 compiler
- Ninja
- Git

Install these tools using the package manager or installer appropriate for your
platform. The sections below list only additional platform-specific
requirements.

### Linux requirements

vcpkg requires host build tools, and Qt requires native window-system
development packages on Linux. Install them on Fedora with:

```sh
sudo dnf install \
  autoconf \
  autoconf-archive \
  automake \
  bison \
  curl \
  glibc-devel \
  libtool \
  libX11-devel \
  libXext-devel \
  libXfixes-devel \
  libXi-devel \
  libXrender-devel \
  libXtst-devel \
  libxcb-devel \
  libxkbcommon-devel \
  libxkbcommon-x11-devel \
  make \
  nasm \
  perl \
  pkgconf-pkg-config \
  tar \
  unzip \
  wayland-devel \
  wayland-protocols-devel \
  xcb-util-devel \
  xcb-util-cursor-devel \
  xcb-util-image-devel \
  xcb-util-keysyms-devel \
  xcb-util-renderutil-devel \
  xcb-util-wm-devel \
  zip
```

On Ubuntu, install the equivalent tools and libraries with:

```sh
sudo apt-get install \
  '^libxcb.*-dev' \
  autoconf \
  autoconf-archive \
  automake \
  bison \
  curl \
  libc6-dev \
  libtool \
  libx11-dev \
  libx11-xcb-dev \
  libxext-dev \
  libxfixes-dev \
  libxrender-dev \
  libxi-dev \
  libxkbcommon-dev \
  libxkbcommon-x11-dev \
  libwayland-dev \
  make \
  nasm \
  perl \
  pkg-config \
  tar \
  unzip \
  wayland-protocols \
  zip
```

### macOS requirements

Install the Xcode Command Line Tools to provide the Apple compiler and platform
SDK:

```sh
xcode-select --install
```

Then install the vcpkg host tools with Homebrew:

```sh
brew install \
  autoconf \
  autoconf-archive \
  automake \
  curl \
  libtool \
  make \
  nasm \
  pkg-config \
  unzip \
  zip
```

### Windows requirements

Use MSVC 2022 or later as the C++23 compiler, with the Desktop development with
C++ workload. This command installs Visual Studio 2022 Build Tools:

```powershell
winget install --exact --id Microsoft.VisualStudio.2022.BuildTools `
  --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

Open a new terminal after installation so the compiler is available.

On Windows, the pinned vcpkg FFmpeg port downloads its pinned NASM host tool
automatically. Linux and macOS use the system NASM installations listed above.

### Bootstrap vcpkg

Initialize the vcpkg submodule and bootstrap it once after cloning. On Linux and
macOS:

```sh
git submodule update --init --recursive
./vcpkg/bootstrap-vcpkg.sh -disableMetrics
```

On Windows PowerShell:

```powershell
git submodule update --init --recursive
.\vcpkg\bootstrap-vcpkg.bat -disableMetrics
```

## Configure and build

Choose the preset matching the target platform:

| Platform | Debug preset | Release preset |
| --- | --- | --- |
| Linux x64 | `debug-x64-linux` | `release-x64-linux` |
| macOS ARM64 | `debug-arm64-osx` | `release-arm64-osx` |
| macOS x64 | `debug-x64-osx` | `release-x64-osx` |
| macOS Universal | `debug-universal-osx` | `release-universal-osx` |
| Windows x64 | `debug-x64-windows` | `release-x64-windows` |

For example, configure and build a Linux debug build. vcpkg installs the
dependencies declared in `vcpkg.json` as part of the configure step:

```sh
cmake --preset debug-x64-linux
cmake --build --preset debug-x64-linux
```

All presets use project-owned vcpkg triplets that require dynamic Qt linkage.
Linux presets also pin the compiler to Clang 19 or newer, for the project and
for every vcpkg port, so a local build matches a CI build; install a suitable
one with `scripts/ci/install-ubuntu-dependencies.sh --build`. Older releases
cannot compile the project's C++23 code against libstdc++, and configuring
fails with that reason rather than a missing-template error.
Release CI builds macOS ARM64 and x64 independently, then combines their
staged bundles into the universal installer.

## Run the application

After building, run the application directly on Linux:

```sh
./build/debug-x64-linux/src/frontends/quick/edit-atlas
```

On macOS, open the application bundle:

```sh
open build/debug-arm64-osx/src/frontends/quick/edit-atlas.app
```

On Windows PowerShell:

```powershell
.\build\debug-x64-windows\src\frontends\quick\edit-atlas.exe
```

Ordinary `debug-` and `release-` presets build both graphical frontends plus
the independent CLI. Qt Quick is the primary application and packaged desktop
frontend. It includes the compiled `EditAtlasStyle` design-system module.
Qt Widgets remains available as the secondary desktop frontend and can be run
from the same Linux debug build with:

```sh
./build/debug-x64-linux/src/frontends/widgets/edit-atlas-widgets
```

The `release-widgets-` and `release-quick-` preset families build only their
named frontend for package validation and provide an explicit rollback path to
Widgets without removing either concrete application target.
`EDIT_ATLAS_DEFAULT_FRONTEND` selects the primary application target and
installed frontend, while `EDIT_ATLAS_BUILD_FRONTENDS` controls whether a
build includes both graphical frontends or only one. Release presets also
build isolated Widgets and Quick packaging applications. These thin targets
reuse the compiled frontend libraries while giving each package independent
product naming, installation, and Qt deployment rules.

Both graphical frontends use the same application and organization identifiers
and the same shared presentation persistence. Existing language, recent-file,
template, and export settings therefore remain available after the Qt Quick
cutover without migration.

Lint every compiled QML module through its generated CMake target:

```sh
cmake --build --preset debug-x64-linux --target all_qmllint
```

English is the source language. Brazilian Portuguese translations are compiled
from `src/presentation/translations/edit_atlas_pt_BR.ts` and embedded through
the shared presentation layer. The interface defaults to Brazilian Portuguese
on first launch. The
**Language** menu switches between Brazilian Portuguese and English and
remembers the choice for subsequent launches.

The **Appearance** menu selects System, Light, or Dark and remembers the
choice for subsequent launches. New profiles follow the system, and while
System is selected the interface tracks the operating system as it changes,
without restarting. Window decorations, native dialogs, and menus follow the
platform color scheme, which some desktop environments control themselves; on
those systems the application content changes while the surrounding chrome may
not.

Spreadsheet export can follow the active interface language or explicitly use
English or Brazilian Portuguese. The selected language changes workbook sheet
names, headings, generated labels, and document properties. Imported titles,
identifiers, comments, file paths, timecodes, metadata keys, and diagnostic
details remain unchanged, while numeric values remain numeric cells. Before
choosing a destination, the export dialog also lets users include, exclude, and
reorder event columns. Standard timeline columns are selected by default;
fields that require supplemental media, such as an initial frame image, are
opt-in.

Selecting **Initial frame** reveals a rendered-video selector. The chosen MOV,
MP4, or MXF file must have a constant frame rate and readable embedded starting
timecode matching the EDL record timeline; its duration and timecode mode must
also match. Edit Atlas validates the file locally, extracts each exported
event's Record In frame with cancellable progress, and embeds the resulting
images in the workbook. Ordinary exports do not require a video.

Open a CMX 3600 EDL with **File → Open Timeline**, the standard open shortcut,
or by dropping the file onto the window. Non-drop-frame EDLs that omit their
frame rate prompt for it before import. Parsed events can be sorted by any
column and filtered with one or more field-specific conditions above the table.
Free-text fields support case matching, whole-word matching, and RE2 regular
expressions; categorical, timecode, and duration fields use typed exact values.
Conditions can be combined by matching all or any condition. Importer warnings
and errors retain their source line numbers.

The template controls above the filters save a named combination of filter
conditions and ordered export columns. Templates are stored only on the local
computer, can be applied to later documents, and can be renamed, updated,
duplicated, or deleted. The interface marks an active template as modified when
its filters or export columns differ from the saved version.

Recent-file history is disabled by default. Enabling **Remember Recent Files**
stores only local file paths in the platform settings. Disabling it clears the
stored history.

Persistent logs are stored privately with bounded rotation and retention.
**Help → Export Diagnostic Logs** creates an offline support bundle after
disclosing its contents. See
[Diagnostic logging and support bundles](docs/diagnostic-support.md) for the
logged metadata and exact bundle contents.

## Use the command-line interface

The Qt-free `edit-atlas-cli` executable converts local CMX 3600 EDL files to
XLSX reports through the same application services as the desktop frontend. A
non-drop-frame EDL needs an explicit frame rate:

```sh
./build/debug-x64-linux/src/frontends/cli/edit-atlas-cli \
  convert --fps 24 timeline.edl report.xlsx
```

Use a rational value for fractional rates:

```sh
edit-atlas-cli convert --fps 30000/1001 timeline.edl report.xlsx
```

Existing output files are preserved unless `--force` is supplied. Workbook
language and sheet inclusion can be selected with the same options exposed by
the desktop application. Paths and imported content remain UTF-8 across Linux,
macOS, and Windows. Conversion performs only local filesystem operations and
has no telemetry or network behavior.

The process exit codes are stable:

| Code | Meaning |
| ---: | --- |
| `0` | Conversion completed without warnings or errors |
| `1` | Conversion completed with warnings |
| `2` | Invalid command-line usage |
| `3` | The readable input contains import errors |
| `4` | The destination exists and replacement was not authorized |
| `5` | A filesystem or application operation failed |

Run `edit-atlas-cli --help` for the command summary,
`edit-atlas-cli convert --help` for conversion options, and
`edit-atlas-cli --version` for the installed version. See the [CLI
reference](docs/cli.md) for all conversion options and platform paths.

## Run tests

Tests are built by default and use GoogleTest with CTest discovery. Run the
test suite with the same platform preset used for the build:

```sh
ctest --preset debug-x64-linux
```

C++ tests are organized as `tests/unit` and `tests/integration`, and every
discovered test has the corresponding CTest label. The existing presets run
both layers. To select one layer from an existing configured build tree, use:

```sh
ctest --preset debug-x64-linux --label-regex '^unit$'
ctest --preset debug-x64-linux --label-regex '^integration$'
```

Immutable synthetic inputs shared by multiple layers live under
`tests/fixtures`. Packaged black-box tests live under `tests/e2e` and run
separately from CTest. See [the test-suite documentation](tests/README.md) for
the classification and fixture policies.

To configure the project without building tests, use CMake's standard
`BUILD_TESTING` option:

```sh
cmake --preset debug-x64-linux -DBUILD_TESTING=OFF
```

To treat project warnings as errors, configure with:

```sh
cmake --preset debug-x64-linux -DEDIT_ATLAS_WARNINGS_AS_ERRORS=ON
```

## Generate API documentation

The optional documentation build requires Doxygen 1.9.8 or newer and Graphviz.
Install both with the platform package manager; they are not required by any
normal application configure or build preset.

Generate the complete searchable C++ API reference with one command:

```sh
cmake --workflow --preset documentation
```

The generated site is written to `build/documentation/docs/html/index.html`.
The dedicated configure path does not initialize vcpkg or look for Qt. It
downloads the reproducibly pinned Doxygen Awesome theme into the ignored build
tree and treats undocumented or malformed public API documentation as an
error.

Every successful documentation run on `master` publishes the API reference to
[GitHub Pages](https://gabrielslrodrigues.github.io/edit-atlas/latest/). Pull
requests still generate and validate the site without changing the published
version.
Release-tag documentation is published under the matching `/vX.Y.Z/` path;
the [documentation version index](https://gabrielslrodrigues.github.io/edit-atlas/versions.html)
lists the available releases.

Public declarations use LLVM-style `///` comments, described under Code
conventions in [CONTRIBUTING.md](CONTRIBUTING.md). Generated HTML, warning
logs, and downloaded theme files must not be committed.

## Create installers

Preset-driven packaging creates a Windows installer, a universal macOS
installer, and Linux portable and Debian packages without requiring users to
install Qt or vcpkg. See [Desktop packaging](docs/packaging.md) for the
one-command workflows, output locations, supported systems, and installation
instructions.

The repository separates ordinary validation, reusable package production,
packaged E2E, documentation, and protected release publication. See
[Continuous integration](docs/continuous-integration.md) for the workflow
graph, artifact lifecycle, permissions, and required-check policy.

## License

Edit Atlas is licensed under the Apache License 2.0. See
[LICENSE](LICENSE).

Edit Atlas dynamically links to Qt 6 under the LGPL-3.0-only option. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and the
[Qt LGPL compliance policy](docs/qt-lgpl-compliance.md) for the distribution
requirements enforced by the project.

The video backend dynamically links to a minimal LGPL-compatible FFmpeg build.
See the [FFmpeg LGPL compliance policy](docs/ffmpeg-lgpl-compliance.md) for the
enabled libraries, codec policy, source-distribution requirements, and patent
licensing boundary.
