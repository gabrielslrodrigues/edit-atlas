"""Linux desktop automation through dogtail and AT-SPI.

This module deliberately avoids dogtail's pointer and keyboard simulation APIs.
Every interaction uses an accessibility action, selection, or editable-text
interface exposed by the application or the native file chooser.
"""

from __future__ import annotations

import os
from pathlib import Path
import shutil
import subprocess
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

            from dogtail import tree
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

        try:
            desktop = Atspi.get_desktop(0)
            desktop.get_child_count()
        except Exception as error:
            raise AccessibilityBackendError(
                f"AT-SPI desktop is not available: {error}"
            ) from error

        self._tree = tree
        self._atspi = Atspi

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
        registry: ProcessRegistry,
        process: subprocess.Popen[str],
        artifact_directory: Path,
        timeout: float,
    ) -> None:
        self._tree = tree
        self._atspi = atspi
        self._registry = registry
        self._process = process
        self._artifact_directory = artifact_directory
        self._timeout = timeout
        self._application: Any = None

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
            description=f"accessible identifier {identifier!r} to disappear",
        )

    def activate(self, identifier: str) -> None:
        self._activate_node(self.element(identifier))

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
        if bool(node.checked) != checked:
            self._activate_node(node)
        wait_until(
            lambda: bool(self.element(identifier).checked),
            lambda value: value == checked,
            timeout=self._timeout,
            description=f"{identifier!r} checked state to become {checked}",
        )

    def is_checked(self, identifier: str) -> bool:
        return bool(self.element(identifier).checked)

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
            lambda: bool(node.selected),
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
        if bool(node.checked) != checked:
            self._activate_node(node)
        wait_until(
            lambda: bool(node.checked),
            lambda value: value == checked,
            timeout=self._timeout,
            description=f"list item {name!r} checked state to become {checked}",
        )

    def select_option(self, identifier: str, option: str) -> None:
        control = self.element(identifier)
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
        dialog = self.element(dialog_identifier)
        try:
            editor = self._find_identifier(dialog, "fileNameEdit")
        except Exception:
            editor = None
        editors = [
            node
            for node in self._walk(dialog)
            if self._is_showing(node) and self._is_editable(node)
        ]
        if editor is None and not editors:
            raise ElementNotFoundError(
                f"{dialog_identifier!r} contains no showing editable path field"
            )
        if editor is None:
            editor = editors[-1]
        try:
            result = editor.set_text_contents(os.fspath(path))
        except Exception:
            editor.text = os.fspath(path)
            result = True
        if result is False:
            raise ActionNotSupportedError("native file chooser rejected the path")

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
            raise ElementNotFoundError("native file chooser accept button is absent")
        self._activate_node(button)
        self.wait_absent(dialog_identifier)

    def element_name(self, identifier: str, *, showing: bool = True) -> str:
        return str(self.element(identifier, showing=showing).name)

    def text(self, identifier: str, *, showing: bool = True) -> str:
        node = self.element(identifier, showing=showing)
        return self._node_text(node) or str(node.name)

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

    def wait_text_contains(self, identifier: str, expected: str) -> str:
        return wait_until(
            lambda: self.text(identifier),
            lambda value: expected in value,
            timeout=self._timeout,
            description=f"{identifier!r} text to contain {expected!r}",
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
        normalized = {str(name).casefold(): str(name) for name in actions}
        action = next(
            (
                normalized[name]
                for name in self._ACTION_PRIORITY
                if name in normalized
            ),
            None,
        )
        if action is None:
            raise ActionNotSupportedError(
                f"{getattr(node, 'name', '')!r} exposes actions "
                f"{tuple(actions)!r}, none of which is supported"
            )
        if node.do_action_named(action) is False:
            raise ActionNotSupportedError(
                f"accessibility action {action!r} failed"
            )

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
        for node in self._walk(root):
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

    def _find_identifier(
        self, root: Any, identifier: str, *, showing_only: bool = True
    ) -> Any | None:
        for node in self._walk(root):
            node_identifier = self._node_identifier(node)
            if node_identifier != identifier and not node_identifier.endswith(
                f".{identifier}"
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
            if len(lines) >= 5000:
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

    def _walk(self, root: Any) -> Iterable[Any]:
        pending = [root]
        visited = 0
        while pending and visited < 10000:
            node = pending.pop()
            visited += 1
            yield node
            try:
                pending.extend(reversed(tuple(node.children)))
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
        return_code = self._process.poll()
        if return_code is not None:
            raise AccessibilityBackendError(
                f"Edit Atlas exited unexpectedly with status {return_code}"
            )

    @staticmethod
    def _normalized_name(name: str) -> str:
        return name.replace("&", "").replace("_", "").strip().casefold()
