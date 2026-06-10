#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

"""Minimal example: solve a VRPTW instance with the ``vrp`` package.

This mirrors ``python -m vrp`` but lives under ``examples/`` to demonstrate how
to drive the column-generation solver (and its RCSPP subproblem) from user code.

Run from the repository root, e.g.::

    python examples/python/solve_vrp.py instances/C101_5.txt --subproblem-only

``--subproblem-only`` runs just the RCSPP pricing subproblem with zero duals and
does not require Gurobi; the full solve uses the Gurobi master problem.
"""

import os
import sys

# The vrp example package lives here under examples/python/; the rcspp package it
# depends on lives under python/src/. Add both to the path so this runs from a
# source tree.
_EXAMPLES_PYTHON = os.path.dirname(os.path.abspath(__file__))
_REPO_ROOT = os.path.dirname(os.path.dirname(_EXAMPLES_PYTHON))
_RCSPP_SRC = os.path.join(_REPO_ROOT, "python", "src")
for _p in (_EXAMPLES_PYTHON, _RCSPP_SRC):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from vrp.vrp import main  # noqa: E402

if __name__ == "__main__":
    main()
