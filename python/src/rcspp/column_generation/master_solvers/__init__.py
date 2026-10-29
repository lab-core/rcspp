#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Master solvers over a model the user built with their LP solver's Python package."""

# Built with USE_HIGHS only, as the wheel is.
try:
    from .highspy_master_solver import HighspyMasterSolver
except ImportError:  # pragma: no cover
    pass

__all__ = ["HighspyMasterSolver"]
