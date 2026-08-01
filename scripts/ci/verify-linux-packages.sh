#!/usr/bin/env bash

set -euo pipefail

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

run_as_root() {
  if (( EUID == 0 )); then
    "$@"
  else
    sudo "$@"
  fi
}

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

cleanup() {
  if [[ "$installed_package" == true ]]; then
    remove_installed_package || true
  fi
  cmake -E remove_directory "$work_dir"
}

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

verify_deployment() {
  local deployment_root="$1"
  local executable="$2"

  cmake \
    "-DEDIT_ATLAS_DEPLOYMENT_ROOT=$deployment_root" \
    "-DEDIT_ATLAS_EXECUTABLE=$executable" \
    -P "$source_dir/cmake/VerifyApplicationDeployment.cmake"
}

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
archive_root="$(dirname "$(dirname "$archive_executable")")"
verify_deployment "$archive_root" "$archive_executable"

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
    verify_deployment "$deb_dir/usr" "$deb_dir/usr/bin/edit-atlas"

    run_as_root apt-get install --yes "$deb"
    installed_package=true
    if [[ "$(dpkg-query --show --showformat='${Status}' edit-atlas)" != \
      "install ok installed" ]]; then
      echo "The DEB package was not registered as installed." >&2
      exit 1
    fi
    verify_deployment "/usr" "/usr/bin/edit-atlas"
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
    verify_deployment "$rpm_dir/usr" "$rpm_dir/usr/bin/edit-atlas"

    run_as_root dnf install --assumeyes "$rpm_package"
    installed_package=true
    rpm --query edit-atlas
    verify_deployment "/usr" "/usr/bin/edit-atlas"
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
