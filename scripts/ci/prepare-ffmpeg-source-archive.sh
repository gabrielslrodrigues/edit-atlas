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

ffmpeg_port_dir="vcpkg/ports/ffmpeg"
ffmpeg_version="$({
  sed -nE 's/^[[:space:]]*"version":[[:space:]]*"([^"]+)",?$/\1/p' \
    "$ffmpeg_port_dir/vcpkg.json"
} | head -n 1)"
ffmpeg_port_version="$({
  sed -nE 's/^[[:space:]]*"port-version":[[:space:]]*([0-9]+),?$/\1/p' \
    "$ffmpeg_port_dir/vcpkg.json"
} | head -n 1)"
ffmpeg_source_hash="$({
  sed -nE 's/^[[:space:]]*SHA512[[:space:]]+([0-9a-f]+)$/\1/p' \
    "$ffmpeg_port_dir/portfile.cmake"
} | head -n 1)"

if [[ -z "$ffmpeg_version" || -z "$ffmpeg_port_version" || \
      -z "$ffmpeg_source_hash" ]]; then
  echo "Could not resolve pinned FFmpeg metadata from $ffmpeg_port_dir." >&2
  exit 1
fi

ffmpeg_source_filename="ffmpeg-ffmpeg-n${ffmpeg_version}.tar.gz"
ffmpeg_source_url="https://github.com/ffmpeg/ffmpeg/archive/n${ffmpeg_version}.tar.gz"
archive_name="edit-atlas-${release_version}-ffmpeg-source"
temporary_dir="$(mktemp -d)"
trap 'rm -rf -- "$temporary_dir"' EXIT
bundle_dir="$temporary_dir/$archive_name"

mkdir -p \
  "$bundle_dir/vcpkg/ports" \
  "$bundle_dir/edit-atlas/.github/workflows" \
  "$bundle_dir/edit-atlas/cmake" \
  "$bundle_dir/edit-atlas/scripts/ci"

cached_source="vcpkg/downloads/$ffmpeg_source_filename"
if [[ -f "$cached_source" ]]; then
  cp -- "$cached_source" "$bundle_dir/$ffmpeg_source_filename"
else
  curl \
    --fail \
    --location \
    --retry 3 \
    --output "$bundle_dir/$ffmpeg_source_filename" \
    "$ffmpeg_source_url"
fi

actual_source_hash="$({
  sha512sum "$bundle_dir/$ffmpeg_source_filename"
} | cut -d ' ' -f 1)"
if [[ "$actual_source_hash" != "$ffmpeg_source_hash" ]]; then
  echo "The downloaded FFmpeg source archive has an unexpected SHA-512 digest." >&2
  echo "Expected: $ffmpeg_source_hash" >&2
  echo "Actual:   $actual_source_hash" >&2
  exit 1
fi

cp -R -- "$ffmpeg_port_dir" "$bundle_dir/vcpkg/ports/ffmpeg"
cp -- \
  vcpkg.json \
  CMakePresets.json \
  README.md \
  "$bundle_dir/edit-atlas/"
cp -R -- vcpkg-triplets "$bundle_dir/edit-atlas/vcpkg-triplets"
cp -- \
  cmake/EditAtlasFfmpegLinkage.cmake \
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
printf '%s  %s\n' \
  "$ffmpeg_source_hash" \
  "$ffmpeg_source_filename" \
  > "$bundle_dir/FFMPEG_SOURCE_SHA512"
printf '%s\n' \
  "FFmpeg version: $ffmpeg_version" \
  "vcpkg port version: $ffmpeg_port_version" \
  "vcpkg commit: $vcpkg_commit" \
  "source URL: $ffmpeg_source_url" \
  "vcpkg features: avcodec,avformat,swscale" \
  "excluded features: gpl,nonfree,x264,x265" \
  > "$bundle_dir/BUILD_INPUTS"

sed \
  -e "s/@EDIT_ATLAS_VERSION@/$release_version/g" \
  -e "s/@EDIT_ATLAS_FFMPEG_VERSION@/$ffmpeg_version/g" \
  -e "s/@EDIT_ATLAS_VCPKG_BASELINE@/$vcpkg_baseline/g" \
  docs/ffmpeg-source-offer.md.in \
  > "$bundle_dir/README.md"

mkdir -p "$output_dir"
tar \
  --create \
  --xz \
  --file "$output_dir/$archive_name.tar.xz" \
  --directory "$temporary_dir" \
  "$archive_name"

echo "Prepared FFmpeg corresponding source archive for $release_tag."
