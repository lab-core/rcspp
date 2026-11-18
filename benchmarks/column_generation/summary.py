#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The summary of a benchmark folder: ``summary.md``, ``summary.csv`` and ``runs.csv``.

    python summary.py results/my_benchmark

``benchmark.py`` writes it at every end; this writes it again on demand, from the folder's
files alone: a folder of run folders written by the runners alone works too.
"""

import argparse
import sys
from collections.abc import Sequence
from pathlib import Path

# The code the runners and the benchmark share is in this folder.
sys.path.insert(0, str(Path(__file__).resolve().parent))

from column_generation_benchmark.summary import SummaryError, write_summary  # noqa: E402


def main(arguments: Sequence[str] | None = None) -> int:
    """Writes the summary of the folder the command line names; returns the exit
    code."""
    parser = argparse.ArgumentParser(
        description="Write the summary of a benchmark folder, or of a folder of run folders."
    )
    parser.add_argument("folder", help="the benchmark folder")
    parsed = parser.parse_args(sys.argv[1:] if arguments is None else arguments)
    folder = Path(parsed.folder)

    if not folder.is_dir():
        print(f"summary.py: {folder} is not a folder", file=sys.stderr)
        return 2

    try:
        written = write_summary(folder)
    except SummaryError as error:
        print(f"summary.py: {error}", file=sys.stderr)
        return 2

    for path in written:
        print(path)

    return 0


if __name__ == "__main__":
    sys.exit(main())
