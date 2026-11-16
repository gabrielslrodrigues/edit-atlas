#!/usr/bin/env bash
set -euo pipefail

if (( $# == 0 )); then
  echo "Usage: $0 command [argument ...]" >&2
  exit 2
fi

log_file="$(mktemp)"
trap 'rm -f "${log_file}"' EXIT
set +e
"$@" 2>&1 | tee "${log_file}"
statuses=("${PIPESTATUS[@]}")
set -e
status="${statuses[0]}"
if (( statuses[1] != 0 )); then
  exit "${statuses[1]}"
fi
if (( status == 0 )); then
  exit 0
fi

# Only setup and download operations use this wrapper. A known permanent
# error takes precedence over transient errors elsewhere in the operation.
network='could not resolve|couldn.t resolve|temporary failure resolving'
network+='|name or service not known|no such host|ETIMEDOUT|ECONNRESET|EAI_AGAIN'
network+='|connection (timed out|reset)|i/o timeout|TLS handshake timeout'
network+='|HTTP(/[^ ]+)?[ :]+(502|503|504)([^0-9]|$)'
network+='|returned error: (502|503|504)([^0-9]|$)'
network+='|status code does not indicate success: (502|503|504)([^0-9]|$)'
permanent='unable to locate package|no installation candidate|unauthorized'
permanent+='|forbidden|authentication failed|checksum mismatch|hash mismatch'
permanent+='|no space left on device|no valid artifacts|no artifact matches'
permanent+='|artifact.*(not found|does not exist)|HTTP[ :]+(401|403|404)([^0-9]|$)'
category=unknown
if grep -Ei "${network}" "${log_file}" >/dev/null \
    && ! grep -Ei "${permanent}" "${log_file}" >/dev/null; then
  category=transient-network
fi
printf '\nEDIT_ATLAS_CI_FAILURE {"version":1,"category":"%s"}\n' "${category}"
exit "${status}"
