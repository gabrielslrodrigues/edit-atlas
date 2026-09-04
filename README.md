# Edit Atlas

[![CI](https://github.com/gabrielslrodrigues/edit-atlas/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/gabrielslrodrigues/edit-atlas/actions/workflows/ci.yml?query=branch%3Amaster)

Edit Atlas is a privacy-first desktop application for inspecting CMX 3600 edit
decision lists and exporting structured XLSX reports. It supports typed event
filters, ordered export columns, reusable templates, and optional initial-frame
images from a matching rendered video. A Qt-free command-line frontend provides
the same local conversion workflow for automation. The application has no
telemetry or network behavior.

Read the [desktop user guide](docs/user-guide.md) to use the graphical
application or the [CLI reference](docs/cli.md) for command-line conversion.
The [documentation index](docs/index.md) routes contributors and maintainers to
the canonical architecture, testing, packaging, CI, and compliance guides.
Release highlights are in [CHANGELOG.md](CHANGELOG.md).

## Development requirements

- CMake 4.0 or newer
- Ninja
- Git
- a C++23 toolchain: Clang 19 or newer on Linux, Xcode Command Line Tools on
  macOS, or MSVC 19.33/Visual Studio 2022 17.3 or newer on Windows

Install the platform build dependencies used by CI on Ubuntu or macOS:

```sh
scripts/ci/install-ubuntu-dependencies.sh --build
scripts/ci/install-macos-dependencies.sh
```

On Windows, install Visual Studio with the Desktop development with C++
workload. For example, from PowerShell:

```powershell
winget install --exact --id Microsoft.VisualStudio.2022.BuildTools `
  --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

These commands require permission to install system packages. Open a new
terminal after installing Visual Studio so the compiler is available.

## Bootstrap vcpkg

Initialize and bootstrap the pinned vcpkg submodule once after cloning. On
Linux and macOS:

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

Choose the preset for the target platform:

| Platform | Debug preset | Release preset |
| --- | --- | --- |
| Linux x64 | `debug-x64-linux` | `release-x64-linux` |
| macOS ARM64 | `debug-arm64-osx` | `release-arm64-osx` |
| macOS x64 | `debug-x64-osx` | `release-x64-osx` |
| macOS Universal | `debug-universal-osx` | `release-universal-osx` |
| Windows x64 | `debug-x64-windows` | `release-x64-windows` |

For example:

```sh
cmake --preset debug-x64-linux
cmake --build --preset debug-x64-linux
```

Configuration installs the dependencies declared by `vcpkg.json`. Ordinary
presets build the Qt Quick production frontend, the maintained Qt Widgets
frontend, and the independent CLI. Frontend-specific release presets build
only their named graphical frontend for package validation.

## Run

Run the primary Qt Quick application from a Linux build:

```sh
./build/debug-x64-linux/src/frontends/quick/edit-atlas
```

On macOS:

```sh
open build/debug-arm64-osx/src/frontends/quick/edit-atlas.app
```

On Windows PowerShell:

```powershell
.\build\debug-x64-windows\src\frontends\quick\edit-atlas.exe
```

The secondary frontend is
`build/<preset>/src/frontends/widgets/edit-atlas-widgets` with `.exe` appended
on Windows. The CLI is `build/<preset>/src/frontends/cli/edit-atlas-cli`.

## Test

Run the complete C++ and QML test suite with the configured preset:

```sh
ctest --preset debug-x64-linux
```

See the [test-suite guide](tests/README.md) for labels and fixture policy, and
the [packaged E2E runbook](tests/e2e/README.md) for installed-package tests.
The project conventions are in [CONTRIBUTING.md](CONTRIBUTING.md).

## Package and API documentation

Create the production Qt Quick package on Linux or Windows with:

```sh
cmake --workflow --preset create-quick-package-x64-linux
```

```powershell
cmake --workflow --preset create-quick-package-x64-windows
```

Packaging produces a Windows MSI, a universal macOS PKG, and Linux `.tar.gz`,
DEB, and RPM artifacts. [Desktop packaging](docs/packaging.md) lists every
frontend workflow, prerequisite, output, and verification command.

Generate the C++ API reference with:

```sh
cmake --workflow --preset documentation
```

The generated site is written to `build/documentation/docs/html/index.html`.
Published API versions are available on
[GitHub Pages](https://gabrielslrodrigues.github.io/edit-atlas/versions.html).

## License

Edit Atlas is licensed under the [Apache License 2.0](LICENSE). It dynamically
links to Qt 6 and a minimal FFmpeg build under LGPL-compatible terms. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), the
[Qt policy](docs/qt-lgpl-compliance.md), and the
[FFmpeg policy](docs/ffmpeg-lgpl-compliance.md) for distribution requirements.
