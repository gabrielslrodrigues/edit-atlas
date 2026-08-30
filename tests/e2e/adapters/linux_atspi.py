"""Linux desktop automation through dogtail and AT-SPI.

This module uses accessibility actions, selection, and editable-text interfaces.
Qt Quick's fallback file chooser exposes only "SetFocus" for its file
delegates, so their accessible bounds select an entry, "SetFocus" claims
keyboard focus for it, and Enter then invokes its keyboard path.
"""

from __future__ import annotations

from collections import deque
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
        self._key_combo_sender: Any = None

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
        self._key_combo_sender = rawinput.keyCombo

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
            key_combo_sender=self._key_combo_sender,
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
        "press",
        "activate",
        "toggle",
        "show menu",
        "open",
    )

    def __init__(
        self,
        *,
        tree: Any,
        atspi: Any,
        keyboard_sender: Any,
        key_combo_sender: Any,
        registry: ProcessRegistry,
        process: subprocess.Popen[str],
        artifact_directory: Path,
        timeout: float,
    ) -> None:
        self._tree = tree
        self._atspi = atspi
        self._keyboard_sender = keyboard_sender
        self._key_combo_sender = key_combo_sender
        self._registry = registry
        self._process = process
        self._artifact_directory = artifact_directory
        self._timeout = timeout
        self._application: Any = None
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
            return wait_until(
                find,
                lambda value: value is not None,
                timeout=self._timeout,
                description=f"accessible identifier {identifier!r}",
            )
        except PollTimeoutError as error:
            raise ElementNotFoundError(str(error)) from error

    def has_element(self, identifier: str, *, showing: bool = True) -> bool:
        self._ensure_running()
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
        return [
            str(node.name)
            for node in self._walk(control)
            if str(getattr(node, "role_name", "")).casefold()
            in ("list item", "table row")
        ]

    def is_list_item_checked(self, identifier: str, name: str) -> bool:
        node = self._list_item(identifier, name)
        self._scroll_into_view(node)
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
        node = self._list_item(identifier, name, control=control)
        self._scroll_into_view(node)
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
        current = self._node_text(control) or str(
            getattr(control, "name", "")
        )
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
        except Exception as error:
            raise ActionNotSupportedError(
                f"combo box {identifier!r} exposes no accessibility interfaces"
            ) from error
        if "Value" not in interfaces:
            raise ActionNotSupportedError(
                f"combo box {identifier!r} exposes no Value interface"
            )

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

    def selected_option(self, identifier: str) -> str:
        control = self.element(identifier)
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
        save_dialog = button_name in ("save", "salvar")
        absolute_path = path.absolute()
        self._navigate_file_dialog(dialog, absolute_path.parent)

        if not save_dialog:
            file_entry = self._file_dialog_entry(dialog, absolute_path.name)
            self._activate_file_dialog_entry(file_entry, absolute_path.name)
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
            result = editor.set_text_contents(expected_path)
        except Exception:
            editor.text = expected_path
            result = True
        if result is False:
            raise ActionNotSupportedError("native file chooser rejected the path")
        wait_until(
            lambda: self._node_text(editor),
            lambda text: text == expected_path,
            timeout=self._timeout,
            description=f"native file chooser path to become {expected_path!r}",
        )

        self._activate_file_dialog_accept(dialog)
        self._wait_file_dialog_closed(dialog, dialog_identifier)

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
        # Prefer the breadcrumb bar's editable path field. Its reveal button
        # exposes a real Press action, while the folder delegates expose
        # only SetFocus, so entering the path outright is both fewer
        # interactions and the only one Qt gives us a real action for.
        if self._enter_file_dialog_path(dialog, directory):
            return
        root_button = self._find_named(
            dialog, ("/",), roles=("push button", "button")
        )
        if root_button is None:
            raise ElementNotFoundError("file chooser root location is absent")
        self._activate_node(root_button)

        components = tuple(
            component
            for component in directory.parts
            if component not in (directory.anchor, "", "/")
        )
        for index, component in enumerate(components):
            entry = self._file_dialog_entry(dialog, component)
            self._activate_file_dialog_entry(entry, component)
            if index + 1 < len(components):
                self._file_dialog_entry(dialog, components[index + 1])
            else:
                wait_until(
                    lambda: self._is_showing(entry),
                    lambda showing: not showing,
                    timeout=self._timeout,
                    description=f"file chooser to enter {component!r}",
                )

    def _enter_file_dialog_path(self, dialog: Any, directory: Path) -> bool:
        """Navigate by typing into the breadcrumb bar's path field.

        Returns False when the dialog does not expose that control, so the
        caller can fall back to activating one folder delegate at a time.
        Once the editor is found, failures are reported at their exact stage
        instead of being hidden by that fallback.
        """
        breadcrumbs = self._find_role(dialog, "page tab list")
        if breadcrumbs is None:
            return False
        editor = self._find_role(breadcrumbs, "text")
        if editor is None:
            return False
        if not self._is_showing(editor):
            # FolderBreadcrumbBar reserves Ctrl+L for revealing and focusing
            # its path editor. The unnamed button beside the breadcrumbs is
            # the Up button, not a path-editor action.
            focus_target = self._find_named(
                breadcrumbs,
                ("/",),
                roles=("push button", "button"),
            )
            if focus_target is not None:
                try:
                    if focus_target.do_action_named("SetFocus") is False:
                        raise ActionNotSupportedError(
                            "file chooser breadcrumb rejected SetFocus"
                        )
                except ActionNotSupportedError:
                    raise
                except Exception as error:
                    raise ActionNotSupportedError(
                        "file chooser breadcrumb focus action failed: "
                        f"{type(error).__name__}: {error}"
                    ) from error
                try:
                    wait_until(
                        lambda: bool(getattr(focus_target, "focused", True)),
                        lambda focused: focused,
                        timeout=self._timeout,
                        description="file chooser breadcrumb to receive focus",
                    )
                except PollTimeoutError as error:
                    raise ActionNotSupportedError(
                        "file chooser breadcrumb never received focus"
                    ) from error
            try:
                self._key_combo_sender("<Control>l")
            except Exception as error:
                raise ActionNotSupportedError(
                    "file chooser Ctrl+L input failed: "
                    f"{type(error).__name__}: {error}"
                ) from error
            try:
                wait_until(
                    lambda: self._is_showing(editor),
                    lambda showing: showing,
                    timeout=self._timeout,
                    description="file chooser path field to appear",
                )
            except PollTimeoutError as error:
                raise ActionNotSupportedError(
                    "file chooser Ctrl+L did not reveal the path field"
                ) from error
        expected = os.fspath(directory)
        try:
            if editor.set_text_contents(expected) is False:
                raise ActionNotSupportedError(
                    "file chooser path field rejected editable text input"
                )
        except ActionNotSupportedError:
            raise
        except Exception as editable_error:
            try:
                editor.text = expected
            except Exception as property_error:
                raise ActionNotSupportedError(
                    "file chooser path assignment failed through editable "
                    f"text ({type(editable_error).__name__}: {editable_error}) "
                    "and the text property "
                    f"({type(property_error).__name__}: {property_error})"
                ) from property_error
        try:
            wait_until(
                lambda: self._node_text(editor),
                lambda text: text == expected,
                timeout=self._timeout,
                description=f"file chooser path to become {expected!r}",
            )
        except PollTimeoutError as error:
            raise ActionNotSupportedError(
                f"file chooser path field did not retain {expected!r}"
            ) from error
        try:
            self._keyboard_sender("enter")
        except Exception as error:
            raise ActionNotSupportedError(
                "file chooser path field rejected Enter input: "
                f"{type(error).__name__}: {error}"
            ) from error
        # Qt hides the path field once it accepts the typed location.
        try:
            wait_until(
                lambda: self._is_showing(editor),
                lambda showing: not showing,
                timeout=self._timeout,
                description=f"file chooser to enter {expected!r}",
            )
        except PollTimeoutError as error:
            raise ActionNotSupportedError(
                f"file chooser kept the path field open after accepting {expected!r}"
            ) from error
        return True

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
        try:
            entry.click()
        except Exception as error:
            raise ActionNotSupportedError(
                f"file chooser entry {name!r} rejected accessible-bounds input"
            ) from error
        # A click at the entry's accessible bounds does not guarantee Qt's
        # fallback file dialog moved its internal keyboard focus onto this
        # delegate, and Enter is delivered to whatever currently holds that
        # focus. "SetFocus" is the one action these delegates expose for
        # exactly this purpose, so claim it explicitly before the keypress.
        try:
            entry.do_action_named("SetFocus")
        except Exception as error:
            raise ActionNotSupportedError(
                f"file chooser entry {name!r} rejected keyboard focus"
            ) from error
        # The SetFocus request is a D-Bus round trip; Qt may not have
        # actually processed the focus change by the time it returns. Wait
        # for the state AT-SPI reports back, rather than assuming the
        # keypress below already has somewhere correct to land. Nodes that
        # do not expose a focused state at all are treated as ready.
        try:
            wait_until(
                lambda: bool(getattr(entry, "focused", True)),
                lambda focused: focused,
                timeout=self._timeout,
                description=f"file chooser entry {name!r} to accept keyboard focus",
            )
        except PollTimeoutError as error:
            raise ActionNotSupportedError(
                f"file chooser entry {name!r} never reported keyboard focus"
            ) from error
        try:
            self._keyboard_sender("enter")
        except Exception as error:
            raise ActionNotSupportedError(
                f"file chooser entry {name!r} rejected Enter input"
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
        self._activate_node(button)

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
                    f"sensitive={node.sensitive!r} actions={tuple(node.actions)!r}"
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
