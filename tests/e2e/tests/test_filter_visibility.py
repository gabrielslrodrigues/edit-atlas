"""Keep filters open during row recreation."""

import pytest

from application.gui import EditAtlasApplication


class FilterSession:
    def __init__(self, expanded: bool, quick: bool = True):
        self.expanded = expanded
        self.quick = quick
        self.text = ""
        self.fields_waited = 0

    def has_element(self, identifier, *, showing=True):
        # Row lookup can be empty during an accessibility tree refresh.
        return identifier == "toggleFiltersButton" and self.quick

    def set_checked(self, identifier, checked):
        assert identifier == "toggleFiltersButton"
        assert self.quick
        self.expanded = checked

    def activate(self, identifier):
        assert identifier == "toggleFiltersButton"
        self.expanded = not self.expanded

    def element(self, identifier):
        assert identifier == "filterCondition0Field"
        assert self.expanded, "A transient row lookup closed the panel"
        self.fields_waited += 1
        return object()

    def set_text(self, identifier, text):
        assert identifier == "filterCondition0Text"
        assert self.expanded
        self.text = text


@pytest.mark.parametrize(
    "expanded,quick", [(True, True), (False, True), (True, False)]
)
def test_setting_filter_text_keeps_panel_open(expanded, quick):
    session = FilterSession(expanded, quick)
    app = EditAtlasApplication(lambda name: session)
    app.set_filter_text(0, "first")
    app.set_filter_text(0, "second")
    assert session.text == "second"
    assert session.fields_waited == 2
    assert session.expanded
