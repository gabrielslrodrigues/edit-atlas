# Contributing to Edit Atlas

This file records the conventions this repository actually follows. They
apply to every contributor and every coding agent working here, and they
take precedence over a tool's or an agent's default behaviour. Where a
default conflicts with a rule below, follow the rule and say so rather than
applying the default silently.

Building, running, and testing are covered by [README.md](README.md), and
the canonical design documents live under [docs/](docs). This file does not
restate either; it states how changes are shaped, named, and reviewed.

## Branches

Branch names are `<type>/<issue-number>-<slug>`, using the same type
vocabulary as commits: `feat/79-rendered-video-export-input`,
`fix/160-quick-accessibility-e2e`, `chore/174-aggregate-ci-check`.

A standalone issue branches from `master`. An epic branches from `master`
and its child issues branch from the epic, so the epic accumulates its
children and merges to `master` once.

## Commits

A commit written by hand is exactly one Conventional Commit subject line. No
body, no trailers, no attribution or session links, however large or subtle
the change. The explanation belongs in the pull request, which is where it
is read.

```text
fix(e2e): enter repeated file chooser path components
```

Squash-merge commits are the exception, and GitHub composes them: the
subject is the pull-request title followed by `(#N)`, and the body is the
pull-request description. Merging a child pull request into an epic produces
the same shape.

## Pull requests

A description opens with `## Summary`, states what changed and how it was
verified, and closes its issue with `Closes #N`.

Do not hard-wrap it. GitHub renders a single newline inside a paragraph as a
line break, so text wrapped at a column width arrives as ragged short lines.
Write each paragraph and each bullet as one line, however long. This is the
opposite of the rule for Markdown committed to the repository, which does
wrap.

## Continuous integration

`master` is gated by a single required status check rather than a list of
job names, so a renamed or re-matrixed job never needs a ruleset edit.

[docs/continuous-integration.md](docs/continuous-integration.md) is
canonical for that gate, for how a job is made mandatory, and for workflow
ownership, triggers, the release path, and the artifact lifecycle. Read it
before changing a workflow.

## Code conventions

### C++

- Headers use include guards derived from the header's path, in the form
  `EDIT_ATLAS_<PATH>_HPP_`, and never `#pragma once`.
- Empty parameter lists are written `(void)`, not `()`.
- Namespaces are snake case under `edit_atlas::<layer>`; types and
  functions are PascalCase; leaf types are `final`; source and header
  filenames are snake case. A type registered as a creatable QML type is the
  exception and must not be `final`, because Qt's registration derives from
  it; MSVC rejects this while Clang does not notice.
- Qt signals are the exception to function naming: they use Qt's lower camel
  case, as `documentChanged` or `exportFinished`, including property-notify
  signals. This is what `Q_PROPERTY NOTIFY` and QML handler resolution
  expect, and it keeps project signals indistinguishable in style from Qt's
  own. Commands, accessors, slots, and private handlers stay PascalCase, so
  `HandleExportFinished` is a slot and `exportFinished` is the signal it
  reacts to.
- Public declarations carry LLVM-style `///` comments. The first sentence
  is the brief summary; follow it with parameter, return-value, ownership,
  lifetime, and invariant details where they form part of the contract.
- Formatting comes from `.clang-format`: LLVM-based, four-space indent,
  80-column limit, no tabs. There is no separate lint step beyond
  `all_qmllint`, `check_translations`, and compiler warnings under
  `EDIT_ATLAS_WARNINGS_AS_ERRORS`.

### QML

- One component per file, named in PascalCase, four-space indent.
- User-visible text uses `qsTr()`; the Widgets and other `QObject` code uses
  `tr()`. English is the source language and Brazilian Portuguese is the
  default for a new profile. Never persist translated text or use it as a
  format key, template value, or automation identifier.
- Accessibility identifiers are a stable automation contract, not
  decoration. See
  [docs/accessibility-automation.md](docs/accessibility-automation.md)
  before adding or renaming one.

### Translations

English is the source language and
`src/presentation/translations/edit_atlas_pt_BR.ts` is the only catalogue.
A string that is marked for translation but missing from it ships in English
under a Portuguese interface, so the catalogue is regenerated and checked
from the build:

```sh
cmake --build --preset debug-x64-linux --target update_translations
cmake --build --preset debug-x64-linux --target check_translations
```

`update_translations` merges new source strings in as unfinished entries,
which then have to be translated by hand. It records no source locations, so
regenerating stays a diff of translatable text rather than of line numbers.

`check_translations` extracts the same strings into a throwaway catalogue in
the build tree and compares source strings, not file text. It fails on a
string the catalogue is missing, on an entry still unfinished, and on a
context that yields no strings at all — which means `lupdate` stopped reading
a file rather than that a translation went stale. `lupdate` parses QML only
when `qttools` is built with its QML parser, which is why the manifest
requests that port's `qml` feature; without it every `qsTr` string is skipped
in silence.

A non-`QObject` class needs a context of its own, through
`Q_DECLARE_TR_FUNCTIONS`. Calling `tr` on a Qt class instead files the string
under that Qt class's context, where this catalogue has no reason to look for
it.

### Shell and PowerShell

- Shell scripts begin with `#!/usr/bin/env bash` and `set -euo pipefail`,
  indent with two spaces, and validate their argument count before doing
  anything.
- Every script has a PowerShell counterpart where the workflow it serves
  runs on Windows.

### Python

Python appears only in `tests/e2e`. It follows the same 80-column habit as
the C++ sources and uses four-space indentation.

## Tests

Every C++ test carries a `unit` or `integration` CTest label. The end-to-end
suites are deliberately not registered with CTest; see
[tests/e2e/README.md](tests/e2e/README.md) before touching them.

pytest runs under `--strict-config --strict-markers` with
`xfail_strict = true`, and a required suite fails when it collects nothing.

Native integration tests must set `EDIT_ATLAS_TEST_STATE_ROOT` before
constructing application services, and must never point it at a real
developer profile.

## Dependencies and versions

- `cmake/EditAtlasCompilerSupport.cmake` declares the compiler floors: Clang
  19 and MSVC 19.33 (Visual Studio 2022 17.3). Configuration also verifies
  that the selected standard library provides `std::expected`; Apple Clang is
  governed by that facility check.
- Linux presets and the `x64-linux` triplet select a matched Clang 19-or-newer
  toolchain through `cmake/LinuxClangToolchain.cmake`. Set both
  `EDIT_ATLAS_LINUX_C_COMPILER` and `EDIT_ATLAS_LINUX_CXX_COMPILER`, or neither,
  for a deliberate override. Changing compilers changes vcpkg package ABIs and
  rebuilds affected ports.
- The `version-string` in `vcpkg.json` is the single project version source;
  CMake reads it, and release tags are validated against it.
- The `builtin-baseline` in `vcpkg.json` must equal the checked-out vcpkg
  submodule commit. The corresponding-source scripts enforce this, because
  the archives a release publishes must match the ports it built from.
- Do not propose replacing the vcpkg binary cache backend described in
  [docs/packaging.md](docs/packaging.md) with the GitHub Actions cache. That
  backend produced ABI mismatches here that cost significant rebuild time,
  and the decision against it stands.
- Python end-to-end dependencies are pinned to exact versions with platform
  markers, the interpreter is constrained to a single minor version, and the
  `uv` version is bounded.
- GitHub Actions are pinned to an exact released tag for first-party
  `actions/*` entries and to a full commit SHA for third-party actions.
- The bundled interface typeface is pinned to a release and verified by
  checksum. Updating it means replacing the faces below, updating these
  checksums, and confirming the notices still describe what ships:

  | File | Release | SHA-256 |
  | --- | --- | --- |
  | `Inter-Regular.ttf` | Inter 4.1 | `40d692fce188e4471e2b3cba937be967878f631ad3ebbbdcd587687c7ebe0c82` |
  | `Inter-Medium.ttf` | Inter 4.1 | `97ad806f526e41546d46365bb3a393145f75b7b1568913db74549ad8b8dba872` |
  | `Inter-SemiBold.ttf` | Inter 4.1 | `78a843fade9d4612a5567302fb595b56976eb5fcebf4fea5a5912d638bafcde3` |
  | `LICENSE.txt` | Inter 4.1 | `262481e844521b326f5ecd053e59b98c8b2da78c8ee1bdbb6e8174305e54935a` |

  Faces come from `extras/ttf/` in the official release archive and are
  embedded unmodified. Only upright text weights are bundled: no italics,
  which the interface never uses, and no `InterDisplay` variants, which would
  double the payload for sizes the interface does not set. Subsetting a face
  makes it a Modified Version under the Open Font License, so a subset would
  need its own review even though Inter reserves no font name.

## Text and formatting

`.editorconfig` sets UTF-8, LF line endings, a final newline, trimmed
trailing whitespace, and space indentation for every file in the
repository. Only the indent width varies: four columns for authored source
— C++, CMake, QML, Python — and two for scripts and data or markup formats
— shell, PowerShell, JSON, YAML, and Markdown.

Markdown committed here wraps at roughly 80 columns. Text destined for a web
form — issue and pull-request bodies, review comments — does not wrap at
all.

## Agent configuration

Coding-agent configuration is tracked when it is shared intent and ignored
when it is machine state.

Track what you would review in a pull request: project settings, subagent
and command definitions, and hook scripts. They change how everyone's tools
behave and belong under review like any other configuration.

Ignore anything per-machine or per-session: `*.local.json` settings,
caches, transcripts, logs, and anything holding a token or a path under a
developer's home directory.

[AGENTS.md](AGENTS.md) is the entry point agents read. It points here rather
than restating any of it.
