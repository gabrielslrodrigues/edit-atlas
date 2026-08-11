from __future__ import annotations

from application.polling import wait_until


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
