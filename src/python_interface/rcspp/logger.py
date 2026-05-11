#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

from . import _core as _ext

LogLevel = _ext.logger.LogLevel


def set_log_level(level: LogLevel) -> None:
    """Set the minimum log level for the rcspp C++ logger."""
    _ext.logger.set_level(level)


def get_log_level() -> LogLevel:
    """Return the current log level."""
    return _ext.logger.get_level()


def init_logger(
    level: LogLevel = LogLevel.Info,
    to_console: bool = True,
    file_path: str = "",
) -> None:
    """Initialize the logger (level, console output, optional log file path)."""
    _ext.logger.init(level, to_console, file_path)
