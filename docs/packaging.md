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
| macOS installer | x86_64 and ARM64 | macOS 13.3 |
| Linux archive | x86_64 | Ubuntu 24.04 or another distribution with glibc 2.39 or newer |
| Debian package | amd64 | Ubuntu 24.04 or a compatible Debian-based distribution with glibc 2.39 or newer |
| RPM package | x86_64 | Fedora 44 or a compatible RPM-based distribution with glibc 2.39 or newer |

Build-system platform baselines are defined centrally in
[EditAtlasPlatformSupport.cmake](../cmake/EditAtlasPlatformSupport.cmake).

Linux packages bundle Qt and the non-system runtime libraries selected by Qt's
deployment tooling. They intentionally rely on the host's GNU C library,
graphics stack, and desktop services. Ubuntu 24.04 is the build baseline.
Ubuntu 24.04 and Fedora 44 are the native-package validation targets; other
distributions meeting the runtime requirements may work but are not CI-tested.
Bundled libraries are isolated below the platform library directory in an
`edit-atlas` subdirectory, such as `/usr/lib64/edit-atlas` on Fedora. The
package never installs generic shared-library files directly into `/usr/lib`
or `/usr/lib64`, where they could conflict with distribution-owned packages.
Linux Qt plugins are kept below the same private directory so their relative
RUNPATHs resolve only package-owned libraries.

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

The Linux workflow configures a release build, builds it, and runs the
matching CPack generators:

```sh
cmake --workflow --preset create-package-x64-linux
```

On Windows PowerShell:

```powershell
cmake --workflow --preset create-package-x64-windows
```

The universal macOS package is assembled by CI from independently built and
staged ARM64 and x64 application bundles. This avoids relying on a universal
vcpkg triplet for dependencies whose upstream build systems do not reliably
support multi-architecture builds. The assembly is automated by
`scripts/ci/create-macos-universal-package.sh`.

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

Replace `X.Y.Z` with the package version being installed.

```sh
sudo apt install ./edit-atlas-X.Y.Z-linux-x86_64.deb
sudo apt remove edit-atlas
```

### RPM package

Install and uninstall with DNF:

Replace `X.Y.Z` with the package version being installed.

```sh
sudo dnf install ./edit-atlas-X.Y.Z-linux-x86_64.rpm
sudo dnf remove edit-atlas
```

## Package verification

CI separates package production from package consumption. The
`build-and-package` matrix builds, tests, and stages native macOS ARM64 and
x64 bundles, while directly packaging Linux x64 and Windows x64. A dedicated
job merges every matching pair of Mach-O files into a universal macOS bundle
and creates its installer. Independent verification jobs then download the
packages and treat them like end-user downloads:

- extracts the portable archive and both Linux native packages, verifies their
  private runtime-library layout, executable RUNPATHs, deployed Qt libraries,
  XCB and Wayland plugins, architecture metadata, and license materials, then
  installs and removes the Debian package through APT and the RPM package
  through DNF on Fedora 44; each installed package is launched under Xvfb and
  its CLI converts a representative CMX 3600 fixture;
- installs the macOS package on both Apple Silicon and Intel runners, verifies
  its deployed Qt runtime, launches each native slice, and checks every Mach-O
  file for both x86_64 and ARM64 slices;
- silently installs the Windows package into an isolated directory, verifies
  Qt and the Windows platform plugin, confirms the uninstall registry entry,
  runs the uninstaller, and confirms the executable was removed.

### Dependency cache lifecycle

CI uses the repository's public GitHub Packages NuGet feed as the vcpkg binary
cache. vcpkg stores each built dependency using its package ABI, so compatible
binaries can be reused across branches and runner images while incompatible
compiler or SDK combinations remain separate. The feed is configured with the
workflow-provided `GITHUB_TOKEN`, and the package IDs use an Edit Atlas prefix
to keep them distinct from caches owned by other repositories. No personal
token is required.

Jobs for the same platform and triplet are serialized to avoid competing
uploads of the same package, while different triplets continue to run
concurrently. Fork pull requests can read packages but cannot publish them.
The job summary reports the number of packages restored and built by vcpkg.

The packages are CI implementation details, not application dependencies for
end users. Public package storage and transfer are free for this public
repository. Old ABI versions may remain in the feed until package-retention
automation is added.

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

## Tagged releases

Release automation runs only for tags matching `vX.Y.Z`. The tag must match
the `version-string` in the tagged `vcpkg.json` and must point at the
checked-out commit. Ordinary branches and pull requests never publish a
release.

Before using the workflow, configure a protected GitHub environment named
`edit-atlas-release` with at least one required reviewer. This approval is the
explicit authorization required for an unsigned tag; signed tags are also
accepted. Do not put signing or notarization credentials in pull-request
workflows.

After approval, the workflow builds and tests Linux x64, macOS ARM64 and x64,
and Windows x64 from the tag. It then assembles the universal macOS package,
validates every package on the supported verification systems, and uploads
the five installers/archives to a draft GitHub Release. The workflow also
downloads the exact Qt Base source archive selected by the pinned vcpkg port,
verifies its SHA-512 digest, and packages it with the complete port and patch
set, vcpkg commit, manifest, triplets, and relevant build configuration. The
draft contains that corresponding-source archive, `SHA256SUMS`, `LICENSE`, and
`THIRD_PARTY_NOTICES.md`.

Each binary package installs a version-specific `QT_SOURCE_OFFER.md` that links
to the corresponding-source asset in its GitHub Release and explains how to
rebuild and replace the bundled Qt libraries. `SHA256SUMS` covers both the
application packages and the Qt source archive.

The draft is created before platform builds begin. If a build or verification
job fails, the draft remains unpublished and can be inspected as an
incomplete release. Fix the tagged commit by creating a new version tag, or
rerun the failed workflow after correcting an infrastructure problem; do not
manually publish a draft whose required platform checks did not pass.
Rerunning a workflow can resume an existing draft, but it refuses to modify a
release that has already been published.

The current workflow does not require signing or Apple notarization secrets.
When those credentials are introduced, keep them in the protected release
environment and add signing steps only to the tag workflow. Local package
creation remains independent of release credentials.
