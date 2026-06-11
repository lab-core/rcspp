#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Rcspp — Python bindings for the Resource-Constrained Shortest Path solver.

Exposes the compiled C++ extension (``_core``) together with high-level
Python helpers for graphs, resources, logging, and column-generation pools.
The package locates the extension in common build directories automatically
when it is imported from a source tree.
"""

# ── Build-directory discovery ─────────────────────────────────────────────────
# When the package is imported from the source tree (e.g. via sys.path pointing
# at python/src/) the compiled _core extension is absent.  Pre-load it
# into sys.modules so the subsequent relative import (from . import _core) works.

import importlib.util as _imputil
import os as _os
import sys as _sys
import sysconfig as _sysconfig

_pkg_dir = _os.path.dirname(_os.path.abspath(__file__))
# EXT_SUFFIX is version+platform specific: e.g. ".cp311-win_amd64.pyd", ".cpython-311-linux-gnu.so"
_ext_suffix = _sysconfig.get_config_var("EXT_SUFFIX") or ".so"

if not _os.path.exists(_os.path.join(_pkg_dir, f"_core{_ext_suffix}")):
    _root = _pkg_dir
    _found = False
    for _ in range(6):
        _root = _os.path.dirname(_root)
        for _build in ("cmake-build-release", "cmake-build-debug", "build", "out"):
            _candidate = _os.path.join(_root, _build, "python", "rcspp")
            _core_path = _os.path.join(_candidate, f"_core{_ext_suffix}")
            if _os.path.exists(_core_path):
                if _sys.platform == "win32":
                    # Pre-load the extension with unrestricted DLL search (LoadLibraryW via
                    # ctypes) so its dependencies are in the process module list before
                    # Python's extension loader uses LOAD_LIBRARY_SEARCH_DEFAULT_DIRS.
                    import ctypes as _ctypes

                    try:
                        _ctypes.CDLL(_core_path)
                    except OSError:
                        pass
                    del _ctypes
                _spec = _imputil.spec_from_file_location("rcspp._core", _core_path)
                _mod = _imputil.module_from_spec(_spec)
                _sys.modules["rcspp._core"] = _mod
                _spec.loader.exec_module(_mod)
                __path__.append(_candidate)
                _found = True
                break
        if _found:
            break

del _imputil, _os, _sys, _sysconfig, _pkg_dir, _ext_suffix

from . import graph, logger, resource  # noqa: E402
from ._core import available_memory_bytes, process_memory_bytes  # noqa: E402
from ._core.graph import (  # noqa: E402
    AlgorithmStatus,
    SolveResult,
    check_interrupted,
)
from .graph import ResourceGraph  # noqa: E402
from .logger import LogLevel, get_log_level, init_logger, set_log_level  # noqa: E402

__all__ = [
    "AlgorithmStatus",
    "ResourceGraph",
    "available_memory_bytes",
    "SolveResult",
    "check_interrupted",
    "graph",
    "process_memory_bytes",
    "resource",
    "logger",
    "LogLevel",
    "set_log_level",
    "get_log_level",
    "init_logger",
]
