#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""What a run records about the code it measured and the machine it ran on.

The record goes into the run's ``metadata.json``. A runner makes it once its run is over, so
that it has the versions of the solvers the run imported. The machine is known by a label the
user gives, never by its host name.
"""

import importlib.metadata
import os
import platform
import re
import subprocess  # nosec B404: git only, with fixed arguments
import sys
import tomllib
from pathlib import Path
from typing import Any

from .source_tree import TREE

#: The groups of a record, in its order.
RECORD_GROUPS = ("source", "code", "versions", "machine", "benchmark")


# MARK: Source


def git(arguments: list[str], tree: Path = TREE) -> str | None:
    """The output of a git command in `tree`, or None when git or the repository is
    missing."""
    try:
        completed = subprocess.run(  # nosec B603 B607: git, found on the path, fixed arguments
            ["git", "-C", str(tree), *arguments],
            capture_output=True,
            text=True,
            check=True,
        )
    except (OSError, subprocess.CalledProcessError):
        return None

    return completed.stdout.strip()


def source_record(tree: Path = TREE) -> dict[str, Any]:
    """The commit, whether tracked files have uncommitted changes, and the branch."""
    changes = git(["status", "--porcelain", "--untracked-files=no"], tree)
    return {
        "commit": git(["rev-parse", "HEAD"], tree),
        "uncommitted_changes": None if changes is None else bool(changes),
        "branch": git(["rev-parse", "--abbrev-ref", "HEAD"], tree),
    }


# MARK: Code


def read_cmake_cache(build: Path) -> dict[str, str]:
    """The entries of a build's ``CMakeCache.txt``, by name (empty when it has none)."""
    entries: dict[str, str] = {}
    path = build / "CMakeCache.txt"

    if not path.is_file():
        return entries

    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = re.match(r"^([A-Za-z_][\w.-]*):[A-Z_]+=(.*)$", line)

        if match:
            entries[match.group(1)] = match.group(2)

    return entries


def compiler_version(build: Path) -> dict[str, str]:
    """The C++ compiler's identity and version, from the files CMake wrote when it first
    configured the build."""
    found: dict[str, str] = {}

    for path in sorted(build.glob("CMakeFiles/*/CMakeCXXCompiler.cmake")):
        text = path.read_text(encoding="utf-8", errors="replace")

        for name in ("CMAKE_CXX_COMPILER", "CMAKE_CXX_COMPILER_ID", "CMAKE_CXX_COMPILER_VERSION"):
            match = re.search(rf'set\({name} "([^"]*)"\)', text)

            if match:
                found[name] = match.group(1)

    return found


def code_record(build: Path) -> dict[str, Any]:
    """Where rcspp and its ``_core`` were imported from, and how the build was
    configured."""
    import rcspp
    import rcspp._core

    cache = read_cmake_cache(build)
    compiler = compiler_version(build)
    return {
        "rcspp": rcspp.__file__,
        "core": rcspp._core.__file__,
        "build": str(build),
        "generator": cache.get("CMAKE_GENERATOR"),
        "build_type": cache.get("CMAKE_BUILD_TYPE"),
        # Multi-configuration generators (Visual Studio) leave it out of the cache.
        "compiler": cache.get("CMAKE_CXX_COMPILER") or compiler.get("CMAKE_CXX_COMPILER"),
        "compiler_id": compiler.get("CMAKE_CXX_COMPILER_ID"),
        "compiler_version": compiler.get("CMAKE_CXX_COMPILER_VERSION"),
        "options": {
            name: value for name, value in sorted(cache.items()) if name.startswith("USE_")
        },
    }


# MARK: Versions


def package_version(name: str) -> str | None:
    try:
        return importlib.metadata.version(name)
    except importlib.metadata.PackageNotFoundError:
        return None


def tree_version(tree: Path = TREE) -> str | None:
    """Rcspp's version in the tree's ``pyproject.toml``: the installed package's
    metadata, if any, describes another rcspp."""
    try:
        with open(tree / "pyproject.toml", "rb") as file:
            version: str = tomllib.load(file)["project"]["version"]
            return version
    except (OSError, KeyError, tomllib.TOMLDecodeError):
        return None


def version_record() -> dict[str, Any]:
    """Python's, rcspp's and numpy's versions, and the solvers' of the packages the run
    imported."""
    import numpy

    versions: dict[str, Any] = {
        "python": platform.python_version(),
        "rcspp": tree_version(),
        "numpy": numpy.__version__,
    }

    if "highspy" in sys.modules:
        highspy = sys.modules["highspy"]
        versions["highspy"] = package_version("highspy")
        versions["highs"] = highspy.Highs().version()

    if "gurobipy" in sys.modules:
        gurobipy = sys.modules["gurobipy"]
        versions["gurobipy"] = package_version("gurobipy")
        versions["gurobi"] = ".".join(str(part) for part in gurobipy.gurobi.version())

    return versions


# MARK: Machine


def processor_name() -> str:
    """The processor's model name, as the system gives it."""
    if sys.platform == "win32":
        import winreg

        key = r"HARDWARE\DESCRIPTION\System\CentralProcessor\0"

        try:
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, key) as handle:
                name, _ = winreg.QueryValueEx(handle, "ProcessorNameString")
                return str(name).strip()
        except OSError:
            pass
    elif sys.platform == "darwin":
        name = subprocess.run(  # nosec B603 B607: sysctl, fixed arguments
            ["sysctl", "-n", "machdep.cpu.brand_string"], capture_output=True, text=True
        ).stdout.strip()

        if name:
            return name
    elif os.path.isfile("/proc/cpuinfo"):
        with open("/proc/cpuinfo", encoding="utf-8") as file:
            for line in file:
                if line.startswith("model name"):
                    return line.split(":", 1)[1].strip()

    return platform.processor()


def machine_record(label: str | None = None) -> dict[str, Any]:
    """The machine's label, system, processor, cores and memory."""
    import psutil

    return {
        "label": label,
        "system": platform.platform(),
        "processor": processor_name(),
        "physical_cores": psutil.cpu_count(logical=False),
        "logical_cores": psutil.cpu_count(logical=True),
        "memory_bytes": psutil.virtual_memory().total,
    }


# MARK: Record


def make_record(
    build: Path,
    machine_label: str | None = None,
    processes: int = 1,
    solver_threads: int | None = None,
) -> dict[str, Any]:
    """The record of a run: its source, code, versions, machine, and how many runs shared the
    machine and how many threads its solver had (None: the solver's default)."""
    return {
        "source": source_record(),
        "code": code_record(build),
        "versions": version_record(),
        "machine": machine_record(machine_label),
        "benchmark": {"processes": processes, "solver_threads": solver_threads},
    }
