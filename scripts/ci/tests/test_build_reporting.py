"""Check that interrupted builds remain visible in cache diagnostics."""

import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


class BuildReportingTest(unittest.TestCase):
    def test_reports_started_and_completed_packages_separately(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            (directory / "configure-debug.log").write_text(
                "Restored 2 package(s) from feed\n"
                "Building qtbase[core,gui]:x64-windows@6.11.1...\n"
                "Elapsed time to handle qtbase:x64-windows: 1 h\n"
                "Building qttools:x64-windows@6.11.1...\n"
                "Unable to load the service index\n"
            )
            environment = os.environ.copy()
            environment.pop("GITHUB_STEP_SUMMARY", None)
            result = subprocess.run(
                [
                    "pwsh",
                    "-NoProfile",
                    "-File",
                    str(ROOT / "scripts/ci/report-vcpkg-install.ps1"),
                    "-LogDirectory",
                    str(directory),
                ],
                capture_output=True,
                text=True,
                check=True,
                env=environment,
            )
            self.assertIn(
                "2 restored; 2 builds started; 1 completed", result.stdout
            )
            self.assertIn("1 cache-error log lines", result.stdout)

    def test_missing_log_is_unknown_not_zero_builds(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = subprocess.run(
                [
                    "pwsh",
                    "-NoProfile",
                    "-File",
                    str(ROOT / "scripts/ci/report-vcpkg-install.ps1"),
                    "-LogDirectory",
                    temporary,
                ],
                capture_output=True,
                text=True,
                check=True,
            )
            self.assertIn("cache activity is unknown", result.stdout)
            self.assertNotIn("0 builds", result.stdout)
