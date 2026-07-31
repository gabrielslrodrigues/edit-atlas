#!/usr/bin/env bash

set -euo pipefail

tag="${GITHUB_REF_NAME:-}"
commit="${GITHUB_SHA:-}"

if [[ ! "$tag" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Release tags must use the vX.Y.Z form: '$tag'" >&2
  exit 1
fi

project_version="$({
  sed -nE 's/^[[:space:]]*VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*$/\1/p' \
    CMakeLists.txt
} | head -n 1)"
if [[ -z "$project_version" ]]; then
  echo "Could not determine the project version from CMakeLists.txt." >&2
  exit 1
fi

expected_tag="v$project_version"
if [[ "$tag" != "$expected_tag" ]]; then
  echo "Tag '$tag' does not match project version '$expected_tag'." >&2
  exit 1
fi

tag_commit="$(git rev-list -n 1 "$tag^{commit}")"
if [[ -n "$commit" && "$tag_commit" != "$commit" ]]; then
  echo "Tag '$tag' does not point at the checked-out commit." >&2
  exit 1
fi

if git verify-tag "$tag" >/dev/null 2>&1; then
  echo "Validated signed release tag $tag."
else
  echo "Validated unsigned release tag $tag; the protected release environment provides explicit approval."
fi

