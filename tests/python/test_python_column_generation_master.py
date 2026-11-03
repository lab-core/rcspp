#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The master problem, the tolerances, the initializations and the master solver self-
test (the cases of the C++ tests and of test_column_generation.py, and more)."""

import math
import os
import sys
import warnings
from dataclasses import replace
from typing import Any

import numpy as np
import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from python_column_generation_fakes import (  # noqa: E402
    Mistake,
    ScriptedMasterSolver,
    ScriptedPricer,
    SelfTestMasterSolver,
    make_path,
)

import rcspp.python_column_generation as layer  # noqa: E402
from rcspp._core.graph import Solution  # noqa: E402


def master_with_rows(rows: int, lower: float = 1.0) -> tuple[Any, Any]:
    """A master over a scripted master solver, with `rows` rows `>= lower`."""
    solver = ScriptedMasterSolver()
    master = layer.MasterProblem(solver)
    master.add_rows(rows, layer.RowSense.GreaterEqual, lower)
    return master, solver


# MARK: The rows


def test_add_rows_by_sense() -> None:
    solver = ScriptedMasterSolver()
    master = layer.MasterProblem(solver)
    equal = master.add_rows(2, layer.RowSense.Equal, 3.0)
    greater = master.add_rows(1, layer.RowSense.GreaterEqual, 1.0)
    less = master.add_rows(count=1, sense=layer.RowSense.LessEqual, right_hand_side=5.0)

    assert (equal.first, equal.count) == (0, 2)
    assert (greater.first, greater.count) == (2, 1)
    assert (less.first, less.count) == (3, 1)
    assert master.number_of_rows == 4
    assert solver.lower == [3.0, 3.0, 1.0, -math.inf]
    assert solver.upper == [3.0, 3.0, math.inf, 5.0]


def test_add_rows_between_bounds_with_names() -> None:
    solver = ScriptedMasterSolver()
    master = layer.MasterProblem(solver)
    rows = master.add_rows_between([1.0, 0.0], [1.0, 5.0], ["visit", "load"])
    assert (rows.first, rows.count) == (0, 2)
    assert solver.names == ["visit", "load"]

    # No names, then by keyword; a numpy array as bounds.
    more = master.add_rows_between(lower=np.array([2.0]), upper=np.array([math.inf]))
    assert (more.first, more.count) == (2, 1)
    assert solver.lower == [1.0, 0.0, 2.0] and solver.upper == [1.0, 5.0, math.inf]
    assert solver.names == ["visit", "load", ""]


def test_add_rows_refuses_mismatched_sizes() -> None:
    solver = ScriptedMasterSolver()
    master = layer.MasterProblem(solver)
    message = "^MasterProblem.add_rows_between: lower, upper and names must have the same size$"

    with pytest.raises(ValueError, match=message):
        master.add_rows_between([1.0, 1.0], [1.0])

    with pytest.raises(ValueError, match=message):
        master.add_rows_between([1.0, 1.0], [1.0, 1.0], ["a"])

    assert master.number_of_rows == 0


def test_add_rows_needs_the_capability() -> None:
    solver = ScriptedMasterSolver()
    solver.capabilities_value = replace(solver.capabilities_value, add_rows=False)
    master = layer.MasterProblem(solver)

    with pytest.raises(NotImplementedError, match="cannot add rows"):
        master.add_rows(1, layer.RowSense.Equal, 1.0)

    with pytest.raises(NotImplementedError, match="cannot add rows"):
        master.add_rows_between([1.0], [1.0])


# MARK: The pricers


def test_convexity_rows() -> None:
    master, solver = master_with_rows(2)
    first, second, third = ScriptedPricer(), ScriptedPricer(), ScriptedPricer()

    created = master.add_pricer(first, layer.ConvexityRow(max_paths=2.0))
    existing = master.add_pricer(second, layer.ConvexityRow(existing_row=1))
    none = master.add_pricer(third)

    assert (created, existing, none) == (0, 1, 2)
    assert solver.lower[2] == -math.inf and solver.upper[2] == 2.0
    assert [(entry.pricer, entry.convexity_row) for entry in master.pricers] == [
        (first, 2),
        (second, 1),
        (third, None),
    ]

    # A copy: the master's pricers change only through add_pricer.
    assert isinstance(master.pricers, tuple)

    with pytest.raises(AttributeError):
        master.pricers[0].convexity_row = 0  # type: ignore[misc]

    assert master.solver is solver


def test_bad_convexity_rows_are_refused() -> None:
    master, _ = master_with_rows(1)
    pricer = ScriptedPricer()

    with pytest.raises(ValueError, match="not both"):
        layer.ConvexityRow(existing_row=0, max_paths=2.0)

    with pytest.raises(TypeError, match="must be an integer"):
        layer.ConvexityRow(existing_row=0.0)  # pyright: ignore[reportArgumentType]

    # A numpy integer is taken as the integer it is.
    numpy_row = layer.ConvexityRow(existing_row=np.int64(0))  # pyright: ignore[reportArgumentType]
    assert type(numpy_row.existing_row) is int

    with pytest.raises(IndexError):
        master.add_pricer(pricer, layer.ConvexityRow(existing_row=1))

    assert master.pricers == ()
    assert master.number_of_rows == 1


def test_a_convexity_row_the_solver_cannot_add_is_refused() -> None:
    solver = ScriptedMasterSolver()
    solver.capabilities_value = replace(solver.capabilities_value, add_rows=False)
    master = layer.MasterProblem(solver)

    with pytest.raises(RuntimeError, match="cannot add rows"):
        master.add_pricer(ScriptedPricer(), layer.ConvexityRow(max_paths=1.0))

    assert master.pricers == ()


def test_an_unknown_pricer_is_an_index_error() -> None:
    master, _ = master_with_rows(1)
    master.add_pricer(ScriptedPricer())

    for index in (1, 7):
        with pytest.raises(IndexError):
            master.add_columns(index, [make_path(1.0, [(0, 1.0)], 0.0)])


# MARK: The columns


def test_add_columns_sends_one_batch() -> None:
    master, solver = master_with_rows(3)
    pricer = ScriptedPricer()
    k = master.add_pricer(pricer, layer.ConvexityRow(max_paths=2.0))  # row 3

    master.add_columns(
        k,
        [
            make_path(7.0, [(0, 1.0)], 0.0),
            make_path(9.0, [(1, 1.0), (2, 2.0)], -4.0),
        ],
    )

    assert solver.add_columns_calls == 1
    first, second = solver.columns
    assert (first.id, first.cost, first.lower, first.upper) == (0, 7.0, 0.0, math.inf)
    assert first.entries == [(0, 1.0), (3, 1.0)]
    assert second.id == 1 and second.entries == [(1, 1.0), (2, 2.0), (3, 1.0)]

    assert master.number_of_columns == 2
    column = master.columns[1]
    assert (column.id, column.pricer, column.tag) == (1, k, layer.ColumnTag.Real)
    assert column.solution.cost == -4.0
    assert column.solution.column.cost == 9.0
    assert master.artificial_column_ids.tolist() == []

    # Later columns go on from the ids.
    master.add_columns(k, [make_path(1.0, [(2, 1.0)], 0.0)])
    assert solver.columns[2].id == 2 and master.columns[2].id == 2


def test_add_columns_without_a_convexity_row() -> None:
    master, solver = master_with_rows(1)
    master.add_pricer(ScriptedPricer())
    master.add_columns(0, [make_path(5.0, [(0, 1.0)], 0.0)])
    assert [column.entries for column in solver.columns] == [[(0, 1.0)]]


def test_a_column_without_entries() -> None:
    master, solver = master_with_rows(1)
    master.add_pricer(ScriptedPricer(), layer.ConvexityRow(max_paths=1.0))
    master.add_columns(0, [make_path(2.0, [], 0.0), make_path(3.0, [(0, 1.0)], 0.0)])
    assert [column.entries for column in solver.columns] == [[(1, 1.0)], [(0, 1.0), (1, 1.0)]]


@pytest.mark.filterwarnings("ignore:MasterProblem.add_columns:RuntimeWarning")
def test_a_row_listed_twice_gets_the_sum() -> None:
    master, solver = master_with_rows(2)
    master.add_pricer(ScriptedPricer())

    # A hand-built column, out of order and with row 1 twice, next to one in order: every master
    # solver gets it merged.
    master.add_columns(
        0,
        [
            make_path(1.0, [(1, 0.5), (0, 2.0), (1, 0.5)], 0.0),
            make_path(2.0, [(0, 1.0), (1, 1.0)], 0.0),
        ],
    )
    assert solver.columns[0].entries == [(0, 2.0), (1, 1.0)]
    assert solver.columns[1].entries == [(0, 1.0), (1, 1.0)]

    # Out of order only: sorted, nothing added together.
    master.add_columns(0, [make_path(1.0, [(1, 3.0), (0, 4.0)], 0.0)])
    assert solver.columns[2].entries == [(0, 4.0), (1, 3.0)]


@pytest.mark.filterwarnings("ignore:MasterProblem.add_columns:RuntimeWarning")
@pytest.mark.parametrize("convexity", [False, True])
def test_random_columns_reach_the_solver_sorted_and_summed(convexity: bool) -> None:
    generator = np.random.default_rng(7)
    master, solver = master_with_rows(6)
    master.add_pricer(ScriptedPricer(), layer.ConvexityRow(max_paths=4.0 if convexity else None))

    # Columns of any length (none included), in any order, with rows repeated.
    paths: list[Solution] = []
    expected: list[list[tuple[int, float]]] = []

    for _ in range(40):
        rows = generator.integers(0, 6, size=generator.integers(0, 9)).tolist()
        values = generator.integers(1, 5, size=len(rows)).astype(float).tolist()
        paths.append(make_path(float(len(rows)), list(zip(rows, values)), 0.0))
        summed: dict[int, float] = {}

        for row, value in zip(rows, values):
            summed[row] = summed.get(row, 0.0) + value

        expected.append(sorted(summed.items()) + ([(6, 1.0)] if convexity else []))

    master.add_columns(0, paths)

    assert [column.entries for column in solver.columns] == expected
    assert [column.cost for column in solver.columns] == [
        float(len(path.column.rows)) for path in paths
    ]
    assert [column.id for column in solver.columns] == list(range(40))


def test_bad_columns_are_refused_and_add_nothing() -> None:
    master, solver = master_with_rows(1)
    master.add_pricer(ScriptedPricer(), layer.ConvexityRow(max_paths=1.0))  # row 1

    # A row that does not exist, then the convexity row; each after a good column.
    good = make_path(1.0, [(0, 1.0)], 0.0)

    with pytest.raises(IndexError):
        master.add_columns(0, [good, make_path(1.0, [(2, 1.0)], 0.0)])

    with pytest.raises(ValueError):
        master.add_columns(0, [good, make_path(1.0, [(1, 1.0)], 0.0)])

    master.add_columns(0, [])
    assert solver.add_columns_calls == 0
    assert master.number_of_columns == 0


def test_a_refused_batch_records_no_column() -> None:
    master, solver = master_with_rows(1)
    master.add_pricer(ScriptedPricer())

    def refuse(columns: Any) -> None:
        raise ValueError("the solver refuses")

    add_columns = solver.add_columns
    solver.add_columns = refuse

    with pytest.raises(ValueError, match="the solver refuses"):
        master.add_columns(0, [make_path(1.0, [(0, 1.0)], 0.0)])

    assert master.number_of_columns == 0

    # The ids are still free.
    solver.add_columns = add_columns
    master.add_columns(0, [make_path(1.0, [(0, 1.0)], 0.0)])
    assert solver.columns[0].id == 0


def test_add_artificial_columns() -> None:
    master, solver = master_with_rows(2)
    pricer = ScriptedPricer()
    master.add_pricer(pricer)
    master.add_columns(0, [make_path(4.0, [(0, 1.0)], 0.0)])

    with pytest.raises(ValueError):
        master.add_artificial_columns([0, 1], [1.0], 5.0)

    with pytest.raises(IndexError):
        master.add_artificial_columns([2], [1.0], 5.0)

    # Rows are numbers of rows: a fraction or a bool is no row.
    for rows in ([0.7, 1.2], np.array([0.0, 1.0]), [True, False]):
        with pytest.raises(TypeError, match="add_artificial_columns: rows must be integers"):
            master.add_artificial_columns(rows, [1.0, 1.0], 5.0)

    assert master.number_of_columns == 1

    master.add_artificial_columns([0, 1], [1.0, -1.0], 5.0)
    assert master.number_of_columns == 3
    columns = master.columns
    assert [column.tag for column in columns] == [
        layer.ColumnTag.Real,
        layer.ColumnTag.Artificial,
        layer.ColumnTag.Artificial,
    ]
    assert columns[1].pricer is None
    assert columns[2].solution.column.rows == []
    assert [(column.id, column.cost, column.entries) for column in solver.columns[1:]] == [
        (1, 5.0, [(0, 1.0)]),
        (2, 5.0, [(1, -1.0)]),
    ]
    assert master.artificial_column_ids.tolist() == [1, 2]
    assert master.artificial_cost == 5.0


def test_no_artificial_column_adds_nothing() -> None:
    master, solver = master_with_rows(1)
    master.add_artificial_columns([], [], 5.0)

    # No batch is sent at all.
    assert solver.columns == [] and master.number_of_columns == 0
    assert master.artificial_cost is None
    assert master.artificial_column_ids.tolist() == []


def test_the_state_of_a_new_master() -> None:
    master = layer.MasterProblem(ScriptedMasterSolver())
    assert master.number_of_rows == 0 and master.number_of_columns == 0
    assert master.pricers == () and master.columns == []
    assert master.artificial_column_ids.tolist() == [] and master.artificial_cost is None

    assert not master.initialized
    master.mark_initialized()
    assert master.initialized


def test_the_master_ranges_and_columns_are_plain_records() -> None:
    master, _ = master_with_rows(2)
    master.add_pricer(ScriptedPricer())
    master.add_columns(0, [make_path(4.0, [(0, 1.0)], 0.0)])
    column = master.columns[0]
    rows = master.add_rows(1, layer.RowSense.Equal, 0.0)

    for record, name in ((rows, "first"), (rows, "count"), (column, "id"), (column, "pricer")):
        with pytest.raises(AttributeError):
            setattr(record, name, 5)


# MARK: More on the master problem


def test_a_repeated_row_warns_once_per_master() -> None:
    master, _ = master_with_rows(2)
    master.add_pricer(ScriptedPricer())
    twice = make_path(1.0, [(1, 0.5), (1, 0.5)], 0.0)

    with pytest.warns(RuntimeWarning, match="more than once") as caught:
        master.add_columns(0, [twice])

    # The warning points at the caller.
    assert caught[0].filename == __file__

    with warnings.catch_warnings():
        warnings.simplefilter("error")
        master.add_columns(0, [twice])

        # Out of order alone is no mistake.
        master.add_columns(0, [make_path(1.0, [(1, 1.0), (0, 1.0)], 0.0)])

    # Another master warns again.
    other, _ = master_with_rows(2)
    other.add_pricer(ScriptedPricer())

    with pytest.warns(RuntimeWarning, match="more than once"):
        other.add_columns(0, [twice])


def test_the_columns_of_the_master_are_a_copy() -> None:
    master, _ = master_with_rows(1)
    master.add_pricer(ScriptedPricer())
    master.add_columns(0, [make_path(1.0, [(0, 1.0)], 0.0)])
    master.columns.clear()
    assert master.number_of_columns == 1 and len(master.columns) == 1


def test_a_batch_has_the_widths_of_the_column_batch() -> None:
    received: list[Any] = []
    master, solver = master_with_rows(2)
    master.add_pricer(ScriptedPricer(), layer.ConvexityRow(max_paths=1.0))
    solver.add_columns = received.append
    master.add_columns(0, [make_path(2.5, [(0, 1.0), (1, 2.0)], 0.0)])

    (batch,) = received
    assert isinstance(batch, layer.ColumnBatch)
    assert batch.ids.dtype == np.uint64 and batch.starts.dtype == np.uint32
    assert batch.row_indices.dtype == np.uint32 and batch.values.dtype == np.float64
    assert batch.starts.tolist() == [0, 3] and batch.row_indices.tolist() == [0, 1, 2]
    assert (batch.lower.tolist(), batch.upper.tolist()) == ([0.0], [math.inf])


def test_add_rows_takes_any_integer_count() -> None:
    master, solver = master_with_rows(0)
    master.add_rows(np.int64(2), layer.RowSense.Equal, 1.0)
    master.add_rows_between(np.array([0.0]), np.array([2.0]))
    assert solver.lower == [1.0, 1.0, 0.0] and solver.upper == [1.0, 1.0, 2.0]

    # A count that is not a whole number is refused, as is a negative one; a bool counts.
    with pytest.raises(TypeError, match="add_rows: count must be an integer"):
        master.add_rows(2.0, layer.RowSense.Equal, 1.0)

    with pytest.raises(ValueError, match="add_rows: count must not be negative"):
        master.add_rows(-1, layer.RowSense.Equal, 1.0)

    assert master.add_rows(True, layer.RowSense.Equal, 1.0).count == 1


def test_add_columns_takes_any_iterable() -> None:
    master, solver = master_with_rows(1)
    master.add_pricer(ScriptedPricer())
    master.add_columns(0, (make_path(float(cost), [(0, 1.0)], 0.0) for cost in range(2)))
    assert [column.cost for column in solver.columns] == [0.0, 1.0]
    assert master.number_of_columns == 2


def test_tolerances_and_initializations_need_their_methods() -> None:
    assert layer.Tolerances.__abstractmethods__ == {
        "reduced_cost",
        "feasibility",
        "optimality_gap",
    }
    assert layer.MasterInitialization.__abstractmethods__ == {"initialize"}
    abstract: list[Any] = [layer.Tolerances, layer.MasterInitialization]

    for base in abstract:
        with pytest.raises(TypeError, match="abstract"):
            base()


# MARK: Tolerances


def test_tolerances() -> None:
    context = layer.ToleranceContext(cost_scale=10.0)
    absolute = layer.AbsoluteTolerances(1e-6, 1e-5, 1e-4)
    assert absolute.reduced_cost(context) == 1e-6
    assert absolute.feasibility(context) == 1e-5
    assert absolute.optimality_gap(context) == 1e-4
    relative = layer.RelativeTolerances(1e-6, 1e-5, 1e-4)
    assert relative.reduced_cost(context) == pytest.approx(1e-5)
    assert relative.feasibility(context) == 1e-5
    assert relative.optimality_gap(context) == pytest.approx(1e-3)
    assert layer.ToleranceContext().cost_scale == 1.0

    # The defaults.
    default = layer.AbsoluteTolerances()
    assert default.reduced_cost(context) == layer.DEFAULT_REDUCED_COST_TOLERANCE == 1e-9
    assert default.feasibility(context) == layer.DEFAULT_FEASIBILITY_TOLERANCE == 1e-6
    assert default.optimality_gap(context) == layer.DEFAULT_OPTIMALITY_GAP == 1e-6
    assert layer.RelativeTolerances().reduced_cost(context) == pytest.approx(1e-8)
    assert layer.AbsoluteTolerances(feasibility=1e-3).feasibility(context) == 1e-3


def test_tolerances_print_as_the_call_that_builds_them() -> None:
    relative = layer.RelativeTolerances(1e-6, feasibility=1e-5)
    assert repr(relative) == (
        "RelativeTolerances(reduced_cost=1e-06, feasibility=1e-05, optimality_gap=1e-06)"
    )

    # Evaluating the repr builds an equal object (the text is the layer's own).
    for tolerances in (relative, layer.AbsoluteTolerances()):
        assert eval(repr(tolerances), dict(vars(layer))) == tolerances  # nosec B307

    # Equal when of the same class with the same numbers; usable as keys.
    assert layer.AbsoluteTolerances(1e-6) == layer.AbsoluteTolerances(reduced_cost=1e-6)
    assert layer.AbsoluteTolerances(1e-6) != layer.AbsoluteTolerances(1e-7)
    assert layer.AbsoluteTolerances(1e-6) != layer.RelativeTolerances(1e-6)
    assert layer.AbsoluteTolerances() != "absolute"
    assert len({layer.AbsoluteTolerances(), layer.AbsoluteTolerances()}) == 1


def test_tolerances_written_in_python() -> None:
    class HalfTolerances(layer.Tolerances):
        """A reduced-cost tolerance of half the cost scale."""

        def reduced_cost(self, context: Any) -> float:
            return 0.5 * float(context.cost_scale)

        def feasibility(self, context: Any) -> float:
            return 1e-6

        def optimality_gap(self, context: Any) -> float:
            return 1e-6

    tolerances = HalfTolerances()
    assert tolerances.reduced_cost(layer.ToleranceContext(cost_scale=4.0)) == 2.0
    assert tolerances.feasibility(layer.ToleranceContext()) == 1e-6
    assert tolerances.optimality_gap(layer.ToleranceContext()) == 1e-6


# MARK: Initializations


def test_the_cost_of_the_artificial_columns() -> None:
    context = layer.ToleranceContext(cost_scale=2.0)
    assert (
        layer.ArtificialColumns().cost(context)
        == 2.0 * layer.ArtificialColumns.AUTOMATIC_COST_FACTOR
    )
    assert layer.ArtificialColumns(5.0).cost(context) == 5.0
    assert layer.ArtificialColumns(None).cost(context) == layer.ArtificialColumns().cost(context)


@pytest.mark.parametrize("cost", [0.0, -1.0, math.inf, math.nan])
def test_an_artificial_cost_that_is_not_positive_and_finite_is_refused(cost: float) -> None:
    with pytest.raises(ValueError, match="positive and finite"):
        layer.ArtificialColumns(cost)


def test_initializations_print_as_the_call_that_builds_them() -> None:
    initializations = [
        layer.ArtificialColumns(),
        layer.ArtificialColumns(2.5),
        layer.NoInitialization(),
    ]
    assert [repr(initialization) for initialization in initializations] == [
        "ArtificialColumns()",
        "ArtificialColumns(2.5)",
        "NoInitialization()",
    ]

    # Evaluating the repr builds an equal object (the text is the layer's own).
    for initialization in initializations:
        assert eval(repr(initialization), dict(vars(layer))) == initialization  # nosec B307

    assert layer.ArtificialColumns(2.5) != layer.ArtificialColumns()
    assert layer.ArtificialColumns() != layer.NoInitialization()
    assert layer.NoInitialization() != "none"
    assert len({layer.ArtificialColumns(1.0), layer.ArtificialColumns(1.0)}) == 1
    assert len({layer.NoInitialization(), layer.NoInitialization()}) == 1


def test_the_automatic_factor() -> None:
    assert layer.ArtificialColumns.AUTOMATIC_COST_FACTOR == 1e4
    assert layer.ArtificialColumns().cost(layer.ToleranceContext(cost_scale=3.0)) == (3e4)


def test_artificial_columns_cover_the_rows_x_equal_zero_violates() -> None:
    solver = ScriptedMasterSolver()
    master = layer.MasterProblem(solver)

    # >= 1, <= -1, = 2 (violated by x = 0 from below), = -2, >= 0, <= 3, free, = 0.
    lower = [1.0, -math.inf, 2.0, -2.0, 0.0, -math.inf, -math.inf, 0.0]
    upper = [math.inf, -1.0, 2.0, -2.0, math.inf, 3.0, math.inf, 0.0]
    master.add_rows_between(lower, upper)

    layer.ArtificialColumns(3.0).initialize(master, layer.ToleranceContext())

    assert [(column.cost, column.entries) for column in solver.columns] == [
        (3.0, [(0, 1.0)]),
        (3.0, [(1, -1.0)]),
        (3.0, [(2, 1.0)]),
        (3.0, [(3, -1.0)]),
    ]
    assert master.artificial_column_ids.tolist() == [0, 1, 2, 3]
    assert master.artificial_cost == 3.0


def test_the_automatic_artificial_cost_follows_the_scale() -> None:
    master, solver = master_with_rows(1)
    context = layer.ToleranceContext(cost_scale=7.0)
    layer.ArtificialColumns().initialize(master, context)
    assert solver.columns[0].cost == 7.0 * layer.ArtificialColumns.AUTOMATIC_COST_FACTOR


def test_nothing_to_cover_adds_nothing() -> None:
    solver = ScriptedMasterSolver()
    master = layer.MasterProblem(solver)
    master.add_rows(2, layer.RowSense.LessEqual, 1.0)
    layer.ArtificialColumns().initialize(master, layer.ToleranceContext())
    assert solver.add_columns_calls == 0 and master.number_of_columns == 0


def test_no_initialization_leaves_the_master_as_it_is() -> None:
    master, solver = master_with_rows(2)
    layer.NoInitialization().initialize(master, layer.ToleranceContext())
    assert solver.columns == [] and master.number_of_columns == 0


def test_an_initialization_written_in_python() -> None:
    class StartingPath(layer.MasterInitialization):
        """Adds a path as a starting column."""

        def __init__(self) -> None:
            super().__init__()  # pyright: ignore[reportUnknownMemberType]
            self.calls: list[tuple[Any, float]] = []

        def initialize(self, master: Any, context: Any) -> None:
            self.calls.append((master, context.cost_scale))
            master.add_columns(0, [make_path(4.0, [(0, 1.0)], 0.0)])

    master, solver = master_with_rows(1)
    master.add_pricer(ScriptedPricer())
    initialization = StartingPath()
    initialization.initialize(master, layer.ToleranceContext(cost_scale=2.0))

    assert initialization.calls[0][0] is master and initialization.calls[0][1] == 2.0
    assert [column.cost for column in solver.columns] == [4.0]


# MARK: The master solver self-test


def test_a_correct_master_solver_passes_the_self_test() -> None:
    report = layer.master_solver_self_test(
        SelfTestMasterSolver(), SelfTestMasterSolver(maximizing=True)
    )
    assert report.passed
    assert report.failures == ()
    assert layer.DEFAULT_SELF_TEST_TOLERANCE == 1e-6


def test_the_self_test_reports_the_mistakes_of_a_master_solver() -> None:
    maximizing = SelfTestMasterSolver(maximizing=True)
    report = layer.master_solver_self_test(
        SelfTestMasterSolver(Mistake.FLIPS_LESS_EQUAL_DUAL), maximizing
    )
    assert not report.passed
    assert list(report.failures) == [
        "minimizing model: first LP: dual of row 2 (<=): expected -0.5, got 0.5"
    ]

    report = layer.master_solver_self_test(
        SelfTestMasterSolver(),
        SelfTestMasterSolver(Mistake.KEEPS_MAXIMIZATION, maximizing=True),
        tolerance=1e-9,
    )
    expected = "maximizing model: first LP: objective: expected 3.75, got -3.75"
    assert any(expected in failure for failure in report.failures)
    assert any("maximizing model: first LP: dual of row 1 (>=)" in f for f in report.failures)
    assert not any("minimizing model" in failure for failure in report.failures)

    report = layer.master_solver_self_test(
        SelfTestMasterSolver(Mistake.VALUES_BY_POSITION),
        SelfTestMasterSolver(maximizing=True),
    )
    expected = "first LP: value of x3 (id 5): expected 0.25, got 1.25"
    assert any(expected in failure for failure in report.failures)
    assert any("LP after adding x4: value of x4 (id 9)" in f for f in report.failures)


def test_the_self_test_reports_an_ignored_column() -> None:
    report = layer.master_solver_self_test(
        SelfTestMasterSolver(Mistake.IGNORES_ADDED_COLUMN), SelfTestMasterSolver(maximizing=True)
    )

    # The column x4 was not added: the master solver answers the first LP again.
    expected = "minimizing model: LP after adding x4: objective: expected 1.0, got 3.75"
    assert expected in report.failures
    assert not any("first LP" in failure for failure in report.failures)


def test_the_self_test_reports_an_integer_solve_that_solves_the_relaxation() -> None:
    report = layer.master_solver_self_test(
        SelfTestMasterSolver(Mistake.SOLVES_THE_RELAXATION), SelfTestMasterSolver(maximizing=True)
    )
    assert "minimizing model: integer solve: objective: expected 1.0, got 0.55" in report.failures
    expected = "minimizing model: integer solve: value of x5 (id 11): expected 0.0, got 0.25"
    assert expected in report.failures
    assert not any("maximizing model" in failure for failure in report.failures)


def test_the_self_test_reports_an_integer_solve_that_ignores_binary_columns() -> None:
    report = layer.master_solver_self_test(
        SelfTestMasterSolver(Mistake.IGNORES_BINARY), SelfTestMasterSolver(maximizing=True)
    )
    assert "minimizing model: binary solve: objective: expected 1.5, got 1.0" in report.failures
    expected = "minimizing model: binary solve: value of x4 (id 9): expected 1.0, got 2.0"
    assert expected in report.failures
    assert not any("after the binary one" in failure for failure in report.failures)


def test_the_self_test_reports_bounds_kept_from_a_binary_solve() -> None:
    report = layer.master_solver_self_test(
        SelfTestMasterSolver(Mistake.KEEPS_BINARY_BOUNDS), SelfTestMasterSolver(maximizing=True)
    )
    assert report.failures[0] == (
        "minimizing model: integer solve after the binary one: objective: expected 1.0, got 1.5"
    )
    assert not any("maximizing model" in failure for failure in report.failures)


def test_the_self_test_needs_master_solvers_that_add_rows() -> None:
    with pytest.raises(ValueError, match="minimizing master solver must have no rows"):
        layer.master_solver_self_test(
            SelfTestMasterSolver(Mistake.CANNOT_ADD_ROWS),
            SelfTestMasterSolver(maximizing=True),
        )

    with pytest.raises(ValueError, match="maximizing master solver must have no rows"):
        layer.master_solver_self_test(
            SelfTestMasterSolver(),
            SelfTestMasterSolver(Mistake.CANNOT_ADD_ROWS, maximizing=True),
        )

    # A master solver that already has rows.
    used = SelfTestMasterSolver()
    used.add_rows(layer.RowBatch([0.0], [1.0]))

    with pytest.raises(ValueError, match="no rows"):
        layer.master_solver_self_test(used, SelfTestMasterSolver(maximizing=True))


def test_a_status_that_is_not_optimal_ends_the_checks_of_that_master_solver() -> None:
    class Infeasible(SelfTestMasterSolver):
        def solve_relaxation(self, params: Any = layer.RelaxationParams()) -> Any:
            return layer.SolverStatus.Infeasible

    report = layer.master_solver_self_test(Infeasible(), SelfTestMasterSolver(maximizing=True))
    assert list(report.failures) == [
        "minimizing model: first LP: status: expected Optimal, got Infeasible"
    ]


def test_a_later_step_that_fails_ends_the_checks_of_that_master_solver() -> None:
    class FailsAfterTheFirstSolve(SelfTestMasterSolver):
        solves = 0

        def solve_relaxation(self, params: Any = layer.RelaxationParams()) -> Any:
            self.solves += 1

            if self.solves == 2:
                return layer.SolverStatus.TimeLimit

            return super().solve_relaxation(params)

    class FailsTheIntegerSolve(SelfTestMasterSolver):
        def solve_integer(self, params: Any = layer.IntegerParams()) -> Any:
            return layer.SolverStatus.Other

    report = layer.master_solver_self_test(
        FailsAfterTheFirstSolve(), FailsTheIntegerSolve(maximizing=True)
    )
    assert list(report.failures) == [
        "minimizing model: LP after adding x4: status: expected Optimal, got TimeLimit",
        "maximizing model: integer solve: status: expected Optimal, got Other",
    ]


def test_the_integer_step_is_skipped_without_the_capability() -> None:
    class NoIntegerSolve(SelfTestMasterSolver):
        def capabilities(self) -> Any:
            return layer.Capabilities(add_rows=True)

        def solve_integer(self, params: Any = layer.IntegerParams()) -> Any:
            raise AssertionError("the integer solve must not run")

    report = layer.master_solver_self_test(NoIntegerSolve(), NoIntegerSolve(maximizing=True))
    assert report.passed


class ShortRowBounds(SelfTestMasterSolver):
    def row_bounds(self) -> Any:
        lower, upper = super().row_bounds()
        return lower[:1], upper


class ShortDuals(SelfTestMasterSolver):
    def duals(self) -> Any:
        return super().duals()[:2]


class ShortValues(SelfTestMasterSolver):
    def primal(self, ids: Any) -> Any:
        return super().primal(ids)[:1]


class ShortDualsAfterAdding(SelfTestMasterSolver):
    def duals(self) -> Any:
        # The column added after the first LP has id 9.
        duals = super().duals()
        return duals[:2] if 9 in self.ids else duals


@pytest.mark.parametrize(
    ("solver", "failure", "ids"),
    [
        (ShortRowBounds, "rows: lower bounds: expected 3 values, got 1", []),
        (ShortDuals, "first LP: duals: expected 3 values, got 2", [7, 3, 5]),
        (ShortValues, "first LP: values: expected 3 values, got 1", [7, 3, 5]),
        (
            ShortDualsAfterAdding,
            "LP after adding x4: duals: expected 3 values, got 2",
            [7, 3, 5, 9],
        ),
    ],
)
def test_an_answer_of_the_wrong_size_ends_the_checks_of_that_master_solver(
    solver: type[SelfTestMasterSolver], failure: str, ids: list[int]
) -> None:
    wrong = solver()
    report = layer.master_solver_self_test(wrong, SelfTestMasterSolver(maximizing=True))
    assert report.failures == (f"minimizing model: {failure}",)

    # No column comes after the failed step.
    assert wrong.ids == ids


def test_a_failed_check_names_what_was_compared() -> None:
    class WrongBounds(SelfTestMasterSolver):
        def row_bounds(self) -> Any:
            lower, upper = super().row_bounds()
            return lower, upper + 1.0

    report = layer.master_solver_self_test(WrongBounds(), SelfTestMasterSolver(maximizing=True))

    # The infinite bound of row 1 stays equal to itself.
    assert list(report.failures) == [
        "minimizing model: rows: upper bound of row 0 (=): expected 2.0, got 3.0",
        "minimizing model: rows: upper bound of row 2 (<=): expected 1.5, got 2.5",
    ]


def test_a_failed_check_writes_numbers_as_python_does() -> None:
    class LargeObjective(SelfTestMasterSolver):
        def objective(self) -> float:
            return 1e20

    report = layer.master_solver_self_test(LargeObjective(), SelfTestMasterSolver(maximizing=True))
    assert report.failures[0] == "minimizing model: first LP: objective: expected 3.75, got 1e+20"


def test_the_report_is_a_record() -> None:
    report = layer.MasterSolverSelfTestReport()
    assert report.passed and report.failures == ()

    with pytest.raises(AttributeError):
        report.failures = ["x"]  # type: ignore[misc]


def test_add_rows_refuses_a_sense_of_another_type() -> None:
    master, solver = master_with_rows(0)
    sense: Any = "LessEqual"

    with pytest.raises(TypeError, match="^MasterProblem.add_rows: sense must be a RowSense$"):
        master.add_rows(1, sense, 1.0)

    assert solver.lower == []
