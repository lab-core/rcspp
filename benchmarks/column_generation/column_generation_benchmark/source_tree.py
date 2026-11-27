#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The source tree the benchmark measures: the one it lives in.

rcspp is imported from a build of this tree (``build*/python``, or a folder given with
``--build``), where the build put the compiled ``_core`` next to a copy of ``python/src/rcspp``.
Nothing else may provide it: a virtual environment often has an older rcspp installed, and a run
would then measure that one without failing. The copy must also equal ``python/src``, or the run
would measure Python sources older than the tree's; and a build configured from another tree is
refused, as its ``_core`` holds that tree's C++. Whether ``_core`` was built from the current C++
sources is not checked: build again after changing them.
"""

import filecmp
import os
import re
import sys
import sysconfig
from pathlib import Path

#: The root of the source tree: this file is benchmarks/column_generation/<package>/.
TREE = Path(__file__).resolve().parents[3]


class SourceTreeError(Exception):
    """The tree has no usable build, or rcspp would not come from it."""


# MARK: Builds


def core_file_name() -> str:
    """The file name of the compiled ``_core`` for this Python, such as
    ``_core.cp313-win_amd64.pyd``."""
    suffix = sysconfig.get_config_var("EXT_SUFFIX") or ".so"
    return f"_core{suffix}"


def find_builds(tree: Path = TREE) -> list[Path]:
    """Every build folder of `tree` (``build*``) whose ``python/rcspp`` has a ``_core``
    for this Python."""
    name = core_file_name()
    return sorted(
        folder for folder in tree.glob("build*") if (folder / "python" / "rcspp" / name).is_file()
    )


def choose_build(build: str | Path | None = None, tree: Path = TREE) -> Path:
    """The build folder to import rcspp from: `build` when given (relative to `tree`'s root,
    or anywhere when absolute), else the tree's only build.

    Raises:
        SourceTreeError: When `build` has no ``_core`` for this Python, or `build` is not given
            and the tree has no build or several, or the build was configured from another
            tree.
    """
    if build is not None:
        folder = Path(build)

        if not folder.is_absolute():
            folder = tree / folder

        if not (folder / "python" / "rcspp" / core_file_name()).is_file():
            raise SourceTreeError(
                f"{folder} has no python/rcspp/{core_file_name()}: build rcspp there with "
                "USE_PYTHON=ON (see benchmarks/column_generation/README.md)"
            )
    else:
        builds = find_builds(tree)

        if not builds:
            raise SourceTreeError(
                f"no build of rcspp for this Python in {tree} "
                f"(build*/python/rcspp/{core_file_name()}): build one as "
                "benchmarks/column_generation/README.md says"
            )

        if len(builds) > 1:
            names = ", ".join(folder.name for folder in builds)
            raise SourceTreeError(f"several builds in {tree} ({names}): choose one with --build")

        folder = builds[0]

    source = configured_from(folder)

    if source is not None and not same_folder(source, tree):
        raise SourceTreeError(
            f"the build {folder} was configured from {source}, not from this tree {tree}: its "
            "_core holds that tree's C++"
        )

    return folder.resolve()


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


def configured_from(build: Path) -> Path | None:
    """The source folder CMake configured `build` from, as its ``CMakeCache.txt`` says;
    None without one."""
    source = read_cmake_cache(build).get("CMAKE_HOME_DIRECTORY")
    return None if source is None else Path(source)


# MARK: Checks


def stale_python_files(build: Path, tree: Path = TREE) -> list[str]:
    """The files of ``python/src/rcspp`` whose copy in `build` is missing or differs,
    and the modules of the copy the sources no longer have, as paths relative to the
    package."""
    source = tree / "python" / "src" / "rcspp"
    copy = build / "python" / "rcspp"
    stale: list[str] = []

    for path in sorted(source.rglob("*.py")):
        relative = path.relative_to(source)
        copied = copy / relative

        if not copied.is_file() or not filecmp.cmp(path, copied, shallow=False):
            stale.append(relative.as_posix())

    # A module removed from the sources can still be imported from the copy.
    for copied in sorted(copy.rglob("*.py")):
        relative = copied.relative_to(copy)

        if not (source / relative).is_file():
            stale.append(f"{relative.as_posix()} (not in the sources)")

    return stale


def is_inside(path: str | Path, folder: Path) -> bool:
    """Whether `path` is in `folder` (letter case ignored where the file system ignores
    it); never across drives."""
    inner = os.path.normcase(os.path.abspath(path))
    outer = os.path.normcase(os.path.abspath(folder))

    try:
        return os.path.commonpath([inner, outer]) == outer
    except ValueError:
        # On Windows, two paths on different drives have no common path.
        return False


def same_folder(first: Path, second: Path) -> bool:
    """Whether two paths name one folder (letter case ignored where the file system
    ignores it)."""
    return os.path.normcase(os.path.abspath(first)) == os.path.normcase(os.path.abspath(second))


def check_rcspp(build: Path) -> None:
    """Imports rcspp and checks that it and its ``_core`` come from `build`.

    Raises:
        SourceTreeError: When either comes from elsewhere.
    """
    import rcspp
    import rcspp._core

    for module in (rcspp, rcspp._core):
        path = module.__file__

        # A namespace package has none: it would pass as the current folder.
        if path is None:
            raise SourceTreeError(f"{module.__name__} has no file: it is not the build's {build}")

        if not is_inside(path, build):
            raise SourceTreeError(
                f"{module.__name__} is imported from {path}, not from the build {build}: another "
                "rcspp was imported first (an installed one, or a PYTHONPATH entry)"
            )


# MARK: Use


def use_tree(build: str | Path | None = None, tree: Path = TREE) -> Path:
    """Makes rcspp import from a build of this tree and checks it; returns the build
    folder.

    Call it before anything imports rcspp. The build's ``python`` folder goes first on
    ``sys.path``, ahead of an installed rcspp.

    Raises:
        SourceTreeError: When there is no usable build, its copy of the Python sources is stale, or
            rcspp still comes from elsewhere.
    """
    folder = choose_build(build, tree)
    stale = stale_python_files(folder, tree)

    if stale:
        shown = ", ".join(stale[:5]) + (", ..." if len(stale) > 5 else "")
        raise SourceTreeError(
            f"the build {folder} has an older copy of python/src/rcspp ({shown}): build it again, "
            "which copies the sources"
        )

    put_first_on_path(str(folder / "python"))
    check_rcspp(folder)
    return folder


def use_example(tree: Path = TREE) -> None:
    """Makes the column generation example's package, ``cpp_column_generation``,
    importable from this tree; call it after :func:`use_tree`.

    The package sits in ``examples/column_generation/python`` and wraps the example's
    bindings, which the build has only when built with the example on.

    Raises:
        SourceTreeError: When the build has no column generation example.
    """
    import rcspp._core

    if not hasattr(rcspp._core, "cpp_column_generation"):
        raise SourceTreeError(
            "the build has no column generation example: build it with "
            "USE_COLUMN_GENERATION_EXAMPLE=ON (see benchmarks/column_generation/README.md)"
        )

    put_first_on_path(str(tree / "examples" / "column_generation" / "python"))


#: The cpp-native layer's program, built with the column generation example.
CPP_NATIVE_PROGRAM = "cpp-native-solver"


def cpp_native_program(build: Path) -> Path:
    """The cpp-native layer's program in `build`: in ``bin/<configuration>`` for a
    multi-configuration generator (Visual Studio), the configuration ``_core`` was built
    in, else in ``bin``.

    Raises:
        SourceTreeError: When the build has not the program.
    """
    import rcspp._core

    name = CPP_NATIVE_PROGRAM + (".exe" if sys.platform == "win32" else "")
    configuration: str = getattr(rcspp._core, "build_config", "")
    candidates = [build / "bin" / configuration / name] if configuration else []
    candidates.append(build / "bin" / name)

    for candidate in candidates:
        if candidate.is_file():
            return candidate

    raise SourceTreeError(
        f"the build {build} has no {CPP_NATIVE_PROGRAM}: build it with "
        "USE_COLUMN_GENERATION_EXAMPLE=ON and one of its master solvers, then "
        f"cmake --build {build} --target {CPP_NATIVE_PROGRAM} (see "
        "benchmarks/column_generation/README.md)"
    )


def put_first_on_path(folder: str) -> None:
    """Puts `folder` first on ``sys.path``, moving it there when it is already on it."""
    if folder in sys.path:
        sys.path.remove(folder)

    sys.path.insert(0, folder)
