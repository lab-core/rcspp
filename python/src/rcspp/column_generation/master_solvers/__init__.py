#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Master solvers over a model the user built with their LP solver's Python package."""

from typing import TYPE_CHECKING, Any

# Built with USE_HIGHS only, as the wheel is.
try:
    from .highspy_master_solver import HighspyMasterSolver
except ImportError:  # pragma: no cover
    pass

if TYPE_CHECKING:
    from .gurobipy_master_solver import GurobipyMasterSolver


def __getattr__(name: str) -> Any:
    """Imports GurobipyMasterSolver, and gurobipy with it, when it is first used:
    importing rcspp does not load gurobipy."""
    if name != "GurobipyMasterSolver":
        raise AttributeError(f"module {__name__!r} has no attribute {name!r}")

    try:
        from .gurobipy_master_solver import GurobipyMasterSolver
    except ImportError as error:
        raise ImportError(
            "GurobipyMasterSolver needs gurobipy: pip install rcspp[gurobipy]"
        ) from error

    globals()[name] = GurobipyMasterSolver
    return GurobipyMasterSolver


__all__ = ["GurobipyMasterSolver", "HighspyMasterSolver"]
