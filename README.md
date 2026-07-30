# Edit Atlas

[![CI](https://github.com/gabrielslrodrigues/edit-atlas/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/gabrielslrodrigues/edit-atlas/actions/workflows/ci.yml?query=branch%3Amaster)

Edit Atlas is a privacy-first desktop application for inspecting editorial
timeline interchange files and exporting structured reports. It currently
supports importing CMX 3600 EDL files and exporting Microsoft Excel workbooks.

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
| Windows x64 | `debug-x64-windows` | `release-x64-windows` |

For example, configure and build a Linux debug build. vcpkg installs the
dependencies declared in `vcpkg.json` as part of the configure step:

```sh
cmake --preset debug-x64-linux
cmake --build --preset debug-x64-linux
```

All presets use project-owned vcpkg triplets that require dynamic Qt linkage.

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

Open a CMX 3600 EDL with **File → Open Timeline**, the standard open shortcut,
or by dropping the file onto the window. Non-drop-frame EDLs that omit their
frame rate prompt for it before import. Parsed events can be sorted by any
column and filtered using the field above the table; importer warnings and
errors retain their source line numbers.

Recent-file history is disabled by default. Enabling **Remember Recent Files**
stores only local file paths in the platform settings. Disabling it clears the
stored history.

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

## License

Edit Atlas is licensed under the Apache License 2.0. See
[LICENSE](LICENSE).

Edit Atlas dynamically links to Qt 6 under the LGPL-3.0-only option. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and the
[Qt LGPL compliance policy](docs/qt-lgpl-compliance.md) for the distribution
requirements enforced by the project.
