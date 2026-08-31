"""Linux desktop automation through dogtail and AT-SPI.

This module uses accessibility actions, selection, and editable-text interfaces.
When Qt Quick exposes only focus actions, accessibility-derived bounds provide
the pointer input needed to operate the control without fixed coordinates.
"""

from __future__ import annotations

from collections import deque
import ctypes
import ctypes.util
import logging
import os
from pathlib import Path
import shutil
import subprocess
import threading
from typing import Any, Iterable, Mapping, Sequence

from adapters.processes import ProcessRegistry
from application.polling import PollTimeoutError, wait_until


class AccessibilityBackendError(RuntimeError):
    """Raised when the required Linux accessibility session is unavailable."""


class ElementNotFoundError(LookupError):
    """Raised when a semantic element is absent after bounded polling."""


class ActionNotSupportedError(RuntimeError):
    """Raised when an element exposes no suitable semantic action."""


class _X11PointerInput:
    """Send pointer input through XTest at accessibility-derived bounds."""

    def __init__(self) -> None:
        x11_name = ctypes.util.find_library("X11")
        xtst_name = ctypes.util.find_library("Xtst")
        if x11_name is None or xtst_name is None:
            raise AccessibilityBackendError(
                "Linux E2E requires the X11 and XTest runtime libraries"
            )
        self._x11 = ctypes.CDLL(x11_name)
        self._xtst = ctypes.CDLL(xtst_name)
        self._x11.XOpenDisplay.argtypes = (ctypes.c_char_p,)
        self._x11.XOpenDisplay.restype = ctypes.c_void_p
        self._x11.XCloseDisplay.argtypes = (ctypes.c_void_p,)
        self._x11.XCloseDisplay.restype = ctypes.c_int
        self._x11.XSync.argtypes = (ctypes.c_void_p, ctypes.c_int)
        self._x11.XSync.restype = ctypes.c_int
        self._xtst.XTestFakeMotionEvent.argtypes = (
            ctypes.c_void_p,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_int,
            ctypes.c_ulong,
        )
        self._xtst.XTestFakeMotionEvent.restype = ctypes.c_int
        self._xtst.XTestFakeButtonEvent.argtypes = (
            ctypes.c_void_p,
            ctypes.c_uint,
            ctypes.c_int,
            ctypes.c_ulong,
        )
        self._xtst.XTestFakeButtonEvent.restype = ctypes.c_int

    def click(self, x: int, y: int) -> None:
        display = self._x11.XOpenDisplay(None)
        if not display:
            raise AccessibilityBackendError("cannot open the X11 display")
        try:
            generated = (
                self._xtst.XTestFakeMotionEvent(display, -1, x, y, 0)
                and self._xtst.XTestFakeButtonEvent(display, 1, True, 0)
                and self._xtst.XTestFakeButtonEvent(display, 1, False, 0)
            )
            self._x11.XSync(display, False)
        finally:
            self._x11.XCloseDisplay(display)
        if not generated:
            raise ActionNotSupportedError("XTest rejected pointer input")


class LinuxAtspiAdapter:
    """Launch packaged applications and connect to them over AT-SPI."""

    def __init__(
        self,
        registry: ProcessRegistry,
        artifact_directory: Path,
        timeout: float,
    ) -> None:
        if timeout <= 0:
            raise ValueError("timeout must be positive")
        self._registry = registry
        self._artifact_directory = artifact_directory
        self._timeout = timeout
        self._tree: Any = None
        self._atspi: Any = None
        self._keyboard_sender: Any = None
        self._pointer_input: _X11PointerInput | None = None

    def preflight(self) -> None:
        missing = [
            name
            for name in ("DISPLAY", "DBUS_SESSION_BUS_ADDRESS")
            if not os.environ.get(name)
        ]
        if missing:
            raise AccessibilityBackendError(
                "Linux E2E requires X11 and a D-Bus session; missing "
                + ", ".join(missing)
            )
        if os.environ.get("XDG_SESSION_TYPE", "x11").lower() != "x11":
            raise AccessibilityBackendError("Linux E2E requires an X11 session")

        try:
            import gi

            gi.require_version("Atspi", "2.0")
            from gi.repository import Atspi

            from dogtail import rawinput, tree
            from dogtail.config import config
        except (ImportError, ValueError) as error:
            raise AccessibilityBackendError(
                f"dogtail/AT-SPI backend could not be loaded: {error}"
            ) from error

        config.action_delay = 0
        config.typing_delay = 0
        config.default_delay = 0
        config.search_back_off_delay = 0
        config.search_cut_off_limit = 1
        config.search_showing_only = False
        config.ensure_sensitivity = False
        logging.getLogger("__dogtail_logger__").setLevel(logging.WARNING)

        try:
            desktop = Atspi.get_desktop(0)
            desktop.get_child_count()
        except Exception as error:
            raise AccessibilityBackendError(
                f"AT-SPI desktop is not available: {error}"
            ) from error

        self._tree = tree
        self._atspi = Atspi
        self._keyboard_sender = rawinput.pressKey
        self._pointer_input = _X11PointerInput()

    def launch(
        self,
        executable: Path,
        state_root: Path,
        *,
        locale: str,
        log_name: str,
        extra_environment: Mapping[str, str] | None = None,
    ) -> "LinuxApplicationSession":
        if self._tree is None:
            self.preflight()
        if not executable.is_file() or not os.access(executable, os.X_OK):
            raise AccessibilityBackendError(
                f"installed GUI executable is unavailable: {executable}"
            )

        environment = os.environ.copy()
        environment.update(
            {
                "EDIT_ATLAS_TEST_STATE_ROOT": os.fspath(state_root),
                "LANG": locale,
                "LC_ALL": locale,
                "NO_AT_BRIDGE": "0",
                "QT_LINUX_ACCESSIBILITY_ALWAYS_ON": "1",
                "QT_QPA_PLATFORM": "xcb",
                "XDG_SESSION_TYPE": "x11",
            }
        )
        if extra_environment:
            environment.update(extra_environment)

        process = self._registry.start(
            [executable],
            environment=environment,
            output_path=self._artifact_directory / "logs" / f"{log_name}.log",
        )
        session = LinuxApplicationSession(
            tree=self._tree,
            atspi=self._atspi,
            keyboard_sender=self._keyboard_sender,
            pointer_click=self._pointer_input.click,
            registry=self._registry,
            process=process,
            artifact_directory=self._artifact_directory,
            timeout=self._timeout,
        )
        try:
            session.wait_ready()
        except BaseException:
            session.capture_artifacts(f"{log_name}-startup")
            session.close()
            raise
        return session


class LinuxApplicationSession:
    _MAX_ACCESSIBILITY_DEPTH = 64
    _IDENTIFIER_SEARCH_LEAVES = ("eventTable",)
    _ACTION_PRIORITY = (
        "click",
        "toggle",
        "press",
        "activate",
        "show menu",
        "open",
    )

    def __init__(
        self,
        *,
        tree: Any,
        atspi: Any,
        keyboard_sender: Any,
        pointer_click: Any,
        registry: ProcessRegistry,
        process: subprocess.Popen[str],
        artifact_directory: Path,
        timeout: float,
    ) -> None:
        self._tree = tree
        self._atspi = atspi
        self._keyboard_sender = keyboard_sender
        self._pointer_click = pointer_click
        self._registry = registry
        self._process = process
        self._artifact_directory = artifact_directory
        self._timeout = timeout
        self._application: Any = None
        self._progress_dialog: Any = None
        self._frame_extraction_cancel_button: Any = None
        self._action_errors: deque[ActionNotSupportedError] = deque()
        self._action_error_lock = threading.Lock()

    def wait_ready(self) -> None:
        def find_application() -> Any | None:
            self._ensure_running()
            try:
                return self._tree.root.application("Edit Atlas")
            except Exception:
                return None

        self._application = wait_until(
            find_application,
            lambda value: value is not None,
            timeout=self._timeout,
            description="Edit Atlas in the AT-SPI tree",
        )
        self.element("mainWindow")

    def element(self, identifier: str, *, showing: bool = True) -> Any:
        if (
            identifier == "spreadsheetExportProgressDialog"
            and self._progress_dialog is not None
            and (not showing or self._is_showing(self._progress_dialog))
        ):
            return self._progress_dialog

        def find() -> Any | None:
            self._ensure_running()
            try:
                node = self._find_identifier(
                    self._application, identifier, showing_only=showing
                )
                if node is not None or not showing:
                    return node
                hidden = self._find_identifier(
                    self._application, identifier, showing_only=False
                )
                if hidden is not None:
                    self._scroll_into_view(hidden)
                    return hidden if self._is_showing(hidden) else None
                return None
            except Exception:
                return None

        try:
            node = wait_until(
                find,
                lambda value: value is not None,
                timeout=self._timeout,
                description=f"accessible identifier {identifier!r}",
            )
            if identifier == "spreadsheetExportProgressDialog":
                self._progress_dialog = node
                self._frame_extraction_cancel_button = self._find_identifier(
                    node,
                    "cancelFrameExtractionButton",
                    showing_only=False,
                )
            return node
        except PollTimeoutError as error:
            raise ElementNotFoundError(str(error)) from error

    def has_element(self, identifier: str, *, showing: bool = True) -> bool:
        self._ensure_running()
        if identifier == "replaceSpreadsheetDialog":
            return self._native_overwrite_button(True) is not None
        try:
            return (
                self._find_identifier(
                    self._application, identifier, showing_only=showing
                )
                is not None
            )
        except Exception:
            return False

    def wait_absent(self, identifier: str) -> None:
        wait_until(
            lambda: self.has_element(identifier),
            lambda present: not present,
            timeout=self._timeout,
            consecutive=2,
            description=f"accessible identifier {identifier!r} to disappear",
        )

    def activate(self, identifier: str, *, showing: bool = True) -> None:
        if identifier == "cancelFrameExtractionButton":
            node = self._frame_extraction_cancel_button
            if node is None or (showing and not self._is_showing(node)):
                node = self.element(identifier, showing=showing)
            self._click_accessible_bounds(
                node,
                "frame extraction cancel button",
            )
            return
        if identifier in (
            "cancelReplaceSpreadsheetButton",
            "replaceSpreadsheetButton",
        ):
            replace = identifier == "replaceSpreadsheetButton"
            save_dialog = self._file_dialog("spreadsheetSaveFileDialog")
            button = self._native_overwrite_button(replace)
            if button is None:
                raise ElementNotFoundError(
                    "native overwrite confirmation button is absent"
                )
            self._click_accessible_bounds(
                button, "native overwrite confirmation button"
            )
            if not replace:
                wait_until(
                    lambda: self._native_overwrite_button(True),
                    lambda value: value is None,
                    timeout=self._timeout,
                    description="native overwrite confirmation to close",
                )
                cancel = self._find_named(
                    save_dialog,
                    ("Cancel", "&Cancel", "Cancelar", "&Cancelar"),
                    roles=("push button", "button"),
                    showing_only=True,
                )
                if cancel is None:
                    raise ElementNotFoundError(
                        "file chooser cancel button is absent"
                    )
                self._click_accessible_bounds(
                    cancel, "file chooser cancel button"
                )
                self._wait_file_dialog_closed(
                    save_dialog, "spreadsheetSaveFileDialog"
                )
            return
        self._activate_node(self.element(identifier, showing=showing))

    def focus(self, identifier: str, *, showing: bool = False) -> None:
        # A virtualized row must actually be brought into view before a
        # click at its accessible bounds can land anywhere meaningful.
        # Qt's accessibility bridge maps "SetFocus" to forceActiveFocus(),
        # which pulls the target back into the viewport the same way
        # keyboard navigation would.
        node = self.element(identifier, showing=showing)
        try:
            node.do_action_named("SetFocus")
        except Exception as error:
            raise ActionNotSupportedError(
                f"{identifier!r} rejected keyboard focus"
            ) from error

    def activate_named(
        self, names: Sequence[str], *, within: str | None = None
    ) -> None:
        root = self.element(within) if within else self._application
        node = self._find_named(root, names)
        if node is None:
            raise ElementNotFoundError(
                f"could not find a showing accessible named one of {tuple(names)!r}"
            )
        self._activate_node(node)

    def set_text(self, identifier: str, value: str) -> None:
        node = self.element(identifier)
        try:
            result = node.set_text_contents(value)
        except Exception:
            try:
                node.text = value
                result = True
            except Exception as error:
                raise ActionNotSupportedError(
                    f"{identifier!r} has no editable-text interface"
                ) from error
        if result is False:
            raise ActionNotSupportedError(
                f"editable-text operation failed for {identifier!r}"
            )
        wait_until(
            lambda: self._node_text(node),
            lambda text: text == value,
            timeout=self._timeout,
            description=f"{identifier!r} text to become {value!r}",
        )

    def set_checked(self, identifier: str, checked: bool) -> None:
        node = self.element(identifier)
        if self._is_checked(node) != checked:
            self._activate_node(node)

        def checked_or_closed() -> bool:
            return self._checked_state(node) == checked or not self._is_showing(
                node
            )

        wait_until(
            checked_or_closed,
            lambda complete: complete,
            timeout=self._timeout,
            description=f"{identifier!r} checked state to become {checked}",
        )

    def is_checked(self, identifier: str) -> bool:
        return self._is_checked(self.element(identifier))

    def is_sensitive(self, identifier: str) -> bool:
        return bool(self.element(identifier).sensitive)

    def list_items(self, identifier: str) -> list[str]:
        control = self.element(identifier)
        if identifier == "eventColumnsList":
            indexed_items: list[tuple[int, str]] = []
            for node in self._walk(control):
                node_identifier = self._node_identifier(node).rsplit(".", 1)[-1]
                prefix = "eventColumn"
                suffix = "CheckBox"
                if not (
                    node_identifier.startswith(prefix)
                    and node_identifier.endswith(suffix)
                ):
                    continue
                index = node_identifier[len(prefix) : -len(suffix)]
                if index.isdigit():
                    indexed_items.append((int(index), str(node.name)))
            if indexed_items:
                return [name for _, name in sorted(indexed_items)]
        return [
            str(node.name)
            for node in self._walk(control)
            if str(getattr(node, "role_name", "")).casefold()
            in ("list item", "table row")
        ]

    def is_list_item_checked(self, identifier: str, name: str) -> bool:
        node = self._visible_list_item(identifier, name)
        return bool(node.checked)

    def select_list_item(self, identifier: str, name: str) -> None:
        control = self.element(identifier)
        node = self._list_item(identifier, name, control=control)
        self._scroll_into_view(node)
        try:
            node.select()
        except Exception:
            self._activate_node(node)
        wait_until(
            lambda: self._selected_state(node),
            lambda selected: selected,
            timeout=self._timeout,
            description=f"list item {name!r} to become selected",
        )

    def set_list_item_checked(
        self, identifier: str, name: str, checked: bool
    ) -> None:
        control = self.element(identifier)
        node = self._visible_list_item(identifier, name, control=control)
        if self._is_checked(node) != checked:
            try:
                actions = node.actions or {}
            except Exception as error:
                raise ActionNotSupportedError(
                    f"cannot query actions for list item {name!r}"
                ) from error
            normalized = {
                self._normalized_action_name(str(action)): str(action)
                for action in actions
            }
            toggle = normalized.get("toggle")
            if toggle is None:
                raise ActionNotSupportedError(
                    f"list item {name!r} exposes no Toggle action"
                )
            try:
                if node.do_action_named(toggle) is False:
                    raise ActionNotSupportedError(
                        f"accessibility action {toggle!r} failed"
                    )
            except ActionNotSupportedError:
                raise
            except Exception as error:
                raise ActionNotSupportedError(
                    f"accessibility action {toggle!r} failed: {error}"
                ) from error
        wait_until(
            lambda: self._checked_state(node),
            lambda value: value == checked,
            timeout=self._timeout,
            description=f"list item {name!r} checked state to become {checked}",
        )

    def select_option(self, identifier: str, option: str) -> None:
        control = self.element(identifier)
        current = self.selected_option(identifier)
        if self._normalized_name(current) == self._normalized_name(option):
            return
        if str(getattr(control, "role_name", "")).casefold() == "combo box":
            self._select_combo_box_option(identifier, control, option)
            return
        self._activate_node(control)

        def find_option() -> Any | None:
            return self._find_named(
                self._application,
                (option,),
                roles=(
                    "list item",
                    "menu item",
                    "radio menu item",
                    "check menu item",
                ),
            )

        node = wait_until(
            find_option,
            lambda value: value is not None,
            timeout=self._timeout,
            description=f"option {option!r} for {identifier!r}",
        )
        try:
            self._activate_node(node)
        except ActionNotSupportedError:
            try:
                node.select()
            except Exception as error:
                raise ActionNotSupportedError(
                    f"option {option!r} exposes neither action nor selection"
                ) from error
        wait_until(
            lambda: self._option_is_selected(control, node, option),
            lambda selected: selected,
            timeout=self._timeout,
            description=f"option {option!r} to become selected",
        )

    def _select_combo_box_option(
        self, identifier: str, control: Any, option: str
    ) -> None:
        try:
            interfaces = tuple(control.get_interfaces())
        except Exception:
            interfaces = ()
        if "Value" not in interfaces:
            self._select_combo_box_option_by_bounds(identifier, control, option)
            return

        option_list = self._find_role(control, "list")
        if option_list is None:
            raise ElementNotFoundError(
                f"combo box {identifier!r} exposes no option list"
            )
        target_index = None
        for index, node in enumerate(tuple(option_list.children)):
            if (
                str(getattr(node, "role_name", "")).casefold() == "list item"
                and self._normalized_name(str(getattr(node, "name", "")))
                == self._normalized_name(option)
            ):
                target_index = index
                break
        if target_index is None:
            raise ElementNotFoundError(
                f"combo box {identifier!r} contains no option {option!r}"
            )

        try:
            control.value = float(target_index)
        except Exception as error:
            raise ActionNotSupportedError(
                f"combo box {identifier!r} rejected option {option!r}"
            ) from error
        wait_until(
            lambda: self._option_is_selected(control, control, option),
            lambda selected: selected,
            timeout=self._timeout,
            description=f"option {option!r} to become selected",
        )

    def _select_combo_box_option_by_bounds(
        self, identifier: str, control: Any, option: str
    ) -> None:
        self._click_accessible_bounds(control, f"combo box {identifier!r}")
        try:
            node = wait_until(
                lambda: self._find_named(
                    self._application,
                    (option,),
                    roles=("list item", "menu item"),
                    showing_only=True,
                ),
                lambda value: value is not None,
                timeout=self._timeout,
                description=f"option {option!r} for {identifier!r}",
            )
        except PollTimeoutError as error:
            raise ElementNotFoundError(str(error)) from error
        self._click_accessible_bounds(node, f"option {option!r}")
        wait_until(
            lambda: self.selected_option(identifier),
            lambda selected: self._normalized_name(selected)
            == self._normalized_name(option),
            timeout=self._timeout,
            description=f"option {option!r} to become selected",
        )

    def selected_option(self, identifier: str) -> str:
        control = self.element(identifier)
        if str(getattr(control, "role_name", "")).casefold() == "combo box":
            try:
                children = tuple(control.children)
            except Exception:
                children = ()
            for node in children:
                if not self._is_showing(node):
                    continue
                value = self._node_text(node) or str(
                    getattr(node, "name", "")
                )
                if value:
                    return value
        for node in self._walk(control):
            try:
                if node.selected:
                    return str(node.name)
            except Exception:
                continue
        return self._node_text(control) or str(control.name)

    def open_file_dialog(self, dialog_identifier: str, path: Path) -> None:
        dialog = self._file_dialog(dialog_identifier)
        quick_filename_editor = self._find_identifier(
            dialog, "fileNameTextField", showing_only=False
        )
        if quick_filename_editor is None:
            self._complete_native_file_dialog(
                dialog, dialog_identifier, os.fspath(path)
            )
            return

        button = self._file_dialog_accept_button(dialog)
        button_name = self._normalized_name(str(getattr(button, "name", "")))
        save_dialog = button_name in ("save", "salvar", "export", "exportar")
        absolute_path = path.absolute()
        self._navigate_file_dialog(dialog, absolute_path.parent)

        if not save_dialog:
            file_entry = self._file_dialog_entry(dialog, absolute_path.name)
            self._activate_file_dialog_entry(file_entry, absolute_path.name)
            self._activate_file_dialog_accept(dialog)
            self._wait_file_dialog_closed(dialog, dialog_identifier)
            return

        editor = self._find_identifier(
            dialog, "fileNameEdit", showing_only=False
        )
        if editor is None:
            editor = quick_filename_editor
        editors = [
            node
            for node in self._walk(dialog)
            if self._is_editable(node)
        ]
        if editor is None and not editors:
            raise ElementNotFoundError(
                f"{dialog_identifier!r} contains no editable filename field"
            )
        if editor is None:
            editor = editors[-1]
        expected_path = absolute_path.name
        try:
            self._click_accessible_bounds(
                editor, "native file chooser filename field"
            )
            wait_until(
                lambda: bool(editor.focused),
                lambda focused: focused,
                timeout=self._timeout,
                description="native file chooser filename field to gain focus",
            )
            result = editor.set_text_contents(expected_path)
        except Exception as error:
            raise ActionNotSupportedError(
                "native file chooser rejected filename input"
            ) from error
        if result is False:
            raise ActionNotSupportedError("native file chooser rejected the path")
        wait_until(
            lambda: self._node_text(editor),
            lambda text: text == expected_path,
            timeout=self._timeout,
            description=f"native file chooser path to become {expected_path!r}",
        )

        self._activate_file_dialog_accept(dialog)
        showing, confirmation = wait_until(
            lambda: (
                self._is_showing(dialog),
                self._find_named(
                    self._application,
                    ("Yes", "&Yes", "Sim", "&Sim"),
                    roles=("push button", "button"),
                    showing_only=True,
                ),
            ),
            lambda state: not state[0] or state[1] is not None,
            timeout=self._timeout,
            description="file chooser to close or request overwrite confirmation",
        )
        if showing and confirmation is not None:
            return
        self._wait_file_dialog_closed(dialog, dialog_identifier)

    def _native_overwrite_button(self, replace: bool) -> Any | None:
        names = (
            ("Yes", "&Yes", "Sim", "&Sim")
            if replace
            else ("No", "&No", "Não", "&Não")
        )
        return self._find_named(
            self._application,
            names,
            roles=("push button", "button"),
            showing_only=True,
        )

    def _complete_native_file_dialog(
        self, dialog: Any, dialog_identifier: str, path: str
    ) -> None:
        editor = self._find_identifier(
            dialog, "fileNameEdit", showing_only=False
        )
        if editor is None:
            editor = next(
                (
                    node
                    for node in self._walk(dialog)
                    if self._is_showing(node) and self._is_editable(node)
                ),
                None,
            )
        if editor is None:
            raise ElementNotFoundError(
                f"{dialog_identifier!r} contains no editable path field"
            )
        try:
            result = editor.set_text_contents(path)
        except Exception:
            editor.text = path
            result = True
        if result is False:
            raise ActionNotSupportedError("native file chooser rejected the path")
        wait_until(
            lambda: self._node_text(editor),
            lambda text: text == path,
            timeout=self._timeout,
            description=f"native file chooser path to become {path!r}",
        )
        self._activate_file_dialog_accept(dialog)
        self._wait_file_dialog_closed(dialog, dialog_identifier)

    def _navigate_file_dialog(self, dialog: Any, directory: Path) -> None:
        components = tuple(
            component
            for component in directory.parts
            if component not in (directory.anchor, "", "/")
        )
        # The packaged tests select files below the checkout, which is also
        # the dialog's initial folder. Start from the first target component
        # that is an actual visible child of that folder instead of walking
        # down again from '/'. The captured Qt tree proves those children are
        # exposed as list items, while Ctrl+L never reveals the hidden path
        # editor under AT-SPI.
        start = next(
            (
                index
                for index, component in enumerate(components)
                if self._find_named(
                    dialog,
                    (component,),
                    roles=("list item",),
                    showing_only=True,
                )
                is not None
            ),
            None,
        )
        if start is None:
            home = Path.home().resolve()
            try:
                directory.relative_to(home)
            except ValueError:
                pass
            else:
                home_button = self._find_named(
                    dialog,
                    ("Home",),
                    roles=("push button", "button"),
                    showing_only=True,
                )
                if home_button is not None:
                    self._click_accessible_bounds(
                        home_button, "file chooser Home location"
                    )
                    start = len(
                        tuple(
                            component
                            for component in home.parts
                            if component not in (home.anchor, "", "/")
                        )
                    )
        if start is None:
            breadcrumb = next(
                (
                    (index, node)
                    for index in range(len(components) - 1, -1, -1)
                    if (
                        node := self._find_named(
                            dialog,
                            (components[index],),
                            roles=("push button", "button"),
                            showing_only=True,
                        )
                    )
                    is not None
                ),
                None,
            )
            if breadcrumb is not None:
                index, node = breadcrumb
                self._click_accessible_bounds(
                    node, f"file chooser breadcrumb {components[index]!r}"
                )
                start = index + 1
        if start is None:
            root_button = self._find_named(
                dialog,
                ("/",),
                roles=("push button", "button"),
                showing_only=True,
            )
            if root_button is None:
                raise ElementNotFoundError("file chooser root location is absent")
            self._activate_node(root_button)
            start = 0

        for component in components[start:]:
            entry = self._file_dialog_entry(dialog, component)
            self._activate_file_dialog_entry(entry, component)
            try:
                # Trigger on the row itself: it is the leading signal that
                # the click only selected the folder. Breadcrumb depth lags
                # behind the click, so triggering on depth presses the
                # accept button before the selection has registered.
                visible_entry, accept_enabled = wait_until(
                    lambda: (
                        self._find_named(
                            dialog,
                            (component,),
                            roles=("list item",),
                            showing_only=True,
                        ),
                        self._sensitive_state(
                            self._file_dialog_accept_button(dialog)
                        ),
                    ),
                    lambda state: state[0] is None or state[1] is True,
                    timeout=self._timeout,
                    description=(
                        f"file chooser entry {component!r} to be selected "
                        "or entered"
                    ),
                )
                if visible_entry is not None and accept_enabled:
                    self._activate_file_dialog_accept(dialog)
                # Entering a folder clears the selection, so the accept
                # button goes insensitive again. Accept that as confirmation
                # alongside the row disappearing: a repeated path component
                # keeps a row of the same name in the new listing -- the
                # runner checkout lives at .../edit-atlas/edit-atlas -- so
                # absence alone never settles there.
                wait_until(
                    lambda: (
                        self._find_named(
                            dialog,
                            (component,),
                            roles=("list item",),
                            showing_only=True,
                        ),
                        self._sensitive_state(
                            self._file_dialog_accept_button(dialog)
                        ),
                    ),
                    lambda state: state[0] is None or state[1] is False,
                    timeout=self._timeout,
                    description=f"file chooser to enter {component!r}",
                )
            except PollTimeoutError as error:
                raise ActionNotSupportedError(str(error)) from error

    def _file_dialog_entry(self, dialog: Any, name: str) -> Any:
        try:
            return wait_until(
                lambda: self._find_named(
                    dialog,
                    (name,),
                    roles=("list item",),
                    showing_only=True,
                ),
                lambda value: value is not None,
                timeout=self._timeout,
                description=f"file chooser entry {name!r}",
            )
        except PollTimeoutError as error:
            raise ElementNotFoundError(str(error)) from error

    def _activate_file_dialog_entry(self, entry: Any, name: str) -> None:
        self._click_accessible_bounds(entry, f"file chooser entry {name!r}")

    def _click_accessible_bounds(self, node: Any, description: str) -> None:
        try:
            x = int(node.position[0] + node.size[0] / 2)
            y = int(node.position[1] + node.size[1] / 2)
            self._pointer_click(x, y)
        except Exception as error:
            raise ActionNotSupportedError(
                f"{description} rejected accessible-bounds input"
            ) from error

    def _file_dialog_accept_button(self, dialog: Any) -> Any:
        button = self._find_named(
            dialog,
            (
                "Open",
                "&Open",
                "Abrir",
                "&Abrir",
                "Save",
                "&Save",
                "Salvar",
                "&Salvar",
                "Export",
                "&Export",
                "Exportar",
                "&Exportar",
            ),
            roles=("push button", "button"),
        )
        if button is None:
            raise ElementNotFoundError("file chooser accept button is absent")
        return button

    def _activate_file_dialog_accept(self, dialog: Any) -> None:
        button = self._file_dialog_accept_button(dialog)
        wait_until(
            lambda: self._sensitive_state(button),
            lambda sensitive: sensitive is True,
            timeout=self._timeout,
            description="file chooser accept button to become enabled",
        )
        self._click_accessible_bounds(button, "file chooser accept button")

    def _wait_file_dialog_closed(
        self, dialog: Any, dialog_identifier: str
    ) -> None:
        wait_until(
            lambda: self._is_showing(dialog),
            lambda showing: not showing,
            timeout=self._timeout,
            consecutive=2,
            description=f"native file chooser {dialog_identifier!r} to close",
        )

    def _file_dialog(self, dialog_identifier: str) -> Any:
        def find() -> Any | None:
            self._ensure_running()
            exact = self._find_identifier(
                self._application, dialog_identifier, showing_only=True
            )
            if exact is not None:
                return exact
            for node in self._walk(self._application):
                try:
                    if (
                        str(node.role_name).casefold() == "dialog"
                        and self._is_showing(node)
                        and self._find_identifier(
                            node, "fileNameTextField", showing_only=False
                        )
                        is not None
                    ):
                        return node
                except Exception:
                    continue
            return None

        try:
            return wait_until(
                find,
                lambda value: value is not None,
                timeout=self._timeout,
                description=f"native file chooser {dialog_identifier!r}",
            )
        except PollTimeoutError as error:
            raise ElementNotFoundError(str(error)) from error

    def activate_menu_action(
        self, menu_identifier: str, action_identifier: str
    ) -> None:
        menu = self.element(menu_identifier)
        try:
            actions = menu.actions or {}
        except Exception as error:
            raise ActionNotSupportedError(
                f"cannot query actions for {menu_identifier!r}"
            ) from error
        normalized = {
            self._normalized_action_name(str(name)): str(name)
            for name in actions
        }
        action = normalized.get(self._normalized_action_name(action_identifier))
        if action is None:
            # A prior interaction with this same menu (e.g. toggling one of
            # its own checkable items) may have left it open. Clicking the
            # menu bar item again would toggle it closed instead of opening
            # it, so only click when the target action is not already
            # showing.
            if not self.has_element(action_identifier):
                self._activate_node(menu)
            self._activate_node(self.element(action_identifier))
            return
        try:
            if menu.do_action_named(action) is False:
                raise ActionNotSupportedError(
                    f"accessibility action {action!r} failed"
                )
        except ActionNotSupportedError:
            raise
        except Exception as error:
            raise ActionNotSupportedError(
                f"accessibility action {action!r} failed: {error}"
            ) from error

    def element_name(self, identifier: str, *, showing: bool = True) -> str:
        return str(self.element(identifier, showing=showing).name)

    def text(self, identifier: str, *, showing: bool = True) -> str:
        node = self.element(identifier, showing=showing)
        return self._node_text(node) or str(node.name)

    def text_content(self, identifier: str) -> list[str]:
        return [
            text
            for node in self._walk(self.element(identifier))
            if (text := self._node_text(node) or str(getattr(node, "name", "")))
        ]

    def visible_text(self, identifier: str) -> list[str]:
        return [
            text
            for node in self._walk(self.element(identifier))
            if self._is_showing(node)
            if (text := self._node_text(node) or str(getattr(node, "name", "")))
        ]

    def wait_name_contains(self, identifier: str, expected: str) -> str:
        return wait_until(
            lambda: self.element_name(identifier),
            lambda value: expected in value,
            timeout=self._timeout,
            description=f"{identifier!r} name to contain {expected!r}",
        )

    def wait_list_items(
        self, identifier: str, expected: Sequence[str]
    ) -> list[str]:
        expected_items = list(expected)
        return wait_until(
            lambda: self.list_items(identifier),
            lambda items: items == expected_items,
            timeout=self._timeout,
            description=f"{identifier!r} items to become {expected_items!r}",
        )

    def wait_selected_option(self, identifier: str, expected: str) -> str:
        return wait_until(
            lambda: self.selected_option(identifier),
            lambda selected: self._normalized_name(selected)
            == self._normalized_name(expected),
            timeout=self._timeout,
            description=f"{identifier!r} selection to become {expected!r}",
        )

    def wait_sensitive(self, identifier: str, expected: bool) -> bool:
        node = self.element(identifier)
        return wait_until(
            lambda: self._sensitive_state(node),
            lambda sensitive: sensitive == expected,
            timeout=self._timeout,
            description=f"{identifier!r} sensitivity to become {expected}",
        )

    def wait_text_contains(self, identifier: str, expected: str) -> str:
        return wait_until(
            lambda: self.text(identifier),
            lambda value: expected in value,
            timeout=self._timeout,
            description=f"{identifier!r} text to contain {expected!r}",
        )

    def wait_text_nonempty(self, identifier: str) -> str:
        return wait_until(
            lambda: self.text(identifier),
            lambda value: bool(value),
            timeout=self._timeout,
            description=f"{identifier!r} text to become nonempty",
        )

    def capture_artifacts(self, stem: str) -> None:
        safe_stem = "".join(
            character if character.isalnum() or character in "-_" else "_"
            for character in stem
        )
        tree_path = (
            self._artifact_directory / "accessibility" / f"{safe_stem}.txt"
        )
        tree_path.parent.mkdir(parents=True, exist_ok=True)
        tree_path.write_text(self._tree_dump(), encoding="utf-8")

        if shutil.which("xwd") is None or not os.environ.get("DISPLAY"):
            return
        screenshot_path = (
            self._artifact_directory / "screenshots" / f"{safe_stem}.xwd"
        )
        screenshot_path.parent.mkdir(parents=True, exist_ok=True)
        try:
            subprocess.run(
                ["xwd", "-root", "-silent", "-out", screenshot_path],
                check=False,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=min(self._timeout, 5.0),
            )
        except (OSError, subprocess.TimeoutExpired):
            pass

    def close(self) -> None:
        if self._process.poll() is not None:
            self._registry.stop(self._process)
            return
        try:
            if self.has_element("exitAction", showing=False):
                self.activate("fileMenu")
                self.activate("exitAction")
                wait_until(
                    self._process.poll,
                    lambda return_code: return_code is not None,
                    timeout=min(self._timeout, 5.0),
                    description="Edit Atlas to exit",
                )
        except Exception:
            pass
        finally:
            self._registry.stop(self._process)

    def _activate_node(self, node: Any) -> None:
        try:
            actions = node.actions or {}
        except Exception as error:
            raise ActionNotSupportedError(
                f"cannot query actions for {getattr(node, 'name', '')!r}"
            ) from error
        normalized = {
            self._normalized_action_name(str(name)): str(name)
            for name in actions
        }
        action = next(
            (
                normalized[self._normalized_action_name(name)]
                for name in self._ACTION_PRIORITY
                if self._normalized_action_name(name) in normalized
            ),
            None,
        )
        if action is None:
            raise ActionNotSupportedError(
                f"{getattr(node, 'name', '')!r} exposes actions "
                f"{tuple(actions)!r}, none of which is supported"
            )
        # Wait for a popup this node owns to open regardless of which
        # action name Qt happened to expose for it. Qt Widgets menu bar
        # items expose "ShowMenu", but Qt Quick's MenuBarItem exposes only
        # "Press"/"SetFocus" for the same open-a-dropdown behavior, so
        # gating this wait on the literal action name misses Quick menus
        # entirely and lets callers race the popup's creation.
        popup = self._find_role(node, "popup menu")
        if popup is not None and self._is_showing(popup):
            return
        threading.Thread(
            target=self._invoke_action,
            args=(node, action),
            daemon=True,
        ).start()
        if popup is not None:
            wait_until(
                lambda: self._showing_state(popup),
                lambda showing: showing,
                timeout=self._timeout,
                description=f"{getattr(node, 'name', '')!r} menu to open",
            )

    def _invoke_action(self, node: Any, action: str) -> None:
        try:
            if node.do_action_named(action) is False:
                raise ActionNotSupportedError(
                    f"accessibility action {action!r} failed"
                )
        except Exception as error:
            # Qt can apply a modal menu action before its AT-SPI D-Bus call
            # times out. Callers verify the resulting UI state separately.
            if self._is_no_reply_error(error):
                return
            failure = ActionNotSupportedError(
                f"accessibility action {action!r} failed: {error}"
            )
            with self._action_error_lock:
                self._action_errors.append(failure)

    def _find_named(
        self,
        root: Any,
        names: Sequence[str],
        *,
        roles: Sequence[str] | None = None,
        showing_only: bool = True,
    ) -> Any | None:
        candidates = {self._normalized_name(name) for name in names}
        valid_roles = (
            None if roles is None else {role.casefold() for role in roles}
        )
        roots = (root,)
        if root is self._application:
            try:
                roots = tuple(reversed(tuple(root.children))) or (root,)
            except Exception:
                pass
        for candidate_root in roots:
            for node in self._walk(
                candidate_root,
                descendant_leaves=self._IDENTIFIER_SEARCH_LEAVES,
            ):
                try:
                    name = self._normalized_name(str(node.name))
                    role = str(node.role_name).casefold()
                    if (
                        name in candidates
                        and (valid_roles is None or role in valid_roles)
                        and (not showing_only or node.showing)
                    ):
                        return node
                except Exception:
                    continue
        return None

    def _find_role(self, root: Any, role: str) -> Any | None:
        expected = role.casefold()
        for node in self._walk(root):
            if node is root:
                continue
            try:
                if str(node.role_name).casefold() == expected:
                    return node
            except Exception:
                continue
        return None

    def _find_identifier(
        self, root: Any, identifier: str, *, showing_only: bool = True
    ) -> Any | None:
        roots = (root,)
        if root is self._application:
            try:
                roots = tuple(reversed(tuple(root.children))) or (root,)
            except Exception:
                pass
        for candidate_root in roots:
            for node in self._walk(
                candidate_root,
                descendant_leaves=self._IDENTIFIER_SEARCH_LEAVES,
            ):
                node_identifier = self._node_identifier(node)
                if (
                    node_identifier != identifier
                    and not node_identifier.endswith(f".{identifier}")
                ):
                    continue
                if not showing_only or self._is_showing(node):
                    return node
        return None

    def _list_item(
        self, identifier: str, name: str, *, control: Any | None = None
    ) -> Any:
        root = self.element(identifier) if control is None else control
        node = self._find_named(root, (name,), showing_only=False)
        if node is None:
            raise ElementNotFoundError(
                f"list {identifier!r} contains no item named {name!r}"
            )
        return node

    def _visible_list_item(
        self, identifier: str, name: str, *, control: Any | None = None
    ) -> Any:
        root = self.element(identifier) if control is None else control
        node = self._list_item(identifier, name, control=root)
        if self._is_showing(node):
            return node
        if identifier != "eventColumnsList":
            self._scroll_into_view(node)
            return node

        def find_visible() -> Any | None:
            current_root = self.element(identifier)
            target = self._find_named(
                current_root, (name,), showing_only=False
            )
            if target is not None and self._is_showing(target):
                return target

            target_index = self._event_column_index(target)
            visible_indices = [
                index
                for candidate in self._walk(current_root)
                if self._is_showing(candidate)
                if (index := self._event_column_index(candidate)) is not None
            ]
            scroll_bar = self._find_role(current_root, "scroll bar")
            if (
                target_index is None
                or not visible_indices
                or scroll_bar is None
            ):
                return None
            direction = (
                "Decrease"
                if target_index < min(visible_indices)
                else "Increase"
            )
            try:
                actions = scroll_bar.actions or {}
            except Exception:
                return None
            normalized = {
                self._normalized_action_name(str(action)): str(action)
                for action in actions
            }
            action = normalized.get(direction.casefold())
            if action is not None:
                threading.Thread(
                    target=self._invoke_action,
                    args=(scroll_bar, action),
                    daemon=True,
                ).start()
            return None

        try:
            return wait_until(
                find_visible,
                lambda value: value is not None,
                timeout=self._timeout,
                description=f"list item {name!r} to scroll into view",
            )
        except PollTimeoutError as error:
            raise ActionNotSupportedError(str(error)) from error

    def _event_column_index(self, node: Any | None) -> int | None:
        if node is None:
            return None
        identifier = self._node_identifier(node).rsplit(".", 1)[-1]
        prefix = "eventColumn"
        suffix = "CheckBox"
        if not identifier.startswith(prefix):
            return None
        index = identifier[len(prefix) :]
        if index.endswith(suffix):
            index = index[: -len(suffix)]
        return int(index) if index.isdigit() else None

    def _scroll_into_view(self, node: Any) -> None:
        if self._is_showing(node):
            return
        try:
            result = node.scroll_to(self._atspi.ScrollType.ANYWHERE)
        except Exception as error:
            raise ActionNotSupportedError(
                f"cannot scroll {getattr(node, 'name', '')!r} into view"
            ) from error
        if result is False:
            raise ActionNotSupportedError(
                f"accessibility scroll failed for {getattr(node, 'name', '')!r}"
            )
        wait_until(
            lambda: self._is_showing(node),
            lambda showing: showing,
            timeout=self._timeout,
            description=f"{getattr(node, 'name', '')!r} to scroll into view",
        )

    def _tree_dump(self) -> str:
        if self._application is None:
            return "Edit Atlas is absent from the AT-SPI tree\n"
        lines: list[str] = []

        def append(node: Any, depth: int) -> None:
            if len(lines) >= 5000 or depth > self._MAX_ACCESSIBILITY_DEPTH:
                return
            try:
                identifier = self._node_identifier(node)
                lines.append(
                    f"{'  ' * depth}{node.role_name!s} name={node.name!r} "
                    f"id={identifier!r} showing={node.showing!r} "
                    f"sensitive={node.sensitive!r} position={node.position!r} "
                    f"size={node.size!r} actions={tuple(node.actions)!r}"
                )
                children = tuple(node.children)
            except Exception as error:
                lines.append(f"{'  ' * depth}<unavailable: {error}>")
                return
            for child in children:
                append(child, depth + 1)

        append(self._application, 0)
        return "\n".join(lines) + "\n"

    def _walk(
        self,
        root: Any,
        *,
        descendant_leaves: Sequence[str] = (),
    ) -> Iterable[Any]:
        pending = [(root, 0)]
        visited = 0
        while pending and visited < 10000:
            node, depth = pending.pop()
            visited += 1
            yield node
            if depth >= self._MAX_ACCESSIBILITY_DEPTH:
                continue
            node_identifier = self._node_identifier(node)
            if any(
                node_identifier == leaf
                or node_identifier.endswith(f".{leaf}")
                for leaf in descendant_leaves
            ):
                continue
            try:
                pending.extend(
                    (child, depth + 1)
                    for child in reversed(tuple(node.children))
                )
            except Exception:
                continue

    @staticmethod
    def _node_text(node: Any) -> str:
        try:
            value = node.text
        except Exception:
            return ""
        return "" if value is None else str(value)

    @staticmethod
    def _is_editable(node: Any) -> bool:
        try:
            return bool(node.editable)
        except Exception:
            return False

    @staticmethod
    def _is_showing(node: Any) -> bool:
        try:
            return bool(node.showing)
        except Exception:
            return False

    @staticmethod
    def _is_checked(node: Any) -> bool:
        try:
            return bool(node.checked)
        except Exception:
            return False

    def _checked_state(self, node: Any) -> bool:
        self._ensure_running()
        return self._is_checked(node)

    def _sensitive_state(self, node: Any) -> bool | None:
        self._ensure_running()
        try:
            return bool(node.sensitive)
        except Exception:
            return None

    def _showing_state(self, node: Any) -> bool:
        self._ensure_running()
        return self._is_showing(node)

    def _selected_state(self, node: Any) -> bool:
        self._ensure_running()
        try:
            return bool(node.selected)
        except Exception:
            return False

    def _option_is_selected(
        self, control: Any, option: Any, expected: str
    ) -> bool:
        self._ensure_running()
        for attribute in ("selected", "checked"):
            try:
                if bool(getattr(option, attribute)):
                    return True
            except Exception:
                continue
        current = self._node_text(control) or str(
            getattr(control, "name", "")
        )
        return self._normalized_name(current) == self._normalized_name(expected)

    @staticmethod
    def _node_identifier(node: Any) -> str:
        for attribute in ("accessible_id", "id"):
            try:
                value = getattr(node, attribute)
            except Exception:
                continue
            if value:
                return str(value)
        return ""

    def _ensure_running(self) -> None:
        with self._action_error_lock:
            if self._action_errors:
                raise self._action_errors.popleft()
        return_code = self._process.poll()
        if return_code is not None:
            raise AccessibilityBackendError(
                f"Edit Atlas exited unexpectedly with status {return_code}"
            )

    @staticmethod
    def _normalized_name(name: str) -> str:
        return name.replace("&", "").replace("_", "").strip().casefold()

    @staticmethod
    def _normalized_action_name(name: str) -> str:
        return "".join(
            character
            for character in name.casefold()
            if character.isalnum()
        )

    @staticmethod
    def _is_no_reply_error(error: Exception) -> bool:
        message = str(error).casefold()
        return (
            "did not receive a reply" in message
            or "org.freedesktop.dbus.error.noreply" in message
        )
