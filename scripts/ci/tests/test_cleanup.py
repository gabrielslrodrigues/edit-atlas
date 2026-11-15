"""Exercise cleanup against disposable dependency trees, never real builds."""

import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


class CleanupTest(unittest.TestCase):
    def test_preserves_installed_tools_and_copies_diagnostics(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary) / "vcpkg with spaces"
            for name in [
                "scripts/marker",
                "downloads/tools/nuget/nuget.exe",
                "downloads/tools/cmake/bin/cmake",
                "downloads/qt.tar.xz",
                "packages/qtbase/library",
                "buildtrees/qtbase/build.log",
            ]:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("fixture")
            diagnostics = Path(temporary) / "diagnostics"
            arguments = [
                "cmake",
                f"-DEDIT_ATLAS_VCPKG_ROOT={root}",
                f"-DEDIT_ATLAS_DIAGNOSTICS_DIR={diagnostics}",
            ]
            environment = os.environ.copy()
            environment.pop("GITHUB_STEP_SUMMARY", None)
            for _ in range(2):
                subprocess.run(
                    [*arguments, "-P", str(ROOT / "cmake/CleanVcpkg.cmake")],
                    check=True,
                    env=environment,
                    capture_output=True,
                )
            tool = root / "downloads/tools/nuget/nuget.exe"
            self.assertTrue(tool.is_file())
            self.assertFalse((root / "downloads/qt.tar.xz").exists())
            self.assertFalse((root / "packages").exists())
            self.assertFalse((root / "buildtrees").exists())
            self.assertEqual(
                (diagnostics / "qtbase/build.log").read_text(), "fixture"
            )
            subprocess.run(
                [
                    *arguments,
                    "-DEDIT_ATLAS_FINAL_CLEANUP=ON",
                    "-P",
                    str(ROOT / "cmake/CleanVcpkg.cmake"),
                ],
                check=True,
                env=environment,
                capture_output=True,
            )
            self.assertFalse((root / "downloads").exists())

    def test_rejects_non_checkout(self):
        with tempfile.TemporaryDirectory() as temporary:
            result = subprocess.run(
                [
                    "cmake",
                    f"-DEDIT_ATLAS_VCPKG_ROOT={temporary}",
                    "-P",
                    str(ROOT / "cmake/CleanVcpkg.cmake"),
                ],
                capture_output=True,
                check=False,
            )
            self.assertNotEqual(result.returncode, 0)
