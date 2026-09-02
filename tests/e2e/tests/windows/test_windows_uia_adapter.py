from __future__ import annotations

import time
from pathlib import Path
from threading import Event, Thread
from typing import Any

import pytest

from adapters.windows_uia import (
    ElementNotFoundError,
    StartupNotReadyError,
    WindowsApplicationSession,
)


class ElementInfo:
    def __init__(
        self,
        name: str,
        control_type: str,
        automation_id: str = "",
        control_id: int = -1,
    ) -> None:
        self.name = name
        self.control_type = control_type
        self.automation_id = automation_id
        self.control_id = control_id


class RunningProcess:
    pid = 42
    returncode = None

    @staticmethod
    def poll() -> None:
        return None


class InvokePattern:
    def __init__(self, invoke: Any | None = None) -> None:
        self.invoked = Event()
        self._invoke = invoke

    def Invoke(self) -> None:
        if self._invoke is not None:
            self._invoke()
        self.invoked.set()


class TogglePattern:
    def __init__(self, checked: bool = False) -> None:
        self.CurrentToggleState = int(checked)

    def Toggle(self) -> None:
        self.CurrentToggleState = 1 - self.CurrentToggleState


class ExpandCollapsePattern:
    def __init__(self) -> None:
        self.CurrentExpandCollapseState = 0

    def Expand(self) -> None:
        self.CurrentExpandCollapseState = 1


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
        click: Any | None = None,
    ) -> None:
        self.element_info = ElementInfo(name, control_type, automation_id)
        self._children = children
        self._click = click
        self.focus_calls = 0

    def descendants(self) -> list["Node"]:
        descendants: list[Node] = []
        for child in self._children:
            descendants.append(child)
            descendants.extend(child.descendants())
        return descendants

    def children(self) -> list["Node"]:
        return list(self._children)

    def click_input(self) -> None:
        if self._click is None:
            raise RuntimeError("node is not clickable")
        self._click()

    def window_text(self) -> str:
        return self.element_info.name

    def class_name(self) -> str:
        return self.element_info.control_type

    def set_focus(self) -> None:
        self.focus_calls += 1

    @staticmethod
    def is_visible() -> bool:
        return True

    @staticmethod
    def is_enabled() -> bool:
        return True


class EmptyDesktop:
    """A desktop that exposes no windows for the process under test."""

    @staticmethod
    def windows(**_: Any) -> list[Any]:
        return []


def application_session(
    artifact_directory: Path,
    *,
    desktop: Any = None,
    timeout: float = 1.0,
    startup_timeout: float = 1.0,
) -> WindowsApplicationSession:
    return WindowsApplicationSession(
        application=None,
        desktop=desktop,
        win32_desktop=None,
        keyboard_sender=None,
        registry=None,
        process=RunningProcess(),
        artifact_directory=artifact_directory,
        timeout=timeout,
        startup_timeout=startup_timeout,
    )


def test_activation_uses_uia_invoke_pattern(tmp_path: Path) -> None:
    session = application_session(tmp_path)
    node = Node("Open", "Button")
    node.iface_invoke = InvokePattern()
    session.element = lambda identifier, *, showing=True: node

    session.activate("openDocumentAction")

    assert node.iface_invoke.invoked.wait(1.0)


def test_focus_claims_keyboard_focus_via_uia_set_focus(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)
    node = Node("Comments", "ListItem")
    session.element = lambda identifier, *, showing=True: node

    session.focus("eventColumn12MoveUpButton")

    assert node.focus_calls == 1


def test_checked_state_uses_uia_toggle_pattern(tmp_path: Path) -> None:
    session = application_session(tmp_path)
    node = Node("Remember recent files", "CheckBox")
    node.iface_toggle = TogglePattern()
    session.element = lambda identifier: node

    session.set_checked("rememberRecentFilesAction", True)

    assert session.is_checked("rememberRecentFilesAction")


def test_checkable_menu_item_uses_uia_invoke_pattern(tmp_path: Path) -> None:
    session = application_session(tmp_path)
    node = Node("Remember recent files", "MenuItem")
    node.iface_toggle = TogglePattern()
    node.iface_invoke = InvokePattern(node.iface_toggle.Toggle)
    session.element = lambda identifier: node

    session.set_checked("rememberRecentFilesAction", True)

    assert node.iface_invoke.invoked.is_set()
    assert node.iface_toggle.CurrentToggleState == 1


def test_combo_selection_uses_accessible_item_bounds(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)
    opened = Event()
    display = Node("Event", "Text")
    control = Node(
        "Field", "ComboBox", children=(display,), click=opened.set
    )
    reel = Node("Reel", "ListItem")
    reel._click = lambda: setattr(display.element_info, "name", "Reel")
    session.element = lambda identifier: control
    session._find_named = (
        lambda *args, root=None, **kwargs: reel if root is None else None
    )

    session.select_option("filterCondition0Field", "Reel")

    assert opened.is_set()
    assert session.selected_option("filterCondition0Field") == "Reel"


def test_combo_selection_scrolls_the_control_into_view(
    tmp_path: Path,
) -> None:
    # A combo box below its scroll area's viewport cannot be opened by a
    # click at its reported bounds until focus scrolls it into view.
    session = application_session(tmp_path)
    opened = Event()
    display = Node("Event", "Text")
    option = Node("Track type", "ListItem")
    option._click = lambda: setattr(
        display.element_info, "name", "Track type"
    )
    control = Node("Field", "ComboBox", children=(display,))

    def click() -> None:
        if control.focus_calls == 0:
            raise RuntimeError("clicked outside the viewport")
        opened.set()

    control._click = click

    def find_named(
        names: Any, *, root: Any = None, control_types: Any = None
    ) -> Any:
        if names is None or root is not None or not opened.is_set():
            return None
        return option

    session.element = lambda identifier: control
    session._find_named = find_named

    session.select_option("filterCondition1Field", "Track type")

    assert control.focus_calls == 1
    assert session.selected_option("filterCondition1Field") == "Track type"


def test_combo_selection_pages_to_an_unrealized_option(
    tmp_path: Path,
) -> None:
    # A native combo popup realizes only the items in its viewport, so the
    # option below the fold is absent from the tree until the popup is paged.
    session = application_session(tmp_path)
    opened = Event()
    display = Node("Event", "Text")
    control = Node("Field", "ComboBox", children=(display,), click=opened.set)
    option = Node("Track type", "ListItem")
    option._click = lambda: setattr(
        display.element_info, "name", "Track type"
    )

    class Popup(Node):
        def __init__(self) -> None:
            super().__init__("", "List")
            self.pages = 0

        def scroll(self, direction: str, amount: str) -> None:
            assert (direction, amount) == ("down", "page")
            self.pages += 1

    popup = Popup()

    def find_named(
        names: Any, *, root: Any = None, control_types: Any = None
    ) -> Any:
        if control_types == ("List",):
            return popup if root is None else None
        if names is None:
            return None
        realized = popup.pages > 0
        matches = "track type" in {str(name).lower() for name in names}
        return option if realized and matches and root is None else None

    session.element = lambda identifier: control
    session._find_named = find_named

    session.select_option("filterCondition1Field", "Track type")

    assert opened.is_set()
    assert popup.pages == 1
    assert session.selected_option("filterCondition1Field") == "Track type"


def test_combo_selection_prefers_items_exposed_while_collapsed(
    tmp_path: Path,
) -> None:
    # Qt Quick's in-scene popup exposes its delegates without opening, so no
    # pointer input and no paging should be needed.
    session = application_session(tmp_path)
    opened = Event()
    display = Node("Event", "Text")
    option = Node("Track type", "ListItem")
    option.iface_selection_item = SelectionPattern(
        lambda: setattr(display.element_info, "name", "Track type")
    )
    control = Node(
        "Field", "ComboBox", children=(display, option), click=opened.set
    )
    session.element = lambda identifier: control

    session.select_option("filterCondition0Field", "Track type")

    assert not opened.is_set()
    assert option.iface_selection_item.CurrentIsSelected
    assert session.selected_option("filterCondition0Field") == "Track type"


def test_combo_selection_uses_uia_range_value_without_input_simulation(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)
    options = (
        Node("Event", "ListItem"),
        Node("Reel", "ListItem"),
        Node("Track type", "ListItem"),
    )
    control = Node("Event", "Custom", children=options)

    def select(index: float) -> None:
        control.element_info.name = options[int(index)].element_info.name

    control.iface_range_value = RangeValuePattern(select)
    session.element = lambda identifier: control

    session.select_option("filterCondition0Field", "Track type")

    assert session.selected_option("filterCondition0Field") == "Track type"


def test_table_text_includes_virtualized_uia_grid_cells(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)
    table = Node("Timeline edit events", "Table")

    class Cell:
        def __init__(self, name: str) -> None:
            self.CurrentName = name

    class GridPattern:
        CurrentRowCount = 2
        CurrentColumnCount = 1

        @staticmethod
        def GetItem(row: int, column: int) -> Cell:
            assert column == 0
            return Cell(("opening.mov", "SYNTHETIC AUDIO NOTE")[row])

    table.iface_grid = GridPattern()
    session.element = lambda identifier: table

    assert "SYNTHETIC AUDIO NOTE" in session.text_content("eventTable")


def test_list_items_scrolls_through_a_virtualized_list(tmp_path: Path) -> None:
    session = application_session(tmp_path)
    fields = [Node(f"Field {index}", "ListItem") for index in range(10)]
    page_size = 3

    class VirtualizedList:
        """Exposes only the rows inside its current viewport."""

        def __init__(self) -> None:
            self.offset = 0
            self.focus_calls = 0

        def scroll(self, direction: str, amount: str) -> None:
            raise RuntimeError("Qt Quick exposes no UIA Scroll pattern")

        def set_focus(self) -> None:
            self.focus_calls += 1

        def descendants(self) -> list[Node]:
            return fields[self.offset : self.offset + page_size]

    control = VirtualizedList()
    session.element = lambda identifier: control

    def send_keys(keys: str, **options: Any) -> None:
        assert options == {"pause": 0}
        step = page_size if keys == "{PGDN}" else -page_size
        control.offset = max(
            0, min(control.offset + step, len(fields) - page_size)
        )

    session._keyboard_sender = send_keys

    names = session.list_items("eventColumnsList")

    assert names == [f"Field {index}" for index in range(10)]
    # The sweep must leave the list where it found it, so that repeated
    # observations of the same list stay consistent.
    assert control.offset == 0
    assert control.focus_calls > 0


def test_list_items_include_flattened_quick_checkboxes(tmp_path: Path) -> None:
    session = application_session(tmp_path)
    visible = tuple(
        Node(
            f"Field {index}",
            "ListItem",
            automation_id=f"eventColumn{index}",
        )
        for index in range(3)
    )
    field_4 = Node(
        "Field 4", "CheckBox", automation_id="eventColumn4CheckBox"
    )
    field_4.iface_toggle = TogglePattern()
    flattened = (
        field_4,
        Node("Field 3", "CheckBox", automation_id="eventColumn3CheckBox"),
    )
    control = Node("Columns", "Group", children=visible + flattened)
    session.element = lambda identifier: control

    assert session.list_items("eventColumnsList") == [
        "Field 0",
        "Field 1",
        "Field 2",
        "Field 3",
        "Field 4",
    ]
    session.set_list_item_checked("eventColumnsList", "Field 4", True)
    assert field_4.iface_toggle.CurrentToggleState == 1


def test_menu_option_selection_uses_uia_invoke_pattern(tmp_path: Path) -> None:
    session = application_session(tmp_path)
    english = Node("English", "MenuItem")
    english.iface_invoke = InvokePattern()
    language = Node("Language", "MenuItem", children=(english,))
    session.element = lambda identifier: language

    session.select_option("languageSelector", "English")

    assert english.iface_invoke.invoked.is_set()


def test_menu_action_uses_accessible_item_bounds(tmp_path: Path) -> None:
    session = application_session(tmp_path)
    clicked: list[str] = []
    menu = Node("Template actions", "Button", click=lambda: clicked.append("menu"))
    action = Node(
        "Edit export columns",
        "MenuItem",
        click=lambda: clicked.append("action"),
    )
    nodes = {
        "templateActionsButton": menu,
        "editExportColumnsAction": action,
    }
    session.element = lambda identifier: nodes[identifier]
    session.has_element = lambda identifier: False

    session.activate_menu_action(
        "templateActionsButton", "editExportColumnsAction"
    )

    assert clicked == ["menu", "action"]


def test_menu_action_waits_for_the_dropdown_to_expand(tmp_path: Path) -> None:
    session = application_session(tmp_path)
    clicked: list[str] = []
    action = Node(
        "Open", "MenuItem", click=lambda: clicked.append("action")
    )
    expand = ExpandCollapsePattern()

    def open_after_delay() -> None:
        time.sleep(0.2)
        expand.CurrentExpandCollapseState = 1

    menu = Node(
        "File",
        "MenuItem",
        click=lambda: Thread(target=open_after_delay, daemon=True).start(),
    )
    menu.iface_expand_collapse = expand
    nodes = {"fileMenu": menu, "openDocumentAction": action}
    session.element = lambda identifier: nodes[identifier]
    session.has_element = lambda identifier: False

    session.activate_menu_action("fileMenu", "openDocumentAction")

    assert clicked == ["action"]


def test_menu_action_skips_reclicking_an_already_open_menu(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)
    clicked: list[str] = []
    menu = Node("File", "MenuItem", click=lambda: clicked.append("menu"))
    action = Node(
        "Open", "MenuItem", click=lambda: clicked.append("action")
    )
    nodes = {"fileMenu": menu, "openDocumentAction": action}
    session.element = lambda identifier: nodes[identifier]
    session.has_element = lambda identifier: identifier == "openDocumentAction"

    session.activate_menu_action("fileMenu", "openDocumentAction")

    assert clicked == ["action"]


def test_identifier_lookup_accepts_qt_hierarchical_automation_id(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)
    language = Node(
        "Language",
        "MenuItem",
        automation_id=(
            "QApplication.mainWindow.applicationMenuBar.languageSelector"
        ),
    )
    root = Node("Edit Atlas", "Window", children=(language,))

    assert session._find_identifier_in(root, "languageSelector") is language
    assert session._find_identifier_in(root, "otherSelector") is None


def test_identifier_lookup_checks_window_before_its_descendants(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)

    class Window(Node):
        def descendants(self) -> list[Node]:
            raise AssertionError("matching window descendants were traversed")

    window = Window("Edit Atlas", "Window", automation_id="mainWindow")

    assert session._find_identifier_in(window, "mainWindow") is window


def test_named_lookup_checks_window_before_its_descendants(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)

    class Window(Node):
        def descendants(self) -> list[Node]:
            raise AssertionError("matching window descendants were traversed")

    window = Window("Export progress", "Window")

    assert session._find_named(("Export progress",), root=window) is window


def test_lookup_checks_all_windows_before_any_descendants(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)

    class MainWindow(Node):
        def descendants(self) -> list[Node]:
            raise AssertionError("main window descendants were traversed")

    main_window = MainWindow(
        "Edit Atlas", "Window", automation_id="mainWindow"
    )
    dialog = Node(
        "Export progress",
        "Window",
        automation_id="spreadsheetExportProgressDialog",
    )

    class Desktop:
        @staticmethod
        def windows(**criteria: Any) -> list[Node]:
            assert criteria == {"process": RunningProcess.pid}
            return [dialog, main_window]

    session._desktop = Desktop()

    assert (
        session._find_identifier("spreadsheetExportProgressDialog") is dialog
    )
    assert session._find_named(("Export progress",)) is dialog


def test_native_file_dialog_uses_win32_backend_without_uia_traversal(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)
    dialog = object()

    class Desktop:
        @staticmethod
        def windows(
            *, process: int, class_name: str, visible_only: bool
        ) -> list[object]:
            assert process == RunningProcess.pid
            assert class_name == "#32770"
            assert not visible_only
            return [dialog]

    session._win32_desktop = Desktop()

    assert session._native_file_dialog("timelineOpenFileDialog") is dialog


def test_native_file_dialog_uses_focused_keyboard_input(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)
    dialog_open = Event()
    dialog_open.set()
    keyboard_calls: list[tuple[str, dict[str, Any]]] = []

    class Dialog:
        @staticmethod
        def exists() -> bool:
            return dialog_open.is_set()

        @staticmethod
        def has_focus() -> bool:
            return True

    dialog = Dialog()

    class Desktop:
        @staticmethod
        def windows(**criteria: Any) -> list[Dialog]:
            return [dialog]

    session._win32_desktop = Desktop()

    def send_keys(keys: str, **options: Any) -> None:
        keyboard_calls.append((keys, options))
        if keys == "{ENTER}":
            dialog_open.clear()

    session._keyboard_sender = send_keys
    path = tmp_path / "timeline.edl"

    session.open_file_dialog("timelineOpenFileDialog", path)

    assert keyboard_calls == [
        (
            str(path),
            {"with_spaces": True, "pause": 0, "vk_packet": True},
        ),
        ("{ENTER}", {"pause": 0}),
    ]


def test_native_save_dialog_accepts_shell_overwrite_confirmation(
    tmp_path: Path,
) -> None:
    session = application_session(tmp_path)
    file_dialog_open = Event()
    file_dialog_open.set()
    confirmation_open = Event()

    class FileDialog:
        @staticmethod
        def exists() -> bool:
            return file_dialog_open.is_set()

        @staticmethod
        def has_focus() -> bool:
            return True

    yes = Node(
        "&Yes",
        "Button",
        click=lambda: confirmation_open.clear(),
    )

    class Confirmation(Node):
        def __init__(self) -> None:
            super().__init__("Confirm Save As", "Window", children=(yes,))

        @staticmethod
        def exists() -> bool:
            return confirmation_open.is_set()

    file_dialog = FileDialog()
    confirmation = Confirmation()

    class Desktop:
        @staticmethod
        def windows(**criteria: Any) -> list[object]:
            if confirmation_open.is_set():
                return [confirmation]
            if file_dialog_open.is_set():
                return [file_dialog]
            return []

    session._win32_desktop = Desktop()

    def send_keys(keys: str, **options: Any) -> None:
        if keys == "{ENTER}":
            file_dialog_open.clear()

            def show_confirmation_after_delay() -> None:
                time.sleep(0.05)
                confirmation_open.set()

            Thread(target=show_confirmation_after_delay, daemon=True).start()

    session._keyboard_sender = send_keys
    destination = tmp_path / "existing.xlsx"
    destination.touch()

    session.open_file_dialog(
        "spreadsheetSaveFileDialog", destination
    )

    assert not confirmation_open.is_set()


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


def test_startup_readiness_uses_the_startup_budget(tmp_path: Path) -> None:
    # A desktop with no windows models a UIA tree that is not enumerable yet.
    session = application_session(
        tmp_path,
        desktop=EmptyDesktop(),
        timeout=0.05,
        startup_timeout=0.4,
    )

    started = time.monotonic()
    with pytest.raises(StartupNotReadyError) as failure:
        session.wait_ready()
    elapsed = time.monotonic() - started

    assert elapsed >= 0.4
    assert "did not become automatable within 0.4 seconds" in str(
        failure.value
    )


def test_element_lookup_uses_the_operation_budget(tmp_path: Path) -> None:
    session = application_session(
        tmp_path,
        desktop=EmptyDesktop(),
        timeout=0.1,
        startup_timeout=30.0,
    )

    started = time.monotonic()
    with pytest.raises(ElementNotFoundError) as failure:
        session.element("absentIdentifier")
    elapsed = time.monotonic() - started

    assert elapsed < 5.0
    assert not isinstance(failure.value, StartupNotReadyError)
    assert "absentIdentifier" in str(failure.value)
