#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'EOF'
Usage: run-provisioned.sh --repository-root PATH --artifact-root PATH
       --package PATH --media-fixture-dir PATH --home PATH
       --host-uid UID --host-gid GID
       [-- PYTEST_ARGUMENT...]
EOF
}

repository_root=""
artifact_root=""
package_path=""
host_uid=""
host_gid=""
media_fixture_directory=""
home_directory=""
pytest_arguments=()

while (( $# > 0 )); do
  case "$1" in
    --repository-root|--artifact-root|--package|--host-uid|--host-gid|\
    --media-fixture-dir|--home)
      if (( $# < 2 )); then
        usage
        exit 2
      fi
      option="$1"
      value="$2"
      shift 2
      case "${option}" in
        --repository-root) repository_root="${value}" ;;
        --artifact-root) artifact_root="${value}" ;;
        --package) package_path="${value}" ;;
        --host-uid) host_uid="${value}" ;;
        --host-gid) host_gid="${value}" ;;
        --media-fixture-dir) media_fixture_directory="${value}" ;;
        --home) home_directory="${value}" ;;
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

if [[ -z "${repository_root}" || -z "${artifact_root}" || \
      -z "${package_path}" || -z "${media_fixture_directory}" || \
      -z "${home_directory}" || \
      -z "${host_uid}" || -z "${host_gid}" ]]; then
  usage
  exit 2
fi
if [[ ! "${host_uid}" =~ ^[0-9]+$ || ! "${host_gid}" =~ ^[0-9]+$ ]]; then
  echo "Host UID and GID must be numeric" >&2
  exit 2
fi
if [[ ! -d "${repository_root}" || ! -d "${artifact_root}" || \
      ! -f "${package_path}" ]]; then
  echo "The mounted repository, artifact root, or package is unavailable" >&2
  exit 2
fi

if [[ "$(dpkg-deb --field "${package_path}" Package)" != "edit-atlas" ]]; then
  echo "The supplied DEB is not an Edit Atlas package" >&2
  exit 2
fi
if [[ "$(dpkg-deb --field "${package_path}" Architecture)" != "amd64" ]]; then
  echo "The supplied DEB does not target amd64" >&2
  exit 2
fi

export DEBIAN_FRONTEND=noninteractive
export NEEDRESTART_MODE=a
apt-get \
  -o Acquire::Retries=3 \
  -o Acquire::http::Timeout=30 \
  -o Acquire::https::Timeout=30 \
  install --yes --no-install-recommends "${package_path}"

# The package installs whenever its declared dependencies are satisfiable, but
# its bundled Qt and FFmpeg libraries are built against the toolchain of
# whichever machine produced it. A package from a distribution newer than the
# pinned Ubuntu base resolves its dpkg dependencies and then fails to load, so
# it is smoke-tested here and reported as an incompatible input rather than
# surfacing as unexplained failures across the suite.
smoke_log="${TMPDIR:-/tmp}/edit-atlas-smoke.log"
if ! /usr/bin/edit-atlas-cli --version >/dev/null 2>"${smoke_log}"; then
  echo "The installed package cannot run in this image:" >&2
  sed 's/^/  /' "${smoke_log}" >&2
  echo "Supply a package built for the pinned Ubuntu base, such as the one" \
    "produced by CI." >&2
  exit 2
fi

e2e_root="${artifact_root}"
runtime_directory="${home_directory}/runtime"
install -d -o "${host_uid}" -g "${host_gid}" \
  "${e2e_root}/artifacts" \
  "${e2e_root}/crash-dumps" \
  "${e2e_root}/output" \
  "${e2e_root}/reports" \
  "${home_directory}" \
  "${runtime_directory}"
chmod 0700 "${runtime_directory}"

run_as_host=(
  setpriv
  --reuid="${host_uid}"
  --regid="${host_gid}"
  --clear-groups
  env
  HOME="${home_directory}"
  XDG_RUNTIME_DIR="${runtime_directory}"
)

if [[ ! -d "${media_fixture_directory}" ]]; then
  echo "The mounted media-fixture directory is unavailable" >&2
  exit 2
fi

ulimit -c unlimited
cd "${e2e_root}/crash-dumps"
"${run_as_host[@]}" \
  GSETTINGS_BACKEND=memory \
  EDIT_ATLAS_E2E_ROOT="${e2e_root}" \
  EDIT_ATLAS_E2E_MEDIA_FIXTURE_DIR="${media_fixture_directory}" \
  dbus-run-session -- \
  xvfb-run \
  --auto-servernum \
  --server-args="-screen 0 1440x1000x24 -nolisten tcp" \
  "${repository_root}/tests/e2e/run-linux.sh" \
  --app /usr/bin/edit-atlas \
  --cli /usr/bin/edit-atlas-cli \
  "${pytest_arguments[@]}"
