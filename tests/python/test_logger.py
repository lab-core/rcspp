#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

import os
import sys

sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "python", "src")
)

from rcspp.logger import LogLevel, get_log_level, init_logger, set_log_level  # noqa: E402


def test_set_get_log_level():
    original = get_log_level()
    set_log_level(LogLevel.Debug)
    assert get_log_level() == LogLevel.Debug
    set_log_level(original)
    assert get_log_level() == original


def test_init_logger_defaults():
    init_logger()


def test_init_logger_explicit():
    init_logger(LogLevel.Warn, to_console=True, file_path="")
