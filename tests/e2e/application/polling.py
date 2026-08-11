"""Runner-independent bounded state polling."""

from __future__ import annotations

from threading import Event
import time
from typing import Callable, TypeVar


Value = TypeVar("Value")


class PollTimeoutError(TimeoutError):
    """Raised when observable state does not reach the requested condition."""


def wait_until(
    observe: Callable[[], Value],
    accept: Callable[[Value], bool],
    *,
    timeout: float,
    interval: float = 0.05,
    consecutive: int = 1,
    description: str = "condition",
) -> Value:
    if timeout <= 0 or interval <= 0 or consecutive <= 0:
        raise ValueError("timeout, interval, and consecutive must be positive")
    deadline = time.monotonic() + timeout
    wake = Event()
    accepted = 0
    while True:
        value = observe()
        if accept(value):
            accepted += 1
            if accepted >= consecutive:
                return value
        else:
            accepted = 0
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise PollTimeoutError(
                f"timed out after {timeout:g} seconds waiting for {description}"
            )
        wake.wait(min(interval, remaining))
