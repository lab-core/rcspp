#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# ── Build-directory discovery ─────────────────────────────────────────────────
# When the package is imported from the source tree (e.g. via sys.path pointing
# at src/python_interface/) the compiled _core extension is absent.  Pre-load it
# into sys.modules so the subsequent relative import (from . import _core) works.

import glob as _glob
import importlib.util as _imputil
import os as _os
import sys as _sys

_pkg_dir = _os.path.dirname(_os.path.abspath(__file__))

if not _glob.glob(_os.path.join(_pkg_dir, "_core*")):
    _root = _pkg_dir
    _found = False
    for _ in range(6):
        _root = _os.path.dirname(_root)
        for _build in ("cmake-build-release", "cmake-build-debug", "build", "out"):
            _candidate = _os.path.join(_root, _build, "src", "python_interface", "rcspp")
            _hits = _glob.glob(_os.path.join(_candidate, "_core*"))
            if _hits:
                # Pre-register the extension in sys.modules before relative imports run.
                _spec = _imputil.spec_from_file_location("rcspp._core", _hits[0])
                _mod = _imputil.module_from_spec(_spec)
                _sys.modules["rcspp._core"] = _mod
                _spec.loader.exec_module(_mod)
                # Also extend __path__ so any other build-dir submodules resolve.
                __path__.append(_candidate)
                _found = True
                break
        if _found:
            break

del _glob, _imputil, _os, _sys, _pkg_dir

from . import graph, logger, resource  # noqa: E402
from .graph import ResourceGraph  # noqa: E402
from .logger import LogLevel, get_log_level, init_logger, set_log_level  # noqa: E402

__all__ = [
    "ResourceGraph",
    "graph",
    "resource",
    "logger",
    "LogLevel",
    "set_log_level",
    "get_log_level",
    "init_logger",
]
