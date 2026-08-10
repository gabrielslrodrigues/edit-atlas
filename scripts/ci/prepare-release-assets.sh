#!/usr/bin/env bash

set -euo pipefail

if (( $# != 2 )); then
  echo "Usage: $0 <downloaded-artifact-directory> <release-directory>" >&2
  exit 2
fi

source_dir="$1"
release_dir="$2"

if [[ ! -d "$source_dir" ]]; then
  echo "Downloaded artifact directory does not exist: $source_dir" >&2
  exit 1
fi

cmake -E remove_directory "$release_dir"
cmake -E make_directory "$release_dir"

mapfile -t packages < <(
  find "$source_dir" -type f \( \
    -name '*.tar.gz' -o \
    -name '*.deb' -o \
    -name '*.rpm' -o \
    -name '*.pkg' -o \
    -name '*.msi' \
  \) -print | sort
)

if (( ${#packages[@]} != 5 )); then
  echo "Expected five platform packages, found ${#packages[@]}." >&2
  printf '%s\n' "${packages[@]}" >&2
  exit 1
fi

mapfile -t qt_source_archives < <(
  find "$source_dir" -type f \
    -name 'edit-atlas-*-qt-source.tar.xz' \
    -print | sort
)
if (( ${#qt_source_archives[@]} != 1 )); then
  echo "Expected one Qt source archive, found ${#qt_source_archives[@]}." >&2
  printf '%s\n' "${qt_source_archives[@]}" >&2
  exit 1
fi

mapfile -t ffmpeg_source_archives < <(
  find "$source_dir" -type f \
    -name 'edit-atlas-*-ffmpeg-source.tar.xz' \
    -print | sort
)
if (( ${#ffmpeg_source_archives[@]} != 1 )); then
  echo \
    "Expected one FFmpeg source archive, found ${#ffmpeg_source_archives[@]}." \
    >&2
  printf '%s\n' "${ffmpeg_source_archives[@]}" >&2
  exit 1
fi

for package in "${packages[@]}"; do
  cp -- "$package" "$release_dir/"
done
cp -- "${qt_source_archives[0]}" "$release_dir/"
cp -- "${ffmpeg_source_archives[0]}" "$release_dir/"

cp -- LICENSE "$release_dir/LICENSE"
cp -- THIRD_PARTY_NOTICES.md "$release_dir/THIRD_PARTY_NOTICES.md"

(
  cd -- "$release_dir"
  sha256sum -- *.tar.gz *.tar.xz *.deb *.rpm *.pkg *.msi > SHA256SUMS
)

echo \
  "Prepared ${#packages[@]} release packages and corresponding sources in " \
  "$release_dir."
