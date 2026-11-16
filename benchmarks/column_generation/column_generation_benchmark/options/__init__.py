#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The options of a run and of a benchmark, from TOML files and the command line.

One module per step, read in this order:

- :mod:`.schema`: what options may hold (options, blocks, parameters) and their checks;
- :mod:`.sources`: reading files and the command line into plain dicts;
- :mod:`.expansion`: stacking the sources of each run, then checking the result;
- :mod:`.naming`: the name of a run and of its folders.

A runner calls :func:`expand_run`; the benchmark calls :func:`expand_benchmark`.
"""

from .expansion import Expansion, Run, expand_benchmark, expand_run, merge
from .naming import folder_name
from .schema import BLOCKS, LAYERS, RUN_OPTIONS, OptionsError, describe, make_algorithm_params
from .sources import parse_assignments, read_toml, split_command_line

__all__ = [
    "BLOCKS",
    "LAYERS",
    "RUN_OPTIONS",
    "Expansion",
    "OptionsError",
    "Run",
    "describe",
    "expand_benchmark",
    "expand_run",
    "folder_name",
    "make_algorithm_params",
    "merge",
    "parse_assignments",
    "read_toml",
    "split_command_line",
]
