from __future__ import annotations

import pytest

from application.polling import PollTimeoutError, wait_until


def test_wait_until_requires_consecutive_accepted_observations() -> None:
    observations = iter((False, True, False, True, True))

    result = wait_until(
        lambda: next(observations),
        lambda value: value,
        timeout=1.0,
        interval=0.001,
        consecutive=2,
    )

    assert result


def test_wait_until_timeout_reports_the_last_observed_value() -> None:
    with pytest.raises(PollTimeoutError, match=r"last observed: \['a', 'b'\]"):
        wait_until(
            lambda: ["a", "b"],
            lambda value: value == ["a", "b", "c"],
            timeout=0.05,
            interval=0.01,
            description="'thing' to become something else",
        )
