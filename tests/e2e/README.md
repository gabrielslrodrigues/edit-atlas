# Packaged end-to-end tests

The pytest E2E suite drives installed Edit Atlas packages through their public
GUI and command-line interfaces. It is separate from CTest and never uses a
build-tree application as the system under test.

Use the platform provisioner when one is available. It installs the package in
an isolated environment, runs the same lower-level entry point used by CI,
copies results to the host, and removes the environment afterwards.

## Requirements

Install uv 0.12 and provide:

- the package for the frontend and platform under test;
- either the matching
  `edit_atlas_e2e_media_fixture_generator` build-tree executable or an existing
  `build/e2e/media-fixtures` directory generated from the same source tree; and
- an interactive desktop for graphical tests.

Generate fixtures only through the repository entry point so their recorded
source digest can be validated:

```sh
tests/e2e/generate-media-fixtures.sh \
  build/<preset>/tests/integration/media/edit_atlas_e2e_media_fixture_generator \
  build/e2e/media-fixtures
```

On Windows PowerShell:

```powershell
tests/e2e/generate-media-fixtures.ps1 `
  -Generator `
    "build\<preset>\tests\integration\media\edit_atlas_e2e_media_fixture_generator.exe" `
  -FixtureDirectory "build\e2e\media-fixtures"
```

A stale or missing generator digest stops collection and prints the required
regeneration command.

## Command-line scenarios

Run the installed CLI on Linux or macOS with:

```sh
tests/e2e/run.sh --cli /path/to/edit-atlas-cli
```

On Windows PowerShell:

```powershell
tests/e2e/run.ps1 -Cli "C:\path\to\edit-atlas-cli.exe"
```

Additional pytest arguments follow the executable option. A required suite
fails when it collects nothing or when a test is deselected, skipped, xfailed,
or unexpectedly passes an xfail.

## Linux desktop tests

Linux provisioning requires Podman or Docker. It runs the generated DEB in a
disposable Ubuntu container with Xvfb and a session D-Bus. Podman is preferred
when both runtimes are installed:

```sh
tests/e2e/provision-linux.sh \
  --package /path/to/edit-atlas-X.Y.Z-linux-x86_64.deb \
  --fixture-generator \
    build/debug-x64-linux/tests/integration/media/edit_atlas_e2e_media_fixture_generator
```

Reuse current fixtures or select Docker explicitly when needed:

```sh
tests/e2e/provision-linux.sh \
  --runtime docker \
  --package /path/to/edit-atlas-X.Y.Z-linux-x86_64.deb \
  --media-fixture-dir build/e2e/media-fixtures
```

The default runner image matches CI's Ubuntu 24.04 environment. A package built
against a newer glibc may not run there; use the compatible CI package, or set
`--base-image` to a matching userspace for a local-only check. The provisioner
pulls the content-addressed runner image and builds it only when no matching
published image exists.

For an already prepared environment, install the DEB and run:

```sh
dbus-run-session -- xvfb-run --auto-servernum \
  --server-args="-screen 0 1440x1000x24 -nolisten tcp" \
  tests/e2e/run-linux.sh \
  --app /usr/bin/edit-atlas \
  --cli /usr/bin/edit-atlas-cli
```

The Linux adapter requires AT-SPI and uses stable accessibility identifiers
and actions. Its only pointer input uses bounds reported by the accessibility
tree for controls whose complete interaction has no AT-SPI action.

## Windows desktop tests

Windows provisioning uses Windows Sandbox and the generated MSI:

```powershell
tests/e2e/provision-windows.ps1 `
  -Msi "C:\packages\edit-atlas-X.Y.Z-windows-x64.msi" `
  -FixtureGenerator `
    "build\debug-x64-windows\tests\integration\media\edit_atlas_e2e_media_fixture_generator.exe"
```

Windows Sandbox requires Windows 10 1903 or newer in Pro, Enterprise, or
Education edition, hardware virtualization, and the
`Containers-DisposableClientVM` feature. The command-line launcher is available
from Windows 11 24H2. Only one sandbox instance may run at a time. The
repository, package, fixture, and artifact paths used by the sandbox must be
local paths rather than UNC paths.

Supply `-MediaFixtureDir` to reuse current fixtures and `-ArtifactDir` to choose
the host result directory. The sandbox writes its result to
`sandbox-exit-code.txt`; the provisioner waits for that file and returns the
same exit code.

When Windows Sandbox is unavailable, `-AllowHostInstall` runs the same harness
on an elevated interactive host:

```powershell
tests/e2e/provision-windows.ps1 `
  -Msi "C:\packages\edit-atlas-X.Y.Z-windows-x64.msi" `
  -MediaFixtureDir "C:\edit-atlas-e2e\media-fixtures" `
  -ArtifactDir "C:\edit-atlas-e2e" `
  -AllowHostInstall
```

Host installation is never selected automatically. It refuses to run when an
Edit Atlas package is already installed because the package under test would
replace it. The harness removes its package and crash-dump settings afterwards;
uv and the selected artifact directory remain.

For an already prepared disposable desktop, install the MSI to a private
directory and run:

```powershell
tests/e2e/run-windows.ps1 `
  -App "C:\path\to\install\bin\edit-atlas.exe" `
  -Cli "C:\path\to\install\bin\edit-atlas-cli.exe"
```

The Windows adapter requires UI Automation. It uses semantic patterns where
they perform the complete interaction and pointer input only at
accessibility-reported bounds for native dialogs, Widgets menus and projection
rows, or a verified fallback. It contains no fixed screen coordinates, image
matching, or fixed sleeps.

## macOS desktop tests

`provision-macos.sh` reports the required environment and exits without making
changes. Packaged GUI automation needs Apple hardware with a persistent Aqua
test session and Accessibility permission granted to the pinned Python
interpreter. GitHub-hosted runners cannot be prepared reliably with that
permission, so the macOS packaged GUI job remains disabled and experimental.

On a prepared host, install the universal PKG and run:

```sh
tests/e2e/run-macos.sh \
  --app /Applications/edit-atlas.app/Contents/MacOS/edit-atlas \
  --cli /Applications/edit-atlas.app/Contents/MacOS/edit-atlas-cli
```

Replacing or moving the automation interpreter, changing its signature, or
upgrading macOS may require Accessibility approval again. The runner fails its
trust preflight instead of skipping tests. Package verification still installs
and launches both macOS application slices without GUI automation.

## Results and cleanup

`--artifact-dir` on Linux, `-ArtifactDir` on Windows, and `build/e2e` by
default contain:

- `reports/` — JUnit XML and a self-contained HTML report;
- `artifacts/` — application logs, accessibility trees, failure screenshots,
  and command transcripts;
- `crash-dumps/` — platform crash reports when available; and
- `output/` — generated workbooks and support bundles.

The harness terminates registered application processes and removes its marked
state root even after failure. Provisioners also uninstall the package and
dispose of their isolated environment. Generated results, the uv virtual
environment, and media fixtures remain under the selected artifact root for
inspection or reuse; remove them when they are no longer needed.

## Maintainer contract

The shared GUI façade exposes user operations rather than toolkit controls.
Widgets and Qt Quick may implement an operation differently, but scenarios must
remain frontend-neutral and use the stable identifiers in
[Accessibility automation](../../docs/accessibility-automation.md).

Use bounded state polling. `--startup-timeout` covers process launch, window
appearance, focus, and initial accessibility enumeration;
`--operation-timeout` covers interactions after readiness. Do not add fixed
sleeps or make a provider accepting an action the verdict; verify the resulting
application state.

Only `conftest.py` and modules under `tests/` may import pytest. Platform
adapters, the semantic application façade, inspectors, polling, and process
management remain runner-independent modules.

## Dependency management

`pyproject.toml` declares the platform-specific automation dependencies and
requires Python 3.12. `uv.lock` pins the complete resolution. Entry points use
`uv run --locked`, so dependency changes require updating and committing both
files. Native accessibility packages are installed by the platform
provisioning scripts.
