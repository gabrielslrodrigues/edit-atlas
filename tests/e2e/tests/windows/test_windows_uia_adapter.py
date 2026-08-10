from __future__ import annotations

from pathlib import Path
from threading import Event
from typing import Any

from adapters.windows_uia import WindowsApplicationSession


class ElementInfo:
    def __init__(
        self, name: str, control_type: str, automation_id: str = ""
    ) -> None:
        self.name = name
        self.control_type = control_type
        self.automation_id = automation_id


class RunningProcess:
    pid = 42
    returncode = None

    @staticmethod
    def poll() -> None:
        return None


class InvokePattern:
    def __init__(self) -> None:
        self.invoked = Event()

    def Invoke(self) -> None:
        self.invoked.set()


class TogglePattern:
    def __init__(self, checked: bool = False) -> None:
        self.CurrentToggleState = int(checked)

    def Toggle(self) -> None:
        self.CurrentToggleState = 1 - self.CurrentToggleState


class SelectionPattern:
    def __init__(self, select: Any) -> None:
        self.CurrentIsSelected = False
        self._select = select

    def Select(self) -> None:
        self.CurrentIsSelected = True
        self._select()


class RangeValuePattern:
    def __init__(self, set_value: Any) -> None:
        self._set_value = set_value

    def SetValue(self, value: float) -> None:
        self._set_value(value)


class Node:
    def __init__(
        self,
        name: str,
        control_type: str,
        *,
        automation_id: str = "",
        children: tuple["Node", ...] = (),
    ) -> None:
        self.element_info = ElementInfo(name, control_type, automation_id)
        self._children = children

    def descendants(self) -> list["Node"]:
        descendants: list[Node] = []
        for child in self._children:
            descendants.append(child)
            descendants.extend(child.descendants())
        return descendants

    @staticmethod
    def is_visible() -> bool:
        return True

    @staticmethod
    def is_enabled() -> bool:
        return True


def application_session(artifact_directory: Path) -> WindowsApplicationSession:
    return WindowsApplicationSession(
        application=None,
        desktop=None,
        registry=None,
        process=RunningProcess(),
        artifact_directory=artifact_directory,
        timeout=1.0,
    )


def test_activation_uses_uia_invoke_pattern(tmp_path: Path) -> None:
    session = application_session(tmp_path)
    node = Node("Open", "Button")
    node.iface_invoke = InvokePattern()
    session.element = lambda identifier: node

    session.activate("openDocumentAction")

    assert node.iface_invoke.invoked.wait(1.0)


def test_checked_state_uses_uia_toggle_pattern(tmp_path: Path) -> None:
    session = application_session(tmp_path)
    node = Node("Remember recent files", "CheckBox")
    node.iface_toggle = TogglePattern()
    session.element = lambda identifier: node

    session.set_checked("rememberRecentFilesAction", True)

    assert session.is_checked("rememberRecentFilesAction")


def test_combo_selection_prefers_uia_selection_item_pattern(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)
    control = Node("Event", "ComboBox")
    reel = Node("Reel", "ListItem")
    reel.iface_selection_item = SelectionPattern(
        lambda: setattr(control.element_info, "name", "Reel")
    )
    control._children = (reel,)
    session.element = lambda identifier: control

    session.select_option("filterCondition0Field", "Reel")

    assert reel.iface_selection_item.CurrentIsSelected
    assert session.selected_option("filterCondition0Field") == "Reel"


def test_combo_selection_uses_uia_range_value_without_input_simulation(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)
    options = (
        Node("Event", "ListItem"),
        Node("Reel", "ListItem"),
        Node("Track type", "ListItem"),
    )
    control = Node("Event", "ComboBox", children=options)

    def select(index: float) -> None:
        control.element_info.name = options[int(index)].element_info.name

    control.iface_range_value = RangeValuePattern(select)
    session.element = lambda identifier: control

    session.select_option("filterCondition0Field", "Track type")

    assert session.selected_option("filterCondition0Field") == "Track type"


def test_menu_option_selection_uses_uia_invoke_pattern(tmp_path: Path) -> None:
    session = application_session(tmp_path)
    english = Node("English", "MenuItem")
    english.iface_invoke = InvokePattern()
    language = Node("Language", "MenuItem", children=(english,))
    session.element = lambda identifier: language

    session.select_option("languageSelector", "English")

    assert english.iface_invoke.invoked.is_set()


def test_uia_actions_do_not_block_the_driver(tmp_path: Path) -> None:
    session = application_session(tmp_path)
    node = Node("Open", "Button")
    started = Event()
    release = Event()

    class BlockingInvokePattern:
        def Invoke(self) -> None:
            started.set()
            release.wait()

    node.iface_invoke = BlockingInvokePattern()
    try:
        session._activate_node(node)
        assert started.wait(1.0)
    finally:
        release.set()
