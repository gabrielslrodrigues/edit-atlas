#!/usr/bin/env bash
set -euo pipefail

script_directory="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repository_root="$(cd -- "${script_directory}/../.." && pwd)"
e2e_root="${repository_root}/build/e2e"
virtual_environment="${e2e_root}/venv"

if ! command -v uv >/dev/null 2>&1; then
  echo "uv is required to run the macOS E2E suite" >&2
  exit 127
fi

mkdir -p "${e2e_root}/reports" "${e2e_root}/output" "${e2e_root}/artifacts"
export UV_PROJECT_ENVIRONMENT="${virtual_environment}"
uv run --locked --project "${script_directory}" \
  python "${script_directory}/preflight_macos.py"
exec uv run --locked --project "${script_directory}" python -m pytest \
  -c "${script_directory}/pytest.ini" \
  "${script_directory}/tests" \
  --ignore "${script_directory}/tests/linux" \
  --ignore "${script_directory}/tests/windows" \
  --fixture-dir "${repository_root}/tests/fixtures/cmx3600" \
  --media-fixture-dir "${e2e_root}/media-fixtures" \
  --output-dir "${e2e_root}/output" \
  --state-root "${e2e_root}/state" \
  --artifact-dir "${e2e_root}/artifacts" \
  --locale "pt_BR.UTF-8" \
  --junitxml "${e2e_root}/reports/junit.xml" \
  --html "${e2e_root}/reports/report.html" \
  --self-contained-html \
  "$@"
