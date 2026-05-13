#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# ── Build-directory discovery ─────────────────────────────────────────────────
# When the package is imported from the source tree (e.g. via sys.path pointing
# at src/python_interface/) the compiled _core extension is absent.  Pre-load it
# into sys.modules so the subsequent relative import (from . import _core) works.

import glob as _glob
import importlib.machinery as _impmach
import importlib.util as _imputil
import os as _os
import sys as _sys

_pkg_dir = _os.path.dirname(_os.path.abspath(__file__))

# On Python 3.8+/Windows, pre-load rcspp.dll from the package directory
# (installed-wheel case) so it is in-process before _core.pyd is imported.
if _sys.platform == "win32":
    import ctypes as _ctypes

    _rcspp_in_pkg = _os.path.join(_pkg_dir, "rcspp.dll")
    if _os.path.exists(_rcspp_in_pkg):
        _ctypes.WinDLL(_rcspp_in_pkg)
    del _ctypes, _rcspp_in_pkg
if hasattr(_os, "add_dll_directory"):
    _os.add_dll_directory(_pkg_dir)

if not [
    f
    for f in _glob.glob(_os.path.join(_pkg_dir, "_core*"))
    if _os.path.splitext(f)[1] in _impmach.EXTENSION_SUFFIXES
]:
    _root = _pkg_dir
    _found = False
    for _ in range(6):
        _root = _os.path.dirname(_root)
        for _build in ("cmake-build-release", "cmake-build-debug", "build", "out"):
            _candidate = _os.path.join(_root, _build, "src", "python_interface", "rcspp")
            _hits = [
                f
                for f in _glob.glob(_os.path.join(_candidate, "_core*"))
                if _os.path.splitext(f)[1] in _impmach.EXTENSION_SUFFIXES
            ]
            if _hits:
                # On Windows, pre-load rcspp.dll by absolute path via ctypes so
                # it is already in the process module list when _core.pyd is
                # loaded.  This sidesteps Python 3.8+ DLL-search-path
                # restrictions entirely: once a DLL is mapped, Windows finds it
                # by name without any directory search.
                if _sys.platform == "win32":
                    import ctypes as _ctypes

                    for _dll_dir in [
                        _candidate,
                        _os.path.join(_root, _build, "bin", "Release"),
                        _os.path.join(_root, _build, "bin", "Debug"),
                        _os.path.join(_root, _build, "bin"),
                    ]:
                        _rcspp_dll = _os.path.join(_dll_dir, "rcspp.dll")
                        if _os.path.exists(_rcspp_dll):
                            _ctypes.WinDLL(_rcspp_dll)
                            break
                    del _ctypes
                # Also register DLL directories for any other transitive deps.
                if hasattr(_os, "add_dll_directory"):
                    for _dll_dir in [
                        _candidate,
                        _os.path.join(_root, _build, "bin"),
                        _os.path.join(_root, _build, "bin", "Release"),
                        _os.path.join(_root, _build, "bin", "Debug"),
                        _os.path.join(_root, _build, "bin", "RelWithDebInfo"),
                        _os.path.join(_root, _build, "bin", "MinSizeRel"),
                    ]:
                        if _os.path.isdir(_dll_dir):
                            _os.add_dll_directory(_dll_dir)
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

del _glob, _impmach, _imputil, _os, _sys, _pkg_dir

from . import graph, logger, resource  # noqa: E402
from ._core.graph import check_interrupted  # noqa: E402
from .graph import ResourceGraph  # noqa: E402
from .logger import LogLevel, get_log_level, init_logger, set_log_level  # noqa: E402

__all__ = [
    "ResourceGraph",
    "check_interrupted",
    "graph",
    "resource",
    "logger",
    "LogLevel",
    "set_log_level",
    "get_log_level",
    "init_logger",
]
