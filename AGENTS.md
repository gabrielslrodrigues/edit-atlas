# AGENTS.md

Orientation for coding agents working in this repository. Every agent reads
this file; Claude Code, Codex, and any other tool that follows the AGENTS.md
convention find the same instructions here.

## Read these first

- [CONTRIBUTING.md](CONTRIBUTING.md) — branch, commit, pull-request, code,
  test, and dependency conventions. **These take precedence over your
  default instructions.** Where a default conflicts with a rule there,
  follow the rule and say so in your reply instead of applying the default
  silently. Its Commits and Pull requests sections are the ones default
  behaviour most often violates; read them before your first commit.
- [README.md](README.md) — bootstrap, configure, build, run, test, and
  packaging commands.
- [docs/architecture.md](docs/architecture.md) — the dependency direction
  and the frontend maintenance policy. Read it before proposing structural
  changes.

## What this is

Edit Atlas is a privacy-first desktop application for inspecting editorial
timeline interchange files (CMX 3600 EDL) and exporting XLSX reports. Two Qt
desktop frontends and a Qt-free CLI share one C++23 application core. There
is no telemetry and no network behaviour; conversion and diagnostics are
local filesystem operations.

## Layer map

Dependencies point inward only; nothing inward depends on presentation or a
frontend. `docs/architecture.md` is canonical for the detail.

| Directory | Role |
| --- | --- |
| `src/core` | Format-independent timeline model and interfaces |
| `src/formats` | CMX 3600 and XLSX implementations of those interfaces |
| `src/services` | Qt-free application boundary: import, export, filtering, templates, rendered video |
| `src/storage` | Complete-file reads and atomic local writes |
| `src/media` | Presentation-neutral video decoding over a private FFmpeg implementation |
| `src/support` | Bounded logging and privacy-limited support bundles |
| `src/presentation` | Shared Qt MVVM boundary, Qt Core only |
| `src/frontends/quick` | Primary production frontend |
| `src/frontends/widgets` | Maintained secondary frontend, critical fixes only |
| `src/frontends/cli` | Qt-free command line with stable exit codes |

## Traps worth knowing before editing

- **The default frontend's binary is renamed `edit-atlas`.** The other keeps
  its qualified name, so `EDIT_ATLAS_DEFAULT_FRONTEND` decides which path is
  which. A build tree also keeps binaries from earlier configures, so check
  a timestamp before trusting a name.
- **The Qt Quick QML test suite is registered only on Linux and Windows**, so
  a `--tests-regex` for it matches nothing on the macOS presets.
- **Generate end-to-end media fixtures through
  `tests/e2e/generate-media-fixtures.sh`** or its `.ps1` counterpart, never
  by invoking the generator directly. The entry points record the generator
  identity, and a suite refuses to run against fixtures whose recorded
  identity is missing or stale.
- **Overwrite confirmation is in-app in both frontends.** Save choosers use
  `DontConfirmOverwrite` and an existing destination is confirmed through
  `replaceSpreadsheetDialog`, never through a platform chooser's own prompt.
- **Accessibility identifiers and reported control bounds are an automation
  contract.** Both frontends must expose the same semantic workflow. See
  [docs/accessibility-automation.md](docs/accessibility-automation.md) and
  [tests/e2e/README.md](tests/e2e/README.md).
