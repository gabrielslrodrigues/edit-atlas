"""Bounded process execution with deterministic teardown."""

from __future__ import annotations

import os
import subprocess
from dataclasses import dataclass
from pathlib import Path
from threading import Lock
from typing import IO, Mapping, Sequence


@dataclass(frozen=True)
class CommandResult:
    """Captured command arguments, exit status, and output streams."""

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
        self._streams: dict[subprocess.Popen[str], IO[str]] = {}
        self._lock = Lock()

    def start(
        self,
        arguments: Sequence[str | os.PathLike[str]],
        *,
        environment: Mapping[str, str] | None = None,
        working_directory: Path | None = None,
        output_path: Path | None = None,
    ) -> subprocess.Popen[str]:
        """Start a process, owning its optional output stream."""
        command = tuple(os.fspath(argument) for argument in arguments)
        stream = None
        if output_path is not None:
            output_path.parent.mkdir(parents=True, exist_ok=True)
            stream = output_path.open("w", encoding="utf-8")
        try:
            process = subprocess.Popen(
                command,
                cwd=working_directory,
                env=None if environment is None else dict(environment),
                stdout=stream if stream is not None else subprocess.DEVNULL,
                stderr=subprocess.STDOUT,
                text=True,
                encoding="utf-8",
                errors="replace",
            )
        except BaseException:
            if stream is not None:
                stream.close()
            raise
        with self._lock:
            self._processes.add(process)
            if stream is not None:
                self._streams[process] = stream
        return process

    def run(
        self,
        arguments: Sequence[str | os.PathLike[str]],
        *,
        timeout: float,
        environment: Mapping[str, str] | None = None,
        working_directory: Path | None = None,
    ) -> CommandResult:
        """Capture output, terminating the process on timeout."""
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
        """Terminate owned processes and close their output streams."""
        with self._lock:
            processes = tuple(self._processes)
        for process in processes:
            self._terminate(process)
        with self._lock:
            self._processes.difference_update(processes)
            streams = [
                self._streams.pop(process, None) for process in processes
            ]
        for stream in streams:
            if stream is not None:
                stream.close()

    def stop(self, process: subprocess.Popen[str]) -> None:
        """Terminate one owned process and release its output stream."""
        self._terminate(process)
        with self._lock:
            self._processes.discard(process)
            stream = self._streams.pop(process, None)
        if stream is not None:
            stream.close()

    def _terminate(self, process: subprocess.Popen[str]) -> None:
        if process.poll() is not None:
            return
        process.terminate()
        try:
            process.wait(timeout=self._termination_timeout)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait(timeout=self._termination_timeout)
