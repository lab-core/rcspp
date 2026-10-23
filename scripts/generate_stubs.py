"""Regenerate the type stubs of the compiled extension, ``rcspp._core``.

Run from the repository root, with the extension built and importable::

    pip install pybind11-stubgen black isort
    PYTHONPATH=python/src python scripts/generate_stubs.py

The stubs are written to ``python/src/rcspp/_core/``. CI checks them against the
extension with ``python -m mypy.stubtest rcspp._core``, so regenerate them after any
change to the bindings. The script fails when a signature names a C++ type that is not
bound, which means a type is bound after a function that uses it.
"""

import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

# Where each enum lives, so that default values print as Python expressions.
ENUM_LOCATIONS = {
    "LogLevel": "rcspp._core.logger",
    "SearchDirection": "rcspp._core.graph",
    "Algorithm": "rcspp._core.graph",
}


def main() -> int:
    """Run pybind11-stubgen on rcspp._core, writing into python/src."""
    command = [
        sys.executable,
        "-m",
        "pybind11_stubgen",
        "rcspp._core",
        "--output-dir",
        str(ROOT / "python" / "src"),
        "--exit-code",
    ]
    for enum, location in ENUM_LOCATIONS.items():
        command += ["--enum-class-locations", f"{enum}:{location}"]
    status = subprocess.call(command)  # nosec B603 - fixed arguments
    if status != 0:
        return status
    # Format them as pre-commit would, so that regenerating them changes only what changed.
    stubs = [
        str(path) for path in sorted((ROOT / "python" / "src" / "rcspp" / "_core").glob("*.pyi"))
    ]
    for formatter in ("isort", "black"):
        status = subprocess.call([sys.executable, "-m", formatter, "-q", *stubs])  # nosec B603
        if status != 0:
            return status
    return 0


if __name__ == "__main__":
    sys.exit(main())
