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

# Qt deployment can provide both a versioned dylib and a compatibility-name
# dylib as regular files. Loading both copies makes Cocoa register every Qt
# Objective-C class twice and abort during application startup. The deployed
# binaries use the versioned install names, so remove the redundant aliases.
frameworks_dir="$universal_app/Contents/Frameworks"
for library in libQt6Concurrent libQt6Core libQt6Gui libQt6Widgets libxlsxwriter; do
  compatibility_library="$frameworks_dir/$library.dylib"
  versioned_library=""
  for candidate in "$frameworks_dir/$library".*.dylib; do
    if [[ -f "$candidate" ]]; then
      versioned_library="$candidate"
      break
    fi
  done
  if [[ -f "$compatibility_library" && -n "$versioned_library" ]]; then
    rm "$compatibility_library"
  fi
done

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
