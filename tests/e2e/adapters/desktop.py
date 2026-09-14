"""Runner-independent contract for semantic desktop automation."""

from __future__ import annotations

from pathlib import Path
from typing import Protocol, Sequence


class DesktopElement(Protocol):
    """Accessible name and role exposed by a platform backend."""

    @property
    def name(self) -> str:
        """Return the accessible name."""
        ...

    @property
    def role_name(self) -> str:
        """Return the platform role name."""
        ...


class DesktopSession(Protocol):
    """Semantic desktop operations shared by the platform backends."""

    def activate(self, identifier: str, *, showing: bool = True) -> None:
        """Activate a control, optionally requiring it to be visible."""
        ...

    def activate_named(
        self, names: Sequence[str], *, within: str | None = None
    ) -> None:
        """Activate a named control within the optional container."""
        ...

    def capture_artifacts(self, stem: str) -> None:
        """Record diagnostic artifacts under the supplied stem."""
        ...

    def close(self) -> None:
        """Release session resources and stop the application."""
        ...

    def element(
        self, identifier: str, *, showing: bool = True
    ) -> DesktopElement:
        """Find an accessible element by its automation identifier."""
        ...

    def element_name(self, identifier: str, *, showing: bool = True) -> str:
        """Return the accessible name of an identified control."""
        ...

    def focus(self, identifier: str, *, showing: bool = False) -> None:
        """Give an identified control keyboard focus."""
        ...

    def has_element(self, identifier: str, *, showing: bool = True) -> bool:
        """Report whether an identified element is present."""
        ...

    def is_checked(self, identifier: str) -> bool:
        """Return the checked state of an identified control."""
        ...

    def is_sensitive(self, identifier: str) -> bool:
        """Report whether an identified control is enabled."""
        ...

    def list_items(self, identifier: str) -> list[str]:
        """Return the item names in list order."""
        ...

    def current_list_item(self, identifier: str) -> str | None:
        """Return the current item name, or None if none is selected."""
        ...

    def is_list_item_checked(self, identifier: str, name: str) -> bool:
        """Return the checked state of the named list item."""
        ...

    def open_file_dialog(self, dialog_identifier: str, path: Path) -> None:
        """Choose a path through the identified file dialog."""
        ...

    def activate_menu_action(
        self, menu_identifier: str, action_identifier: str
    ) -> None:
        """Open a menu and activate its identified action."""
        ...

    def select_option(self, identifier: str, option: str) -> None:
        """Choose a named option in the identified control."""
        ...

    def select_list_item(self, identifier: str, name: str) -> None:
        """Select a named list item."""
        ...

    def move_list_item(self, identifier: str, name: str, control: str) -> None:
        """Move a named item with the identified reorder control."""
        ...

    def selected_option(self, identifier: str) -> str:
        """Return the selected option name."""
        ...

    def set_checked(self, identifier: str, checked: bool) -> None:
        """Set a control to the requested checked state."""
        ...

    def set_list_item_checked(
        self, identifier: str, name: str, checked: bool
    ) -> None:
        """Set the checked state of a named list item."""
        ...

    def set_text(self, identifier: str, value: str) -> None:
        """Replace the text in an identified control."""
        ...

    def text(self, identifier: str, *, showing: bool = True) -> str:
        """Read the text of an identified control."""
        ...

    def text_content(self, identifier: str) -> list[str]:
        """Collect text exposed below an identified element."""
        ...

    def visible_text(self, identifier: str) -> list[str]:
        """Collect visible text below an identified element."""
        ...

    def wait_absent(self, identifier: str) -> None:
        """Wait until an identified element disappears."""
        ...

    def wait_name_contains(self, identifier: str, expected: str) -> str:
        """Wait until the accessible name contains the expected text."""
        ...

    def wait_list_items(
        self, identifier: str, expected: Sequence[str]
    ) -> list[str]:
        """Wait until ordered list contents match the expected names."""
        ...

    def wait_selected_option(self, identifier: str, expected: str) -> str:
        """Wait until the selected option matches the expected name."""
        ...

    def wait_sensitive(self, identifier: str, expected: bool) -> bool:
        """Wait until the enabled state matches the expected value."""
        ...

    def wait_text_contains(self, identifier: str, expected: str) -> str:
        """Wait until control text contains the expected substring."""
        ...

    def wait_text_nonempty(self, identifier: str) -> str:
        """Wait until the control exposes nonempty text."""
        ...
