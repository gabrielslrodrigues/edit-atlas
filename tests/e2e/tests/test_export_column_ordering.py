from __future__ import annotations

import pytest

from application.gui import EditAtlasApplication
from application.polling import PollTimeoutError


DEFAULT_COLUMNS = [
    "Event",
    "Reel",
    "Clip name",
    "Source line",
    "Initial frame",
]


class ProjectionSession:
    """A projection list whose current row is lost when it is enumerated.

    Measured on Windows: reading the list's accessible children resets the
    widget's current row, and the movement control acts on that row. Clicking
    a row and then looking anything up therefore destroys the precondition
    for the move. `current_override` additionally models a control that acts
    on a row other than the current one.
    """

    def __init__(self, *, current_override: str | None = None) -> None:
        self.items = list(DEFAULT_COLUMNS)
        self.current: str | None = self.items[0]
        self.checked: dict[str, bool] = {name: True for name in self.items}
        self._current_override = current_override

    def has_element(self, identifier: str, *, showing: bool = True) -> bool:
        return identifier == "eventColumnsList"

    def list_items(self, identifier: str) -> list[str]:
        self.current = None
        return list(self.items)

    def current_list_item(self, identifier: str) -> str | None:
        return self.current

    def is_sensitive(self, identifier: str) -> bool:
        return self.current is not None and self.items.index(self.current) > 0

    def select_list_item(self, identifier: str, name: str) -> None:
        self.current = name

    def activate(self, identifier: str, **kwargs: object) -> None:
        assert identifier == "moveColumnUpButton"
        moving = self._current_override or self.current
        if moving is None:
            return
        position = self.items.index(moving)
        if position == 0:
            return
        self.items[position - 1], self.items[position] = (
            self.items[position],
            self.items[position - 1],
        )
        self.current = moving

    def move_list_item(
        self, identifier: str, name: str, control: str
    ) -> None:
        self.select_list_item(identifier, name)
        self.activate(control)

    def wait_list_items(
        self, identifier: str, expected: list[str]
    ) -> list[str]:
        observed = self.list_items(identifier)
        if observed != list(expected):
            raise PollTimeoutError(
                f"timed out waiting for {identifier!r} items to become "
                f"{list(expected)!r} (last observed: {observed!r})"
            )
        return observed

    def set_list_item_checked(
        self, identifier: str, name: str, checked: bool
    ) -> None:
        self.checked[name] = checked


def application_for(session: ProjectionSession) -> EditAtlasApplication:
    return EditAtlasApplication(lambda name: session)


def test_moving_a_column_does_not_enumerate_between_its_two_steps() -> None:
    session = ProjectionSession()

    application_for(session).set_export_columns({"Clip name"}, ["Clip name"])

    assert session.items[0] == "Clip name"


def test_enumerating_between_the_steps_would_lose_the_current_row() -> None:
    # Guards the reason the move is one operation: the select and the
    # activation cannot be separated by a lookup.
    session = ProjectionSession()

    session.select_list_item("eventColumnsList", "Clip name")
    session.list_items("eventColumnsList")
    session.activate("moveColumnUpButton")

    assert session.items == DEFAULT_COLUMNS


def test_moving_a_column_reports_the_row_that_actually_moved() -> None:
    session = ProjectionSession(current_override="Initial frame")

    with pytest.raises(AssertionError) as failure:
        application_for(session).set_export_columns(
            {"Clip name"}, ["Clip name"]
        )

    message = str(failure.value)
    assert "'Clip name'" in message
    assert "'Initial frame'" in message
