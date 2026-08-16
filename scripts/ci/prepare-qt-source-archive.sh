#!/usr/bin/env bash

set -euo pipefail

if (( $# != 2 )); then
  echo "Usage: $0 <release-tag> <output-directory>" >&2
  exit 2
fi

release_tag="$1"
output_dir="$2"
release_version="${release_tag#v}"

if [[ ! "$release_tag" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Release tag must use the vX.Y.Z form: $release_tag" >&2
  exit 1
fi

project_version="$({
  sed -nE 's/^[[:space:]]*"version-string":[[:space:]]*"([0-9]+\.[0-9]+\.[0-9]+)",?$/\1/p' \
    vcpkg.json
} | head -n 1)"
if [[ "$release_version" != "$project_version" ]]; then
  echo "Release tag $release_tag does not match project version $project_version." >&2
  exit 1
fi

vcpkg_baseline="$({
  sed -nE 's/^[[:space:]]*"builtin-baseline":[[:space:]]*"([0-9a-f]+)",?$/\1/p' \
    vcpkg.json
} | head -n 1)"
vcpkg_commit="$(git -C vcpkg rev-parse HEAD)"
if [[ -z "$vcpkg_baseline" || "$vcpkg_commit" != "$vcpkg_baseline" ]]; then
  echo "The vcpkg submodule commit does not match the manifest baseline." >&2
  echo "Manifest:  $vcpkg_baseline" >&2
  echo "Submodule: $vcpkg_commit" >&2
  exit 1
fi

qt_ports=(
  qtbase
  qtdeclarative
  qtlanguageserver
  qtshadertools
  qtsvg
)
declare -A qt_port_versions
declare -A qt_source_filenames
declare -A qt_source_hashes
declare -A qt_source_urls
qt_version=""

for qt_port in "${qt_ports[@]}"; do
  qt_port_dir="vcpkg/ports/$qt_port"
  port_version="$({
    sed -nE \
      's/^[[:space:]]*"version":[[:space:]]*"([^"]+)",?$/\1/p' \
      "$qt_port_dir/vcpkg.json"
  } | head -n 1)"
  qt_port_versions["$qt_port"]="$({
    sed -nE \
      's/^[[:space:]]*"port-version":[[:space:]]*([0-9]+),?$/\1/p' \
      "$qt_port_dir/vcpkg.json"
  } | head -n 1)"
  qt_port_versions["$qt_port"]="${qt_port_versions[$qt_port]:-0}"
  qt_source_hashes["$qt_port"]="$({
    sed -nE \
      "s/^set\\(${qt_port}_HASH \"([0-9a-f]+)\"\\)$/\\1/p" \
      "$qt_port_dir/port.data.cmake"
  } | head -n 1)"
  qt_source_filenames["$qt_port"]="$({
    sed -nE \
      "s/^set\\(${qt_port}_FILENAME \"([^\"]+)\"\\)$/\\1/p" \
      "$qt_port_dir/port.data.cmake"
  } | head -n 1)"
  source_urls="$({
    sed -nE \
      "s/^set\\(${qt_port}_URL \"([^\"]+)\"\\)$/\\1/p" \
      "$qt_port_dir/port.data.cmake"
  } | head -n 1)"
  qt_source_urls["$qt_port"]="${source_urls%%;*}"

  if [[ -z "$port_version" || \
        -z "${qt_source_hashes[$qt_port]}" || \
        -z "${qt_source_filenames[$qt_port]}" || \
        -z "${qt_source_urls[$qt_port]}" ]]; then
    echo \
      "Could not resolve the pinned Qt source metadata from $qt_port_dir." \
      >&2
    exit 1
  fi
  if [[ -z "$qt_version" ]]; then
    qt_version="$port_version"
  elif [[ "$port_version" != "$qt_version" ]]; then
    echo \
      "Qt port $qt_port uses $port_version instead of $qt_version." \
      >&2
    exit 1
  fi
done

archive_name="edit-atlas-${release_version}-qt-source"
temporary_dir="$(mktemp -d)"
trap 'rm -rf -- "$temporary_dir"' EXIT
bundle_dir="$temporary_dir/$archive_name"

mkdir -p \
  "$bundle_dir/vcpkg/ports" \
  "$bundle_dir/edit-atlas/.github/workflows" \
  "$bundle_dir/edit-atlas/cmake" \
  "$bundle_dir/edit-atlas/scripts/ci"

: > "$bundle_dir/QT_SOURCE_SHA512"
: > "$bundle_dir/BUILD_INPUTS"
for qt_port in "${qt_ports[@]}"; do
  qt_source_filename="${qt_source_filenames[$qt_port]}"
  qt_source_hash="${qt_source_hashes[$qt_port]}"
  qt_source_url="${qt_source_urls[$qt_port]}"
  cached_source="vcpkg/downloads/$qt_source_filename"
  if [[ -f "$cached_source" ]]; then
    cp -- "$cached_source" "$bundle_dir/$qt_source_filename"
  else
    curl \
      --fail \
      --location \
      --retry 3 \
      --output "$bundle_dir/$qt_source_filename" \
      "$qt_source_url"
  fi

  actual_source_hash="$({
    sha512sum "$bundle_dir/$qt_source_filename"
  } | cut -d ' ' -f 1)"
  if [[ "$actual_source_hash" != "$qt_source_hash" ]]; then
    echo \
      "The downloaded $qt_port source archive has an unexpected SHA-512 digest." \
      >&2
    echo "Expected: $qt_source_hash" >&2
    echo "Actual:   $actual_source_hash" >&2
    exit 1
  fi

  cp -R -- \
    "vcpkg/ports/$qt_port" \
    "$bundle_dir/vcpkg/ports/$qt_port"
  printf '%s  %s\n' \
    "$qt_source_hash" \
    "$qt_source_filename" \
    >> "$bundle_dir/QT_SOURCE_SHA512"
  printf '%s\n' \
    "$qt_port version: $qt_version" \
    "$qt_port vcpkg port version: ${qt_port_versions[$qt_port]}" \
    "$qt_port source URL: $qt_source_url" \
    >> "$bundle_dir/BUILD_INPUTS"
done

cp -- \
  vcpkg.json \
  CMakePresets.json \
  README.md \
  "$bundle_dir/edit-atlas/"
cp -R -- vcpkg-triplets "$bundle_dir/edit-atlas/vcpkg-triplets"
cp -- \
  cmake/EditAtlasPlatformSupport.cmake \
  "$bundle_dir/edit-atlas/cmake/"
cp -- \
  .github/workflows/ci.yml \
  "$bundle_dir/edit-atlas/.github/workflows/"
cp -- \
  scripts/ci/install-macos-dependencies.sh \
  scripts/ci/install-ubuntu-dependencies.sh \
  "$bundle_dir/edit-atlas/scripts/ci/"

printf '%s\n' "$vcpkg_commit" > "$bundle_dir/vcpkg/COMMIT"
printf '%s\n' \
  "vcpkg commit: $vcpkg_commit" \
  >> "$bundle_dir/BUILD_INPUTS"

sed \
  -e "s/@EDIT_ATLAS_VERSION@/$release_version/g" \
  -e "s/@EDIT_ATLAS_QT_VERSION@/$qt_version/g" \
  -e "s/@EDIT_ATLAS_VCPKG_BASELINE@/$vcpkg_baseline/g" \
  docs/qt-source-offer.md.in \
  > "$bundle_dir/README.md"

mkdir -p "$output_dir"
tar \
  --create \
  --xz \
  --file "$output_dir/$archive_name.tar.xz" \
  --directory "$temporary_dir" \
  "$archive_name"

echo "Prepared Qt corresponding source archive for $release_tag."
