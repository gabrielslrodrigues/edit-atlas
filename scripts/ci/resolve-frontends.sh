#!/usr/bin/env bash

set -euo pipefail

# Resolves which graphical frontend production ships and lists every other
# supported frontend, from the single source of truth in CMakeLists.txt.
# Nothing in CI should hardcode that Qt Quick ships and Qt Widgets is the
# alternative: the default is a build option, the roles swap the moment it
# changes, and the supported list may grow beyond two.

if (( $# > 1 )); then
  echo "Usage: $0 [output-file]" >&2
  exit 2
fi

production="$(
  sed -nE '/^[[:space:]]*EDIT_ATLAS_DEFAULT_FRONTEND[[:space:]]*$/{
    n
    s/^[[:space:]]*"([a-z]+)"[[:space:]]*$/\1/p
  }' CMakeLists.txt | head -n 1
)"

supported="$(
  sed -nE 's/^[[:space:]]*PROPERTY STRINGS ([a-z ]+)$/\1/p' CMakeLists.txt |
    head -n 1
)"

if [[ -z "$production" ]]; then
  echo "Could not determine the default frontend from CMakeLists.txt." >&2
  exit 1
fi

if [[ -z "$supported" ]]; then
  echo "Could not determine the supported frontends from CMakeLists.txt." >&2
  exit 1
fi

others=()
found=0
for frontend in $supported; do
  if [[ "$frontend" == "$production" ]]; then
    found=1
  else
    others+=("$frontend")
  fi
done

if (( found == 0 )); then
  echo "Default frontend '$production' is not in the supported set:" >&2
  echo "  $supported" >&2
  exit 1
fi

others_json="[]"
if (( ${#others[@]} > 0 )); then
  others_json="[$(printf '"%s",' "${others[@]}" | sed 's/,$//')]"
fi

echo "Production frontend: $production" >&2
echo "Other frontends:      ${others[*]:-none}" >&2

output="${1:-/dev/stdout}"
{
  echo "production=$production"
  echo "others=$others_json"
} >> "$output"
