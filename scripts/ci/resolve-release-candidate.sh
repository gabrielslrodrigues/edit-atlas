#!/usr/bin/env bash

set -euo pipefail

# Derives the release candidate version from the checked-out manifest, which
# is the single source of truth CMake also reads. A dry run has no tag to
# validate against, so the candidate tag is synthesized from that version and
# handed to the same release scripts a tagged run uses.

if (( $# > 1 )); then
  echo "Usage: $0 [output-file]" >&2
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

candidate_tag="v$project_version"
commit="$(git rev-parse HEAD)"

echo "Release candidate $candidate_tag at $commit." >&2
if git rev-parse -q --verify "refs/tags/$candidate_tag" >/dev/null; then
  echo "Note: tag $candidate_tag already exists; this run publishes nothing." >&2
fi

output="${1:-/dev/stdout}"
{
  echo "version=$project_version"
  echo "tag=$candidate_tag"
  echo "commit=$commit"
} >> "$output"
