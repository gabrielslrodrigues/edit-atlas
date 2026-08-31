# Continuous integration

Edit Atlas separates ordinary validation, reusable package production,
packaged end-to-end testing, documentation, and release publication by
lifecycle. Reusable workflows contain implementation; caller workflows own
triggers and permissions.

## Workflow ownership

| Workflow | Trigger | Responsibility |
| --- | --- | --- |
| `ci.yml` | `master`, pull requests, daily schedule, manual dispatch | Orchestrate ordinary package validation and packaged E2E, report the single required gate, then delete transient package-transfer artifacts |
| `build-and-package.yml` | Reusable only | Build and test every supported triplet, package Widgets and Quick, and create universal macOS packages |
| `package-verification.yml` | Reusable only | Install and verify both frontend packages on every supported clean verification system |
| `packaged-e2e.yml` | Reusable only | Install the Qt Quick production package and run CLI and graphical E2E on Linux and Windows; retain the disabled macOS implementation |
| `release.yml` | Version tags | Validate the tag, create the protected draft, prepare corresponding source, reuse package and E2E validation, publish release assets, and publish versioned documentation |
| `documentation.yml` | `master`, pull requests, manual dispatch, reusable call | Validate documentation, publish `/latest/`, and publish release documentation under `/vX.Y.Z/` |

The ordinary and release paths share the same package and E2E implementations:

```text
build-and-package.yml ─┬─> package-verification.yml ─┐
                       └─> packaged-e2e.yml ─────────┤
                                                    └─> completion

ci.yml ───────> completion ─> artifact cleanup
release.yml ──> completion ─> release publication ─> documentation.yml
       └──────> corresponding source ──────────┘
```

Package verification and packaged E2E consume the same artifacts concurrently.
Neither waits for the other, and neither rebuilds or repackages the application.

Do not copy package or E2E jobs into a caller. Add a reusable-workflow input
or matrix field when orchestration needs another supported variant. Keep
operating-system differences in matrix data when the commands are equivalent,
and use a script under `scripts/ci` when a platform requires a substantial
different procedure.

## Package matrix

`build-and-package.yml` owns one native build matrix:

- Linux x64;
- macOS ARM64;
- macOS x64;
- Windows x64.

Each native runner performs dependency setup once and builds the shared Debug
configuration. It then configures, builds, and tests one generic Release tree
containing both graphical frontends. Thin package-only application targets
reuse those frontend libraries while providing separate product names,
installation components, and Qt deployment scripts. CI combines each
frontend component with the shared runtime component when staging or
packaging it. Shared Release code is therefore compiled once per native
triplet while frontend-specific staging, package names, and verification
remain symmetric.

The two macOS jobs upload native staged bundles. One universal-packaging job
combines the matching ARM64 and x64 bundles for both frontends, uploads both
PKGs, and immediately deletes the native staging artifacts. Linux and Windows
packages are uploaded alongside them for concurrent clean-machine package
verification and packaged E2E consumption.

## Artifact lifecycle

Application packages and native macOS stages are inter-job transfer artifacts,
not public development releases. Ordinary CI gives them one-day fallback
retention and deletes them after package and E2E validation. E2E reports,
crash dumps, installer logs, and build diagnostics remain available for seven
days because they do not distribute a runnable Edit Atlas package.

Release runs retain transfer artifacts for up to three days as recovery data.
The release workflow publishes only the Qt Quick production frontend and its
matching Qt and FFmpeg corresponding-source archives, notices, license, and
checksums. Widgets packages remain CI verification artifacts rather than
release assets. After successful publication, the workflow deletes the
transfer copies from the workflow run; the GitHub Release remains the
distribution location.

The vcpkg NuGet feed is a dependency binary cache rather than a workflow
artifact. Package jobs retain the existing per-triplet concurrency groups so
the same triplet is serialized while different triplets run concurrently.

## Permissions and publication boundary

The reusable workflows default to read-only repository access. Callers grant
`packages: write` only to package production and `actions: write` only where
transient artifacts must be deleted. Ordinary CI has no release or Pages
permission.

Only `release.yml` responds to version tags. Its draft-release job uses the
protected `edit-atlas-release` environment, and only its publication job can
write release contents. Versioned documentation runs only after the draft has
been successfully published. Pull-request and branch workflows therefore
cannot create releases or publish versioned documentation.

## Required status checks

`master` requires exactly one status check, `CI gate`. Nothing else belongs
in the ruleset.

The gate job in `ci.yml` depends on package production, package
verification, and packaged E2E, runs with `if: always()`, and fails unless
every dependency concluded `success`. Making a job mandatory therefore means
adding it to the gate's `needs:` list, which is reviewed with the change that
introduces it. Renaming, re-matrixing, or splitting a job needs no ruleset
edit, and a required context that nothing reports can no longer strand a pull
request at "Expected — Waiting for status to be reported".

The comparison is against `success` rather than a tolerance for `skipped`,
because a job that a failed dependency prevented from running concludes
`skipped` and would otherwise pass the gate. A job that a reusable workflow
skips through its own `if:` condition, such as the disabled experimental
macOS E2E job, does not change its caller's result and needs no exception.
A job that does not run on pull requests must stay out of the gate, or every
pull request fails.

Artifact cleanup is deliberately ungated. It runs even when validation
fails, and it reports housekeeping rather than a quality signal.

Reusable workflow jobs still appear individually, with their caller and
nested job names, in the GitHub checks interface. This preserves separate
visibility for operating systems, architectures, frontends, package
verification, and E2E platforms, so a failure stays attributable without
opening the run.
