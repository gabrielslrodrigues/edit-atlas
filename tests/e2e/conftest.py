from __future__ import annotations

import os
from pathlib import Path
import shutil

import pytest

from adapters.processes import ProcessRegistry
from application.cli import InstalledCli


def pytest_addoption(parser: pytest.Parser) -> None:
    group = parser.getgroup("edit-atlas")
    group.addoption("--cli", type=Path, required=True, help="installed CLI path")
    group.addoption("--app", type=Path, help="installed desktop application path")
    group.addoption(
        "--fixture-dir", type=Path, required=True, help="CMX fixture directory"
    )
    group.addoption("--output-dir", type=Path, required=True)
    group.addoption("--state-root", type=Path, required=True)
    group.addoption("--artifact-dir", type=Path, required=True)
    group.addoption("--locale", default="C.UTF-8")
    group.addoption("--operation-timeout", type=float, default=15.0)


def pytest_collection_finish(session: pytest.Session) -> None:
    if not session.items:
        raise pytest.UsageError("required E2E suite collected no tests")


def pytest_deselected(items: list[pytest.Item]) -> None:
    if items:
        config = items[0].config
        config.stash[DESELECTED_KEY] = config.stash.get(DESELECTED_KEY, 0) + len(
            items
        )


def pytest_sessionfinish(
    session: pytest.Session, exitstatus: int | pytest.ExitCode
) -> None:
    reporter = session.config.pluginmanager.get_plugin("terminalreporter")
    forbidden = session.config.stash.get(DESELECTED_KEY, 0)
    if reporter is not None:
        forbidden += sum(
            len(reporter.stats.get(outcome, []))
            for outcome in ("skipped", "xfailed", "xpassed")
        )
    if forbidden and exitstatus == pytest.ExitCode.OK:
        session.exitstatus = pytest.ExitCode.TESTS_FAILED


DESELECTED_KEY = pytest.StashKey[int]()


@pytest.fixture(scope="session")
def fixture_directory(pytestconfig: pytest.Config) -> Path:
    path = pytestconfig.getoption("fixture_dir").resolve()
    if not path.is_dir():
        raise pytest.UsageError(f"fixture directory does not exist: {path}")
    return path


@pytest.fixture(scope="session")
def output_directory(pytestconfig: pytest.Config) -> Path:
    path = pytestconfig.getoption("output_dir").resolve()
    path.mkdir(parents=True, exist_ok=True)
    return path


@pytest.fixture(scope="session")
def artifact_directory(pytestconfig: pytest.Config) -> Path:
    path = pytestconfig.getoption("artifact_dir").resolve()
    for name in ("accessibility", "logs", "screenshots", "commands"):
        (path / name).mkdir(parents=True, exist_ok=True)
    return path


@pytest.fixture(scope="session")
def state_root(pytestconfig: pytest.Config) -> Path:
    path = pytestconfig.getoption("state_root").resolve()
    if path.exists():
        marker = path / ".edit-atlas-e2e-state"
        if not marker.is_file():
            raise pytest.UsageError(
                f"refusing to remove unowned state directory: {path}"
            )
        shutil.rmtree(path)
    path.mkdir(parents=True)
    marker = path / ".edit-atlas-e2e-state"
    marker.write_text("owned by the Edit Atlas E2E harness\n", encoding="utf-8")
    try:
        yield path
    finally:
        if marker.is_file():
            shutil.rmtree(path, ignore_errors=True)


@pytest.fixture(scope="session")
def process_registry() -> ProcessRegistry:
    registry = ProcessRegistry()
    try:
        yield registry
    finally:
        registry.close_all()


@pytest.fixture(scope="session")
def installed_cli(
    pytestconfig: pytest.Config,
    process_registry: ProcessRegistry,
    state_root: Path,
    artifact_directory: Path,
) -> InstalledCli:
    executable = pytestconfig.getoption("cli").resolve()
    if not executable.is_file():
        raise pytest.UsageError(f"installed CLI does not exist: {executable}")
    if os.name != "nt" and not os.access(executable, os.X_OK):
        raise pytest.UsageError(f"installed CLI is not executable: {executable}")
    return InstalledCli(
        executable,
        process_registry,
        state_root,
        artifact_directory / "commands",
        pytestconfig.getoption("operation_timeout"),
        pytestconfig.getoption("locale"),
    )
