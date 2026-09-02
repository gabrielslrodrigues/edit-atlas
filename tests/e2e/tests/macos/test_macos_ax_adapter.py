from __future__ import annotations

from pathlib import Path
from threading import Event
from time import monotonic
from typing import Any, Callable

import pytest

from adapters.macos_ax import (
    ElementNotFoundError,
    MacApplicationSession,
    MacElement,
    StartupNotReadyError,
)


class Node:
    def __init__(
        self,
        **attributes: Any,
    ) -> None:
        self.attributes = attributes
        self.actions: dict[str, Callable[[], None]] = {}
        self.settable: set[str] = set()


class FakeAx:
    kAXErrorSuccess = 0
    kAXErrorCannotComplete = -25204
    kCGEventFlagMaskCommand = 1 << 20
    kCGEventFlagMaskShift = 1 << 17
    kCGHIDEventTap = 0

    def __init__(self, application: Node, system: Node) -> None:
        self.application = application
        self.system = system
        self.keyboard_events: list[tuple[int, bool, int]] = []

    def AXUIElementCreateApplication(self, pid: int) -> Node:
        assert pid == 42
        return self.application

    def AXUIElementCreateSystemWide(self) -> Node:
        return self.system

    @staticmethod
    def AXUIElementSetMessagingTimeout(node: Node, timeout: float) -> int:
        assert timeout > 0
        return 0

    @staticmethod
    def AXUIElementCopyAttributeValue(
        node: Node, attribute: str, output: None
    ) -> tuple[int, Any]:
        assert output is None
        if attribute not in node.attributes:
            return 1, None
        return 0, node.attributes[attribute]

    @staticmethod
    def AXUIElementCopyActionNames(
        node: Node, output: None
    ) -> tuple[int, list[str]]:
        assert output is None
        return 0, list(node.actions)

    @staticmethod
    def AXUIElementIsAttributeSettable(
        node: Node, attribute: str, output: None
    ) -> tuple[int, bool]:
        assert output is None
        return 0, attribute in node.settable

    @staticmethod
    def AXUIElementSetAttributeValue(
        node: Node, attribute: str, value: Any
    ) -> int:
        node.attributes[attribute] = value
        return 0

    @staticmethod
    def AXUIElementPerformAction(node: Node, action: str) -> int:
        node.actions[action]()
        return 0

    @staticmethod
    def CGEventCreateKeyboardEvent(
        source: None, key_code: int, pressed: bool
    ) -> dict[str, Any]:
        assert source is None
        return {"key_code": key_code, "pressed": pressed, "flags": 0}

    @staticmethod
    def CGEventSetFlags(event: dict[str, Any], flags: int) -> None:
        event["flags"] = flags

    def CGEventPost(self, tap: int, event: dict[str, Any]) -> None:
        assert tap == self.kCGHIDEventTap
        self.keyboard_events.append(
            (event["key_code"], event["pressed"], event["flags"])
        )


class RunningProcess:
    pid = 42

    @staticmethod
    def poll() -> None:
        return None


def application_session(
    artifact_directory: Path,
    application: Node | None = None,
    system: Node | None = None,
    *,
    timeout: float = 1.0,
    startup_timeout: float = 1.0,
) -> tuple[MacApplicationSession, FakeAx]:
    application = application or Node(AXRole="AXApplication")
    system = system or Node(AXRole="AXSystemWide")
    ax = FakeAx(application, system)
    return (
        MacApplicationSession(
            ax=ax,
            registry=None,
            process=RunningProcess(),
            artifact_directory=artifact_directory,
            timeout=timeout,
            startup_timeout=startup_timeout,
        ),
        ax,
    )


def test_identifier_lookup_accepts_qt_hierarchical_identifier(
    tmp_path: Path,
) -> None:
    language = Node(
        AXRole="AXPopUpButton",
        AXIdentifier="QApplication.mainWindow.languageSelector",
        AXVisible=True,
    )
    application = Node(AXRole="AXApplication", AXChildren=[language])
    session, _ = application_session(tmp_path, application)

    assert session.element("languageSelector")._raw is language
    assert not session.has_element("otherSelector")


def test_ax_actions_do_not_block_the_driver(tmp_path: Path) -> None:
    session, _ = application_session(tmp_path)
    started = Event()
    release = Event()
    button = Node(AXRole="AXButton", AXTitle="Open")

    def press() -> None:
        started.set()
        release.wait()

    button.actions["AXPress"] = press
    session.element = lambda identifier: MacElement(session, button)
    try:
        session.activate("openDocumentAction")
        assert started.wait(1.0)
    finally:
        release.set()


def test_checked_state_uses_ax_value_and_press(tmp_path: Path) -> None:
    session, _ = application_session(tmp_path)
    checkbox = Node(AXRole="AXCheckBox", AXTitle="Timeline", AXValue=0)
    checkbox.actions["AXPress"] = lambda: checkbox.attributes.update(AXValue=1)
    session.element = lambda identifier: MacElement(session, checkbox)

    session.set_checked("includeTimelineSheetCheckBox", True)

    assert session.is_checked("includeTimelineSheetCheckBox")


def test_combo_selection_uses_ax_menu_actions(tmp_path: Path) -> None:
    session, _ = application_session(tmp_path)
    control = Node(AXRole="AXPopUpButton", AXValue="Event")
    option = Node(AXRole="AXMenuItem", AXTitle="Reel")
    control.actions["AXShowMenu"] = lambda: None
    option.actions["AXPress"] = lambda: control.attributes.update(AXValue="Reel")
    session.element = lambda identifier: MacElement(session, control)
    session._find_option_in_roots = lambda name: option

    session.select_option("filterCondition0Field", "Reel")

    assert session.selected_option("filterCondition0Field") == "Reel"


def test_checkable_list_item_uses_ax_value_and_selection(tmp_path: Path) -> None:
    item = Node(
        AXRole="AXRow",
        AXTitle="Comments",
        AXValue=0,
        AXSelected=False,
    )
    item.settable.add("AXSelected")
    item.actions["AXPress"] = lambda: item.attributes.update(AXValue=1)
    control = Node(AXRole="AXList", AXChildren=[item])
    session, _ = application_session(tmp_path)
    session.element = lambda identifier: MacElement(session, control)

    session.set_list_item_checked("eventColumnsList", "Comments", True)
    session.select_list_item("eventColumnsList", "Comments")

    assert session.is_list_item_checked("eventColumnsList", "Comments")
    assert item.attributes["AXSelected"]


def test_native_open_panel_uses_go_to_folder_and_ax_actions(
    tmp_path: Path,
) -> None:
    editor = Node(AXRole="AXTextField", AXFocused=True, AXValue="")
    editor.settable.add("AXValue")
    go_button = Node(AXRole="AXButton", AXTitle="Go")
    go_sheet = Node(
        AXRole="AXSheet",
        AXVisible=True,
        AXChildren=[editor, go_button],
        AXDefaultButton=go_button,
    )
    open_button = Node(AXRole="AXButton", AXTitle="Open")
    dialog = Node(
        AXRole="AXWindow",
        AXVisible=True,
        AXChildren=[open_button],
        AXDefaultButton=open_button,
    )
    focused_application = Node(
        AXRole="AXApplication", AXFocusedWindow=dialog
    )
    system = Node(
        AXRole="AXSystemWide", AXFocusedApplication=focused_application
    )
    session, ax = application_session(tmp_path, system=system)

    original_shortcut = session._send_go_to_folder_shortcut

    def open_go_sheet() -> None:
        focused_application.attributes["AXFocusedWindow"] = go_sheet
        original_shortcut()

    session._send_go_to_folder_shortcut = open_go_sheet

    def accept_go_sheet() -> None:
        go_sheet.attributes["AXVisible"] = False
        go_sheet.attributes.pop("AXRole")
        focused_application.attributes["AXFocusedWindow"] = dialog

    def accept_open_panel() -> None:
        dialog.attributes["AXVisible"] = False
        dialog.attributes.pop("AXRole")

    go_button.actions["AXPress"] = accept_go_sheet
    open_button.actions["AXPress"] = accept_open_panel
    timeline = tmp_path / "timeline.edl"

    session.open_file_dialog("timelineOpenFileDialog", timeline)

    assert editor.attributes["AXValue"] == str(timeline.resolve())
    assert ax.keyboard_events == [
        (5, True, ax.kCGEventFlagMaskCommand | ax.kCGEventFlagMaskShift),
        (5, False, ax.kCGEventFlagMaskCommand | ax.kCGEventFlagMaskShift),
    ]


def test_native_save_panel_sets_directory_and_filename(tmp_path: Path) -> None:
    path_editor = Node(AXRole="AXTextField", AXFocused=True, AXValue="")
    path_editor.settable.add("AXValue")
    go_button = Node(AXRole="AXButton", AXTitle="Go")
    go_sheet = Node(
        AXRole="AXSheet",
        AXVisible=True,
        AXChildren=[path_editor, go_button],
        AXDefaultButton=go_button,
    )
    filename_editor = Node(
        AXRole="AXTextField", AXTitle="Save As:", AXValue=""
    )
    filename_editor.settable.add("AXValue")
    save_button = Node(AXRole="AXButton", AXTitle="Save")
    dialog = Node(
        AXRole="AXWindow",
        AXVisible=True,
        AXChildren=[filename_editor, save_button],
        AXDefaultButton=save_button,
    )
    focused_application = Node(
        AXRole="AXApplication", AXFocusedWindow=dialog
    )
    system = Node(
        AXRole="AXSystemWide", AXFocusedApplication=focused_application
    )
    session, _ = application_session(tmp_path, system=system)
    original_shortcut = session._send_go_to_folder_shortcut

    def open_go_sheet() -> None:
        focused_application.attributes["AXFocusedWindow"] = go_sheet
        original_shortcut()

    session._send_go_to_folder_shortcut = open_go_sheet

    def accept_go_sheet() -> None:
        go_sheet.attributes["AXVisible"] = False
        go_sheet.attributes.pop("AXRole")
        focused_application.attributes["AXFocusedWindow"] = dialog

    def accept_save_panel() -> None:
        dialog.attributes["AXVisible"] = False
        dialog.attributes.pop("AXRole")

    go_button.actions["AXPress"] = accept_go_sheet
    save_button.actions["AXPress"] = accept_save_panel
    destination = tmp_path / "export.xlsx"

    session.open_file_dialog("spreadsheetSaveFileDialog", destination)

    assert path_editor.attributes["AXValue"] == str(tmp_path.resolve())
    assert filename_editor.attributes["AXValue"] == destination.name


def test_startup_readiness_uses_the_startup_budget(tmp_path: Path) -> None:
    # An application element with no main window stands in for a launch
    # whose AX tree has not become usable yet.
    session, _ = application_session(
        tmp_path, timeout=0.05, startup_timeout=0.4
    )

    started = monotonic()
    with pytest.raises(StartupNotReadyError) as failure:
        session.wait_ready()
    elapsed = monotonic() - started

    # Readiness must spend the startup budget, not the operation one.
    assert elapsed >= 0.4
    assert "did not become automatable within 0.4 seconds" in str(
        failure.value
    )


def test_element_lookup_uses_the_operation_budget(tmp_path: Path) -> None:
    # The generous startup budget must not slow a genuine lookup failure
    # inside an application that is already running.
    session, _ = application_session(
        tmp_path, timeout=0.1, startup_timeout=30.0
    )

    started = monotonic()
    with pytest.raises(ElementNotFoundError) as failure:
        session.element("absentIdentifier")
    elapsed = monotonic() - started

    assert elapsed < 5.0
    assert not isinstance(failure.value, StartupNotReadyError)
    assert "absentIdentifier" in str(failure.value)
