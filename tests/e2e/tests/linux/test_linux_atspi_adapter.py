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


class NoReplyActionNode:
    actions = {"Press": 0}
    name = "modal menu action"

    def do_action_named(self, action: str) -> bool:
        raise RuntimeError(
            "atspi_error: Did not receive a reply. Possible causes include "
            "the remote application not sending a reply."
        )


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


class AccessibilityNode:
    def __init__(
        self,
        identifier: str,
        children: tuple[object, ...] = (),
        *,
        name: str = "",
        role_name: str = "panel",
    ) -> None:
        self.accessible_id = identifier
        self.children = children
        self.name = name
        self.role_name = role_name
        self.showing = True


class ClosingCheckableNode(SuccessfulActionNode):
    def __init__(self) -> None:
        super().__init__("Remember recent files", "Press")
        self.checked = False

    def do_action_named(self, action: str) -> bool:
        assert action in self.actions
        self.showing = False
        self.invoked.set()
        return True


class UnexpectedTraversalNode:
    accessible_id = "mainWindow"
    showing = True

    @property
    def children(self) -> tuple[object, ...]:
        raise AssertionError("inactive window was traversed")


class TimelineTableNode:
    accessible_id = "QApplication.mainWindow.eventTable"
    showing = True

    @property
    def children(self) -> tuple[object, ...]:
        raise AssertionError("timeline table descendants were traversed")


class PopupNode:
    children = ()
    role_name = "popup menu"

    def __init__(self, showing: bool) -> None:
        self.showing = showing


class MenuActionNode:
    actions = {"ShowMenu": 0}
    name = "File"

    def __init__(self, popup: PopupNode) -> None:
        self.children = (popup,)
        self.invocations = 0
        self.popup = popup

    def do_action_named(self, action: str) -> bool:
        self.invocations += 1
        self.popup.showing = True
        return True


class ComboOptionNode:
    role_name = "list item"
    children = ()

    def __init__(self, name: str) -> None:
        self.name = name


class ComboOptionListNode:
    role_name = "list"

    def __init__(self, options: tuple[ComboOptionNode, ...]) -> None:
        self.name = ""
        self.children = options


class ValueComboBoxNode:
    role_name = "combo box"

    def __init__(self, options: tuple[str, ...]) -> None:
        self._options = options
        self._value = 0.0
        self.name = options[0]
        self.children = (
            ComboOptionListNode(
                tuple(ComboOptionNode(option) for option in options)
            ),
        )

    @property
    def value(self) -> float:
        return self._value

    @value.setter
    def value(self, new_value: float) -> None:
        self._value = new_value
        self.name = self._options[int(new_value)]

    @staticmethod
    def get_interfaces() -> tuple[str, ...]:
        return ("Accessible", "Value")


def application_session(artifact_directory: Path) -> LinuxApplicationSession:
    return LinuxApplicationSession(
        tree=None,
        atspi=None,
        registry=None,
        process=None,
        artifact_directory=artifact_directory,
        timeout=1.0,
    )


def test_identifier_lookup_prioritizes_the_newest_window(tmp_path: Path) -> None:
    session = application_session(tmp_path)
    inactive = UnexpectedTraversalNode()
    dialog = AccessibilityNode("progressDialog")
    application = AccessibilityNode("application", (inactive, dialog))
    session._application = application

    assert session._find_identifier(application, "progressDialog") is dialog


def test_identifier_lookup_skips_timeline_table_descendants(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)
    dialog = AccessibilityNode("progressDialog")
    main_window = AccessibilityNode(
        "mainWindow", (TimelineTableNode(), dialog)
    )
    application = AccessibilityNode("application", (main_window,))
    session._application = application

    assert session._find_identifier(application, "progressDialog") is dialog


def test_accessibility_walk_has_a_bounded_depth(tmp_path: Path) -> None:
    session = application_session(tmp_path)
    node = AccessibilityNode("recursive")
    node.children = (node,)

    walked = list(session._walk(node))

    assert len(walked) == session._MAX_ACCESSIBILITY_DEPTH + 1


def test_named_lookup_prioritizes_dialogs_and_skips_timeline_rows(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)
    dialog_action = AccessibilityNode(
        "cancelButton", name="Cancel", role_name="push button"
    )
    main_window = AccessibilityNode("mainWindow", (TimelineTableNode(),))
    dialog = AccessibilityNode("progressDialog", (dialog_action,))
    application = AccessibilityNode("application", (main_window, dialog))
    session._application = application

    assert session._find_named(application, ("Cancel",)) is dialog_action


def test_file_dialog_lookup_accepts_qt_fallback_identifier(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)
    session._process = RunningProcess()
    dialog = AccessibilityNode(
        "QGuiApplication.mainWindow.FileDialog",
        (AccessibilityNode("fileNameTextField"),),
        name="Open Timeline",
        role_name="dialog",
    )
    session._application = AccessibilityNode("application", (dialog,))

    assert session._file_dialog("timelineOpenFileDialog") is dialog


def test_checkable_menu_action_can_close_before_state_is_observed(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)
    session._process = RunningProcess()
    node = ClosingCheckableNode()
    session.element = lambda identifier: node

    session.set_checked("rememberRecentFilesAction", True)

    assert node.invoked.wait(1.0)


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


def test_no_reply_after_modal_action_does_not_override_observed_state(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)

    session._invoke_action(NoReplyActionNode(), "Press")

    assert not session._action_errors


def test_show_menu_waits_for_open_state_and_does_not_toggle_an_open_menu(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)
    session._process = RunningProcess()
    popup = PopupNode(showing=False)
    menu = MenuActionNode(popup)

    session._activate_node(menu)
    session._activate_node(menu)

    assert popup.showing
    assert menu.invocations == 1


def test_selecting_the_current_option_is_idempotent(tmp_path: Path) -> None:
    session = application_session(tmp_path)
    control = SuccessfulActionNode("24 fps", "ShowMenu")
    session.element = lambda identifier: control

    session.select_option("frameRateSelector", "24 fps")

    assert not control.invoked.is_set()


def test_combo_option_selection_uses_accessible_value(tmp_path: Path) -> None:
    session = application_session(tmp_path)
    session._process = RunningProcess()
    control = ValueComboBoxNode(("Event", "Reel", "Track type"))
    session.element = lambda identifier: control

    session.select_option("filterCondition0Field", "Reel")

    assert control.value == 1.0
    assert control.name == "Reel"


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
