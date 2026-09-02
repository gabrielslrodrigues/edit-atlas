#!/usr/bin/env bash
set -euo pipefail

cat >&2 <<'EOF'
macOS packaged E2E provisioning requires a trusted interactive host on Apple
hardware: a persistent machine or a virtual machine, logged into an Aqua
session, whose test user has granted Accessibility permission to the pinned
automation executable. This project provides no such host, and the permission
cannot be granted from a script, so no package was installed and nothing on
this machine was changed.

See tests/e2e/README.md#macos-desktop-tests for the host requirements, what
invalidates the permission, and the conditions for enabling the CI job.
EOF
exit 2
