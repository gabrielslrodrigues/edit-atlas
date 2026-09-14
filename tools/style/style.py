"""Format or check owned files without configuring the application."""

import argparse
import re
import shutil
import subprocess
import sys
import sysconfig
from collections import Counter
from importlib.metadata import PackageNotFoundError, version
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL_DIRECTORY = Path(sysconfig.get_path("scripts"))
PRESERVED = {
    "vcpkg": "pinned third-party submodule",
    ".gitmodules": "git-generated; git writes tab indentation itself",
    "LICENSE": "standard licence text",
    "src/frontends/resources/fonts/LICENSE.txt": "third-party licence text",
    "tests/e2e/uv.lock": "generated dependency lock; update with uv",
    "vcpkg-overlays/libxlsxwriter/minizip.diff": (
        "byte-sensitive upstream patch"
    ),
}
BINARY_SUFFIXES = {".ttf", ".icns", ".ico", ".png"}
TEXT_SUFFIXES = {
    ".cmake",
    ".in",
    ".ini",
    ".json",
    ".md",
    ".qss",
    ".toml",
    ".ts",
    ".txt",
    ".yaml",
    ".yml",
    ".desktop",
    ".dockerignore",
}
TEXT_NAMES = {
    ".clang-format",
    ".editorconfig",
    ".gitattributes",
    ".gitignore",
    ".python-version",
    ".shellcheckrc",
    "Containerfile",
    "CPPLINT.cfg",
}


def run(arguments: list[str]) -> bool:
    """Run a tool, retaining its diagnostics and failure status."""
    print("+", " ".join(arguments), flush=True)
    return subprocess.run(arguments, cwd=ROOT, check=False).returncode == 0


def inventory() -> dict[str, str]:
    """Classify tracked and unignored files, skipping gitlinks."""
    output = subprocess.check_output(
        [
            "git",
            "ls-files",
            "--cached",
            "--others",
            "--exclude-standard",
            "-z",
        ],
        cwd=ROOT,
    )
    result = {}
    for raw_name in sorted(set(output.split(b"\0")) - {b""}):
        name = raw_name.decode("utf-8")
        path = Path(name)
        if name in PRESERVED:
            kind = "preserved: " + PRESERVED[name]
        elif path.suffix in BINARY_SUFFIXES:
            kind = "preserved: binary asset"
        elif name.startswith("tests/fixtures/") and path.suffix == ".edl":
            kind = "preserved: byte-sensitive parser fixture"
        elif path.suffix in {".cpp", ".hpp"}:
            kind = "cpp"
        elif path.suffix == ".py":
            kind = "python"
        elif path.suffix == ".sh":
            kind = "bash"
        elif path.suffix in {".ps1", ".psd1"}:
            kind = "powershell"
        elif path.suffix == ".qml":
            kind = "qml"
        elif path.suffix in TEXT_SUFFIXES or path.name in TEXT_NAMES:
            kind = "text"
        else:
            raise ValueError(f"No style policy for tracked file: {name}")
        result[name] = kind
    if not result:
        raise ValueError("The style inventory is empty")
    return result


def tools() -> dict[str, str]:
    """Require pinned dependencies without installing them."""
    requirements = ROOT / "tools/style/requirements.txt"
    required_versions = {}
    for line in requirements.read_text(encoding="utf-8").splitlines():
        if ";" in line and sys.platform == "win32":
            continue
        package, required = line.split(";", 1)[0].strip().split("==")
        actual = version(package)
        if actual != required:
            raise ValueError(f"{package}: expected {required}, found {actual}")
        required_versions[package] = required
    commands = ["clang-format", "cpplint", "ruff"]
    if sys.platform != "win32":
        commands.append("shellcheck")
    result = {}
    for command in commands:
        executable = shutil.which(command, path=str(TOOL_DIRECTORY))
        if executable is None:
            raise ValueError(f"Missing pinned style executable: {command}")
        # shellcheck-py adds a packaging suffix to ShellCheck's version.
        required = (
            required_versions["shellcheck-py"].rsplit(".", 1)[0]
            if command == "shellcheck"
            else required_versions[command]
        )
        reported = subprocess.check_output(
            [executable, "--version"],
            cwd=ROOT,
            text=True,
            stderr=subprocess.STDOUT,
        )
        if re.search(rf"\b{re.escape(required)}(?![\w.])", reported) is None:
            raise ValueError(
                f"{command}: expected executable version {required}, "
                f"found {reported.strip()}"
            )
        result[command] = executable
    if sys.platform == "win32":
        executable = shutil.which("pwsh")
        if executable is None:
            raise ValueError(
                "PowerShell 7 is required for Windows style checks"
            )
        result["pwsh"] = executable
    return result


def check_text(name: str) -> bool:
    """Enforce text invariants without rewriting content or literals."""
    data = (ROOT / name).read_bytes()
    text = data.decode("utf-8")
    problems = []
    if data.startswith(b"\xef\xbb\xbf"):
        problems.append("UTF-8 BOM")
    if "\r" in text:
        problems.append("non-LF line ending")
    if data and not data.endswith(b"\n"):
        problems.append("missing final newline")
    for number, line in enumerate(text.splitlines(), 1):
        if line.rstrip(" \t") != line:
            problems.append(f"line {number}: trailing whitespace")
        if "\t" in line:
            problems.append(f"line {number}: tab character")
    if name.endswith((".cpp", ".hpp")):
        if not text.startswith(
            "// Licensed under the Apache License, Version 2.0"
        ):
            problems.append("missing Apache 2.0 licence boilerplate")
    if name.endswith(".hpp"):
        logical = (
            name.split("/include/", 1)[1]
            if "/include/edit_atlas/" in name
            else "edit_atlas/" + name.removeprefix("src/")
        )
        guard = re.sub(r"[^A-Za-z0-9]", "_", logical).upper() + "_"
        if not re.search(rf"^#ifndef {guard}\n#define {guard}$", text, re.M):
            problems.append(f"expected include guard {guard}")
        if not re.search(rf"^#endif  // {guard}\s*$", text, re.M):
            problems.append(f"expected closing guard comment {guard}")
        if "#pragma once" in text:
            problems.append("#pragma once is prohibited")
    for problem in problems:
        print(f"{name}: {problem}", file=sys.stderr)
    return not problems


def main() -> int:
    """Format or check the complete file inventory."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("check", "format", "inventory"))
    arguments = parser.parse_args()
    files = inventory()
    if arguments.mode == "inventory":
        for name, kind in files.items():
            print(f"{name}\t{kind}")
        return 0
    commands = tools()
    checking = arguments.mode == "check"
    success = True
    for name, kind in files.items():
        if kind.startswith("preserved:"):
            continue
        if checking:
            success = check_text(name) and success
        if kind == "cpp":
            options = ["--dry-run", "--Werror"] if checking else ["-i"]
            success = (
                run([commands["clang-format"], *options, name]) and success
            )
            if checking:
                success = run([commands["cpplint"], name]) and success
        elif kind == "python":
            options = ["--check"] if checking else []
            success = (
                run(
                    [
                        commands["ruff"],
                        "format",
                        "--no-cache",
                        *options,
                        name,
                    ]
                )
                and success
            )
            if checking:
                success = (
                    run(
                        [
                            commands["ruff"],
                            "check",
                            "--no-cache",
                            name,
                        ]
                    )
                    and success
                )
        elif kind == "bash" and checking and sys.platform != "win32":
            success = (
                run([commands["shellcheck"], "--external-sources", name])
                and success
            )
        elif kind == "powershell" and sys.platform == "win32":
            success = (
                run(
                    [
                        commands["pwsh"],
                        "-NoProfile",
                        "-File",
                        str(ROOT / "tools/style/Invoke-Style.ps1"),
                        "-Mode",
                        arguments.mode,
                        "-SourcePath",
                        str(ROOT / name),
                    ]
                )
                and success
            )
    for kind, count in sorted(Counter(files.values()).items()):
        print(f"{kind}: {count}")
    print(
        "QML semantic lint and translations remain build targets; "
        "see CONTRIBUTING.md."
    )
    return 0 if success else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (
        OSError,
        ValueError,
        PackageNotFoundError,
        subprocess.CalledProcessError,
    ) as error:
        sys.exit(str(error))
