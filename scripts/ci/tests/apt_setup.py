"""Exercise APT failures without installing packages or changing the host."""

import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
INSTALLER = ROOT / "scripts/ci/install-ubuntu-dependencies.sh"
APT = '''#!/usr/bin/env bash
set -euo pipefail
printf '%s\\n' "$*" >> "$APT_TEST_LOG"
for required in APT::Update::Error-Mode=any Acquire::Retries=3 \\
    Acquire::http::Timeout=30 Acquire::https::Timeout=30; do
  [[ " $* " == *" $required "* ]] || exit 90
done
if [[ " $* " == *" update "* || "${!#}" == update ]]; then
  case "$APT_TEST_RESULT" in
    dns)
      echo "Temporary failure resolving 'archive.ubuntu.com'" >&2
      exit 100
      ;;
    recovered)
      echo 'Repository available after bounded APT retries'
      ;;
  esac
elif [[ "$APT_TEST_RESULT" == missing ]]; then
  echo 'E: Unable to locate package fixture' >&2
  exit 100
fi
'''


class AptSetupTest(unittest.TestCase):
    def test_setup_stops_on_bad_indexes_and_permanent_package_errors(self):
        path = ROOT / ".github/workflows/package-verification.yml"
        workflow = path.read_text()
        marker = "      - name: Install Ubuntu verification tools"
        step = workflow.split(marker, 1)[1]
        step = step.split("      - name: Install Fedora", 1)[0]
        body = step.split("        run: |\n", 1)[1]
        verification = "\n".join(line[10:] for line in body.splitlines())
        # Only package setup is exercised; source-rewriting is not run on
        # the developer's host. The same options remain in the extracted body.
        verification = verification[verification.index("apt_options=(") :]
        for profile in ["host", "verification"]:
            for outcome in ["dns", "recovered", "missing"]:
                with self.subTest(profile=profile, outcome=outcome):
                    with tempfile.TemporaryDirectory() as temporary:
                        directory = Path(temporary)
                        scripts = {
                            "apt-get": APT,
                            "sudo": '#!/usr/bin/env bash\nexec "$@"\n',
                            "tee": '#!/usr/bin/env bash\ncat >/dev/null\n',
                        }
                        for name, contents in scripts.items():
                            path = directory / name
                            path.write_text(contents)
                            path.chmod(0o755)
                        log = directory / "apt.log"
                        environment = os.environ | {
                            "PATH": f"{directory}:{os.environ['PATH']}",
                            "APT_TEST_LOG": str(log),
                            "APT_TEST_RESULT": outcome,
                        }
                        if profile == "host":
                            command = ["bash", str(INSTALLER), "--build"]
                        else:
                            command = [
                                "bash",
                                "-euo",
                                "pipefail",
                                "-c",
                                verification,
                            ]
                        result = subprocess.run(
                            command,
                            env=environment,
                            capture_output=True,
                            text=True,
                            check=False,
                        )
                        self.assertEqual(
                            result.returncode,
                            0 if outcome == "recovered" else 100,
                            result.stderr,
                        )
                        calls = log.read_text().splitlines()
                        self.assertEqual(
                            len(calls), 1 if outcome == "dns" else 2
                        )
