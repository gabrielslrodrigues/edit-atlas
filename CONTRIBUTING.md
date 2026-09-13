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

### Standards and precedence

Apply each applicable guide in full: language usage, design, ownership,
error handling, interfaces, imports, documentation, formatting, and naming.
Custom rules below override guides and tool defaults. Any uncertainty about
meaning, applicability, or precedence requires asking the owner before the
affected change; independent work with clear rules may continue.

### C++

Follow the full [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
with these custom rules:

- Use C++23 and snake_case `.cpp`/`.hpp` filenames.
- Permit `<filesystem>` and `std::filesystem` as an explicit exception to
  Google's disallowed standard library features.
- Headers use path-derived `EDIT_ATLAS_<PATH>_HPP_` include guards, never
  `#pragma once`.
- Write `(void)` on named zero-parameter functions, including constructors
  and destructors.
- Mark leaf types `final`. Creatable QML types must remain non-final because
  Qt registration derives from them; preserve framework-required names.
- Use Google's getter/setter naming exception, including accessors without
  matching backing members. A getter need not trivially return a member.
- Qt signals and dedicated receiving slots use lowerCamelCase. Determine
  dedicated-slot status from actual callers. Methods used for other purposes
  retain ordinary naming, even when also connected to a signal.
- Return typed failures for expected domain errors. Bounded exceptions remain
  appropriate for library integration and failure containment; contain them
  at the responsible boundary without losing existing failure categories.
- Public declarations carry `///` API comments describing the contract,
  including ownership, lifetime, and invariants where relevant.
- Include What You Use applies with no corresponding-header exception: a
  `.cpp` includes its own header, but must still directly include the header
  for any other symbol it uses, even one that header happens to pull in
  transitively.

Formatting follows Google's two-space, 80-column style. Formatting and
linting do not establish semantic compliance: review headers, dependencies,
ownership, lifetimes, class design, type usage, concurrency, and exception
safety separately.

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

### Bash

Follow the full [Google Shell Style Guide](https://google.github.io/styleguide/shellguide.html),
including quoting, expansion, pipelines, error handling, naming, functions,
and documentation. Scripts begin with `#!/usr/bin/env bash` and
`set -euo pipefail`, use two-space indentation, and validate argument counts
before other work. Lint with ShellCheck. Provide a PowerShell counterpart
where the workflow runs on Windows.

The guide's ~100-line threshold for switching to a structured language is not
a rewrite trigger here: CI, build, and E2E orchestration scripts stay in Bash
regardless of length, as an explicit project deviation.

### PowerShell

Use Microsoft's approved PascalCase `Verb-Noun` function names and lint with
PSScriptAnalyzer. Use two-space indentation, spaces around binary operators
and after commas, opening braces on the declaration line, and closing braces
on their own lines. Use single-quoted literal strings unless interpolation
or escape processing is needed. Keep lines within 80 columns where practical;
do not split indivisible paths, identifiers, or URLs. Preserve native-command
exit checks and cross-platform counterpart behavior.

### Python

Follow full [PEP 8](https://peps.python.org/pep-0008/): four-space indentation,
79-column code, and 72-column comments and docstrings, including imports,
naming, documentation, and programming recommendations. Use Ruff formatting
and linting; review rules the tools do not enforce. The standard applies to
utilities and embedded Python as well as the packaged E2E suites.

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

`.gitattributes` keeps authored text at LF on every checkout, including
Windows, and exempts byte-sensitive files from conversion.
`.editorconfig` sets UTF-8, LF line endings, a final newline, trimmed
trailing whitespace, and space indentation for every file in the
repository. Only the indent width varies: four columns for authored source
— CMake, QML, Python — and two for scripts and data or markup formats
— C++, shell, PowerShell, JSON, YAML, and Markdown.

Markdown committed here wraps at roughly 80 columns. Text destined for a web
form — issue and pull-request bodies, review comments — does not wrap at
all.

## Style commands

Style tooling does not require configuring or building the application.
Bootstrap its pinned dependencies with uv >=0.12.3,<0.13:

```sh
cmake -P cmake/BootstrapStyle.cmake
```

Bootstrap creates `tools/style/.venv` with Python 3.12 and the exact versions
in `tools/style/requirements.txt`. On Windows it also saves PSScriptAnalyzer
1.24.0 under `tools/style/modules`. These directories are ignored machine
state. Installation is explicit; checking never installs missing tools.

```sh
cmake -P cmake/Format.cmake
cmake -P cmake/CheckStyle.cmake
```

Formatting applies clang-format to C++, Ruff to Python, and PSScriptAnalyzer
formatting to PowerShell on Windows. Bash, QML, other authored formats, and
embedded code require manual corrections where no formatter is selected.
Checking also runs cpplint, Ruff lint, ShellCheck on Linux/macOS, and
PSScriptAnalyzer on Windows. Use both CI platforms for complete tool coverage.
Both jobs also run `cmake -P cmake/TestStyle.cmake` to exercise formatter
idempotence, read-only checking, inventory coverage, and representative
violations in disposable repositories. Both jobs are required by `CI gate`.
Missing tools, incorrect pinned package versions, and unknown file
classifications fail the check.

The inventory includes tracked files, dotfiles, and unignored additions and
never descends into the vcpkg gitlink. Licence texts, binary assets, parser
fixtures, dependency locks, and the upstream patch retain their byte
representation. The inventory reports each reason. Change generated content
through its established inputs and commands. Text checks enforce UTF-8 without
a BOM, LF endings, a final newline, no tabs, and no trailing whitespace on
other files. Header guards follow logical public include paths; private
headers use repository-relative paths with the leading `src/` removed.

QML linting and translation checks remain the existing `all_qmllint` and
`check_translations` build targets. Standalone style checks do not replace
those targets, compiler diagnostics, or behavioral verification. Embedded
scripts and documentation examples require the same language review as
standalone code, including quoting across their enclosing format.

Semantic review must cover every applicable guide section, particularly
ownership, lifetimes, interfaces, imports, class design, initialization,
integer conversions, concurrency, failure containment, and naming exceptions.
Formatter success is not a compliance claim. Build, lint, test, and installation
commands remain subject to the current-handover authorization rules.
The [migration audit](docs/standards-audit.md) records guide-section coverage
and outstanding review separately from tool execution.

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
