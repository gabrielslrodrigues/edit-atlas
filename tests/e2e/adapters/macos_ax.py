"""macOS desktop automation through the Accessibility API.

Application controls are located by their stable Qt accessibility identifiers
and manipulated through AX attributes and actions. Native open/save panels are
owned by a separate macOS process, so they are located through the focused
system-wide accessibility hierarchy.
"""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass
import os
from pathlib import Path
import subprocess
import sys
import threading
from typing import Any, Iterable, Mapping, Sequence

from adapters.processes import ProcessRegistry
from application.polling import PollTimeoutError, wait_until


class AccessibilityBackendError(RuntimeError):
    """Raised when macOS Accessibility automation is unavailable."""


class ElementNotFoundError(LookupError):
    """Raised when a semantic element is absent after bounded polling."""


class ActionNotSupportedError(RuntimeError):
    """Raised when an element exposes no suitable AX operation."""


@dataclass(frozen=True)
class MacElement:
    """Small protocol-compatible view of an AXUIElement."""

    _session: "MacApplicationSession"
    _raw: Any

    @property
    def name(self) -> str:
        return self._session._node_name(self._raw)

    @property
    def role_name(self) -> str:
        return self._session._string_attribute(self._raw, "AXRole")


class MacAxAdapter:
    """Launch packaged applications and connect to them through macOS AX."""

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
        self._ax: Any = None

    def preflight(self) -> None:
        if sys.platform != "darwin":
            raise AccessibilityBackendError(
                "macOS AX automation requires an interactive macOS session"
            )
        try:
            import ApplicationServices
        except ImportError as error:
            raise AccessibilityBackendError(
                f"PyObjC ApplicationServices could not be loaded: {error}"
            ) from error

        if not ApplicationServices.AXIsProcessTrusted():
            raise AccessibilityBackendError(
                "the E2E process is not trusted for Accessibility; grant the "
                "terminal or pinned automation executable access under System "
                "Settings > Privacy & Security > Accessibility"
            )
        self._ax = ApplicationServices

    def launch(
        self,
        executable: Path,
        state_root: Path,
        *,
        locale: str,
        log_name: str,
        extra_environment: Mapping[str, str] | None = None,
    ) -> "MacApplicationSession":
        if self._ax is None:
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
                "NSUnbufferedIO": "YES",
            }
        )
        if extra_environment:
            environment.update(extra_environment)

        process = self._registry.start(
            [executable],
            environment=environment,
            output_path=self._artifact_directory / "logs" / f"{log_name}.log",
        )
        session = MacApplicationSession(
            ax=self._ax,
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


class MacApplicationSession:
    """Semantic operations against one running Edit Atlas process."""

    _CHILD_ATTRIBUTES = (
        "AXChildren",
        "AXRows",
        "AXVisibleChildren",
        "AXWindows",
        "AXMenuBar",
    )
    _ACTION_PRIORITY = ("AXPress", "AXConfirm", "AXPick", "AXOpen")
    _LIST_ROLES = ("AXRow", "AXListItem", "AXMenuItem")
    _EDITABLE_ROLES = ("AXTextField", "AXTextArea", "AXComboBox")

    def __init__(
        self,
        *,
        ax: Any,
        registry: ProcessRegistry,
        process: subprocess.Popen[str],
        artifact_directory: Path,
        timeout: float,
    ) -> None:
        self._ax = ax
        self._registry = registry
        self._process = process
        self._artifact_directory = artifact_directory
        self._timeout = timeout
        self._application = ax.AXUIElementCreateApplication(process.pid)
        self._system = ax.AXUIElementCreateSystemWide()
        if hasattr(ax, "AXUIElementSetMessagingTimeout"):
            messaging_timeout = min(timeout, 5.0)
            ax.AXUIElementSetMessagingTimeout(
                self._application, messaging_timeout
            )
            ax.AXUIElementSetMessagingTimeout(self._system, messaging_timeout)
        self._action_errors: deque[ActionNotSupportedError] = deque()
        self._action_error_lock = threading.Lock()

    def wait_ready(self) -> None:
        self.element("mainWindow")
        window = self.element("mainWindow")._raw
        if "AXRaise" in self._action_names(window):
            self._perform_action(window, "AXRaise")

    def element(self, identifier: str, *, showing: bool = True) -> MacElement:
        try:
            raw = wait_until(
                lambda: self._find_identifier(identifier, showing=showing),
                lambda value: value is not None,
                timeout=self._timeout,
                description=f"accessible identifier {identifier!r}",
            )
        except PollTimeoutError as error:
            raise ElementNotFoundError(str(error)) from error
        return MacElement(self, raw)

    def has_element(self, identifier: str, *, showing: bool = True) -> bool:
        self._ensure_running()
        return self._find_identifier(identifier, showing=showing) is not None

    def wait_absent(self, identifier: str) -> None:
        wait_until(
            lambda: self.has_element(identifier),
            lambda present: not present,
            timeout=self._timeout,
            consecutive=2,
            description=f"accessible identifier {identifier!r} to disappear",
        )

    def activate(self, identifier: str, *, showing: bool = True) -> None:
        self._activate_node(self.element(identifier, showing=showing)._raw)

    def activate_named(
        self, names: Sequence[str], *, within: str | None = None
    ) -> None:
        root = self.element(within)._raw if within else self._application
        node = self._find_named(root, names)
        if node is None:
            raise ElementNotFoundError(
                f"could not find a showing accessible named one of {tuple(names)!r}"
            )
        self._activate_node(node)

    def set_text(self, identifier: str, value: str) -> None:
        node = self.element(identifier)._raw
        self._set_value(node, value, identifier)
        wait_until(
            lambda: self._node_text(node),
            lambda text: text == value,
            timeout=self._timeout,
            description=f"{identifier!r} text to become {value!r}",
        )

    def set_checked(self, identifier: str, checked: bool) -> None:
        node = self.element(identifier)._raw
        self._set_checked_node(node, checked, identifier)

    def is_checked(self, identifier: str) -> bool:
        return self._is_checked(self.element(identifier)._raw)

    def is_sensitive(self, identifier: str) -> bool:
        node = self.element(identifier)._raw
        return bool(self._attribute(node, "AXEnabled", True))

    def list_items(self, identifier: str) -> list[str]:
        return [self._node_name(node) for node in self._list_nodes(identifier)]

    def is_list_item_checked(self, identifier: str, name: str) -> bool:
        return self._is_checked(self._list_item(identifier, name))

    def select_list_item(self, identifier: str, name: str) -> None:
        node = self._list_item(identifier, name)
        if self._is_settable(node, "AXSelected"):
            self._set_attribute(node, "AXSelected", True, f"list item {name!r}")
        else:
            self._activate_node(node)
        wait_until(
            lambda: self._selected_state_while_running(node),
            lambda selected: selected,
            timeout=self._timeout,
            description=f"list item {name!r} to become selected",
        )

    def set_list_item_checked(
        self, identifier: str, name: str, checked: bool
    ) -> None:
        self._set_checked_node(
            self._list_item(identifier, name), checked, f"list item {name!r}"
        )

    def select_option(self, identifier: str, option: str) -> None:
        control = self.element(identifier)._raw
        if self._normalized_name(self._node_text(control)) == self._normalized_name(
            option
        ):
            return

        actions = self._action_names(control)
        action = "AXShowMenu" if "AXShowMenu" in actions else "AXPress"
        if action not in actions:
            raise ActionNotSupportedError(
                f"{identifier!r} exposes neither AXShowMenu nor AXPress"
            )
        self._perform_action(control, action)
        target = wait_until(
            lambda: self._find_option_in_roots(option),
            lambda value: value is not None,
            timeout=self._timeout,
            description=f"option {option!r} for {identifier!r}",
        )
        self._activate_node(target)
        self.wait_selected_option(identifier, option)

    def selected_option(self, identifier: str) -> str:
        control = self.element(identifier)._raw
        selected = self._attribute(control, "AXSelectedChildren", [])
        if selected:
            return self._node_name(selected[0])
        return self._node_text(control)

    def open_file_dialog(self, dialog_identifier: str, path: Path) -> None:
        path = path.resolve()
        dialog = self._native_file_dialog(dialog_identifier)
        self._send_go_to_folder_shortcut()
        go_sheet = wait_until(
            self._focused_dialog,
            lambda value: value is not None and not self._same_element(value, dialog),
            timeout=self._timeout,
            description="native Go to Folder sheet",
        )
        path_editor = wait_until(
            lambda: self._focused_editable(go_sheet),
            lambda value: value is not None,
            timeout=self._timeout,
            description="Go to Folder path editor",
        )
        go_path = path.parent if "save" in dialog_identifier.casefold() else path
        self._set_value(path_editor, os.fspath(go_path), "Go to Folder path")
        self._activate_default_button(go_sheet, ("Go", "Ir"))
        wait_until(
            lambda: self._is_available(go_sheet) and self._is_showing(go_sheet),
            lambda showing: not showing,
            timeout=self._timeout,
            description="Go to Folder sheet to close",
        )

        dialog = self._native_file_dialog(dialog_identifier)
        if "save" in dialog_identifier.casefold():
            filename = self._filename_editor(dialog)
            self._set_value(filename, path.name, "save-panel filename")
            self._activate_default_button(dialog, ("Save", "Salvar"))
        else:
            self._activate_default_button(dialog, ("Open", "Abrir"))
        wait_until(
            lambda: self._is_available(dialog) and self._is_showing(dialog),
            lambda showing: not showing,
            timeout=self._timeout,
            description=f"native file chooser {dialog_identifier!r} to close",
        )

    def activate_menu_action(
        self, menu_identifier: str, action_identifier: str
    ) -> None:
        self.activate(menu_identifier)
        self.activate(action_identifier)

    def element_name(self, identifier: str, *, showing: bool = True) -> str:
        return self.element(identifier, showing=showing).name

    def text(self, identifier: str, *, showing: bool = True) -> str:
        node = self.element(identifier, showing=showing)._raw
        return self._node_text(node) or self._node_name(node)

    def text_content(self, identifier: str) -> list[str]:
        root = self.element(identifier)._raw
        return [
            text
            for node in self._walk(root)
            if (text := self._node_text(node))
        ]

    def visible_text(self, identifier: str) -> list[str]:
        root = self.element(identifier)._raw
        return [
            text
            for node in self._walk(root)
            if self._is_showing(node)
            if (text := self._node_text(node))
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
        node = self.element(identifier)._raw
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
        accessibility_path = (
            self._artifact_directory / "accessibility" / f"{safe_stem}.txt"
        )
        accessibility_path.parent.mkdir(parents=True, exist_ok=True)
        try:
            accessibility_path.write_text(self._tree_dump(), encoding="utf-8")
        except Exception as error:
            accessibility_path.write_text(
                f"Accessibility tree unavailable: {error}\n", encoding="utf-8"
            )

        screenshot_path = (
            self._artifact_directory / "screenshots" / f"{safe_stem}.png"
        )
        screenshot_path.parent.mkdir(parents=True, exist_ok=True)
        try:
            subprocess.run(
                ["/usr/sbin/screencapture", "-x", screenshot_path],
                check=False,
                timeout=5,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
        except (OSError, subprocess.SubprocessError):
            pass

    def close(self) -> None:
        if self._process.poll() is not None:
            self._registry.stop(self._process)
            return
        try:
            if self.has_element("exitAction", showing=False):
                self.activate_menu_action("fileMenu", "exitAction")
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
        actions = self._action_names(node)
        for action in self._ACTION_PRIORITY:
            if action in actions:
                self._perform_action(node, action)
                return
        if self._is_settable(node, "AXSelected"):
            self._set_attribute(node, "AXSelected", True, self._node_name(node))
            return
        raise ActionNotSupportedError(
            f"{self._node_name(node)!r} exposes no supported AX action"
        )

    def _perform_action(self, node: Any, action: str) -> threading.Event:
        completed = threading.Event()

        def perform() -> None:
            try:
                error = self._ax.AXUIElementPerformAction(node, action)
                cannot_complete = int(
                    getattr(self._ax, "kAXErrorCannotComplete", -25204)
                )
                if error not in (self._success, cannot_complete):
                    raise ActionNotSupportedError(
                        f"AX action {action!r} failed with error {error}"
                    )
            except Exception as error:
                failure = (
                    error
                    if isinstance(error, ActionNotSupportedError)
                    else ActionNotSupportedError(
                        f"AX action {action!r} failed: {error}"
                    )
                )
                with self._action_error_lock:
                    self._action_errors.append(failure)
            finally:
                completed.set()

        threading.Thread(target=perform, daemon=True).start()
        return completed

    def _set_checked_node(
        self, node: Any, checked: bool, description: str
    ) -> None:
        if self._is_checked(node) != checked:
            self._activate_node(node)
        wait_until(
            lambda: self._checked_state_while_running(node),
            lambda state: state == checked,
            timeout=self._timeout,
            description=f"{description} checked state to become {checked}",
        )

    def _native_file_dialog(self, identifier: str) -> Any:
        def find_dialog() -> Any | None:
            identified = self._find_identifier(identifier, showing=True)
            return identified if identified is not None else self._focused_dialog()

        try:
            return wait_until(
                find_dialog,
                lambda value: value is not None,
                timeout=self._timeout,
                description=f"native file chooser {identifier!r}",
            )
        except PollTimeoutError as error:
            raise ElementNotFoundError(str(error)) from error

    def _focused_dialog(self) -> Any | None:
        self._ensure_running()
        focused_application = self._attribute(
            self._system, "AXFocusedApplication", None
        )
        roots = [focused_application] if focused_application is not None else []
        for root in roots:
            focused_window = self._attribute(root, "AXFocusedWindow", None)
            if focused_window is not None:
                sheet = next(
                    (
                        node
                        for node in self._walk(focused_window)
                        if node is not focused_window
                        and self._string_attribute(node, "AXRole") == "AXSheet"
                        and self._is_showing(node)
                    ),
                    None,
                )
                if sheet is not None:
                    return sheet
                return focused_window
            for node in self._walk(root):
                if self._string_attribute(node, "AXRole") in (
                    "AXDialog",
                    "AXSheet",
                    "AXWindow",
                ) and self._is_showing(node):
                    return node
        return None

    def _focused_editable(self, root: Any) -> Any | None:
        candidates = []
        for node in self._walk(root):
            if (
                self._string_attribute(node, "AXRole") in self._EDITABLE_ROLES
                and self._is_settable(node, "AXValue")
            ):
                candidates.append(node)
        return next(
            (
                node
                for node in candidates
                if bool(self._attribute(node, "AXFocused", False))
            ),
            candidates[0] if len(candidates) == 1 else None,
        )

    def _filename_editor(self, dialog: Any) -> Any:
        candidates = [
            node
            for node in self._walk(dialog)
            if self._string_attribute(node, "AXRole") in self._EDITABLE_ROLES
            and self._is_settable(node, "AXValue")
        ]
        preferred = next(
            (
                node
                for node in candidates
                if any(
                    token in self._normalized_name(self._node_name(node))
                    for token in ("save as", "filename", "file name", "salvar como")
                )
            ),
            None,
        )
        if preferred is not None:
            return preferred
        if candidates:
            return candidates[0]
        raise ElementNotFoundError("native save panel contains no filename editor")

    def _activate_default_button(
        self, dialog: Any, fallback_names: Sequence[str]
    ) -> None:
        button = self._attribute(dialog, "AXDefaultButton", None)
        if button is None:
            button = self._find_named(dialog, fallback_names, roles=("AXButton",))
        if button is None:
            raise ElementNotFoundError(
                "dialog contains no default button named one of "
                f"{tuple(fallback_names)!r}"
            )
        wait_until(
            lambda: bool(self._attribute(button, "AXEnabled", True)),
            lambda enabled: enabled,
            timeout=self._timeout,
            description=f"dialog button {self._node_name(button)!r} to be enabled",
        )
        self._activate_node(button)

    def _send_go_to_folder_shortcut(self) -> None:
        required = (
            "CGEventCreateKeyboardEvent",
            "CGEventSetFlags",
            "CGEventPost",
            "kCGEventFlagMaskCommand",
            "kCGEventFlagMaskShift",
            "kCGHIDEventTap",
        )
        missing = [name for name in required if not hasattr(self._ax, name)]
        if missing:
            raise ActionNotSupportedError(
                "ApplicationServices lacks keyboard-event symbols: "
                + ", ".join(missing)
            )
        flags = (
            self._ax.kCGEventFlagMaskCommand | self._ax.kCGEventFlagMaskShift
        )
        for pressed in (True, False):
            event = self._ax.CGEventCreateKeyboardEvent(None, 5, pressed)
            if event is None:
                raise ActionNotSupportedError(
                    "could not create the native Go to Folder shortcut"
                )
            self._ax.CGEventSetFlags(event, flags)
            self._ax.CGEventPost(self._ax.kCGHIDEventTap, event)

    def _list_nodes(self, identifier: str) -> list[Any]:
        control = self.element(identifier)._raw
        rows = [
            node
            for node in self._walk(control)
            if node is not control
            and self._string_attribute(node, "AXRole") in self._LIST_ROLES
            and self._node_name(node)
        ]
        if rows:
            return rows
        return [
            node
            for node in self._children(control)
            if self._node_name(node)
        ]

    def _list_item(self, identifier: str, name: str) -> Any:
        normalized = self._normalized_name(name)
        node = next(
            (
                candidate
                for candidate in self._list_nodes(identifier)
                if self._normalized_name(self._node_name(candidate)) == normalized
            ),
            None,
        )
        if node is None:
            raise ElementNotFoundError(
                f"list {identifier!r} contains no item named {name!r}"
            )
        return node

    def _find_identifier(
        self, identifier: str, *, showing: bool
    ) -> Any | None:
        self._ensure_running()
        for node in self._walk(self._application):
            actual = self._string_attribute(node, "AXIdentifier")
            if self._identifier_matches(actual, identifier) and (
                not showing or self._is_showing(node)
            ):
                return node
        return None

    def _find_option_in_roots(self, name: str) -> Any | None:
        normalized = self._normalized_name(name)
        roots = [self._application]
        focused = self._attribute(self._system, "AXFocusedApplication", None)
        if focused is not None:
            roots.insert(0, focused)
        for root in roots:
            for node in self._walk(root):
                if self._normalized_name(self._node_name(node)) != normalized:
                    continue
                actions = self._action_names(node)
                if any(action in actions for action in self._ACTION_PRIORITY):
                    return node
                if self._is_settable(node, "AXSelected"):
                    return node
        return None

    def _find_named(
        self,
        root: Any,
        names: Sequence[str],
        *,
        roles: Sequence[str] | None = None,
    ) -> Any | None:
        normalized = {self._normalized_name(name) for name in names}
        for node in self._walk(root):
            if roles and self._string_attribute(node, "AXRole") not in roles:
                continue
            if self._normalized_name(self._node_name(node)) in normalized:
                return node
        return None

    def _walk(self, root: Any) -> Iterable[Any]:
        pending = deque([root])
        visited: set[str] = set()
        while pending:
            node = pending.popleft()
            key = repr(node)
            if key in visited:
                continue
            visited.add(key)
            yield node
            pending.extend(self._children(node))

    def _children(self, node: Any) -> list[Any]:
        children: list[Any] = []
        seen: set[str] = set()
        for attribute in self._CHILD_ATTRIBUTES:
            value = self._attribute(node, attribute, [])
            if isinstance(value, (str, bytes)) or not hasattr(value, "__iter__"):
                values = [value]
            else:
                values = list(value)
            for child in values:
                if child is None:
                    continue
                key = repr(child)
                if key not in seen:
                    seen.add(key)
                    children.append(child)
        return children

    def _set_value(self, node: Any, value: str, description: str) -> None:
        if not self._is_settable(node, "AXValue"):
            raise ActionNotSupportedError(
                f"{description!r} exposes no settable AXValue"
            )
        self._set_attribute(node, "AXValue", value, description)

    def _set_attribute(
        self, node: Any, attribute: str, value: Any, description: str
    ) -> None:
        error = self._ax.AXUIElementSetAttributeValue(node, attribute, value)
        if error != self._success:
            raise ActionNotSupportedError(
                f"setting {attribute} for {description!r} failed with error {error}"
            )

    def _is_settable(self, node: Any, attribute: str) -> bool:
        try:
            result = self._ax.AXUIElementIsAttributeSettable(
                node, attribute, None
            )
        except Exception:
            return False
        if not isinstance(result, tuple) or len(result) != 2:
            return False
        error, settable = result
        return error == self._success and bool(settable)

    def _attribute(self, node: Any, attribute: str, default: Any) -> Any:
        try:
            result = self._ax.AXUIElementCopyAttributeValue(
                node, attribute, None
            )
        except Exception:
            return default
        if not isinstance(result, tuple) or len(result) != 2:
            return default
        error, value = result
        return value if error == self._success and value is not None else default

    def _action_names(self, node: Any) -> tuple[str, ...]:
        try:
            result = self._ax.AXUIElementCopyActionNames(node, None)
        except Exception:
            return ()
        if not isinstance(result, tuple) or len(result) != 2:
            return ()
        error, names = result
        if error != self._success or names is None:
            return ()
        return tuple(str(name) for name in names)

    def _string_attribute(self, node: Any, attribute: str) -> str:
        value = self._attribute(node, attribute, "")
        return str(value) if value is not None else ""

    def _node_name(self, node: Any) -> str:
        for attribute in ("AXTitle", "AXDescription", "AXHelp", "AXValue"):
            value = self._attribute(node, attribute, "")
            if isinstance(value, str) and value:
                return value
        return ""

    def _node_text(self, node: Any) -> str:
        value = self._attribute(node, "AXValue", "")
        if isinstance(value, str) and value:
            return value
        return self._node_name(node)

    def _is_checked(self, node: Any) -> bool:
        return bool(self._checked_state(node))

    def _checked_state(self, node: Any) -> bool:
        value = self._attribute(node, "AXValue", False)
        if isinstance(value, (bool, int, float)):
            return bool(value)
        mark = self._string_attribute(node, "AXMenuItemMarkChar")
        return bool(mark)

    def _checked_state_while_running(self, node: Any) -> bool:
        self._ensure_running()
        return self._checked_state(node)

    def _is_selected(self, node: Any) -> bool:
        return bool(self._attribute(node, "AXSelected", False))

    def _selected_state_while_running(self, node: Any) -> bool:
        self._ensure_running()
        return self._is_selected(node)

    def _sensitive_state(self, node: Any) -> bool | None:
        self._ensure_running()
        value = self._attribute(node, "AXEnabled", None)
        return None if value is None else bool(value)

    def _is_showing(self, node: Any) -> bool:
        if bool(self._attribute(node, "AXHidden", False)):
            return False
        return bool(self._attribute(node, "AXVisible", True))

    def _is_available(self, node: Any) -> bool:
        self._ensure_running()
        role = self._attribute(node, "AXRole", None)
        return role is not None

    def _ensure_running(self) -> None:
        with self._action_error_lock:
            if self._action_errors:
                raise self._action_errors.popleft()
        return_code = self._process.poll()
        if return_code is not None:
            raise RuntimeError(
                f"Edit Atlas exited unexpectedly with code {return_code}"
            )

    def _tree_dump(self) -> str:
        lines: list[str] = []
        for root_name, root in self._diagnostic_roots():
            lines.append(f"[{root_name}]")
            for depth, node in self._walk_with_depth(root):
                attributes = {
                    name: self._attribute(node, name, None)
                    for name in (
                        "AXRole",
                        "AXIdentifier",
                        "AXTitle",
                        "AXDescription",
                        "AXValue",
                        "AXEnabled",
                        "AXVisible",
                        "AXSelected",
                    )
                }
                rendered = " ".join(
                    f"{name}={value!r}"
                    for name, value in attributes.items()
                    if value is not None
                )
                lines.append(f"{'  ' * depth}{rendered}")
        return "\n".join(lines) + "\n"

    def _diagnostic_roots(self) -> list[tuple[str, Any]]:
        roots = [("application", self._application)]
        focused = self._attribute(self._system, "AXFocusedApplication", None)
        if focused is not None and not self._same_element(focused, self._application):
            roots.append(("focused application", focused))
        return roots

    def _walk_with_depth(self, root: Any) -> Iterable[tuple[int, Any]]:
        pending = deque([(0, root)])
        visited: set[str] = set()
        while pending:
            depth, node = pending.popleft()
            key = repr(node)
            if key in visited:
                continue
            visited.add(key)
            yield depth, node
            pending.extend((depth + 1, child) for child in self._children(node))

    @property
    def _success(self) -> int:
        return int(getattr(self._ax, "kAXErrorSuccess", 0))

    @staticmethod
    def _same_element(left: Any, right: Any) -> bool:
        return left is right or repr(left) == repr(right)

    @staticmethod
    def _identifier_matches(actual: str, expected: str) -> bool:
        return actual == expected or actual.endswith(f".{expected}")

    @staticmethod
    def _normalized_name(value: str) -> str:
        return " ".join(value.split()).casefold()
