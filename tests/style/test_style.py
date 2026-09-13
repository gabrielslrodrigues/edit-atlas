"""Exercise the standalone style command in disposable repositories."""

import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BOILERPLATE = (
    (ROOT / "src/core/version.cpp")
    .read_text(encoding="utf-8")
    .split("#include", 1)[0]
)


class StyleCommandTest(unittest.TestCase):
    """Use the real pinned tools without touching the checkout."""

    def setUp(self) -> None:
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        (self.root / "tools/style").mkdir(parents=True)
        for name in (
            ".clang-format",
            "CPPLINT.cfg",
            "ruff.toml",
            "tools/style/requirements.txt",
            "tools/style/style.py",
            "tools/style/Invoke-Style.ps1",
            "tools/style/PSScriptAnalyzerSettings.psd1",
        ):
            shutil.copyfile(ROOT / name, self.root / name)
        if sys.platform == "win32":
            shutil.copytree(
                ROOT / "tools/style/modules",
                self.root / "tools/style/modules",
            )
        (self.root / ".gitignore").write_text(
            "/tools/style/modules/\n", encoding="utf-8", newline="\n"
        )
        subprocess.run(["git", "init", "--quiet", str(self.root)], check=True)

    def invoke(self, mode: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [sys.executable, str(self.root / "tools/style/style.py"), mode],
            cwd=self.root,
            text=True,
            capture_output=True,
            check=False,
        )

    def snapshot(self) -> dict[str, bytes]:
        return {
            path.relative_to(self.root).as_posix(): path.read_bytes()
            for path in self.root.rglob("*")
            if path.is_file()
            and ".git" not in path.relative_to(self.root).parts
        }

    def test_format_is_idempotent_and_check_is_read_only(self) -> None:
        source = self.root / "sample.cpp"
        source.write_text(
            BOILERPLATE + "int main(void){return 0;}\n",
            encoding="utf-8",
            newline="\n",
        )
        if sys.platform == "win32":
            (self.root / "sample.ps1").write_text(
                "function Get-Sample {return 'value'}\nGet-Sample\n",
                encoding="utf-8",
                newline="\n",
            )
        result = self.invoke("format")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        formatted = self.snapshot()
        result = self.invoke("format")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.snapshot(), formatted)
        result = self.invoke("check")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(self.snapshot(), formatted)

    def test_rejects_representative_violations_without_writing(self) -> None:
        cases = {
            "sample.cpp": BOILERPLATE + "int main(void){return 0;}\n",
            "sample.hpp": BOILERPLATE + "#pragma once\n",
            "sample.py": "import os\n",
            "sample.md": "Trailing whitespace. \r\n",
        }
        if sys.platform != "win32":
            cases["sample.sh"] = (
                "#!/usr/bin/env bash\nset -euo pipefail\necho $missing\n"
            )
        else:
            cases["sample.ps1"] = "function Frobnicate-Thing {\n  'value'\n}\n"
        for name, content in cases.items():
            with self.subTest(name=name):
                path = self.root / name
                path.write_bytes(content.encode("utf-8"))
                before = self.snapshot()
                result = self.invoke("check")
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(name, result.stdout + result.stderr)
                self.assertEqual(self.snapshot(), before)
                path.unlink()

    def test_inventory_includes_dotfiles_and_preserves_fixtures(self) -> None:
        fixture = self.root / "tests/fixtures/cmx3600/bytes.edl"
        fixture.parent.mkdir(parents=True)
        fixture.write_bytes(b"\xff\r\n")
        (self.root / ".editorconfig").write_text(
            "root = true\n", newline="\n"
        )
        result = self.invoke("inventory")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        expected = {
            path.relative_to(self.root).as_posix()
            for path in self.root.rglob("*")
            if path.is_file()
            and ".git" not in path.relative_to(self.root).parts
            and "modules" not in path.relative_to(self.root).parts
        }
        reported = {
            line.split("\t", 1)[0] for line in result.stdout.splitlines()
        }
        self.assertEqual(reported, expected)
        self.assertIn(
            "preserved: byte-sensitive parser fixture", result.stdout
        )
        self.assertEqual(fixture.read_bytes(), b"\xff\r\n")

    def test_unknown_file_type_fails_inventory(self) -> None:
        (self.root / "unclassified.custom").write_text(
            "data\n", newline="\n"
        )
        result = self.invoke("inventory")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unclassified.custom", result.stderr)

    def test_missing_pinned_package_fails_before_formatting(self) -> None:
        requirements = self.root / "tools/style/requirements.txt"
        requirements.write_text(
            "edit-atlas-nonexistent-style-tool==0.0.0\n", newline="\n"
        )
        before = self.snapshot()
        result = self.invoke("format")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("edit-atlas-nonexistent-style-tool", result.stderr)
        self.assertEqual(self.snapshot(), before)


if __name__ == "__main__":
    unittest.main()
