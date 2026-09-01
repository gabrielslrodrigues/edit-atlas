#!/usr/bin/env bash
set -euo pipefail

cat >&2 <<'EOF'
Linux packaged E2E provisioning requires the disposable container tracked by
issue #163. No package was installed and the host was not modified.

See tests/e2e/README.md#provisioned-environments for the provisioning contract.
EOF
exit 2
