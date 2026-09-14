#!/usr/bin/env bash

set -euo pipefail

# Install and exercise a macOS package, then remove the application.

run_as_root() {
  if (( EUID == 0 )); then
    "$@"
  else
    sudo "$@"
  fi
}

# Stop the launched application and remove the installed package.
# Globals: application, application_pid, installed_package (local to the
# active main invocation).
cleanup() {
  if [[ -n "$application_pid" ]] && kill -0 "$application_pid" 2>/dev/null; then
    kill "$application_pid" || true
    wait "$application_pid" || true
  fi
  if [[ "$installed_package" == true ]]; then
    run_as_root rm -rf "$application"
  fi
}

# Install the macOS package, launch the application, then remove it.
# Arguments: package directory.
# Outputs: verification progress; failures to stderr.
# Returns: 2 for invalid arguments, otherwise the first failing command's status.
main() {
  local application
  local application_pid
  local executable
  local installed_package
  local package
  local package_dir
  local script_dir
  local source_dir

  if (( $# != 1 )); then
    echo "Usage: $0 <package-directory>" >&2
    exit 2
  fi

  package_dir="$1"
  if [[ ! -d "$package_dir" ]]; then
    echo "Package directory does not exist: $package_dir" >&2
    exit 1
  fi

  package_dir="$(cd -- "$package_dir" && pwd)"
  script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
  source_dir="$(cd -- "$script_dir/../.." && pwd)"
  application="/Applications/edit-atlas.app"
  application_pid=""
  installed_package=false

  trap cleanup EXIT

  package="$(
    find "$package_dir" -maxdepth 1 -type f -name '*.pkg' -print -quit
  )"
  if [[ -z "$package" ]]; then
    echo "No PKG installer found in $package_dir." >&2
    exit 1
  fi
  if [[ -e "$application" ]]; then
    echo "The package verification target already exists: $application" >&2
    exit 1
  fi

  run_as_root installer -pkg "$package" -target /
  installed_package=true
  executable="$application/Contents/MacOS/edit-atlas"
  if [[ ! -x "$executable" ]]; then
    echo "The installed application executable is missing: $executable" >&2
    exit 1
  fi

  cmake \
    "-DEDIT_ATLAS_DEPLOYMENT_ROOT=$application" \
    "-DEDIT_ATLAS_EXECUTABLE=$executable" \
    -P "$source_dir/cmake/VerifyApplicationDeployment.cmake"
  cmake \
    "-DEDIT_ATLAS_BUNDLE=$application" \
    -P "$source_dir/cmake/VerifyMacOSUniversalBundle.cmake"

  "$executable" &
  application_pid="$!"
  sleep 5
  if ! kill -0 "$application_pid" 2>/dev/null; then
    wait "$application_pid" || true
    echo "The installed application exited during its launch smoke test." >&2
    exit 1
  fi
  kill "$application_pid"
  wait "$application_pid" || true
  application_pid=""

  run_as_root rm -rf "$application"
  installed_package=false
  if [[ -e "$application" ]]; then
    echo "The application remains after package cleanup: $application" >&2
    exit 1
  fi

  trap - EXIT
  echo "Verified macOS package: $package"

  # Exit before main unwinds the local state used by EXIT callbacks.
  exit 0
}

main "$@"
