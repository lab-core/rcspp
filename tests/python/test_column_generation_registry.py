#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Pure-Python column generation components by name."""

import os
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from column_generation_fakes import ScriptedMasterSolver, ScriptedPricer  # noqa: E402

from rcspp.column_generation import (  # noqa: E402
    AbsoluteTolerances,
    ArtificialColumns,
    ColumnGeneration,
    ColumnGenerationParams,
    ColumnGenerationStatus,
    IterationInfo,
    MasterInitialization,
    MasterProblem,
    NoInitialization,
    RelativeTolerances,
    RelaxationParams,
    RowSense,
    SolverStatus,
    StatisticsParams,
    ToleranceContext,
    algorithm_names,
    initialization_names,
    make_initialization,
    make_tolerances,
    register_initialization,
    register_tolerances,
    registry,
    tolerance_names,
)
from rcspp.graph import ALGORITHMS  # noqa: E402


def test_the_names_and_what_they_build() -> None:
    assert tolerance_names()[:2] == ["absolute", "relative"]
    assert initialization_names()[:2] == ["artificial_columns", "none"]
    assert algorithm_names() == list(ALGORITHMS)
    assert {"simple", "greedy", "bidirectional"} <= set(algorithm_names())

    relative = make_tolerances("relative", reduced_cost=1e-6)
    assert isinstance(relative, RelativeTolerances)
    assert relative.reduced_cost(ToleranceContext(cost_scale=10.0)) == pytest.approx(1e-5)
    assert isinstance(make_tolerances("absolute"), AbsoluteTolerances)
    artificial = make_initialization("artificial_columns", cost=5.0)
    assert isinstance(artificial, ArtificialColumns)
    assert artificial.cost(ToleranceContext()) == 5.0
    assert isinstance(make_initialization("none"), NoInitialization)


def test_the_module_has_the_functions() -> None:
    assert registry.make_tolerances is make_tolerances
    assert registry.tolerance_names is tolerance_names


def test_unknown_and_taken_names_are_refused() -> None:
    with pytest.raises(ValueError, match="Unknown tolerances 'tight'. Choose from: absolute"):
        make_tolerances("tight")

    with pytest.raises(ValueError, match="Unknown initialization 'two_phase'"):
        make_initialization("two_phase")

    with pytest.raises(ValueError, match="already registered"):
        register_tolerances("absolute", AbsoluteTolerances)

    with pytest.raises(ValueError, match="already registered"):
        register_initialization("none", NoInitialization)


def test_components_of_ones_own_by_name() -> None:
    class Tight(AbsoluteTolerances):
        def __init__(self, scale: float = 1.0) -> None:
            super().__init__(1e-12 * scale)

    class NothingToDo(MasterInitialization):
        def initialize(self, master: MasterProblem, context: ToleranceContext) -> None:
            pass

    register_tolerances("test-tight", Tight)
    register_initialization("test-nothing", NothingToDo)
    assert "test-tight" in tolerance_names() and "test-nothing" in initialization_names()
    tight = make_tolerances("test-tight", scale=2.0)
    assert isinstance(tight, Tight)
    assert tight.reduced_cost(ToleranceContext()) == pytest.approx(2e-12)
    assert isinstance(make_initialization("test-nothing"), NothingToDo)


def test_the_params_take_names() -> None:
    params = ColumnGenerationParams(tolerances="relative", initialization="none")
    assert (params.tolerances, params.initialization) == ("relative", "none")

    # The loop builds them, and runs with them: no initialization, so the empty master stays
    # infeasible.
    solver = ScriptedMasterSolver()
    solver.on_solve = lambda lp: SolverStatus.Infeasible
    master = MasterProblem(solver)
    master.add_rows(1, RowSense.GreaterEqual, 1.0)
    master.add_pricer(ScriptedPricer())
    loop = ColumnGeneration(master, params)
    assert isinstance(loop.params.tolerances, RelativeTolerances)
    assert isinstance(loop.params.initialization, NoInitialization)
    result = loop.run()
    assert result.status == ColumnGenerationStatus.RestrictedMasterInfeasible
    assert solver.columns == []


def test_an_unknown_name_in_the_params_is_refused() -> None:
    master = MasterProblem(ScriptedMasterSolver())

    with pytest.raises(ValueError, match="Unknown tolerances 'tight'"):
        ColumnGeneration(master, ColumnGenerationParams(tolerances="tight"))

    with pytest.raises(ValueError, match="Unknown initialization 'two_phase'"):
        ColumnGeneration(master, ColumnGenerationParams(initialization="two_phase"))


def test_the_params_keep_every_other_option() -> None:
    seen: list[IterationInfo] = []
    params = ColumnGenerationParams(
        max_iterations=3,
        timeout_s=9.0,
        relaxation=RelaxationParams(threads=2),
        tolerances=AbsoluteTolerances(1e-7),
        initialization=ArtificialColumns(4.0),
        statistics=StatisticsParams(timers=False),
        on_iteration=seen.append,
    )
    assert (params.max_iterations, params.timeout_s) == (3, 9.0)
    assert params.relaxation.threads == 2 and not params.statistics.timers
    assert isinstance(params.tolerances, AbsoluteTolerances)
    assert isinstance(params.initialization, ArtificialColumns)
    assert params.on_iteration is not None and params.on_converged is None
