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
    except AttributeError:
        pass  # not a coverage build or __gcov_dump not exported


def pytest_configure(config):
    # mip's SolverGurobi.__del__ crashes with AttributeError when Gurobi is not
    # licensed: __init__ raises before setting _ownsModel, then __del__ accesses it.
    # This is an upstream mip bug; suppress the resulting unraisable-exception warning.
    warnings.filterwarnings(
        "ignore",
        message=".*SolverGurobi.*",
        category=pytest.PytestUnraisableExceptionWarning,
    )
    # pytest's internal cache plugin leaves sqlite3 connections open; suppress the
    # resulting ResourceWarning so it doesn't pollute test output.
    warnings.filterwarnings(
        "ignore",
        message=".*unclosed.*<sqlite3.Connection.*",
        category=ResourceWarning,
    )
