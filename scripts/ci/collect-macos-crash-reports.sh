#!/usr/bin/env bash

set -euo pipefail

if (( $# != 2 )); then
  echo "Usage: $0 <marker-file> <destination-directory>" >&2
  exit 2
fi

marker_file="$1"
destination_directory="$2"
reports_directory="${HOME}/Library/Logs/DiagnosticReports"

if [[ ! -f "$marker_file" ]]; then
  printf \
    'Crash-report marker does not exist; no reports will be collected: %s\n' \
    "$marker_file" >&2
  exit 0
fi

mkdir -p "$destination_directory"
if [[ ! -d "$reports_directory" ]]; then
  exit 0
fi

find "$reports_directory" \
  -type f \
  -newer "$marker_file" \
  \( -name 'edit-atlas*.ips' -o -name 'edit-atlas*.crash' \) \
  -exec cp -p {} "$destination_directory" \;
