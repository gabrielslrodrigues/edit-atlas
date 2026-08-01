#!/usr/bin/env bash

set -euo pipefail

if (( $# != 3 )); then
  echo \
    "Usage: $0 <arm64-stage-archive> <x64-stage-archive> <package-directory>" \
    >&2
  exit 2
fi

arm64_archive="$1"
x64_archive="$2"
package_dir="$3"

for archive in "$arm64_archive" "$x64_archive"; do
  if [[ ! -f "$archive" ]]; then
    echo "Stage archive does not exist: $archive" >&2
    exit 1
  fi
done

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
source_dir="$(cd -- "$script_dir/../.." && pwd)"
work_dir="$source_dir/build/macos-universal"
arm64_dir="$work_dir/arm64"
x64_dir="$work_dir/x64"
universal_dir="$work_dir/universal"
universal_app="$universal_dir/edit-atlas.app"

cmake -E remove_directory "$work_dir"
cmake -E make_directory "$arm64_dir" "$x64_dir" "$universal_dir"
tar -xzf "$arm64_archive" -C "$arm64_dir"
tar -xzf "$x64_archive" -C "$x64_dir"

arm64_app="$arm64_dir/edit-atlas.app"
x64_app="$x64_dir/edit-atlas.app"
for application in "$arm64_app" "$x64_app"; do
  if [[ ! -d "$application" ]]; then
    echo "Stage archive does not contain edit-atlas.app: $application" >&2
    exit 1
  fi
done

ditto "$arm64_app" "$universal_app"

mach_o_count=0
while IFS= read -r -d '' arm64_file; do
  if ! file -b "$arm64_file" | grep -q "Mach-O"; then
    continue
  fi

  relative_path="${arm64_file#"$arm64_app/"}"
  x64_file="$x64_app/$relative_path"
  universal_file="$universal_app/$relative_path"
  if [[ ! -f "$x64_file" ]]; then
    echo "The x64 bundle is missing Mach-O file: $relative_path" >&2
    exit 1
  fi
  if ! file -b "$x64_file" | grep -q "Mach-O"; then
    echo "The x64 counterpart is not Mach-O: $relative_path" >&2
    exit 1
  fi

  lipo -create "$arm64_file" "$x64_file" -output "$universal_file.tmp"
  chmod "$(stat -f '%Lp' "$arm64_file")" "$universal_file.tmp"
  mv "$universal_file.tmp" "$universal_file"
  ((mach_o_count += 1))
done < <(find "$arm64_app" -type f -print0)

if (( mach_o_count == 0 )); then
  echo "No Mach-O files were found in the ARM64 bundle." >&2
  exit 1
fi

# Cached deployment packages can flatten dylib symlinks into regular files.
# Restore each compatibility name as a relative symlink so dyld resolves both
# names to one physical universal binary.
frameworks_dir="$universal_app/Contents/Frameworks"
normalize_dylib_alias() {
  local compatibility_library="$1"
  shift

  if [[ -L "$compatibility_library" || ! -e "$compatibility_library" ]]; then
    return
  fi

  local versioned_library=""
  local candidate
  for candidate in "$@"; do
    if [[ ! -f "$candidate" ]]; then
      continue
    fi
    if [[ -n "$versioned_library" ]]; then
      echo \
        "Multiple versioned dylibs match $compatibility_library." \
        >&2
      exit 1
    fi
    versioned_library="$candidate"
  done

  if [[ -z "$versioned_library" ]]; then
    echo "No versioned dylib matches $compatibility_library." >&2
    exit 1
  fi
  if ! cmp -s "$compatibility_library" "$versioned_library"; then
    echo \
      "Compatibility dylib differs from $versioned_library: " \
      "$compatibility_library" \
      >&2
    exit 1
  fi

  rm "$compatibility_library"
  ln -s "$(basename "$versioned_library")" "$compatibility_library"
}

normalize_dylib_alias \
  "$frameworks_dir/libQt6Concurrent.6.dylib" \
  "$frameworks_dir"/libQt6Concurrent.6.*.dylib
normalize_dylib_alias \
  "$frameworks_dir/libQt6Core.6.dylib" \
  "$frameworks_dir"/libQt6Core.6.*.dylib
normalize_dylib_alias \
  "$frameworks_dir/libQt6Gui.6.dylib" \
  "$frameworks_dir"/libQt6Gui.6.*.dylib
normalize_dylib_alias \
  "$frameworks_dir/libQt6Widgets.6.dylib" \
  "$frameworks_dir"/libQt6Widgets.6.*.dylib

codesign --force --deep --sign - "$universal_app"

cmake \
  "-DEDIT_ATLAS_DEPLOYMENT_ROOT=$universal_dir" \
  "-DEDIT_ATLAS_EXECUTABLE=$universal_app/Contents/MacOS/edit-atlas" \
  -P "$source_dir/cmake/VerifyQtDeployment.cmake"
cmake \
  "-DEDIT_ATLAS_BUNDLE=$universal_app" \
  -P "$source_dir/cmake/VerifyMacOSUniversalBundle.cmake"

version="$(
  /usr/libexec/PlistBuddy \
    -c "Print :CFBundleShortVersionString" \
    "$universal_app/Contents/Info.plist"
)"
cmake -E make_directory "$package_dir"
package="$package_dir/edit-atlas-$version-macos-universal.pkg"
pkgbuild \
  --component "$universal_app" \
  --install-location /Applications \
  "$package"
shasum -a 256 "$package" >"$package.sha256"

echo "Created universal macOS package: $package"
