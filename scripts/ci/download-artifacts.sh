#!/usr/bin/env bash
set -euo pipefail

if (( $# != 0 )); then
  echo "Usage: $0 (configured through ARTIFACT_* environment variables)" >&2
  exit 2
fi

if [[ -n "${ARTIFACT_NAME}" && -z "${ARTIFACT_PATTERN}" ]]; then
  selector=(--name "${ARTIFACT_NAME}")
elif [[ -n "${ARTIFACT_PATTERN}" && -z "${ARTIFACT_NAME}" ]]; then
  selector=(--pattern "${ARTIFACT_PATTERN}")
else
  echo 'Specify exactly one artifact name or pattern' >&2
  exit 2
fi
gh run download "${GITHUB_RUN_ID}" --repo "${GITHUB_REPOSITORY}" \
  --dir "${ARTIFACT_DIRECTORY}" "${selector[@]}"
