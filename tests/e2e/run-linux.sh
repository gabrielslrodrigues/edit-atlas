#!/usr/bin/env bash
set -euo pipefail

script_directory="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repository_root="$(cd -- "${script_directory}/../.." && pwd)"
e2e_root="${repository_root}/build/e2e"
virtual_environment="${e2e_root}/venv"

if ! command -v uv >/dev/null 2>&1; then
  echo "uv is required to run the Linux E2E suite" >&2
  exit 127
fi

if [[ -z "${DISPLAY:-}" || -z "${DBUS_SESSION_BUS_ADDRESS:-}" ]]; then
  echo "Linux GUI E2E requires X11 and a D-Bus session" >&2
  exit 2
fi

mkdir -p "${e2e_root}/reports" "${e2e_root}/output" "${e2e_root}/artifacts"
export UV_PROJECT_ENVIRONMENT="${virtual_environment}"
export NO_AT_BRIDGE=0
export QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1
export QT_QPA_PLATFORM=xcb
export XDG_SESSION_TYPE=x11
exec uv run --locked --project "${script_directory}" python -m pytest \
  -c "${script_directory}/pytest.ini" \
  "${script_directory}/tests/test_packaged_cli.py" \
  "${script_directory}/tests/test_packaged_gui.py" \
  "${script_directory}/tests/linux" \
  --fixture-dir "${repository_root}/tests/fixtures/cmx3600" \
  --output-dir "${e2e_root}/output" \
  --state-root "${e2e_root}/state" \
  --artifact-dir "${e2e_root}/artifacts" \
  --locale "pt_BR.UTF-8" \
  --junitxml "${e2e_root}/reports/junit.xml" \
  --html "${e2e_root}/reports/report.html" \
  --self-contained-html \
  "$@"
