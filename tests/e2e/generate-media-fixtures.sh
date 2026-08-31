#!/usr/bin/env bash
set -euo pipefail

# Generates the rendered-video E2E fixtures and records the generator identity
# that produced them. A desktop suite rejects fixtures without a matching
# record, so fixtures must be produced through this entry point rather than by
# invoking the generator directly.

if [ "$#" -ne 2 ]; then
  echo "usage: $0 <generator-executable> <fixture-directory>" >&2
  exit 64
fi

generator="$1"
fixture_directory="$2"
script_directory="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repository_root="$(cd -- "${script_directory}/../.." && pwd)"

if [ ! -x "${generator}" ]; then
  echo "generator is not an executable: ${generator}" >&2
  exit 66
fi

mkdir -p -- "${fixture_directory}"
"${generator}" "${fixture_directory}"
python3 "${script_directory}/application/media_fixtures.py" \
  --repository-root "${repository_root}" \
  "${fixture_directory}"
