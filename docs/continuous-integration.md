# Continuous integration

Edit Atlas separates ordinary validation, reusable package production,
packaged end-to-end testing, documentation, and release publication by
lifecycle. Reusable workflows contain implementation; caller workflows own
triggers and permissions.

## Workflow ownership

| Workflow | Trigger | Responsibility |
| --- | --- | --- |
| `ci.yml` | `master`, pull requests, daily schedule, manual dispatch | Orchestrate ordinary package validation and packaged E2E, then delete transient package-transfer artifacts |
| `build-and-package.yml` | Reusable only | Build and test every supported triplet, package Widgets and Quick, and create universal macOS packages |
| `package-verification.yml` | Reusable only | Install and verify both frontend packages on every supported clean verification system |
| `packaged-e2e.yml` | Reusable only | Install the selected production package and run CLI and graphical E2E on Linux and Windows; retain the disabled macOS implementation |
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

Each native runner performs dependency setup once, builds the shared Debug
configuration, and then builds the Widgets and Quick release configurations
on that same runner. This preserves serialization of vcpkg publication by
triplet without duplicating expensive host setup. Frontend-specific staging,
package names, and verification remain symmetric.

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
The release workflow publishes only the selected production frontend and its
matching Qt and FFmpeg corresponding-source archives, notices, license, and
checksums. After successful publication, it deletes the transfer copies from
the workflow run; the GitHub Release remains the distribution location.

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

Reusable workflow jobs appear with their caller and nested job names in the
GitHub checks interface. After changing a job identity, update the `master`
ruleset to require the replacement checks shown by a successful pull-request
run. Do not require the disabled experimental macOS E2E job.

Keep required checks at the independently actionable matrix-job level. This
preserves separate visibility for operating systems, architectures,
frontends, package verification, and required E2E platforms.
