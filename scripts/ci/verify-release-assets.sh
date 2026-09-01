#!/usr/bin/env bash

set -euo pipefail

# Validates an assembled release-asset directory against what a release of
# the given version must contain: the exact package set with the expected
# versions and architectures, both corresponding-source archives, licensing
# material, and checksums that match the files beside them.

if (( $# != 2 )); then
  echo "Usage: $0 <release-directory> <release-tag>" >&2
  exit 2
fi

release_dir="$1"
release_tag="$2"
version="${release_tag#v}"

if [[ ! "$release_tag" =~ ^v[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Release tag must use the vX.Y.Z form: $release_tag" >&2
  exit 1
fi

if [[ ! -d "$release_dir" ]]; then
  echo "Release directory does not exist: $release_dir" >&2
  exit 1
fi

expected=(
  "edit-atlas-$version-linux-x86_64.tar.gz"
  "edit-atlas-$version-linux-x86_64.deb"
  "edit-atlas-$version-linux-x86_64.rpm"
  "edit-atlas-$version-macos-universal.pkg"
  "edit-atlas-$version-windows-x64.msi"
  "edit-atlas-$version-qt-source.tar.xz"
  "edit-atlas-$version-ffmpeg-source.tar.xz"
  LICENSE
  THIRD_PARTY_NOTICES.md
  SHA256SUMS
)

failures=0

fail() {
  echo "  MISSING OR INVALID: $*" >&2
  failures=$((failures + 1))
}

echo "Verifying release assets for $release_tag in $release_dir."

for name in "${expected[@]}"; do
  path="$release_dir/$name"
  if [[ ! -f "$path" ]]; then
    fail "$name is absent"
  elif [[ ! -s "$path" ]]; then
    fail "$name is empty"
  fi
done

mapfile -t present < <(cd -- "$release_dir" && find . -maxdepth 1 -type f -printf '%f\n' | sort)
for name in "${present[@]}"; do
  found=0
  for known in "${expected[@]}"; do
    if [[ "$name" == "$known" ]]; then
      found=1
      break
    fi
  done
  if (( found == 0 )); then
    fail "$name is not an expected release asset"
  fi
done

# A package built from the wrong tree can still carry a plausible name, so
# check that nothing carries a version other than the candidate's.
mapfile -t misversioned < <(
  cd -- "$release_dir" &&
    find . -maxdepth 1 -type f -name 'edit-atlas-*' -printf '%f\n' |
    grep -v -- "-$version-" | sort || true
)
for name in "${misversioned[@]}"; do
  fail "$name does not carry version $version"
done

for notice in LICENSE THIRD_PARTY_NOTICES.md; do
  if [[ -s "$release_dir/$notice" ]]; then
    for required in Qt FFmpeg; do
      if [[ "$notice" == THIRD_PARTY_NOTICES.md ]] &&
        ! grep -qi -- "$required" "$release_dir/$notice"; then
        fail "THIRD_PARTY_NOTICES.md does not mention $required"
      fi
    done
  fi
done

for archive in \
  "edit-atlas-$version-qt-source.tar.xz" \
  "edit-atlas-$version-ffmpeg-source.tar.xz"; do
  path="$release_dir/$archive"
  [[ -s "$path" ]] || continue
  if ! tar -tJf "$path" >/dev/null 2>&1 < /dev/null; then
    fail "$archive is not a readable xz archive"
  fi
done

if [[ -s "$release_dir/SHA256SUMS" ]]; then
  listed="$(wc -l < "$release_dir/SHA256SUMS")"
  if (( listed != 7 )); then
    fail "SHA256SUMS lists $listed files, expected 7"
  fi
  if ! (cd -- "$release_dir" && sha256sum --check --quiet SHA256SUMS); then
    fail "SHA256SUMS does not match the assets beside it"
  fi
fi

echo
echo "Candidate inventory:"
(cd -- "$release_dir" && find . -maxdepth 1 -type f -printf '  %-48f %10s bytes\n' | sort)
echo

if (( failures > 0 )); then
  echo "Release asset verification failed with $failures problem(s)." >&2
  exit 1
fi

echo "Release assets for $release_tag are complete and consistent."
