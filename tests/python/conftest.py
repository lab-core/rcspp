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


def pytest_configure(config):
    # mip's Gurobi backend crashes in __del__ when Gurobi is not licensed:
    # SolverGurobi.__init__ raises before setting _ownsModel, then __del__
    # accesses it (AttributeError); on Windows, LoadLibrary(None) or a
    # NoneType-path check raises TypeError.  All of these surface as
    # PytestUnraisableExceptionWarning — suppress any that mention mip or
    # Gurobi, and also the LoadLibrary / NoneType variants from the DLL probe.
    for _pat in (
        r".*SolverGurobi.*",
        r".*LoadLibrary.*",
        r".*NoneType.*iterable.*",
        r".*gurobipy.*",
    ):
        warnings.filterwarnings(
            "ignore",
            message=_pat,
            category=pytest.PytestUnraisableExceptionWarning,
        )
    # pytest's internal cache plugin leaves sqlite3 connections open; suppress the
    # resulting ResourceWarning so it doesn't pollute test output.
    warnings.filterwarnings(
        "ignore",
        message=".*unclosed.*<sqlite3.Connection.*",
        category=ResourceWarning,
    )
