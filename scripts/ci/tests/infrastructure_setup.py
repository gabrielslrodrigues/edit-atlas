"""Exercise infrastructure evidence using disposable commands, never a network."""

import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
WRAPPER = ROOT / "scripts/ci/run-infrastructure.sh"
DOWNLOAD = ROOT / "scripts/ci/download-artifacts.sh"
PREFIX = "EDIT_ATLAS_CI_FAILURE "


class InfrastructureTest(unittest.TestCase):
    def invoke(self, output, status):
        return subprocess.run(
            [
                "bash",
                str(WRAPPER),
                "bash",
                "-c",
                'printf "%s\\n" "$1"; exit "$2"',
                "fixture",
                output,
                str(status),
            ],
            capture_output=True,
            text=True,
            check=False,
        )

    def test_transient_errors_emit_evidence_and_preserve_exit_status(self):
        for output in [
            "Could not resolve 'azure.archive.ubuntu.com'",
            "ETIMEDOUT",
            "HTTP 503",
            "HTTP/2 502",
            "HTTP 504",
            "dial tcp: lookup api.github.com: no such host",
            "connection reset by peer",
            "i/o timeout",
        ]:
            with self.subTest(output=output):
                result = self.invoke(output, 100)
                self.assertEqual(result.returncode, 100)
                self.assertIn(output, result.stdout)
                record = json.loads(result.stdout.split(PREFIX)[-1])
                self.assertEqual(record["category"], "transient-network")

    def test_permanent_and_unknown_failures_stay_blocking(self):
        for output in [
            "Unknown failure",
            "ETIMEDOUT\nUnable to locate package fixture",
            "ETIMEDOUT\nHTTP 403",
            "ETIMEDOUT\nno artifact matches any of the names",
            "ETIMEDOUT\nchecksum mismatch",
            "ETIMEDOUT\nno space left on device",
        ]:
            with self.subTest(output=output):
                result = self.invoke(output, 7)
                self.assertEqual(result.returncode, 7)
                record = json.loads(result.stdout.split(PREFIX)[-1])
                self.assertEqual(record["category"], "unknown")

    def test_success_after_internal_retry_emits_no_failure(self):
        result = self.invoke("ETIMEDOUT\nRetry succeeded", 0)
        self.assertEqual(result.returncode, 0)
        self.assertNotIn(PREFIX, result.stdout)

    def test_download_passes_literal_names_paths_and_run_identity(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            gh = directory / "gh"
            gh.write_text('#!/usr/bin/env bash\nprintf "%s\\n" "$@"\n')
            gh.chmod(0o755)
            for name, pattern, selector in [
                ("package with spaces", "", "--name"),
                ("", "*-frontend-*-osx-stage", "--pattern"),
            ]:
                environment = os.environ | {
                    "PATH": f"{directory}:{os.environ['PATH']}",
                    "ARTIFACT_NAME": name,
                    "ARTIFACT_PATTERN": pattern,
                    "ARTIFACT_DIRECTORY": "build/my packages",
                    "GITHUB_RUN_ID": "42",
                    "GITHUB_REPOSITORY": "owner/project",
                }
                result = subprocess.run(
                    ["bash", str(WRAPPER), "bash", str(DOWNLOAD)],
                    env=environment,
                    capture_output=True,
                    text=True,
                    check=False,
                )
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertEqual(
                    result.stdout.splitlines(),
                    [
                        "run",
                        "download",
                        "42",
                        "--repo",
                        "owner/project",
                        "--dir",
                        "build/my packages",
                        selector,
                        name or pattern,
                    ],
                )
