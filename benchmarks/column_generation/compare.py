#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The comparison of two runs, A the run checked against B the reference:

    python compare.py results/my_benchmark:<run A> results/my_benchmark:<run B> --time-within 5%

Each side is a round's folder, a run folder, or ``<benchmark folder>:<run name>``, the run
over its rounds. The comparison is printed in markdown; ``--output`` also writes it to a
file. The exit code is 0 without the time rule or when A is equivalent, 1 when A is slower
than the rule allows, 2 on a wrong argument, 3 when the rule cannot judge.
"""

import argparse
import sys
from collections.abc import Sequence
from pathlib import Path

# The code the runners and the benchmark share is in this folder.
sys.path.insert(0, str(Path(__file__).resolve().parent))

from column_generation_benchmark import compare  # noqa: E402
from column_generation_benchmark.run_folder import write_atomically  # noqa: E402
from column_generation_benchmark.summary import SummaryError  # noqa: E402

EXIT_CODES = {compare.EQUIVALENT: 0, compare.SLOWER: 1, compare.NO_VERDICT: 3}
OUTPUTS = (".md", ".csv")


def percent(value: str) -> float:
    """A positive percent, with or without its ``%``."""
    try:
        found = float(value.strip().removesuffix("%"))
    except ValueError:
        raise argparse.ArgumentTypeError(f"not a percent: {value}") from None

    if found < 0:
        raise argparse.ArgumentTypeError(f"a negative percent: {value}")

    return found


def seconds(value: str) -> float:
    try:
        found = float(value)
    except ValueError:
        raise argparse.ArgumentTypeError(f"not a number of seconds: {value}") from None

    if found < 0:
        raise argparse.ArgumentTypeError(f"negative seconds: {value}")

    return found


def main(arguments: Sequence[str] | None = None) -> int:
    """Prints the comparison the command line asks for; returns the exit code."""
    default = compare.TimeRule()
    parser = argparse.ArgumentParser(description="Compare two runs: A, checked, against B.")
    parser.add_argument(
        "a",
        help="the run checked: a round's folder, a run folder, or <benchmark folder>:<run name>",
    )
    parser.add_argument("b", help="the reference, named as A is")
    parser.add_argument(
        "--time-within",
        type=percent,
        metavar="PERCENT",
        help="judge A's median solve time: at most this percent of B's above it, or the floor "
        f"if more (default {default.margin_percent:g} %% when --floor is given)",
    )
    parser.add_argument(
        "--floor",
        type=seconds,
        metavar="SECONDS",
        help="the time rule's floor, in seconds "
        f"(default {default.floor_seconds:g} when --time-within is given)",
    )
    parser.add_argument(
        "--output",
        action="append",
        default=[],
        metavar="FILE",
        help="also write the comparison to FILE: .md the text, .csv one row per field; "
        "may be given more than once",
    )
    parsed = parser.parse_args(sys.argv[1:] if arguments is None else arguments)
    outputs = [Path(path) for path in parsed.output]

    for path in outputs:
        if path.suffix not in OUTPUTS:
            print(f"compare.py: {path} is neither .md nor .csv", file=sys.stderr)
            return 2

    rule = None

    if parsed.time_within is not None or parsed.floor is not None:
        rule = compare.TimeRule(
            default.margin_percent if parsed.time_within is None else parsed.time_within,
            default.floor_seconds if parsed.floor is None else parsed.floor,
        )

    try:
        comparison = compare.compare(parsed.a, parsed.b, rule)
    except (compare.CompareError, SummaryError) as error:
        print(f"compare.py: {error}", file=sys.stderr)
        return 2

    markdown_text = compare.comparison_text(comparison)
    csv_text = compare.comparison_csv(comparison)

    print(markdown_text, end="")

    for path in outputs:
        if path.suffix == ".md":
            written = markdown_text
        else:
            written = csv_text

        write_atomically(path, written)

    return 0 if comparison.verdict is None else EXIT_CODES[comparison.verdict[0]]


if __name__ == "__main__":
    sys.exit(main())
