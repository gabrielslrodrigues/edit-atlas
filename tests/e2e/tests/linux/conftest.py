from __future__ import annotations

from collections.abc import Generator
import os
from pathlib import Path
import shutil
import warnings

import pytest

from adapters.linux_atspi import LinuxAtspiAdapter
from application.gui import EditAtlasApplication


REPORTS_KEY = pytest.StashKey[dict[str, pytest.TestReport]]()


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(
    item: pytest.Item, call: pytest.CallInfo[None]
) -> Generator[None, None, None]:
    outcome = yield
    report = outcome.get_result()
    reports = item.stash.get(REPORTS_KEY, {})
    reports[report.when] = report
    item.stash[REPORTS_KEY] = reports


@pytest.fixture
def edit_atlas_application(
    request: pytest.FixtureRequest,
    pytestconfig: pytest.Config,
    process_registry,
    state_root: Path,
    artifact_directory: Path,
) -> EditAtlasApplication:
    executable_option = pytestconfig.getoption("app")
    if executable_option is None:
        raise pytest.UsageError("Linux GUI E2E requires --app")
    executable = executable_option.resolve()
    if not executable.is_file():
        raise pytest.UsageError(
            f"installed desktop application does not exist: {executable}"
        )
    if not os.access(executable, os.X_OK):
        raise pytest.UsageError(
            f"installed desktop application is not executable: {executable}"
        )

    test_state_root = state_root / request.node.name
    test_state_root.mkdir(parents=True, exist_ok=True)
    adapter = LinuxAtspiAdapter(
        process_registry,
        artifact_directory,
        pytestconfig.getoption("operation_timeout"),
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
                    f"could not capture Linux E2E failure artifacts: {error}",
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
