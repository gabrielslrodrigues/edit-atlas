# Continuous integration

Edit Atlas separates ordinary validation, reusable package production,
packaged end-to-end testing, documentation, and release publication by
lifecycle. Reusable workflows contain implementation; caller workflows own
triggers and permissions.

## Workflow ownership

| Workflow | Trigger | Responsibility |
| --- | --- | --- |
| `ci.yml` | `master`, pull requests, daily schedule, manual dispatch | Orchestrate ordinary package validation and packaged E2E, add the non-production frontend's E2E off the pull-request path, report the single required gate, then delete transient package-transfer artifacts |
| `build-and-package.yml` | Reusable only | Build and test every supported triplet, package Widgets and Quick, and create universal macOS packages |
| `package-verification.yml` | Reusable only | Install and verify both frontend packages on every supported clean verification system |
| `packaged-e2e.yml` | Reusable only | Install the Qt Quick production package and run CLI and graphical E2E on Linux and Windows; retain the disabled macOS implementation |
| `e2e-runner-image.yml` | `master` when its inputs change, reusable call, manual dispatch | Publish the Linux packaged E2E runner image, which contains the test environment and never the application |
| `release-candidate.yml` | Reusable only | Resolve the candidate version, generate corresponding source, build and package every triplet, then assemble and verify the release assets |
| `release-dry-run.yml` | Manual dispatch | Rehearse a release by running the candidate pipeline and publishing nothing |
| `release.yml` | Version tags | Validate the tag, create the protected draft, prepare corresponding source, reuse package and E2E validation, publish release assets, and publish versioned documentation |
| `documentation.yml` | `master`, pull requests, manual dispatch, reusable call | Validate documentation, publish `/latest/`, and publish release documentation under `/vX.Y.Z/` |

The ordinary and release paths share the same package and E2E implementations:

```text
build-and-package.yml ─┬─> package-verification.yml ─┐
                       └─> packaged-e2e.yml ─────────┤
                                                    └─> completion

ci.yml ──────────────> completion ─> artifact cleanup

release-candidate.yml: corresponding source + build-and-package.yml
                       ─> assemble ─> verify ─> assets

release-dry-run.yml ─> release-candidate.yml ─> inspectable assets
release.yml ─────────> release-candidate.yml ─> completion
                       ─> release publication ─> documentation.yml
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

## Precompiled-header boundaries

Selected C++ targets use private precompiled headers to reduce native CI build
time. The selection is deliberately narrow: a target must repeatedly parse a
stable Qt or GoogleTest dependency across enough translation units to recover
the cost of creating its own PCH. Source files must still include every header
they use directly.

The evaluation used the Linux `debug-x64-linux` and `release-x64-linux`
presets with Clang 22.1.6 and Ninja 1.13.2. Dependency installation and CMake
configuration were excluded. Clean Debug builds were alternated between
otherwise identical trees at four parallel jobs, matching the CPU count of the
hosted Linux runner. Release used the same clean-build method. Times are wall
clock seconds:

| Configuration | PCH disabled | PCH enabled | Improvement |
| --- | ---: | ---: | ---: |
| Debug clean build, median of 3 | 88.58 s | 62.70 s | 29.2% |
| Release clean build | 96.35 s | 69.03 s | 28.4% |

Representative one-source Debug rebuilds were also measured three times on a
24-core developer system. Median presentation, Widgets, Quick, and
presentation-test rebuilds improved from 2.74, 2.65, 2.65, and 2.54 seconds to
1.31, 0.90, 1.15, and 1.45 seconds respectively. These local results confirm
that PCH generation does not trade CI savings for slower ordinary edits; the
four-core clean measurements above are the primary selection criterion.

PCHs are enabled for these target groups:

- shared presentation, for its repeated Qt Core and common standard-library
  parsing;
- the Widgets frontend library, for repeated Qt Widgets and common
  standard-library parsing;
- the Quick frontend library, for repeated Qt QML-facing and common
  standard-library parsing;
- the four largest GoogleTest executables: core unit tests, presentation unit
  tests, services integration tests, and Quick frontend integration tests.

The GoogleTest targets each compile their own PCH; none reuse a PCH from a
target with a different compile context. Ninja action timings for the selected
targets fell between 29% and 64%. Smaller test executables, thin application
executables, and the small Quick style target do not contain enough C++ source
files to justify another PCH. Core, formats, media, services, storage, support,
and the CLI remain PCH-free.

CMake's standard switch provides the self-contained-source control build:

```sh
cmake --preset debug-x64-linux \
  -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON
cmake --build --preset debug-x64-linux
```

The evaluation completed clean Debug and Release builds with that switch, so
PCH availability does not replace direct includes.

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

## Release dry run

Tag-only jobs generate the Qt and FFmpeg corresponding-source archives and
assemble the final asset set. Creating a production version tag is not an
acceptable way to test them, so `release-dry-run.yml` rehearses a release on
demand and publishes nothing.

The rehearsal is faithful because both paths call the same pipeline.
`release-candidate.yml` owns everything that produces and validates a
candidate: resolving the version, generating both source archives, building
and packaging every triplet, assembling the assets, and verifying them. A
tagged release calls it and then publishes what it produced; the dry run
calls it and stops. There is no second copy of that graph to drift from, and
the dry run exercises the same jobs a release does.

Publication capability is split by structure rather than by condition.
`release-candidate.yml` declares no environment and no job that writes
repository contents, and a reusable workflow can hold no more permission than
its caller grants, so a dry run cannot create a release, write a tag, or
publish documentation regardless of how its jobs are edited later. Only
`release.yml` holds `contents: write` and the protected
`edit-atlas-release` environment, and it is reachable only by pushing a
`v*` tag.

Dispatch the dry run from the Actions interface against the branch to
validate. The candidate version comes from the `version-string` in
`vcpkg.json`, the same single source CMake reads, and the candidate tag is
synthesized from it. No tag needs to exist, and an existing tag of the same
name is reported but never touched. The reusable build workflow checks out
the dispatched ref, so validating an arbitrary commit means pointing a
branch at it first.

`verify-release-assets.sh` runs in both paths. It requires the exact
expected asset set, rejects a filename carrying another version, checks that
the notices name Qt and FFmpeg, confirms both source archives are readable,
and verifies every checksum against the file beside it. A tagged release
therefore fails before publishing rather than after, and the bytes it
uploads are the bytes that were verified, because publication downloads the
assembled artifact instead of rebuilding it.

The dry run keeps its assets as `edit-atlas-release-dry-run-candidate` for
inspection and deletes the transfer artifacts. It does not run package
verification or packaged E2E: ordinary CI covers both on the same commit, so
repeating them would double a costly dispatch without adding signal.

## Frontend coverage

Packaged E2E runs against the frontend production ships on every merge-path
run. Every other supported frontend is verified once the change is on
`master`, on the daily schedule, and on a manual run that sets the
`other_frontend_e2e` dispatch input. Pull requests never execute them and
take no longer than before. Master pushes pay no build cost, because every
run packages every frontend regardless, and almost no wall clock, because
those jobs run beside the production frontend's.

Which frontend is which is not written into CI. The `frontends` job derives
the production frontend from `EDIT_ATLAS_DEFAULT_FRONTEND` in
`CMakeLists.txt` and emits the rest of the supported list as the frontends to
schedule. Changing the default swaps which frontend the merge path exercises
and which the schedule does, with no workflow edit, and no frontend is ever
exercised twice in one run. Those jobs are a matrix over that list, so a
third supported frontend needs no workflow change either.

They reuse the packages the run already produced and the same reusable
workflow, selected through its `frontend` input, so scenarios, strictness,
and artifacts match the merge-path jobs. Job names and result artifacts carry
the frontend. That input is required: no caller can inherit a frontend by
omission.

The release path derives the frontend the same way. `release-candidate.yml`
resolves it once, assembles that frontend's packages, and exposes it to its
callers, so a tagged release publishes and end-to-end tests whatever
production ships rather than a toolkit named in an artifact pattern.

Building and verifying packages is deliberately not derived: those jobs cover
every supported frontend, because a frontend that is not shipped today is
still expected to compile, install, and launch.

These jobs are deliberately outside the gate. A job that does not run on
pull requests would leave a required context unreported on every one of
them.

A red run in a workflow list is not a signal, so
`scripts/ci/report-frontend-e2e-status.sh` records the outcome on a tracking
issue per frontend: a failure opens one, or comments on the one already open,
naming the run, the commit, and the failing scenarios per platform. A later
run that passes comments and closes it, from the merge path or the schedule.
A pass with no open issue records nothing, and a cancelled run records
nothing. The issue is found again by its exact title, so that title is a
lookup key: changing it orphans whatever a previous run raised.

Each kind of trigger holds its own concurrency lane, keyed by event and ref.
Superseding still applies within a lane, so a new push to a branch cancels
the run for the commit it replaced, which is what you want when only the tip
matters. Across lanes nothing is cancelled: a merge to `master` no longer
cancels a nightly partway through, which used to leave a tracking issue open
long after the run that would have closed it, and a deliberate dispatch no
longer cancels the merge-path run for the same commit.

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
