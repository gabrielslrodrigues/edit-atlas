#!/usr/bin/env bash

set -euo pipefail

# Inspect, install, exercise, and remove Linux release packages.

run_as_root() {
  if (( EUID == 0 )); then
    "$@"
  else
    sudo "$@"
  fi
}

# Uninstall the previously-installed release package.
# Globals: distribution (local to the active main invocation).
remove_installed_package() {
  case "$distribution" in
    ubuntu)
      run_as_root apt-get remove --yes edit-atlas
      ;;
    fedora)
      run_as_root dnf remove --assumeyes edit-atlas
      ;;
  esac
}

# Remove the installed package and the temporary work directory.
# Globals: installed_package, work_dir (local to the active main invocation).
cleanup() {
  if [[ "$installed_package" == true ]]; then
    remove_installed_package || true
  fi
  cmake -E remove_directory "$work_dir"
}

# Finds the one file matching \p 1 directly under the package directory.
# Globals: package_dir (local to the active main invocation).
find_package() {
  find "$package_dir" -maxdepth 1 -type f -name "$1" -print -quit
}

require_package() {
  local package_path="$1"
  local package_description="$2"

  if [[ -z "$package_path" ]]; then
    echo "No $package_description package found in $package_dir." >&2
    exit 1
  fi
}

# Globals: source_dir (local to the active main invocation).
verify_deployment() {
  local deployment_root="$1"
  local executable="$2"
  local verify_private_runtime_boundary="$3"

  cmake \
    "-DEDIT_ATLAS_DEPLOYMENT_ROOT=$deployment_root" \
    "-DEDIT_ATLAS_EXECUTABLE=$executable" \
    "-DEDIT_ATLAS_VERIFY_PRIVATE_RUNTIME_BOUNDARY=$verify_private_runtime_boundary" \
    -P "$source_dir/cmake/VerifyApplicationDeployment.cmake"
}

# Globals: source_dir, work_dir (local to the active main invocation).
verify_installed_runtime() {
  local application="$1"
  local cli
  local fixture="$source_dir/tests/fixtures/cmx3600/mixed_tracks.edl"
  local output="$work_dir/installed-cli.xlsx"
  local application_status

  cli="${application%/*}/edit-atlas-cli"
  "$cli" convert --fps 24 "$fixture" "$output"
  if [[ ! -s "$output" ]]; then
    echo "The installed CLI did not create a workbook: $output" >&2
    exit 1
  fi

  set +e
  XDG_CACHE_HOME="$work_dir/cache" \
    XDG_CONFIG_HOME="$work_dir/config" \
    XDG_DATA_HOME="$work_dir/data" \
    XDG_STATE_HOME="$work_dir/state" \
    xvfb-run --auto-servernum \
    timeout --signal=TERM 5s "$application"
  application_status=$?
  set -e
  if (( application_status != 124 )); then
    printf \
      'The installed application exited during its launch smoke test with status %s.\n' \
      "$application_status" >&2
    exit 1
  fi
}

# Install a distribution package, exercise the CLI and GUI, then remove it.
# Arguments: distribution (ubuntu|fedora), package directory.
# Outputs: verification progress; failures to stderr.
# Returns: 2 for invalid arguments, otherwise the first failing command's status.
main() {
  local archive
  local archive_dir
  local archive_executable
  local archive_root
  local deb
  local deb_dir
  local distribution
  local installed_package
  local package_dir
  local rpm_dir
  local rpm_package
  local script_dir
  local source_dir
  local work_dir

  if (( $# != 2 )); then
    echo "Usage: $0 <ubuntu|fedora> <package-directory>" >&2
    exit 2
  fi

  distribution="$1"
  package_dir="$2"

  case "$distribution" in
    ubuntu | fedora) ;;
    *)
      echo "Unsupported Linux distribution: $distribution" >&2
      exit 2
      ;;
  esac

  if [[ ! -d "$package_dir" ]]; then
    echo "Package directory does not exist: $package_dir" >&2
    exit 1
  fi

  package_dir="$(cd -- "$package_dir" && pwd)"
  script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
  source_dir="$(cd -- "$script_dir/../.." && pwd)"
  work_dir="$source_dir/build/package-check-$distribution"
  installed_package=false

  trap cleanup EXIT
  cmake -E remove_directory "$work_dir"
  mkdir -p "$work_dir"

  archive="$(find_package '*.tar.gz')"
  require_package "$archive" "portable archive"

  archive_dir="$work_dir/archive"
  mkdir -p "$archive_dir"
  cmake -E chdir "$archive_dir" cmake -E tar xzf "$archive"
  archive_executable="$(
    find "$archive_dir" -path '*/bin/edit-atlas' -type f -print -quit
  )"
  if [[ -z "$archive_executable" ]]; then
    echo "The portable archive does not contain bin/edit-atlas." >&2
    exit 1
  fi
  archive_root="${archive_executable%/*/*}"
  verify_deployment "$archive_root" "$archive_executable" TRUE

  case "$distribution" in
    ubuntu)
      deb="$(find_package '*.deb')"
      require_package "$deb" "DEB"

      if [[ "$(dpkg-deb --field "$deb" Architecture)" != "amd64" ]]; then
        echo "The DEB package does not target amd64: $deb" >&2
        exit 1
      fi

      deb_dir="$work_dir/deb"
      mkdir -p "$deb_dir"
      dpkg-deb --extract "$deb" "$deb_dir"
      verify_deployment "$deb_dir/usr" "$deb_dir/usr/bin/edit-atlas" TRUE

      run_as_root apt-get install --yes "$deb"
      installed_package=true
      if [[ "$(dpkg-query --show --showformat='${Status}' edit-atlas)" != \
        "install ok installed" ]]; then
        echo "The DEB package was not registered as installed." >&2
        exit 1
      fi
      verify_deployment "/usr" "/usr/bin/edit-atlas" FALSE
      verify_installed_runtime "/usr/bin/edit-atlas"
      remove_installed_package
      installed_package=false
      if [[ -e /usr/bin/edit-atlas ]]; then
        echo "The DEB uninstall left /usr/bin/edit-atlas behind." >&2
        exit 1
      fi
      ;;
    fedora)
      rpm_package="$(find_package '*.rpm')"
      require_package "$rpm_package" "RPM"

      if [[ "$(rpm --query --package --queryformat '%{ARCH}' "$rpm_package")" != \
        "x86_64" ]]; then
        echo "The RPM package does not target x86_64: $rpm_package" >&2
        exit 1
      fi

      rpm_dir="$work_dir/rpm"
      mkdir -p "$rpm_dir"
      (
        cd "$rpm_dir"
        rpm2cpio "$rpm_package" | cpio --extract --make-directories --quiet
      )
      verify_deployment "$rpm_dir/usr" "$rpm_dir/usr/bin/edit-atlas" TRUE

      run_as_root dnf install --assumeyes "$rpm_package"
      installed_package=true
      rpm --query edit-atlas
      verify_deployment "/usr" "/usr/bin/edit-atlas" FALSE
      verify_installed_runtime "/usr/bin/edit-atlas"
      remove_installed_package
      installed_package=false
      if [[ -e /usr/bin/edit-atlas ]]; then
        echo "The RPM uninstall left /usr/bin/edit-atlas behind." >&2
        exit 1
      fi
      if rpm --query edit-atlas; then
        echo "The Edit Atlas RPM remains installed." >&2
        exit 1
      fi
      ;;
  esac

  cleanup
  trap - EXIT
  echo "Verified $distribution packages in $package_dir."

  # Exit before main unwinds the local state used by EXIT callbacks.
  exit 0
}

main "$@"
