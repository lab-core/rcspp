"""
Logging control
"""

from __future__ import annotations

import typing

__all__: list[str] = [
    "Debug",
    "Error",
    "Fatal",
    "Info",
    "LogLevel",
    "Trace",
    "Warn",
    "get_level",
    "init",
    "set_level",
]

class LogLevel:
    """
    Members:

      Trace

      Debug

      Info

      Warn

      Error

      Fatal
    """

    Debug: typing.ClassVar[LogLevel]
    Error: typing.ClassVar[LogLevel]
    Fatal: typing.ClassVar[LogLevel]
    Info: typing.ClassVar[LogLevel]
    Trace: typing.ClassVar[LogLevel]
    Warn: typing.ClassVar[LogLevel]
    __members__: typing.ClassVar[dict[str, LogLevel]]
    def __eq__(self, other: typing.Any) -> bool: ...
    def __getstate__(self) -> int: ...
    def __hash__(self) -> int: ...
    def __index__(self) -> int: ...
    def __init__(self, value: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    def __int__(self) -> int: ...
    def __ne__(self, other: typing.Any) -> bool: ...
    def __repr__(self) -> str: ...
    def __setstate__(self, state: typing.SupportsInt | typing.SupportsIndex) -> None: ...
    def __str__(self) -> str: ...
    @property
    def name(self) -> str: ...
    @property
    def value(self) -> int: ...

def get_level() -> LogLevel:
    """
    Return the current log level.
    """

def init(level: LogLevel = ..., to_console: bool = True, file_path: str = "") -> None:
    """
    Initialize the logger (level, console output, optional log file).
    """

def set_level(level: LogLevel) -> None:
    """
    Set the minimum log level.
    """

Debug: LogLevel
Error: LogLevel
Fatal: LogLevel
Info: LogLevel
Trace: LogLevel
Warn: LogLevel
