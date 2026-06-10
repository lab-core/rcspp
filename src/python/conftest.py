import warnings

import pytest


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
