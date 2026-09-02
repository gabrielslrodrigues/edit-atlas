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

#: Inputs that determine the image's contents.
image_inputs=(
  "${script_directory}/Containerfile"
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
