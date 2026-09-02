from __future__ import annotations

from collections.abc import Generator
import os
from pathlib import Path
import shutil
import sys
import warnings

import pytest

from adapters.processes import ProcessRegistry
from application.cli import InstalledCli
from application.gui import EditAtlasApplication
from application.media_fixtures import (
    regeneration_command,
    stale_fixture_reason,
)


REPORTS_KEY = pytest.StashKey[dict[str, pytest.TestReport]]()
REPOSITORY_ROOT = Path(__file__).resolve().parents[2]


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(
    item: pytest.Item, call: pytest.CallInfo[None]
) -> Generator[None, None, None]:
    outcome = yield
    report = outcome.get_result()
    reports = item.stash.get(REPORTS_KEY, {})
    reports[report.when] = report
    item.stash[REPORTS_KEY] = reports


def pytest_addoption(parser: pytest.Parser) -> None:
    group = parser.getgroup("edit-atlas")
    group.addoption("--cli", type=Path, required=True, help="installed CLI path")
    group.addoption("--app", type=Path, help="installed desktop application path")
    group.addoption(
        "--fixture-dir", type=Path, required=True, help="CMX fixture directory"
    )
    group.addoption(
        "--media-fixture-dir",
        type=Path,
        help="generated rendered-video fixture directory",
    )
    group.addoption("--output-dir", type=Path, required=True)
    group.addoption("--state-root", type=Path, required=True)
    group.addoption("--artifact-dir", type=Path, required=True)
    group.addoption("--locale", default="C.UTF-8")
    group.addoption("--operation-timeout", type=float, default=15.0)
    group.addoption("--startup-timeout", type=float, default=60.0)


def pytest_collection_finish(session: pytest.Session) -> None:
    if not session.items:
        raise pytest.UsageError("required E2E suite collected no tests")
    # Reject fixtures left over from an older generator before anything
    # runs, rather than when the first rendered-video scenario reaches
    # them. The directory is optional, so an absent option is not an error.
    option = session.config.getoption("media_fixture_dir")
    if option is None:
        return
    directory = option.resolve()
    stale = stale_fixture_reason(REPOSITORY_ROOT, directory)
    if stale is not None:
        raise pytest.UsageError(_stale_fixture_message(directory, stale))


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


def _stale_fixture_message(directory: Path, reason: str) -> str:
    return (
        f"rendered-video fixtures in {directory} are not current: {reason}. "
        "Regenerate them from a configured build tree:\n"
        f"{regeneration_command(directory)}"
    )


@pytest.fixture(scope="session")
def fixture_directory(pytestconfig: pytest.Config) -> Path:
    path = pytestconfig.getoption("fixture_dir").resolve()
    if not path.is_dir():
        raise pytest.UsageError(f"fixture directory does not exist: {path}")
    return path


@pytest.fixture(scope="session")
def media_fixture_directory(pytestconfig: pytest.Config) -> Path:
    option = pytestconfig.getoption("media_fixture_dir")
    if option is None:
        raise pytest.UsageError(
            "desktop rendered-video E2E requires --media-fixture-dir"
        )
    path = option.resolve()
    if not path.is_dir():
        raise pytest.UsageError(
            f"rendered-video fixture directory does not exist: {path}"
        )
    required = {
        "cancellation-render.mov",
        "cancellation.edl",
        "incompatible-timecode.mov",
        "matching-render.mov",
        "missing-timecode.mov",
    }
    missing = sorted(name for name in required if not (path / name).is_file())
    if missing:
        raise pytest.UsageError(
            "rendered-video fixture directory is incomplete: "
            + ", ".join(missing)
        )
    stale = stale_fixture_reason(REPOSITORY_ROOT, path)
    if stale is not None:
        raise pytest.UsageError(_stale_fixture_message(path, stale))
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


@pytest.fixture
def edit_atlas_application(
    request: pytest.FixtureRequest,
    pytestconfig: pytest.Config,
    process_registry: ProcessRegistry,
    state_root: Path,
    artifact_directory: Path,
) -> EditAtlasApplication:
    executable_option = pytestconfig.getoption("app")
    if executable_option is None:
        raise pytest.UsageError("desktop E2E requires --app")
    executable = executable_option.resolve()
    if not executable.is_file():
        raise pytest.UsageError(
            f"installed desktop application does not exist: {executable}"
        )
    if os.name != "nt" and not os.access(executable, os.X_OK):
        raise pytest.UsageError(
            f"installed desktop application is not executable: {executable}"
        )

    test_state_root = state_root / request.node.name
    test_state_root.mkdir(parents=True, exist_ok=True)
    timeout = pytestconfig.getoption("operation_timeout")
    startup_timeout = pytestconfig.getoption("startup_timeout")
    if sys.platform == "linux":
        from adapters.linux_atspi import LinuxAtspiAdapter

        adapter = LinuxAtspiAdapter(
            process_registry,
            artifact_directory,
            timeout,
            startup_timeout=startup_timeout,
        )
    elif sys.platform == "win32":
        from adapters.windows_uia import WindowsUiaAdapter

        adapter = WindowsUiaAdapter(
            process_registry,
            artifact_directory,
            timeout,
            startup_timeout=startup_timeout,
        )
    elif sys.platform == "darwin":
        from adapters.macos_ax import MacAxAdapter

        adapter = MacAxAdapter(
            process_registry,
            artifact_directory,
            timeout,
            startup_timeout=startup_timeout,
        )
    else:
        raise pytest.UsageError(
            f"packaged desktop E2E is unsupported on {sys.platform!r}"
        )
    adapter.preflight()

    def launch(log_name: str):
        return adapter.launch(
            executable,
            test_state_root,
            locale=pytestconfig.getoption("locale"),
            log_name=f"{request.node.name}-{log_name}",
        )

    application = EditAtlasApplication(launch)
    try:
        yield application
    finally:
        reports = request.node.stash.get(REPORTS_KEY, {})
        failed = any(report.failed for report in reports.values())
        if failed:
            try:
                application.capture_artifacts(request.node.name)
            except Exception as error:
                warnings.warn(
                    f"could not capture desktop E2E failure artifacts: {error}",
                    RuntimeWarning,
                    stacklevel=1,
                )
        application.close()
        if failed:
            log_directory = test_state_root / "logs"
            if log_directory.is_dir():
                for log_path in log_directory.iterdir():
                    if log_path.is_file():
                        shutil.copy2(
                            log_path,
                            artifact_directory
                            / "logs"
                            / f"{request.node.name}-{log_path.name}",
                        )
