#!/usr/bin/env bash

set -euo pipefail

# Prints the reference of the Linux E2E runner image for this checkout.
#
# The tag is derived from the inputs that determine the image's contents, so a
# local run and a CI run computing it from the same checkout always name the
# same image, and changing any input names a different one. Nothing has to be
# updated by hand after publishing.
#
# The image holds only the E2E environment. It never contains Edit Atlas, and
# ordinary runs supply the package and fixtures as inputs.

script_directory="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repository_root="$(cd -- "${script_directory}/../../.." && pwd)"

#: Inputs that determine the image's contents. Every repository file the
#: Containerfile copies belongs here, or a change to it would produce the same
#: tag and a stale published image would be pulled in its place. The check
#: below enforces that rather than trusting this list to be maintained.
image_inputs=(
  "${script_directory}/Containerfile"
  "${script_directory}/run-provisioned.sh"
  "${repository_root}/scripts/ci/install-ubuntu-dependencies.sh"
  "${repository_root}/tests/e2e/pyproject.toml"
  "${repository_root}/tests/e2e/uv.lock"
)

repository="${EDIT_ATLAS_E2E_IMAGE_REPOSITORY:-}"
if [[ -z "${repository}" ]]; then
  owner="${GITHUB_REPOSITORY_OWNER:-gabrielslrodrigues}"
  repository="ghcr.io/${owner}/edit-atlas-linux-e2e"
fi

for image_input in "${image_inputs[@]}"; do
  if [[ ! -f "${image_input}" ]]; then
    echo "Image input is missing: ${image_input}" >&2
    exit 1
  fi
done

# A file baked into the image but absent from the inputs above would not
# change the tag when it changed, so a stale published image would be pulled
# instead of a rebuilt one. That is silent and hard to diagnose, so the two
# lists are compared here.
while read -r copied; do
  [[ -n "${copied}" ]] || continue
  for image_input in "${image_inputs[@]}"; do
    if [[ "${image_input#"${repository_root}/"}" == "${copied}" ]]; then
      continue 2
    fi
  done
  echo "The Containerfile copies ${copied}, which does not determine the" >&2
  echo "image tag. Add it to image_inputs in $(basename "${BASH_SOURCE[0]}")." >&2
  exit 1
done < <(
  awk '$1 == "COPY" && $2 !~ /^--/ {
    for (field = 2; field < NF; field++) print $field
  }' "${script_directory}/Containerfile"
)

# An input that determines the tag but does not trigger the publish workflow
# names an image that was never built, so those two lists are kept in step
# here as well.
publish_workflow="${repository_root}/.github/workflows/e2e-runner-image.yml"
publish_paths="$(
  awk '/^ *paths:/ { collecting = 1; next }
       collecting && $1 == "-" { print $2; next }
       collecting { collecting = 0 }' "${publish_workflow}"
)"
for image_input in "${image_inputs[@]}"; do
  relative="${image_input#"${repository_root}/"}"
  if ! printf '%s\n' "${publish_paths}" | grep -qxF -- "${relative}"; then
    echo "${relative} determines the image tag but does not trigger" >&2
    echo "$(basename "${publish_workflow}"), so its image would never be" >&2
    echo "published. Add it to that workflow's paths." >&2
    exit 1
  fi
done

# Base image overrides produce their own tag, so an image built on a different
# userspace can never be mistaken for the published default.
base_image="${EDIT_ATLAS_E2E_BASE_IMAGE:-default}"

tag="$(
  {
    printf '%s\n' "${base_image}"
    # Hashed by content, and by name so that reordering or renaming an input
    # is a change. Paths are relative so the digest does not depend on where
    # the repository is checked out.
    for image_input in "${image_inputs[@]}"; do
      printf '%s\n' "${image_input#"${repository_root}/"}"
      sha256sum -- "${image_input}" | cut -d' ' -f1
    done
  } | sha256sum | cut -c1-32
)"

printf '%s:%s\n' "${repository}" "${tag}"
