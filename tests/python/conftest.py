import ctypes
import warnings

import pytest


@pytest.fixture(scope="session", autouse=True)
def _flush_gcov_on_exit():
    """Flush gcov coverage data after all tests complete.

    When _core.so is built with --coverage, GCC normally flushes .gcda files at process
    exit via atexit().  pytest-cov can interfere with the normal exit path, causing gcov
    data to be silently dropped.  Calling __gcov_dump() explicitly at session end
    guarantees the data is written before gcovr runs.
    """
    yield
    try:
        ctypes.CDLL(None).__gcov_dump()
    except (AttributeError, TypeError, OSError):
        pass  # not a coverage build, __gcov_dump not exported, or non-Unix platform


def pytest_configure(config: pytest.Config) -> None:
    # pytest's internal cache plugin leaves sqlite3 connections open; suppress the
    # resulting ResourceWarning so it doesn't pollute test output.
    warnings.filterwarnings(
        "ignore",
        message=".*unclosed.*<sqlite3.Connection.*",
        category=ResourceWarning,
    )
