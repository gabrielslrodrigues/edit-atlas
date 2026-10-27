#!/usr/bin/env bash

set -euo pipefail

# Use the caller's release tag, or synthesize a final tag for a dry run.

if (( $# > 2 )); then
  echo "Usage: $0 [output-file [release-tag]]" >&2
  exit 2
fi

project_version="$({
  sed -nE 's/^[[:space:]]*"version-string":[[:space:]]*"([0-9]+\.[0-9]+\.[0-9]+)",?$/\1/p' \
    vcpkg.json
} | head -n 1)"

if [[ -z "$project_version" ]]; then
  echo "Could not determine the project version from vcpkg.json." >&2
  exit 1
fi

# The manifest baseline is deliberately not checked here. It is only
# meaningful against a checked-out vcpkg submodule, which this step does not
# need, and the corresponding-source scripts already enforce the match in the
# jobs that do check the submodule out.

candidate_tag="${2:-v$project_version}"
if [[ ! "$candidate_tag" =~ ^v[0-9]+\.[0-9]+\.[0-9]+(-rc\.[1-9][0-9]*)?$ ||
  "${candidate_tag%%-rc.*}" != "v$project_version" ]]; then
  echo "Invalid release tag for version $project_version: $candidate_tag" >&2
  exit 1
fi
candidate_version="${candidate_tag#v}"
prerelease_suffix="${candidate_version#"$project_version"}"

commit="$(git rev-parse HEAD)"

echo "Release candidate $candidate_tag at $commit." >&2
if [[ -z "${2:-}" ]] &&
  git rev-parse -q --verify "refs/tags/$candidate_tag" >/dev/null; then
  printf 'Note: tag %s already exists; this run publishes nothing.\n' \
    "$candidate_tag" >&2
fi

output="${1:-/dev/stdout}"
{
  echo "version=$candidate_version"
  echo "prerelease_suffix=$prerelease_suffix"
  echo "tag=$candidate_tag"
  echo "commit=$commit"
} >> "$output"
