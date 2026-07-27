# Edit Atlas

Edit Atlas is a privacy-first desktop application for inspecting editorial
timeline interchange files and exporting structured reports. It currently
supports importing CMX 3600 EDL files and exporting Microsoft Excel workbooks.

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
  libtool \
  libx11-dev \
  libx11-xcb-dev \
  libxext-dev \
  libxfixes-dev \
  libxrender-dev \
  libxi-dev \
  libxkbcommon-dev \
  libxkbcommon-x11-dev \
  make \
  pkg-config \
  tar \
  unzip \
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

Configure a debug build. vcpkg installs the dependencies declared in
`vcpkg.json` as part of this step:

```sh
cmake --preset debug
```

Build it:

```sh
cmake --build --preset debug
```

Release builds use the corresponding `release` presets.

## Run tests

Tests are built by default and use GoogleTest with CTest discovery. Run the
debug test suite with:

```sh
ctest --preset debug
```

Use the corresponding `release` preset to test a release build. To configure
the project without building tests, use CMake's standard `BUILD_TESTING`
option:

```sh
cmake --preset debug -DBUILD_TESTING=OFF
```

To treat project warnings as errors, configure with:

```sh
cmake --preset debug -DEDIT_ATLAS_WARNINGS_AS_ERRORS=ON
```

## License

Edit Atlas is licensed under the Apache License 2.0. See
[LICENSE](LICENSE).
