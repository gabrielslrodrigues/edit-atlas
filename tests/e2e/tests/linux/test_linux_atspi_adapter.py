from __future__ import annotations

from pathlib import Path
from threading import Event, Thread

import pytest

from adapters.linux_atspi import (
    ActionNotSupportedError,
    LinuxApplicationSession,
)
from application.polling import wait_until


class BlockingActionNode:
    def __init__(self, action: str) -> None:
        self.actions = {action: 0}
        self.name = "blocking control"
        self.started = Event()
        self.release = Event()

    def do_action_named(self, action: str) -> bool:
        assert action in self.actions
        self.started.set()
        self.release.wait()
        return True


class FailingActionNode:
    actions = {"Press": 0}
    name = "failing control"

    def do_action_named(self, action: str) -> bool:
        raise RuntimeError("synthetic AT-SPI failure")


class SuccessfulActionNode:
    def __init__(self, name: str, action: str) -> None:
        self.actions = {action: 0}
        self.name = name
        self.showing = True
        self.selected = False
        self.invoked = Event()

    def do_action_named(self, action: str) -> bool:
        assert action in self.actions
        self.selected = True
        self.invoked.set()
        return True


class RunningProcess:
    @staticmethod
    def poll() -> None:
        return None


def application_session(artifact_directory: Path) -> LinuxApplicationSession:
    return LinuxApplicationSession(
        tree=None,
        atspi=None,
        registry=None,
        process=None,
        artifact_directory=artifact_directory,
        timeout=1.0,
    )


@pytest.mark.parametrize("action", ["Press", "ShowMenu"])
def test_accessibility_actions_do_not_block_the_driver(
    tmp_path: Path, action: str
) -> None:
    session = application_session(tmp_path)
    node = BlockingActionNode(action)
    invocation_returned = Event()

    def invoke() -> None:
        session._activate_node(node)
        invocation_returned.set()

    caller = Thread(target=invoke, daemon=True)
    caller.start()
    try:
        assert node.started.wait(1.0)
        assert invocation_returned.wait(1.0)
    finally:
        node.release.set()
        caller.join(1.0)


def test_asynchronous_action_failure_surfaces_on_next_interaction(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)
    node = FailingActionNode()

    session._activate_node(node)
    wait_until(
        lambda: bool(session._action_errors),
        lambda failed: failed,
        timeout=1.0,
        description="synthetic action failure to be recorded",
    )

    with pytest.raises(ActionNotSupportedError, match="synthetic AT-SPI failure"):
        session._ensure_running()


def test_option_selection_does_not_wait_for_qt_to_hide_the_option_node(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)
    session._process = RunningProcess()
    control = SuccessfulActionNode("Language", "ShowMenu")
    option = SuccessfulActionNode("English", "Press")
    session.element = lambda identifier: control
    session._find_named = lambda *args, **kwargs: option

    session.select_option("languageSelector", "English")

    assert option.invoked.wait(1.0)
    assert option.selected
    assert option.showing
