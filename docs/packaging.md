# Desktop packaging

Edit Atlas packages the installed runtime tree produced by CMake. Each package
contains the application, dynamically linked Qt libraries, required platform
plugins, the embedded English and Brazilian Portuguese interface resources,
and license notices from the exact vcpkg dependency installation. End users do
not need CMake, vcpkg, or a separate Qt installation.

Packages are unsigned development artifacts until release signing and Apple
notarization are implemented. Windows SmartScreen and macOS Installer may
therefore warn when opening them.

## Supported systems

| Package | Architecture | Minimum supported system |
| --- | --- | --- |
| Windows installer | x86_64 | Windows 10 version 1809 |
| macOS installer | x86_64 and ARM64 | macOS 13 |
| Linux archive | x86_64 | Ubuntu 24.04 or another distribution with glibc 2.39 or newer |
| Debian package | amd64 | Ubuntu 24.04 or a compatible Debian-based distribution with glibc 2.39 or newer |
| RPM package | x86_64 | Fedora 44 or a compatible RPM-based distribution with glibc 2.39 or newer |

Linux packages bundle Qt and the non-system runtime libraries selected by Qt's
deployment tooling. They intentionally rely on the host's GNU C library,
graphics stack, and desktop services. Ubuntu 24.04 is the build baseline.
Ubuntu 24.04 and Fedora 44 are the native-package validation targets; other
distributions meeting the runtime requirements may work but are not CI-tested.

## Packaging prerequisites

Complete the common and platform requirements in the project
[README](../README.md), including bootstrapping vcpkg.

Additional packaging tools are:

- Linux: `dpkg-dev` for the Debian package and RPM build tools for the RPM
- macOS: `productbuild` and `pkgbuild`, included with Xcode
- Windows: WiX Toolset 3.14

On Ubuntu:

```sh
sudo apt-get install dpkg-dev rpm
```

On Fedora:

```sh
sudo dnf install dpkg-dev rpm-build
```

On Windows PowerShell:

```powershell
winget install --exact --id WiXToolset.WiXToolset
```

The project explicitly uses the MS-RL-licensed WiX Toolset 3 generator. It
does not use the differently licensed current WiX command-line tool.

## Create packages

Each workflow configures a release build, builds it, and runs the matching
CPack generator:

```sh
cmake --workflow --preset create-package-x64-linux
cmake --workflow --preset create-package-universal-osx
```

On Windows PowerShell:

```powershell
cmake --workflow --preset create-package-x64-windows
```

Outputs are written below:

```text
build/packages/
├── x64-linux/
├── universal-osx/
└── x64-windows/
```

Linux produces a `.tar.gz` portable archive plus `.deb` and `.rpm` native
packages. macOS produces a `.pkg` containing a true universal application
bundle. Windows produces an `.msi` installer with a standard Apps & features
uninstall entry. CPack also writes a SHA-256 checksum beside each package.

## Install and uninstall

### Windows

Run the generated MSI and follow its prompts. Remove Edit Atlas through
**Settings → Apps → Installed apps**.

### macOS

Run the package installer to install `edit-atlas.app` in Applications. Remove
the application by moving it from Applications to the Trash.

### Linux portable archive

Extract the archive and run `bin/edit-atlas` from the extracted directory.
Remove the extracted directory to uninstall it.

### Debian package

Install and uninstall with APT:

```sh
sudo apt install ./edit-atlas-0.1.0-linux-x86_64.deb
sudo apt remove edit-atlas
```

### RPM package

Install and uninstall with DNF:

```sh
sudo dnf install ./edit-atlas-0.1.0-linux-x86_64.rpm
sudo dnf remove edit-atlas
```

## Package verification

CI separates package production from package consumption. The
`build-and-package` matrix builds, tests, stages, and packages Linux x64,
macOS Universal, and Windows x64 artifacts. Independent verification jobs
then download those artifacts and treat them like end-user downloads:

- extracts the portable archive and both Linux native packages, verifies their
  deployed Qt libraries, XCB and Wayland plugins, architecture metadata, and
  license materials, then installs and removes the Debian package through APT
  and the RPM package through DNF on Fedora 44;
- installs the macOS package on both Apple Silicon and Intel runners, verifies
  its deployed Qt runtime, launches each native slice, and checks every Mach-O
  file for both x86_64 and ARM64 slices;
- silently installs the Windows package into an isolated directory, verifies
  Qt and the Windows platform plugin, confirms the uninstall registry entry,
  runs the uninstaller, and confirms the executable was removed.

The package checks are scripts rather than embedded workflow fragments, so the
same checks can be run locally after creating the matching package:

```sh
./scripts/ci/verify-linux-packages.sh ubuntu build/packages/x64-linux
./scripts/ci/verify-linux-packages.sh fedora build/packages/x64-linux
./scripts/ci/verify-macos-package.sh build/packages/universal-osx
```

On Windows PowerShell:

```powershell
.\scripts\ci\verify-windows-package.ps1 `
  -PackageDirectory build\packages\x64-windows
```

Run each Linux command on the named distribution. The Linux scripts install
and remove the native package through the system package manager. The macOS
script installs the application under `/Applications` for its launch check and
then removes it. The Windows script performs a silent MSI installation and
uninstallation. All three therefore require permission to install software.
Failed Windows verification jobs upload the verbose Windows Installer logs as
the `windows-installer-logs` workflow artifact. Other verifier output remains
in the corresponding GitHub Actions step log.

The producer's Ubuntu and macOS host dependencies are also captured in
`scripts/ci/install-ubuntu-dependencies.sh` and
`scripts/ci/install-macos-dependencies.sh`. These supplement the common tools
listed in the project README and are usable when reproducing their respective
CI environments.

These checks validate package structure and dependency deployment. Before a
release is published, a manual clean-machine smoke test should still open the
application, switch between English and Brazilian Portuguese, import a
timeline, export a spreadsheet, and uninstall or remove the application.
