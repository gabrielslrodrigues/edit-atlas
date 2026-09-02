#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'EOF'
Usage: provision-linux.sh --package DEB
       (--fixture-generator PATH | --media-fixture-dir PATH)
       [--artifact-dir PATH] [--runtime podman|docker]
       [--base-image REFERENCE]
       [-- PYTEST_ARGUMENT...]
EOF
}

script_directory="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repository_root="$(cd -- "${script_directory}/../.." && pwd)"
package_path=""
fixture_generator=""
media_fixture_directory=""
artifact_directory="${repository_root}/build/e2e"
container_runtime=""
base_image=""
pytest_arguments=()

while (( $# > 0 )); do
  case "$1" in
    --package|--fixture-generator|--media-fixture-dir|--artifact-dir|\
    --runtime|--base-image)
      if (( $# < 2 )); then
        usage
        exit 2
      fi
      option="$1"
      value="$2"
      shift 2
      case "${option}" in
        --package) package_path="${value}" ;;
        --fixture-generator) fixture_generator="${value}" ;;
        --media-fixture-dir) media_fixture_directory="${value}" ;;
        --artifact-dir) artifact_directory="${value}" ;;
        --runtime) container_runtime="${value}" ;;
        --base-image) base_image="${value}" ;;
      esac
      ;;
    --)
      shift
      pytest_arguments=("$@")
      break
      ;;
    *)
      usage
      exit 2
      ;;
  esac
done

if [[ -z "${package_path}" ]]; then
  usage
  exit 2
fi
if [[ -n "${fixture_generator}" && -n "${media_fixture_directory}" ]] || \
   [[ -z "${fixture_generator}" && -z "${media_fixture_directory}" ]]; then
  echo "Supply exactly one fixture source" >&2
  exit 2
fi

if [[ -z "${container_runtime}" ]]; then
  if command -v podman >/dev/null 2>&1; then
    container_runtime="podman"
  elif command -v docker >/dev/null 2>&1; then
    container_runtime="docker"
  else
    echo "Podman or Docker is required for Linux E2E provisioning" >&2
    exit 127
  fi
elif [[ "${container_runtime}" != "podman" && \
        "${container_runtime}" != "docker" ]]; then
  echo "Container runtime must be podman or docker" >&2
  exit 2
elif ! command -v "${container_runtime}" >/dev/null 2>&1; then
  echo "Requested container runtime is unavailable: ${container_runtime}" >&2
  exit 127
fi

if [[ ! -f "${package_path}" ]]; then
  echo "DEB package does not exist: ${package_path}" >&2
  exit 2
fi
package_path="$(realpath -- "${package_path}")"

mkdir -p "${artifact_directory}"
artifact_directory="$(realpath -- "${artifact_directory}")"

# Everything under test lives beneath one home directory, mirroring CI, where
# the checkout sits under the runner's home. File dialogs open at the home
# directory, so a layout that puts the sources somewhere unrelated exercises
# navigation paths CI never reaches. The paths are fixed rather than mirroring
# the host, so every machine presents the same layout.
container_home="/home/edit-atlas-e2e"
container_source="${container_home}/source"
container_results="${container_home}/results"
container_fixture_directory="${container_home}/media-fixtures"
container_package="${container_home}/edit-atlas.deb"

container_arguments=(
  run
  --rm
  --init
  --platform linux/amd64
  --volume "${repository_root}:${container_source}:ro"
  --volume "${artifact_directory}:${container_results}:rw"
  --volume "${package_path}:${container_package}:ro"
)

if [[ "${container_runtime}" == "podman" ]]; then
  container_arguments+=(--security-opt label=disable)
  if (( EUID != 0 )); then
    container_arguments+=(--userns keep-id --user 0:0)
  fi
fi

provision_arguments=(
  --repository-root "${container_source}"
  --artifact-root "${container_results}"
  --package "${container_package}"
  --home "${container_home}"
  --host-uid "$(id -u)"
  --host-gid "$(id -g)"
)

if [[ -n "${fixture_generator}" ]]; then
  if [[ ! -x "${fixture_generator}" ]]; then
    echo "Fixture generator is not executable: ${fixture_generator}" >&2
    exit 2
  fi
  # Generated on the host rather than inside the container. The generator is a
  # build-tree binary linked against the host's toolchain and vcpkg libraries,
  # so running it in the image would require the host and the image to share an
  # ABI; a host with a newer glibc than the pinned Ubuntu base cannot satisfy
  # that. The fixtures it writes are ordinary data and travel in fine.
  media_fixture_directory="${artifact_directory}/media-fixtures"
  mkdir -p -- "${media_fixture_directory}"
  "${repository_root}/tests/e2e/generate-media-fixtures.sh" \
    "${fixture_generator}" \
    "${media_fixture_directory}"
elif [[ ! -d "${media_fixture_directory}" ]]; then
  echo "Media-fixture directory does not exist: ${media_fixture_directory}" >&2
  exit 2
fi

media_fixture_directory="$(realpath -- "${media_fixture_directory}")"
container_arguments+=(
  --volume "${media_fixture_directory}:${container_fixture_directory}:ro"
)
provision_arguments+=(
  --media-fixture-dir "${container_fixture_directory}"
)

build_arguments=()
image="edit-atlas-linux-e2e:local"
if [[ -n "${base_image}" ]]; then
  # Tagged per base so an overridden image never reuses the default's layers,
  # and so both can coexist without rebuilding one another.
  image="edit-atlas-linux-e2e:local-$(
    printf '%s' "${base_image}" | tr -c '[:alnum:]._-' '-'
  )"
  build_arguments+=(
    --build-arg "EDIT_ATLAS_E2E_BASE_IMAGE=${base_image}"
  )
fi

"${container_runtime}" build \
  --file "${script_directory}/linux/Containerfile" \
  --platform linux/amd64 \
  "${build_arguments[@]+"${build_arguments[@]}"}" \
  --tag "${image}" \
  "${repository_root}"

exec "${container_runtime}" "${container_arguments[@]}" \
  "${image}" \
  "${provision_arguments[@]}" \
  -- "${pytest_arguments[@]}"
