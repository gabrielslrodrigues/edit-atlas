"""Fail fast unless this process is a trusted macOS AX client."""

from __future__ import annotations

from pathlib import Path

from adapters.macos_ax import AccessibilityBackendError, MacAxAdapter
from adapters.processes import ProcessRegistry


def main() -> int:
    adapter = MacAxAdapter(ProcessRegistry(), Path("build/e2e/artifacts"), 15.0)
    try:
        adapter.preflight()
    except AccessibilityBackendError as error:
        print(f"macOS Accessibility preflight failed: {error}")
        return 2
    print("macOS Accessibility preflight passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
