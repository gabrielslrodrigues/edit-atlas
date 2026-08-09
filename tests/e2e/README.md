# End-to-end tests

This directory contains black-box pytest scenarios that exercise installed
Edit Atlas packages. Pytest is only the runner layer: platform adapters,
semantic application operations, polling, process management, and archive
inspectors are ordinary Python modules that can be reused by another runner.

E2E tests are not registered with CTest. They must use the public GUI or command
line, keep user state isolated, use bounded polling instead of fixed sleeps, and
provide reproducible entry points that place generated environments and output
under `build/`.

## Layout

- `adapters/` contains platform adapters and deterministic process control.
- `application/` exposes semantic user operations and bounded state polling.
- `inspectors/` validates XLSX and support-bundle ZIP/XML contents.
- `tests/` contains thin black-box scenarios.
- `conftest.py` owns pytest options, fixtures, teardown, and strict outcomes.

Only `conftest.py` and files under `tests/` may import pytest.

## Running packaged tests

The entry points use uv and the committed lockfile to create a pinned Python
3.12 virtual environment at `build/e2e/venv`. They write JUnit XML, a
self-contained HTML report, command transcripts, outputs, and future GUI
failure artifacts beneath `build/e2e`. Install a compatible uv 0.12 release
before using either entry point.

Packaged CLI tests on Linux and macOS:

```sh
tests/e2e/run.sh --cli /usr/bin/edit-atlas-cli
```

Windows PowerShell:

```powershell
tests/e2e/run.ps1 -Cli "C:\Program Files\Edit Atlas\edit-atlas-cli.exe"
```

The CLI path must refer to the executable installed by the package under test,
not a build-tree target. The entry points pass the fixture directory, output
directory, isolated state root, locale, operation timeout, and artifact
directory as explicit pytest options. Additional pytest arguments may follow
the CLI option.

Required scenarios are intentionally strict. A run fails if no tests are
collected or if a required test is deselected, skipped, or unexpectedly
xfails/xpasses. Teardown terminates registered processes and removes the
isolated state root even after a failure. Generated outputs and diagnostics are
retained for inspection.

### Linux desktop tests

The required Linux suite drives the installed GUI with dogtail over AT-SPI in
an X11 and D-Bus session. It uses semantic accessibility identifiers, actions,
selection, and editable-text interfaces only. It never uses pointer
coordinates, image matching, or fixed sleeps.

On Ubuntu 24.04, install the native accessibility and Python build
dependencies used by the pinned PyGObject package:

```sh
sudo apt-get install \
  at-spi2-core dbus-x11 gir1.2-atspi-2.0 libgirepository1.0-dev \
  libcairo2-dev pkg-config python3-dev xauth x11-apps xvfb
```

After installing the generated DEB, run the complete required Linux suite in a
deterministic desktop session:

```sh
dbus-run-session -- xvfb-run --auto-servernum \
  --server-args="-screen 0 1440x1000x24 -nolisten tcp" \
  tests/e2e/run-linux.sh \
  --app /usr/bin/edit-atlas \
  --cli /usr/bin/edit-atlas-cli
```

The paths must point to executables installed by the DEB. The Linux runner
performs an explicit backend preflight, and a missing AT-SPI backend is an
error. Tests run serially with bounded state polling. Accessibility-tree dumps,
application logs, XWD screenshots when available, generated workbooks, the
support bundle, and pytest reports are written below `build/e2e`.

## Dependency management

`pyproject.toml` declares pytest, HTML reporting, pywinauto on Windows, dogtail
and its PyGObject/PyCairo bridge on Linux, and the macOS ApplicationServices
PyObjC bridge. `uv.lock` pins the complete cross-platform resolution. The entry
points use `uv run --locked`, so an outdated or missing lockfile is an error
rather than an implicit dependency update. Native packages needed by a
platform accessibility backend are installed by that platform's CI job rather
than by uv.
