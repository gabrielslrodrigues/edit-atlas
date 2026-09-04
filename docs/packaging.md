# Desktop packaging

Edit Atlas packages the installed runtime tree produced by CMake. Each package
contains the application, dynamically linked Qt libraries, required platform
plugins, the embedded English and Brazilian Portuguese interface resources,
and license notices from the exact vcpkg dependency installation. End users do
not need CMake, vcpkg, or a separate Qt installation.

Qt Quick is the primary packaged frontend. The shared maintenance policy and
secondary Widgets rollback boundary are documented in
[Architecture](architecture.md), with toolkit-specific details in the
[Qt Quick](api/qt-quick-frontend.md) and
[Qt Widgets](api/qt-widgets-frontend.md) frontend guides.

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

The explicit Qt Widgets and Qt Quick workflows configure a release build,
build it, and run the matching CPack generators:

```sh
cmake --workflow --preset create-widgets-package-x64-linux
cmake --workflow --preset create-quick-package-x64-linux
```

On Windows PowerShell:

```powershell
cmake --workflow --preset create-widgets-package-x64-windows
cmake --workflow --preset create-quick-package-x64-windows
```

The generic `create-package-` workflows follow the project's current default
frontend, which is Qt Quick. Explicit frontend workflows remain available for
validation and emergency rollback. CI verifies both packages on every run.

The universal macOS package is assembled by CI from independently built and
staged ARM64 and x64 application bundles. This avoids relying on a universal
vcpkg triplet for dependencies whose upstream build systems do not reliably
support multi-architecture builds. The assembly is automated by
`scripts/ci/create-macos-universal-package.sh`.

Outputs are written below:

```text
build/packages/
├── default/
│   ├── x64-linux/
│   └── x64-windows/
├── quick/
│   ├── x64-linux/
│   ├── universal-osx/
│   └── x64-windows/
└── widgets/
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

Package verification checks archive structure, deployed dependencies,
architecture metadata, license material, installation, launch, representative
CLI conversion, and removal where the platform provides a package manager.
Run the same scripts CI uses after creating the matching package:

```sh
./scripts/ci/verify-linux-packages.sh ubuntu build/packages/widgets/x64-linux
./scripts/ci/verify-linux-packages.sh fedora build/packages/widgets/x64-linux
./scripts/ci/verify-macos-package.sh build/packages/widgets/universal-osx
./scripts/ci/verify-linux-packages.sh ubuntu build/packages/quick/x64-linux
./scripts/ci/verify-macos-package.sh build/packages/quick/universal-osx
```

On Windows PowerShell:

```powershell
.\scripts\ci\verify-windows-package.ps1 `
  -PackageDirectory build\packages\widgets\x64-windows
.\scripts\ci\verify-windows-package.ps1 `
  -PackageDirectory build\packages\quick\x64-windows
```

Run each Linux command on the named distribution. The Linux scripts install
and remove the native package through the system package manager. The macOS
script installs the application under `/Applications` for its launch check and
then removes it. The Windows script performs a silent MSI installation and
uninstallation. All three therefore require permission to install software.
Failed Windows verification jobs upload variant-specific verbose Windows
Installer log artifacts. Other verifier output remains in the corresponding
GitHub Actions step log.

The producer's Ubuntu and macOS host dependencies are also captured in
`scripts/ci/install-ubuntu-dependencies.sh` and
`scripts/ci/install-macos-dependencies.sh`. These supplement the common tools
listed in the project README and are usable when reproducing their respective
CI environments.

Workflow ownership, artifact lifetimes, permissions, and the relationship
between ordinary validation and tagged releases are documented in
[Continuous integration](continuous-integration.md).

These checks validate package structure and dependency deployment. Before a
release is published, a manual clean-machine smoke test should still open the
application, switch between English and Brazilian Portuguese, import a
timeline, export a spreadsheet, and uninstall or remove the application.

## Tagged releases

The dedicated `release.yml` workflow runs only for tags matching `vX.Y.Z`.
The tag must match the `version-string` in the tagged `vcpkg.json` and must
point at the checked-out commit. Ordinary branches and pull requests never
publish a release.

Before using the workflow, configure a protected GitHub environment named
`edit-atlas-release` with at least one required reviewer. This approval is the
explicit authorization required for an unsigned tag; signed tags are also
accepted. Do not put signing or notarization credentials in pull-request
workflows.

After approval, the workflow calls the same reusable package and packaged-E2E
workflows used by ordinary CI. They build and test Linux x64, macOS ARM64 and
x64, and Windows x64 from the tag, assemble the universal macOS package, and
validate every package on the supported verification systems. The release
workflow uploads only the five packages for the production frontend resolved
from `EDIT_ATLAS_DEFAULT_FRONTEND`. Other frontend packages remain CI
artifacts used for verification. The release workflow also downloads the
exact source archives selected by the pinned vcpkg ports for Qt Base,
Declarative, Language Server, Shader Tools, and SVG, verifies their
SHA-512 digests, and packages them with the complete ports and patch sets,
vcpkg commit, manifest, triplets, and relevant build configuration. It does
the same for the exact FFmpeg source and vcpkg port and patch set used by the
dynamically linked media backend. The draft contains both corresponding-source
archives, `SHA256SUMS`, `LICENSE`, and `THIRD_PARTY_NOTICES.md`.

Each binary package installs a version-specific `QT_SOURCE_OFFER.md` that links
to the corresponding-source asset in its GitHub Release and explains how to
rebuild and replace the bundled Qt libraries. A parallel
`FFMPEG_SOURCE_OFFER.md` identifies the FFmpeg asset and explains rebuilding
and shared-library replacement. `SHA256SUMS` covers the application packages
and both corresponding-source archives.

The draft is created before platform builds begin. If a build or verification
job fails, the draft remains unpublished and can be inspected as an
incomplete release. Fix the tagged commit by creating a new version tag, or
rerun the failed workflow after correcting an infrastructure problem; do not
manually publish a draft whose required platform checks did not pass.
Rerunning a workflow can resume an existing draft, but it refuses to modify a
release that has already been published.

### Emergency frontend rollback

Qt Widgets remains independently buildable, packaged, and verified so a
regression in the primary Qt Quick frontend does not require reverting the
shared presentation or service architecture. Before tagging an emergency
Widgets release:

1. set `EDIT_ATLAS_DEFAULT_FRONTEND` in `CMakeLists.txt` to `widgets`;
2. run `scripts/ci/resolve-frontends.sh` and confirm that it reports Widgets as
   production;
3. run the complete ordinary CI workflow and confirm that the Widgets package
   passes package verification and packaged E2E on every required platform;
4. create the version tag only after those checks pass.

These changes select the already-maintained secondary frontend; they do not
remove Qt Quick, change persistent application identifiers, or migrate user
state. CI and release workflows derive their package and E2E selection through
`scripts/ci/resolve-frontends.sh`; no workflow artifact pattern or input is
edited separately. The explicit `release-widgets-` presets provide the
equivalent local package-validation path. Restore the CMake default to Qt Quick
after the blocking regression is fixed and validated.

The Qt corresponding-source archive includes the exact source archives,
vcpkg ports, and patch sets for Qt Base, Declarative, Language Server, Shader
Tools, and SVG. Together they cover the Qt module closure used to build and
deploy the Qt Quick frontend.

The current workflow does not require signing or Apple notarization secrets.
When those credentials are introduced, keep them in the protected release
environment and add signing steps only to the tag workflow. Local package
creation remains independent of release credentials.
