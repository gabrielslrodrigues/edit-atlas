# Edit Atlas

[![CI](https://github.com/gabrielslrodrigues/edit-atlas/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/gabrielslrodrigues/edit-atlas/actions/workflows/ci.yml?query=branch%3Amaster)

Edit Atlas is a privacy-first desktop application for inspecting editorial
timeline interchange files and exporting structured reports. It currently
supports importing CMX 3600 EDL files and exporting Microsoft Excel workbooks.
The desktop interface can combine field-specific event filters and exports
exactly the currently matching rows without modifying the imported timeline.

The project separates its UI-independent core, built-in formats, application
services, and frontend adapters. See [Architecture](docs/architecture.md) for
the dependency direction and extension points.

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
  curl \
  glibc-devel \
  libtool \
  libX11-devel \
  libXext-devel \
  libXfixes-devel \
  libXi-devel \
  libXrender-devel \
  libxcb-devel \
  libxkbcommon-devel \
  libxkbcommon-x11-devel \
  make \
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
Release CI builds macOS ARM64 and x64 independently, then combines their
staged bundles into the universal installer.

## Run the application

After building, run the application directly on Linux:

```sh
./build/debug-x64-linux/src/app/edit-atlas
```

On macOS, open the application bundle:

```sh
open build/debug-arm64-osx/src/app/edit-atlas.app
```

On Windows PowerShell:

```powershell
.\build\debug-x64-windows\src\app\edit-atlas.exe
```

English is the source language. Brazilian Portuguese translations are compiled
from `src/app/translations/edit_atlas_pt_BR.ts` and embedded in the executable.
The interface defaults to Brazilian Portuguese on first launch. The language
selector in the top-right switches between Brazilian Portuguese and English
and remembers the choice for subsequent launches.

Spreadsheet export can follow the active interface language or explicitly use
English or Brazilian Portuguese. The selected language changes workbook sheet
names, headings, generated labels, and document properties. Imported titles,
identifiers, comments, file paths, timecodes, metadata keys, and diagnostic
details remain unchanged, while numeric values remain numeric cells.

Open a CMX 3600 EDL with **File → Open Timeline**, the standard open shortcut,
or by dropping the file onto the window. Non-drop-frame EDLs that omit their
frame rate prompt for it before import. Parsed events can be sorted by any
column and filtered with one or more field-specific conditions above the table.
Free-text fields support case matching, whole-word matching, and RE2 regular
expressions; categorical, timecode, and duration fields use typed exact values.
Conditions can be combined by matching all or any condition. Importer warnings
and errors retain their source line numbers.

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
./build/debug-x64-linux/src/cli/edit-atlas-cli \
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

Public declarations use LLVM-style `///` comments. The first sentence is a
brief summary; follow it with parameter, return-value, ownership, lifetime, and
invariant details where they form part of the contract. Generated HTML, warning
logs, and downloaded theme files must not be committed.

## Create installers

Preset-driven packaging creates a Windows installer, a universal macOS
installer, and Linux portable and Debian packages without requiring users to
install Qt or vcpkg. See [Desktop packaging](docs/packaging.md) for the
one-command workflows, output locations, supported systems, and installation
instructions.

## License

Edit Atlas is licensed under the Apache License 2.0. See
[LICENSE](LICENSE).

Edit Atlas dynamically links to Qt 6 under the LGPL-3.0-only option. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and the
[Qt LGPL compliance policy](docs/qt-lgpl-compliance.md) for the distribution
requirements enforced by the project.
