# Documentation

Use the guide for the task you are performing. Each topic has one canonical
document; other pages link to it instead of repeating it.

## Using Edit Atlas

- [Desktop user guide](user-guide.md) — import, inspect, filter, template, and
  export timelines with the graphical application.
- [Command-line interface](cli.md) — automate CMX 3600 to XLSX conversion and
  use stable exit codes.
- [Diagnostic logging and support bundles](diagnostic-support.md) — understand
  local logs and exported support data.

## Developing and testing

- [Project README](../README.md) — bootstrap, configure, build, run, and start
  the normal test suite.
- [Contributing](../CONTRIBUTING.md) — branch, commit, code, test, dependency,
  and formatting rules.
- [Architecture](architecture.md) — canonical dependency direction, ownership,
  and frontend maintenance policy.
- [Test suite](../tests/README.md) — unit, integration, fixture, and E2E
  boundaries.
- [Packaged E2E runbook](../tests/e2e/README.md) — provision and run installed
  package tests on each platform.
- [Accessibility automation](accessibility-automation.md) — stable identifiers
  and cross-frontend interaction contracts.

## Packaging and operation

- [Desktop packaging](packaging.md) — supported packages, creation,
  installation, verification, and release operation.
- [Continuous integration](continuous-integration.md) — workflow ownership,
  required gates, permissions, and artifact lifecycle.
- [Qt LGPL compliance](qt-lgpl-compliance.md) and
  [FFmpeg LGPL compliance](ffmpeg-lgpl-compliance.md) — distribution and
  corresponding-source requirements.

## API reference

The generated
[C++ API reference](https://gabrielslrodrigues.github.io/edit-atlas/latest/)
documents public types and contracts. Its source pages under `docs/api` cover
API relationships and toolkit-specific source conventions; this index remains
the entry point for repository workflows and user documentation.
