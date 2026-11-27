#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The analysis of the benchmark's output, a command per question:

    python analysis.py compare results/my_benchmark:<run A> results/my_benchmark:<run B>
    python analysis.py convergence results/my_benchmark --gap
    python analysis.py sweep results/my_benchmark --option graph.ng_size

``python analysis.py <command> --help`` lists a command's options. A command reads run folders
and instances, never runs a solve; what it writes goes into an analysis folder, with
``analysis.json``, what it read, and ``command.txt``, the command line that makes it again.
"""

import argparse
import sys
from collections.abc import Callable, Sequence
from pathlib import Path

# The analysis and the benchmark's shared code are in this folder.
sys.path.insert(0, str(Path(__file__).resolve().parent))

from column_generation_analysis.commands import compare, convergence, sweep  # noqa: E402

#: Each command: its name, what it does, its arguments, and what runs it.
COMMANDS: list[
    tuple[
        str,
        str,
        Callable[[argparse.ArgumentParser], None],
        Callable[[argparse.Namespace, Sequence[str]], int],
    ]
] = [
    (compare.NAME, compare.HELP, compare.add_arguments, compare.run),
    (convergence.NAME, convergence.HELP, convergence.add_arguments, convergence.run),
    (sweep.NAME, sweep.HELP, sweep.add_arguments, sweep.run),
]


def main(arguments: Sequence[str] | None = None) -> int:
    """Runs the command the command line names; returns its exit code."""
    given = sys.argv[1:] if arguments is None else list(arguments)
    script = sys.argv[0] if arguments is None else Path(__file__).name
    parser = argparse.ArgumentParser(
        description="The analysis of the benchmark's run folders: tables, plots, drawings."
    )
    commands = parser.add_subparsers(dest="command", required=True, metavar="command")

    for name, help_text, add_arguments, run in COMMANDS:
        command = commands.add_parser(name, help=help_text, description=help_text)
        add_arguments(command)
        command.set_defaults(run=run)

    parsed = parser.parse_args(given)
    run_command: Callable[[argparse.Namespace, Sequence[str]], int] = parsed.run
    return run_command(parsed, [script, *given])


if __name__ == "__main__":
    sys.exit(main())
