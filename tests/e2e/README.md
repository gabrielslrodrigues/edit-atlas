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

## Running packaged CLI tests

The entry points use uv and the committed lockfile to create a pinned Python
3.12 virtual environment at `build/e2e/venv`. They write JUnit XML, a
self-contained HTML report, command transcripts, outputs, and future GUI
failure artifacts beneath `build/e2e`. Install a compatible uv 0.11 release
before using either entry point.

Linux and macOS:

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

## Dependency management

`pyproject.toml` declares pytest, HTML reporting, pywinauto on Windows, dogtail
on Linux, and the macOS ApplicationServices PyObjC bridge. `uv.lock` pins the
complete cross-platform resolution. The entry points use `uv run --locked`, so
an outdated or missing lockfile is an error rather than an implicit dependency
update. Native packages needed by a platform accessibility backend are
installed by that platform's CI job rather than by uv.
