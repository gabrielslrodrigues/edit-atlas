"""Bounded process execution with deterministic teardown."""

from __future__ import annotations

from dataclasses import dataclass
import os
from pathlib import Path
import subprocess
from threading import Lock
from typing import Mapping, Sequence


@dataclass(frozen=True)
class CommandResult:
    arguments: tuple[str, ...]
    exit_code: int
    standard_output: str
    standard_error: str


class CommandTimeoutError(RuntimeError):
    """Raised after a command exceeds its bounded execution time."""


class ProcessRegistry:
    """Owns launched processes and guarantees bounded termination."""

    def __init__(self, termination_timeout: float = 5.0) -> None:
        if termination_timeout <= 0:
            raise ValueError("termination_timeout must be positive")
        self._termination_timeout = termination_timeout
        self._processes: set[subprocess.Popen[str]] = set()
        self._lock = Lock()

    def run(
        self,
        arguments: Sequence[str | os.PathLike[str]],
        *,
        timeout: float,
        environment: Mapping[str, str] | None = None,
        working_directory: Path | None = None,
    ) -> CommandResult:
        if timeout <= 0:
            raise ValueError("timeout must be positive")
        command = tuple(os.fspath(argument) for argument in arguments)
        process = subprocess.Popen(
            command,
            cwd=working_directory,
            env=None if environment is None else dict(environment),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            errors="replace",
        )
        with self._lock:
            self._processes.add(process)
        try:
            try:
                standard_output, standard_error = process.communicate(
                    timeout=timeout
                )
            except subprocess.TimeoutExpired as error:
                self._terminate(process)
                raise CommandTimeoutError(
                    f"command exceeded {timeout:g} seconds: {command!r}"
                ) from error
            return CommandResult(
                arguments=command,
                exit_code=process.returncode,
                standard_output=standard_output,
                standard_error=standard_error,
            )
        finally:
            with self._lock:
                self._processes.discard(process)

    def close_all(self) -> None:
        with self._lock:
            processes = tuple(self._processes)
        for process in processes:
            self._terminate(process)
        with self._lock:
            self._processes.difference_update(processes)

    def _terminate(self, process: subprocess.Popen[str]) -> None:
        if process.poll() is not None:
            return
        process.terminate()
        try:
            process.wait(timeout=self._termination_timeout)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=self._termination_timeout)
