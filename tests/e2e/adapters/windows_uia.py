"""Windows desktop automation through pywinauto and UI Automation.

Interactions in this module use UIA control patterns, except for Windows'
native file chooser. Its blocking modal UIA provider exposes neither its
filename field nor accept button through traversal, so the adapter sets the
globally focused filename editor through its Value pattern. A combo box is
selected through its items' patterns when the provider exposes them while
collapsed, and otherwise by pointer input at the accessible bounds of an item
in its popup, because Qt does not commit desktop ExpandCollapse or
SelectionItem actions. A native popup realizes only the items in its viewport,
so it is paged like any virtualized list. Explicit coordinates and image
matching are absent.
"""

from __future__ import annotations

from collections import deque
import os
from pathlib import Path
import subprocess
import threading
from typing import Any, Mapping, Sequence

from adapters.processes import ProcessRegistry
from application.polling import PollTimeoutError, wait_until


class AccessibilityBackendError(RuntimeError):
    """Raised when the required Windows UIA backend is unavailable."""


class ElementNotFoundError(LookupError):
    """Raised when a semantic element is absent after bounded polling."""


class StartupNotReadyError(ElementNotFoundError):
    """Raised when a launched application never becomes automatable."""


class ActionNotSupportedError(RuntimeError):
    """Raised when an element exposes no suitable automation action."""


class WindowsUiaAdapter:
    """Launch packaged applications and connect to them through UIA."""

    def __init__(
        self,
        registry: ProcessRegistry,
        artifact_directory: Path,
        timeout: float,
        *,
        startup_timeout: float,
    ) -> None:
        if timeout <= 0:
            raise ValueError("timeout must be positive")
        if startup_timeout <= 0:
            raise ValueError("startup_timeout must be positive")
        self._registry = registry
        self._artifact_directory = artifact_directory
        self._timeout = timeout
        self._startup_timeout = startup_timeout
        self._application_class: Any = None
        self._desktop: Any = None
        self._win32_desktop: Any = None
        self._keyboard_sender: Any = None
        self._focused_element_getter: Any = None

    def preflight(self) -> None:
        if os.name != "nt":
            raise AccessibilityBackendError(
                "Windows UIA automation requires a Windows desktop session"
            )
        try:
            from pywinauto import Application, Desktop
            from pywinauto.controls.uiawrapper import UIAWrapper
            from pywinauto.keyboard import send_keys
            from pywinauto.uia_defines import IUIA
            from pywinauto.uia_element_info import UIAElementInfo

            desktop = Desktop(backend="uia")
            desktop.windows()
            win32_desktop = Desktop(backend="win32")
            win32_desktop.windows()
        except Exception as error:
            raise AccessibilityBackendError(
                f"pywinauto Windows backends could not be loaded: {error}"
            ) from error
        self._application_class = Application
        self._desktop = desktop
        self._win32_desktop = win32_desktop
        self._keyboard_sender = send_keys
        self._focused_element_getter = lambda: UIAWrapper(
            UIAElementInfo(IUIA().get_focused_element())
        )

    def launch(
        self,
        executable: Path,
        state_root: Path,
        *,
        locale: str,
        log_name: str,
        extra_environment: Mapping[str, str] | None = None,
    ) -> "WindowsApplicationSession":
        if self._desktop is None:
            self.preflight()
        if not executable.is_file():
            raise AccessibilityBackendError(
                f"installed GUI executable is unavailable: {executable}"
            )

        environment = os.environ.copy()
        environment.update(
            {
                "EDIT_ATLAS_TEST_STATE_ROOT": os.fspath(state_root),
                "LANG": locale,
                "LC_ALL": locale,
            }
        )
        if extra_environment:
            environment.update(extra_environment)

        process = self._registry.start(
            [executable],
            environment=environment,
            output_path=self._artifact_directory / "logs" / f"{log_name}.log",
        )
        session = None
        try:
            application = self._application_class(backend="uia").connect(
                process=process.pid, timeout=self._startup_timeout
            )
            session = WindowsApplicationSession(
                application=application,
                desktop=self._desktop,
                win32_desktop=self._win32_desktop,
                keyboard_sender=self._keyboard_sender,
                focused_element_getter=self._focused_element_getter,
                registry=self._registry,
                process=process,
                artifact_directory=self._artifact_directory,
                timeout=self._timeout,
                startup_timeout=self._startup_timeout,
            )
            session.wait_ready()
        except BaseException:
            if session is not None:
                session.capture_artifacts(f"{log_name}-startup")
                session.close()
            else:
                self._registry.stop(process)
            raise
        return session


class WindowsApplicationSession:
    # Bounds the paging sweep used to enumerate virtualized lists. Every
    # list in this application is small; this only prevents an unbounded
    # loop if a control keeps accepting scrolls without revealing rows.
    _MAX_LIST_PAGES = 32

    def __init__(
        self,
        *,
        application: Any,
        desktop: Any,
        win32_desktop: Any,
        keyboard_sender: Any,
        focused_element_getter: Any,
        registry: ProcessRegistry,
        process: subprocess.Popen[str],
        artifact_directory: Path,
        timeout: float,
        startup_timeout: float,
    ) -> None:
        self._application = application
        self._desktop = desktop
        self._win32_desktop = win32_desktop
        self._keyboard_sender = keyboard_sender
        self._focused_element_getter = focused_element_getter
        self._registry = registry
        self._process = process
        self._artifact_directory = artifact_directory
        self._timeout = timeout
        self._startup_timeout = startup_timeout
        self._action_errors: deque[ActionNotSupportedError] = deque()
        self._action_error_lock = threading.Lock()

    def wait_ready(self) -> None:
        try:
            main_window = self.element(
                "mainWindow", timeout=self._startup_timeout
            )
            # A visible UIA tree does not mean the launched window is in the
            # foreground. Pointer-backed interactions would otherwise land on
            # whichever application still covers it on a developer desktop.
            main_window.set_focus()
        except ElementNotFoundError as error:
            raise StartupNotReadyError(
                "the application did not become automatable within "
                f"{self._startup_timeout:g} seconds: {error}"
            ) from error
        except Exception as error:
            raise StartupNotReadyError(
                "the application became automatable but its main window "
                f"could not be focused: {error}"
            ) from error

    def element(
        self,
        identifier: str,
        *,
        showing: bool = True,
        timeout: float | None = None,
    ) -> Any:
        try:
            return wait_until(
                lambda: self._find_identifier(identifier, showing=showing),
                lambda value: value is not None,
                timeout=self._timeout if timeout is None else timeout,
                description=f"accessible identifier {identifier!r}",
            )
        except PollTimeoutError as error:
            raise ElementNotFoundError(str(error)) from error

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
        self._activate_node(self.element(identifier, showing=showing))

    def focus(self, identifier: str, *, showing: bool = False) -> None:
        # A virtualized row must actually be brought into view before a
        # coordinate-based click can land anywhere meaningful. UIA's
        # SetFocus scrolls the target element into view as part of
        # granting it focus, the same way keyboard navigation would.
        node = self.element(identifier, showing=showing)
        try:
            node.set_focus()
        except Exception as error:
            raise ActionNotSupportedError(
                f"{identifier!r} rejected keyboard focus"
            ) from error

    def activate_named(
        self, names: Sequence[str], *, within: str | None = None
    ) -> None:
        root = self.element(within) if within else None
        node = self._find_named(names, root=root)
        if node is None:
            raise ElementNotFoundError(
                f"could not find a showing accessible named one of {tuple(names)!r}"
            )
        self._activate_node(node)

    def set_text(self, identifier: str, value: str) -> None:
        node = self.element(identifier)
        pattern = self._pattern(node, "iface_value")
        if pattern is None:
            raise ActionNotSupportedError(
                f"{identifier!r} exposes no UIA Value pattern"
            )
        try:
            pattern.SetValue(value)
        except Exception as error:
            raise ActionNotSupportedError(
                f"UIA Value operation failed for {identifier!r}: {error}"
            ) from error
        wait_until(
            lambda: self._node_text(node),
            lambda text: text == value,
            timeout=self._timeout,
            description=f"{identifier!r} text to become {value!r}",
        )

    def set_checked(self, identifier: str, checked: bool) -> None:
        node = self.element(identifier)
        self._set_toggle_state(node, checked, identifier)

    def is_checked(self, identifier: str) -> bool:
        return self._is_checked(self.element(identifier))

    def is_sensitive(self, identifier: str) -> bool:
        return bool(self.element(identifier).is_enabled())

    def list_items(self, identifier: str) -> list[str]:
        return [self._node_name(node) for node in self._list_nodes(identifier)]

    def is_list_item_checked(self, identifier: str, name: str) -> bool:
        return self._is_checked(self._list_item(identifier, name))

    def select_list_item(self, identifier: str, name: str) -> None:
        # Qt exposes checkable QListWidget items through SelectionItem, but
        # CurrentIsSelected can reflect their check state without making them
        # current. The shared movement buttons act on the current row, which
        # UIA does not expose separately. A click at the reported bounds is
        # therefore the complete selection interaction; the caller verifies
        # its effect by waiting for the requested list order.
        node = self._list_item(identifier, name)
        self._click_accessible_node(node, f"list item {name!r}")

    def set_list_item_checked(
        self, identifier: str, name: str, checked: bool
    ) -> None:
        node = self._list_item(identifier, name)
        self._set_toggle_state(node, checked, f"list item {name!r}")

    def select_option(self, identifier: str, option: str) -> None:
        control = self.element(identifier)
        control_type = self._control_type(control)
        current = (
            self._combo_display_text(control)
            if control_type == "ComboBox"
            else self._node_text(control)
        )
        if self._normalized_name(current) == self._normalized_name(option):
            return

        if control_type == "ComboBox":
            self._select_combo_option(control, identifier, option)
            return

        options = self._option_nodes(control)
        target = self._named_node(options, option)
        if target is not None:
            if self._select_option_node(control, target, option):
                return

        range_value = self._pattern(control, "iface_range_value")
        if range_value is not None and options:
            target_index = next(
                (
                    index
                    for index, node in enumerate(options)
                    if self._normalized_name(self._node_name(node))
                    == self._normalized_name(option)
                ),
                None,
            )
            if target_index is not None:
                range_value.SetValue(float(target_index))
                self.wait_selected_option(identifier, option)
                return

        expand = self._pattern(control, "iface_expand_collapse")
        if expand is None:
            raise ActionNotSupportedError(
                f"{identifier!r} exposes neither UIA selection, RangeValue, "
                "nor ExpandCollapse"
            )
        expand.Expand()
        target = wait_until(
            lambda: self._find_named((option,)),
            lambda value: value is not None,
            timeout=self._timeout,
            description=f"option {option!r} for {identifier!r}",
        )
        if not self._select_option_node(control, target, option):
            raise ActionNotSupportedError(
                f"option {option!r} exposes no UIA SelectionItem, Invoke, "
                "or Toggle pattern"
            )

    def selected_option(self, identifier: str) -> str:
        control = self.element(identifier)
        selection = self._pattern(control, "iface_selection")
        if selection is not None:
            try:
                selected = selection.GetCurrentSelection()
                if selected:
                    return str(selected[0].CurrentName)
            except Exception:
                pass
        return self._combo_display_text(control)

    def open_file_dialog(self, dialog_identifier: str, path: Path) -> None:
        overwrite_expected = path.exists()
        dialog = self._native_file_dialog(dialog_identifier)
        wait_until(
            dialog.has_focus,
            lambda focused: focused,
            timeout=self._timeout,
            description=f"native file chooser {dialog_identifier!r} to receive focus",
        )
        try:
            # The shell exposes no traversable filename editor, but Ctrl+A
            # primes its focus and GetFocusedElement returns the standard edit
            # with a writable Value pattern. Set the whole path atomically so
            # autocomplete cannot discard characters from a typed long path.
            self._keyboard_sender("^a", pause=0)
            if not self._set_focused_native_filename(os.fspath(path)):
                self._keyboard_sender("^a", pause=0)
                self._keyboard_sender(
                    os.fspath(path),
                    with_spaces=True,
                    pause=0.05,
                    vk_packet=True,
                )
            self._keyboard_sender("{ENTER}", pause=0)
        except Exception as error:
            raise ActionNotSupportedError(
                "native file chooser rejected keyboard input: "
                f"{type(error).__name__}: {error}"
            ) from error
        wait_until(
            lambda: self._window_exists_while_running(dialog),
            lambda present: not present,
            timeout=self._timeout,
            description=(
                f"native file chooser {dialog_identifier!r} to accept keyboard input"
            ),
        )
        if overwrite_expected:
            self._accept_native_overwrite_confirmation()

    def _accept_native_overwrite_confirmation(self) -> None:
        """Accept the shell's overwrite prompt when Windows raises one.

        The packaged dialogs are declared with ``popupType: Popup.Item``, so
        they render in-scene and Windows adds no ``Confirm Save As`` window
        of its own; the application-owned replacement dialog is then the
        only confirmation. Pass through the shell prompt when a build does
        raise one, and continue when it does not, rather than requiring a
        window the frontend never creates.
        """
        try:
            confirmation = wait_until(
                self._current_native_overwrite_confirmation,
                lambda value: value is not None,
                timeout=min(2.0, self._timeout),
                description="native overwrite confirmation to appear",
            )
        except PollTimeoutError:
            return
        yes = wait_until(
            lambda: self._find_win32_button(
                confirmation, ("Yes", "&Yes", "Sim", "&Sim")
            ),
            lambda value: value is not None,
            timeout=self._timeout,
            description="native overwrite confirmation Yes button",
        )
        try:
            yes.click_input()
        except Exception as error:
            raise ActionNotSupportedError(
                "native overwrite confirmation rejected the Yes command"
            ) from error
        wait_until(
            lambda: self._window_exists_while_running(confirmation),
            lambda present: not present,
            timeout=self._timeout,
            description="native overwrite confirmation to close",
        )

    def activate_menu_action(
        self, menu_identifier: str, action_identifier: str
    ) -> None:
        menu = self.element(menu_identifier)
        # Windows offers an Invoke provider for any element with an action
        # interface, so accepting a pattern does not prove that it opened the
        # menu. Qt Quick's QML menu bar item implements its press action, while
        # Qt Widgets exposes menu titles as QAction and its accepted action does
        # not provide a usable complete interaction. Preserve the Quick action
        # contract and route only QAction openers through their bounds. A prior
        # interaction may already have left this menu open, so do neither when
        # the action is already showing.
        if not self.has_element(action_identifier):
            opened = False
            if self._uia_class_name(menu) != "QAction":
                try:
                    opened = self._open_menu_through_action(
                        menu, menu_identifier, action_identifier
                    )
                except (ActionNotSupportedError, PollTimeoutError):
                    pass
            if not opened:
                self._click_accessible_node(menu, menu_identifier)
                wait_until(
                    lambda: self.has_element(action_identifier),
                    lambda present: present,
                    timeout=self._timeout,
                    consecutive=10,
                    description=f"{menu_identifier!r} menu to open",
                )
        action = self.element(action_identifier)
        # Pointer input on purpose. Qt Widgets fulfils a menu item's press
        # action with QAction::trigger, which runs the action but leaves the
        # menu open, and an open menu grabs input from everything after it. A
        # click both triggers and dismisses, which is the whole interaction.
        self._click_accessible_node(action, action_identifier)

    def _open_menu_through_action(
        self, menu: Any, menu_identifier: str, action_identifier: str
    ) -> bool:
        invoke = self._pattern(menu, "iface_invoke")
        expand = self._pattern(menu, "iface_expand_collapse")
        if invoke is not None and expand is None:
            completed = self._invoke(menu)
            try:
                wait_until(
                    completed.is_set,
                    lambda done: done,
                    timeout=min(2.0, self._timeout),
                    description=f"{menu_identifier!r} opener action to complete",
                )
            except PollTimeoutError:
                # A Qt Widgets menu can keep Invoke blocked for as long as the
                # popup is active, while its provider serializes every tree
                # query behind that call. Dismiss it without querying UIA so
                # the action thread can return, then use the click path below.
                self._keyboard_sender("{ESC}", pause=0)
                wait_until(
                    completed.is_set,
                    lambda done: done,
                    timeout=self._timeout,
                    description=(
                        f"{menu_identifier!r} blocked opener action to dismiss"
                    ),
                )
                self._ensure_running()
                wait_until(
                    lambda: self.has_element(action_identifier),
                    lambda present: not present,
                    timeout=self._timeout,
                    consecutive=2,
                    description=(
                        f"{menu_identifier!r} blocked opener menu to dismiss"
                    ),
                )
                return False
            self._ensure_running()
        else:
            self._activate_node(menu)

        wait_until(
            lambda: self.has_element(action_identifier),
            lambda present: present,
            timeout=min(2.0, self._timeout),
            consecutive=3,
            description=f"{menu_identifier!r} action to open its menu",
        )
        return True

    @staticmethod
    def _expand_collapse_state(expand: Any) -> int:
        try:
            return int(expand.CurrentExpandCollapseState)
        except Exception:
            return 0

    def element_name(self, identifier: str, *, showing: bool = True) -> str:
        return self._node_name(self.element(identifier, showing=showing))

    def text(self, identifier: str, *, showing: bool = True) -> str:
        return self._node_text(self.element(identifier, showing=showing))

    def text_content(self, identifier: str) -> list[str]:
        root = self.element(identifier)
        content = [
            text
            for node in (root, *self._descendants(root))
            if (text := self._node_text(node))
        ]
        grid = self._pattern(root, "iface_grid")
        if grid is not None:
            try:
                for row in range(int(grid.CurrentRowCount)):
                    for column in range(int(grid.CurrentColumnCount)):
                        name = str(grid.GetItem(row, column).CurrentName or "")
                        if name:
                            content.append(name)
            except Exception:
                pass
        return content

    def visible_text(self, identifier: str) -> list[str]:
        root = self.element(identifier)
        return [
            text
            for node in (root, *self._descendants(root))
            if node.is_visible()
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
        accessibility_path = (
            self._artifact_directory / "accessibility" / f"{safe_stem}.txt"
        )
        accessibility_path.parent.mkdir(parents=True, exist_ok=True)
        try:
            native_dialog = self._current_win32_file_dialog()
        except Exception:
            native_dialog = None
        accessibility_tree = (
            self._win32_tree_dump(native_dialog)
            if native_dialog is not None
            else self._tree_dump()
        )
        accessibility_path.write_text(accessibility_tree, encoding="utf-8")
        windows = (
            [native_dialog]
            if native_dialog is not None
            else self._windows()
        )
        if not windows:
            return
        screenshot_path = (
            self._artifact_directory / "screenshots" / f"{safe_stem}.png"
        )
        screenshot_path.parent.mkdir(parents=True, exist_ok=True)
        try:
            windows[0].capture_as_image().save(screenshot_path)
        except Exception:
            pass

    def close(self) -> None:
        if self._process.poll() is not None:
            self._registry.stop(self._process)
            return
        if self._current_win32_file_dialog() is not None:
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
        expand = self._pattern(node, "iface_expand_collapse")
        if expand is not None:
            if self._expand_collapse_state(expand) != 1:
                self._execute_pattern(expand.Expand, node)
            return
        invoke = self._pattern(node, "iface_invoke")
        if invoke is not None:
            self._invoke(node)
            return
        toggle = self._pattern(node, "iface_toggle")
        if toggle is not None:
            self._execute_pattern(toggle.Toggle, node)
            return
        selection = self._pattern(node, "iface_selection_item")
        if selection is not None:
            self._execute_pattern(selection.Select, node)
            return
        raise ActionNotSupportedError(
            f"{self._node_name(node)!r} exposes no supported UIA control pattern"
        )

    def _select_option_node(
        self,
        control: Any,
        target: Any,
        option: str,
        *,
        timeout: float | None = None,
    ) -> bool:
        invoke = self._pattern(target, "iface_invoke")
        if invoke is not None:
            completed = self._invoke(target)
            wait_until(
                completed.is_set,
                lambda value: value,
                timeout=self._timeout if timeout is None else timeout,
                description=f"menu option {option!r} invocation to complete",
            )
            self._ensure_running()
            return True
        selection = self._pattern(target, "iface_selection_item")
        if selection is not None:
            self._execute_pattern(selection.Select, target)
            self._wait_selected_option_for(
                control, target, option, timeout=timeout
            )
            return True
        toggle = self._pattern(target, "iface_toggle")
        if toggle is not None:
            self._set_toggle_state(target, True, f"option {option!r}")
            return True
        return False

    def _select_combo_option(
        self, control: Any, identifier: str, option: str
    ) -> None:
        # A provider that exposes its items while collapsed can be selected
        # without the popup, which no scroll position can then affect. Qt
        # Quick's in-scene popup is such a provider. Qt does not always
        # commit the action, so a selection that does not take effect falls
        # through to the popup rather than failing.
        direct = self._named_node(self._option_nodes(control), option)
        if direct is not None:
            # A committed selection is observable at once, so this is bounded
            # briefly: the cost of the attempt is paid before every fallback.
            budget = min(2.0, self._timeout)
            try:
                if self._select_option_node(
                    control, direct, option, timeout=budget
                ):
                    self._wait_selected_option_for(
                        control, direct, option, timeout=budget
                    )
                    return
            except (ActionNotSupportedError, PollTimeoutError):
                pass

        self._open_combo_popup(control, identifier)
        target = wait_until(
            lambda: self._reveal_combo_option(control, option),
            lambda value: value is not None,
            timeout=self._timeout,
            description=f"combo box option {option!r} for {identifier!r}",
        )
        # Windows offers an Invoke provider for anything with an action
        # interface, whether or not the element implements the press action
        # behind it: a Qt Widgets popup item names only its toggle action, so
        # invoking it is accepted and selects nothing. The pattern is tried
        # first and the selection is what decides, with a click as the
        # fallback, because that is what commits on every provider seen here.
        budget = min(2.0, self._timeout)
        committed = False
        try:
            if self._select_option_node(
                control, target, option, timeout=budget
            ):
                self._wait_selected_option_for(
                    control, target, option, timeout=budget
                )
                committed = True
        except (ActionNotSupportedError, PollTimeoutError):
            pass
        if not committed:
            self._click_accessible_node(target, f"combo box option {option!r}")
        self._wait_selected_option_for(control, target, option)

    def _open_combo_popup(self, control: Any, identifier: str) -> None:
        """Open a combo box popup through the control's own press action.

        Qt offers an ExpandCollapse provider for every combo box but fulfils
        Expand with a ShowMenu action, which a Qt Quick item cannot declare,
        so that pattern accepts the request and opens nothing. Invoke reaches
        the press action the control does declare, which is why this does not
        go through the pattern order the other controls use.
        """
        if self._pattern(control, "iface_invoke") is not None:
            self._invoke(control)
            return
        self._click_accessible_node(control, identifier)

    def _reveal_combo_option(self, control: Any, option: str) -> Any | None:
        target = self._find_combo_option(control, option)
        if target is not None:
            return target

        # A native combo popup is a separate window that realizes only the
        # items inside its viewport, so an option below the fold is absent
        # from the UIA tree however long the wait. Page it the way a
        # virtualized list is paged instead of depending on where the
        # control sits on screen.
        popup = self._combo_popup_list(control)
        if popup is None:
            return None
        for _ in range(self._MAX_LIST_PAGES):
            if not self._page_list(popup, "down"):
                return None
            target = self._find_combo_option(control, option)
            if target is not None:
                return target
        return None

    def _find_combo_option(self, control: Any, option: str) -> Any | None:
        for root in (control, None):
            target = self._find_named(
                (option,), root=root, control_types=("ListItem",)
            )
            if target is not None:
                return target
        return None

    def _combo_popup_list(self, control: Any) -> Any | None:
        for root in (control, None):
            popup = self._find_named(
                None, root=root, control_types=("List",)
            )
            if popup is not None:
                return popup
        return None

    @staticmethod
    def _click_accessible_node(node: Any, description: str) -> None:
        try:
            node.click_input()
        except Exception as error:
            raise ActionNotSupportedError(
                f"accessible element {description!r} could not be clicked: {error}"
            ) from error

    def _wait_selected_option_for(
        self,
        control: Any,
        target: Any,
        expected: str,
        *,
        timeout: float | None = None,
    ) -> None:
        selection = self._pattern(target, "iface_selection_item")

        def selected_state() -> tuple[str, bool]:
            self._ensure_running()
            try:
                item_selected = selection is not None and bool(
                    selection.CurrentIsSelected
                )
            except Exception:
                item_selected = False
            return (self._combo_display_text(control), item_selected)

        wait_until(
            selected_state,
            lambda state: self._normalized_name(state[0])
            == self._normalized_name(expected)
            or state[1],
            timeout=self._timeout if timeout is None else timeout,
            description=f"option {expected!r} to become selected",
        )

    def _invoke(self, node: Any) -> threading.Event:
        pattern = self._pattern(node, "iface_invoke")
        if pattern is None:
            raise ActionNotSupportedError(
                f"{self._node_name(node)!r} exposes no UIA Invoke pattern"
            )
        return self._invoke_pattern(pattern.Invoke, node)

    def _invoke_pattern(
        self, operation: Any, node: Any
    ) -> threading.Event:
        return self._execute_async(operation, self._node_name(node))

    def _execute_async(
        self, operation: Any, node_name: str
    ) -> threading.Event:
        completed = threading.Event()

        def invoke() -> None:
            pythoncom = None
            try:
                if os.name == "nt":
                    import pythoncom

                    pythoncom.CoInitializeEx(pythoncom.COINIT_MULTITHREADED)
                operation()
            except Exception as error:
                failure = ActionNotSupportedError(
                    f"automation action failed for {node_name!r}: {error}"
                )
                with self._action_error_lock:
                    self._action_errors.append(failure)
            finally:
                try:
                    if pythoncom is not None:
                        pythoncom.CoUninitialize()
                finally:
                    completed.set()

        threading.Thread(target=invoke, daemon=True).start()
        return completed

    def _execute_pattern(self, operation: Any, node: Any) -> None:
        try:
            operation()
        except Exception as error:
            raise ActionNotSupportedError(
                f"UIA action failed for {self._node_name(node)!r}: {error}"
            ) from error

    def _set_toggle_state(self, node: Any, checked: bool, description: str) -> None:
        toggle = self._pattern(node, "iface_toggle")
        if toggle is None:
            raise ActionNotSupportedError(
                f"{description} exposes no UIA Toggle pattern"
            )
        if self._current_toggle_state(toggle) == checked:
            return

        invoke = self._pattern(node, "iface_invoke")
        if self._control_type(node) == "MenuItem" and invoke is not None:
            completed = self._invoke(node)
            wait_until(
                completed.is_set,
                lambda value: value,
                timeout=self._timeout,
                description=f"{description} invocation to complete",
            )
            self._ensure_running()

            def current_state() -> tuple[bool | None, bool]:
                try:
                    return self._toggle_state(toggle), node.is_visible()
                except Exception:
                    # Qt removes a menu item from the UIA tree when its menu
                    # closes. The setting is verified after the menu is opened
                    # again by the calling workflow.
                    return None, False

            wait_until(
                current_state,
                lambda current: current[0] == checked or not current[1],
                timeout=self._timeout,
                description=f"{description} invocation to update its state",
            )
            return

        self._execute_pattern(toggle.Toggle, node)
        wait_until(
            lambda: self._current_toggle_state(toggle),
            lambda current: current == checked,
            timeout=self._timeout,
            description=f"{description} checked state to become {checked}",
        )

    def _is_checked(self, node: Any) -> bool:
        toggle = self._pattern(node, "iface_toggle")
        if toggle is None:
            raise ActionNotSupportedError(
                f"{self._node_name(node)!r} exposes no UIA Toggle pattern"
            )
        return self._current_toggle_state(toggle)

    def _current_toggle_state(self, toggle: Any) -> bool:
        self._ensure_running()
        return self._toggle_state(toggle)

    def _sensitive_state(self, node: Any) -> bool | None:
        self._ensure_running()
        try:
            return bool(node.is_enabled())
        except Exception:
            return None

    @staticmethod
    def _toggle_state(toggle: Any) -> bool:
        return int(toggle.CurrentToggleState) == 1

    def _list_nodes(self, identifier: str) -> list[Any]:
        control = self.element(identifier)
        seen: set[str] = set()
        ordered: list[Any] = []
        indexed: dict[int, Any] = {}

        def collect() -> bool:
            added = False
            for node in self._descendants(control):
                control_type = self._control_type(node)
                if control_type not in ("ListItem", "DataItem", "CheckBox"):
                    continue
                name = self._node_name(node)
                index = self._event_column_index(node)
                if index is not None:
                    existing = indexed.get(index)
                    if existing is None or control_type in (
                        "ListItem",
                        "DataItem",
                    ):
                        indexed[index] = node
                        added = existing is None
                    continue
                if control_type == "CheckBox":
                    continue
                if name in seen:
                    continue
                seen.add(name)
                ordered.append(node)
                added = True
            return added

        collect()
        # A virtualized list exposes only the rows inside its viewport, so
        # a single snapshot silently omits everything scrolled out of view.
        # Page to the bottom merging newly revealed rows, then page back so
        # repeated observations of the same list stay consistent.
        pages = 0
        while pages < self._MAX_LIST_PAGES:
            if not self._page_list(control, "down"):
                break
            pages += 1
            if not collect():
                break
        for _ in range(pages):
            self._page_list(control, "up")
        if indexed:
            return [indexed[index] for index in sorted(indexed)]
        return ordered

    def _event_column_index(self, node: Any) -> int | None:
        automation_id = self._automation_id(node)
        marker = "eventColumn"
        marker_position = automation_id.rfind(marker)
        if marker_position < 0:
            return None
        suffix = automation_id[marker_position + len(marker) :]
        if suffix.endswith("CheckBox"):
            suffix = suffix[: -len("CheckBox")]
        return int(suffix) if suffix.isdigit() else None

    def _page_list(self, control: Any, direction: str) -> bool:
        """Page a scrollable control, reporting whether it accepted it."""
        try:
            control.scroll(direction, "page")
            return True
        except Exception:
            pass

        # Qt Quick's ListView currently exposes its rows through UIA but no
        # Scroll pattern. It does implement keyboard paging once UIA gives
        # the list focus, so use that semantic fallback instead of wheel or
        # coordinate input.
        try:
            control.set_focus()
            key = "{PGDN}" if direction == "down" else "{PGUP}"
            self._keyboard_sender(key, pause=0)
        except Exception:
            return False
        return True

    def _list_item(self, identifier: str, name: str) -> Any:
        node = self._named_node(self._list_nodes(identifier), name)
        if node is None:
            raise ElementNotFoundError(
                f"{identifier!r} contains no list item {name!r}"
            )
        return node

    def _option_nodes(self, control: Any) -> list[Any]:
        return [
            node
            for node in self._descendants(control)
            if self._control_type(node) in ("ListItem", "MenuItem", "RadioButton")
        ]

    def _native_file_dialog(self, identifier: str) -> Any:
        return wait_until(
            lambda: self._find_native_file_dialog(),
            lambda value: value is not None,
            timeout=self._timeout,
            description=f"native file chooser {identifier!r}",
        )

    def _find_native_file_dialog(self) -> Any | None:
        self._ensure_running()
        return self._current_win32_file_dialog()

    def _current_win32_file_dialog(self) -> Any | None:
        try:
            dialogs = self._win32_desktop.windows(
                process=self._process.pid,
                class_name="#32770",
                visible_only=False,
            )
        except Exception:
            return None
        return dialogs[-1] if dialogs else None

    def _current_native_overwrite_confirmation(self) -> Any | None:
        self._ensure_running()
        try:
            dialogs = self._win32_desktop.windows(
                class_name="#32770",
                visible_only=True,
            )
        except Exception:
            return None
        expected_titles = {
            self._normalized_name(title)
            for title in ("Confirm Save As", "Confirmar Salvar Como")
        }
        return next(
            (
                dialog
                for dialog in reversed(dialogs)
                if self._normalized_name(self._win32_text(dialog))
                in expected_titles
            ),
            None,
        )

    def _find_win32_button(
        self, root: Any, names: Sequence[str]
    ) -> Any | None:
        candidates = {self._normalized_name(name) for name in names}
        try:
            nodes = (root, *root.descendants())
        except Exception:
            nodes = (root,)
        for node in nodes:
            try:
                if (
                    str(node.class_name()).casefold() == "button"
                    and self._normalized_name(self._win32_text(node))
                    in candidates
                    and node.is_visible()
                ):
                    return node
            except Exception:
                continue
        return None

    def _set_focused_native_filename(self, path: str) -> bool:
        if self._focused_element_getter is None:
            return False
        try:
            editor = wait_until(
                self._focused_element_getter,
                lambda node: (
                    self._control_type(node) == "Edit"
                    and self._automation_id(node) == "1148"
                    and self._pattern(node, "iface_value") is not None
                ),
                timeout=min(2.0, self._timeout),
                description="focused native filename editor",
            )
            value = self._pattern(editor, "iface_value")
            value.SetValue(path)
            wait_until(
                lambda: self._focused_native_filename_value(),
                lambda text: text == path,
                timeout=min(2.0, self._timeout),
                description="native filename editor to contain the full path",
            )
            return True
        except Exception:
            return False

    def _focused_native_filename_value(self) -> str:
        if self._focused_element_getter is None:
            return ""
        editor = self._focused_element_getter()
        if (
            self._control_type(editor) != "Edit"
            or self._automation_id(editor) != "1148"
        ):
            return ""
        return self._node_text(editor)

    def _find_identifier(
        self, identifier: str, *, showing: bool = True
    ) -> Any | None:
        self._ensure_running()
        windows = tuple(reversed(self._windows()))
        for window in windows:
            if self._identifier_node_matches(window, identifier, showing):
                return window
        for window in windows:
            node = self._find_identifier_in(
                window, identifier, showing=showing, include_root=False
            )
            if node is not None:
                return node
        return None

    def _find_identifier_in(
        self,
        root: Any,
        identifier: str,
        *,
        showing: bool = False,
        include_root: bool = True,
    ) -> Any | None:
        if include_root and self._identifier_node_matches(
            root, identifier, showing
        ):
            return root
        for node in self._descendants(root):
            if self._identifier_node_matches(node, identifier, showing):
                return node
        return None

    def _find_named(
        self,
        names: Sequence[str] | None,
        *,
        root: Any | None = None,
        control_types: Sequence[str] | None = None,
    ) -> Any | None:
        candidates = (
            None
            if names is None
            else {self._normalized_name(name) for name in names}
        )
        valid_types = None if control_types is None else set(control_types)
        roots = (root,) if root is not None else tuple(reversed(self._windows()))
        for candidate_root in roots:
            if self._named_node_matches(candidate_root, candidates, valid_types):
                return candidate_root
        for candidate_root in roots:
            for node in self._descendants(candidate_root):
                if self._named_node_matches(node, candidates, valid_types):
                    return node
        return None

    def _windows(self) -> list[Any]:
        try:
            return list(self._desktop.windows(process=self._process.pid))
        except Exception:
            return list(self._application.windows())

    @staticmethod
    def _descendants(root: Any) -> list[Any]:
        try:
            return list(root.descendants())
        except Exception:
            return []

    @staticmethod
    def _pattern(node: Any, name: str) -> Any | None:
        try:
            return getattr(node, name)
        except Exception:
            return None

    @staticmethod
    def _automation_id(node: Any) -> str:
        return str(node.element_info.automation_id or "")

    @staticmethod
    def _identifier_matches(actual: str, expected: str) -> bool:
        return actual == expected or actual.endswith(f".{expected}")

    def _identifier_node_matches(
        self, node: Any, identifier: str, showing: bool
    ) -> bool:
        try:
            return self._identifier_matches(
                self._automation_id(node), identifier
            ) and (not showing or node.is_visible())
        except Exception:
            return False

    def _named_node_matches(
        self,
        node: Any,
        candidates: set[str] | None,
        valid_types: set[str] | None,
    ) -> bool:
        try:
            return (
                (
                    candidates is None
                    or self._normalized_name(self._node_name(node))
                    in candidates
                )
                and (
                    valid_types is None
                    or self._control_type(node) in valid_types
                )
                and node.is_visible()
            )
        except Exception:
            return False

    @staticmethod
    def _control_type(node: Any) -> str:
        return str(node.element_info.control_type or "")

    @staticmethod
    def _uia_class_name(node: Any) -> str:
        try:
            return str(node.element_info.class_name or "")
        except Exception:
            return ""

    @staticmethod
    def _control_id(node: Any) -> int:
        try:
            return int(node.element_info.control_id)
        except Exception:
            return -1

    @staticmethod
    def _win32_text(node: Any) -> str:
        try:
            return str(node.window_text() or "")
        except Exception:
            return ""

    @staticmethod
    def _node_name(node: Any) -> str:
        try:
            return str(node.element_info.name or "")
        except Exception:
            return ""

    def _node_text(self, node: Any) -> str:
        value = self._pattern(node, "iface_value")
        if value is not None:
            try:
                current = str(value.CurrentValue or "")
                if current:
                    return current
            except Exception:
                pass
        return self._node_name(node)

    def _combo_display_text(self, control: Any) -> str:
        for node in self._descendants(control):
            if self._control_type(node) != "Text":
                continue
            text = self._node_text(node)
            if text:
                return text
        return self._node_text(control)

    @classmethod
    def _named_node(cls, nodes: Sequence[Any], name: str) -> Any | None:
        expected = cls._normalized_name(name)
        return next(
            (
                node
                for node in nodes
                if cls._normalized_name(cls._node_name(node)) == expected
            ),
            None,
        )

    @staticmethod
    def _normalized_name(value: str) -> str:
        return value.replace("&", "").strip().casefold()

    @staticmethod
    def _window_exists(window: Any) -> bool:
        try:
            return bool(window.exists())
        except Exception:
            return False

    def _window_exists_while_running(self, window: Any) -> bool:
        self._ensure_running()
        return self._window_exists(window)

    def _ensure_running(self) -> None:
        if self._process.poll() is not None:
            raise RuntimeError(
                f"Edit Atlas exited with code {self._process.returncode}"
            )
        with self._action_error_lock:
            if self._action_errors:
                raise self._action_errors.popleft()

    def _tree_dump(self) -> str:
        lines: list[str] = []

        def append_node(node: Any, depth: int) -> None:
            lines.append(
                f"{'  ' * depth}{self._control_type(node)} "
                f"name={self._node_name(node)!r} "
                f"automation_id={self._automation_id(node)!r}"
            )
            try:
                children = node.children()
            except Exception:
                children = ()
            for child in children:
                append_node(child, depth + 1)

        for window in self._windows():
            append_node(window, 0)
        return "\n".join(lines) + ("\n" if lines else "")

    def _win32_tree_dump(self, root: Any) -> str:
        lines: list[str] = []

        def append_node(node: Any, depth: int) -> None:
            try:
                class_name = str(node.class_name() or "")
            except Exception:
                class_name = ""
            try:
                visible = bool(node.is_visible())
            except Exception:
                visible = False
            try:
                enabled = bool(node.is_enabled())
            except Exception:
                enabled = False
            lines.append(
                f"{'  ' * depth}{class_name} "
                f"text={self._win32_text(node)!r} "
                f"control_id={self._control_id(node)} "
                f"visible={visible} enabled={enabled}"
            )
            try:
                children = node.children()
            except Exception:
                children = ()
            for child in children:
                append_node(child, depth + 1)

        append_node(root, 0)
        return "\n".join(lines) + "\n"
