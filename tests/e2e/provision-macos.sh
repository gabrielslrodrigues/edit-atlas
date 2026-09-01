#!/usr/bin/env bash
set -euo pipefail

cat >&2 <<'EOF'
macOS packaged E2E provisioning requires the trusted interactive Apple host
tracked by issue #166. Its test user must grant Accessibility permission to
the pinned automation executable. No package was installed.

See tests/e2e/README.md#provisioned-environments for the provisioning contract.
EOF
exit 2
