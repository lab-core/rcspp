#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The options of a run and of a benchmark (draft with dataclasses).

- :mod:`.blocks`: one dataclass per block of a complete run, built from its table;
- :mod:`.run`: :class:`RunOptions`, what one place gives (a run file, ``[defaults]``, an
  entry), and :class:`Run`, a complete run;
- :mod:`.benchmark`: :class:`Benchmark`, a benchmark file, and its expansion into runs;
- :mod:`.command_line`: the same options through argparse.
"""

from .benchmark import Benchmark, Expansion, expand_run, folder_name, read_presets, read_toml
from .blocks import BLOCKS, OptionsError, make_algorithm_params
from .command_line import add_benchmark_options, add_run_options, run_from_arguments
from .run import LAYERS, RUN_OPTIONS, MasterNotOnLayerError, Run, RunOptions

__all__ = [
    "BLOCKS",
    "LAYERS",
    "RUN_OPTIONS",
    "Benchmark",
    "Expansion",
    "MasterNotOnLayerError",
    "OptionsError",
    "Run",
    "RunOptions",
    "add_benchmark_options",
    "add_run_options",
    "expand_run",
    "folder_name",
    "make_algorithm_params",
    "read_presets",
    "read_toml",
    "run_from_arguments",
]
