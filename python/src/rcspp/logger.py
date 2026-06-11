#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Thin Python wrappers around the rcspp C++ logger."""

from . import _core as _ext

LogLevel = _ext.logger.LogLevel


def set_log_level(level: LogLevel) -> None:
    """Set the minimum log level for the rcspp C++ logger.

    Args:
        level: Minimum level; messages below this level are suppressed.
    """
    _ext.logger.set_level(level)


def get_log_level() -> LogLevel:
    """Return the current log level.

    Returns:
        The active minimum :class:`LogLevel`.
    """
    return _ext.logger.get_level()


def init_logger(
    level: LogLevel = LogLevel.Info,
    to_console: bool = True,
    file_path: str = "",
) -> None:
    """Initialize the rcspp C++ logger.

    Args:
        level: Minimum log level (default ``Info``).
        to_console: Whether to emit log messages to stdout (default ``True``).
        file_path: Optional path to a log file; an empty string disables
            file logging.
    """
    _ext.logger.init(level, to_console, file_path)
