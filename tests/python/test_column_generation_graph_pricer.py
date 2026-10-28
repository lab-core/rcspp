#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The graph pricer from Python: a ResourceGraph priced in C++, alone and in the loop."""

import gc
import math
import os
import sys

import numpy as np
import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from column_generation_fakes import ScriptedMasterSolver  # noqa: E402

from rcspp._core.column_generation import GraphPricer as CppGraphPricer  # noqa: E402
from rcspp._core.graph import Algorithm  # noqa: E402
from rcspp.column_generation import (  # noqa: E402
    ColumnGeneration,
    ColumnGenerationStatus,
    GraphPricer,
    MasterProblem,
    PricingRequest,
    PricingStage,
    PricingStatus,
    RowSense,
    SolverStatus,
)
from rcspp.graph import BucketAlgorithmParams, ResourceGraph  # noqa: E402
from rcspp.resource import (  # noqa: E402
    AdditionExtensionFunction,
    TrivialFeasibilityFunction,
    ValueCostFunction,
    ValueDominanceFunction,
)

# Path A, 0 → 1 → 3, costs 3 and covers row 0; path B, 0 → 2 → 3, costs 5 and covers row 1.
COST_A = 3.0
COST_B = 5.0


def two_path_graph(sink: bool = True) -> ResourceGraph:
    """Two paths from the source 0 to the sink 3, each covering one master row."""
    graph = ResourceGraph()
    graph.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    graph.add_node(0, source=True)
    graph.add_node(1)
    graph.add_node(2)
    graph.add_node(3, sink=sink)
    graph.add_arc(COST_A, 0, 1, cost=COST_A, rows=[(0, 1.0)])
    graph.add_arc(0.0, 1, 3, cost=0.0)
    graph.add_arc(COST_B, 0, 2, cost=COST_B, rows=[(1, 1.0)])
    graph.add_arc(0.0, 2, 3, cost=0.0)
    return graph


def threshold(value: float) -> PricingRequest:
    """A request for the paths of reduced cost below `value`."""
    return PricingRequest(reduced_cost_threshold=value)


# ── One pricing call ─────────────────────────────────────────────────────────


def test_returns_the_paths_below_the_threshold_as_columns() -> None:
    pricer = GraphPricer(two_path_graph())
    # Reduced costs: A 3 − 10 = −7, B 5 − 0 = 5.
    result = pricer.price(np.array([10.0, 0.0]), threshold(0.0))

    assert result.status == PricingStatus.Optimal and result.exact_stage
    assert len(result.columns) == 1
    path = result.columns[0]
    assert path.cost == -7.0
    assert path.column.cost == COST_A
    assert [(row.index, row.coefficient) for row in path.column.rows] == [(0, 1.0)]
    assert result.min_reduced_cost_bound == -7.0


def test_nothing_below_the_threshold_is_a_proof() -> None:
    pricer = GraphPricer(two_path_graph())
    result = pricer.price(np.array([1.0, 1.0]), threshold(-0.5))

    assert result.status == PricingStatus.Optimal and result.exact_stage
    assert result.columns == []
    assert result.min_reduced_cost_bound == -0.5


def test_a_heuristic_stage_that_finds_columns_stops_the_call() -> None:
    pricer = GraphPricer(two_path_graph(), [PricingStage("greedy"), PricingStage("simple")])
    result = pricer.price(np.array([10.0, 0.0]), threshold(0.0))

    # Found by the greedy stage, which proves nothing.
    assert len(result.columns) == 1
    assert result.status == PricingStatus.SolverLimit and not result.exact_stage
    assert result.min_reduced_cost_bound == -math.inf


def test_a_stage_that_finds_nothing_hands_over_to_the_next() -> None:
    pricer = GraphPricer(two_path_graph(), [PricingStage("greedy"), PricingStage("simple")])
    result = pricer.price(np.array([1.0, 1.0]), threshold(-0.5))

    # The exact stage ran after the greedy one, and proves it.
    assert result.status == PricingStatus.Optimal and result.exact_stage


def test_a_stage_said_exact_proves() -> None:
    pricer = GraphPricer(two_path_graph(), [PricingStage(Algorithm.Greedy, exact=True)])
    result = pricer.price(np.array([1.0, 1.0]), threshold(-0.5))
    assert result.status == PricingStatus.Optimal and result.exact_stage


def test_a_bucket_stage_and_its_params() -> None:
    stage = PricingStage("simple", BucketAlgorithmParams(range_buckets=10))
    pricer = GraphPricer(two_path_graph(), [stage])
    result = pricer.price(np.array([0.0, 10.0]), threshold(0.0))
    assert [path.column.cost for path in result.columns] == [COST_B]


def test_the_cost_scale_is_the_largest_arc_cost() -> None:
    assert GraphPricer(two_path_graph()).cost_scale() == COST_B


def test_refused_graphs_and_stages() -> None:
    with pytest.raises(RuntimeError, match="no source or no sink"):
        GraphPricer(two_path_graph(sink=False)).price(np.array([0.0, 0.0]), threshold(0.0))
    with pytest.raises(IndexError, match="cost index"):
        GraphPricer(two_path_graph(), cost_index=3).price(np.array([0.0, 0.0]), threshold(0.0))
    with pytest.raises(ValueError, match="non-negative"):
        GraphPricer(two_path_graph(), cost_index=-1)
    with pytest.raises(ValueError, match="searches bidirectionally"):
        GraphPricer(two_path_graph(), [PricingStage("bidirectional", direction="forward")])
    with pytest.raises(ValueError, match="no stage"):
        GraphPricer(two_path_graph(), [])
    with pytest.raises(TypeError, match="bound resource graph"):
        CppGraphPricer(object(), [])


def test_the_pricer_keeps_its_graph_alive() -> None:
    def build() -> GraphPricer:
        return GraphPricer(two_path_graph())

    pricer = build()
    gc.collect()
    assert pricer.graph.get_arcs_size() == 4
    result = pricer.price(np.array([10.0, 0.0]), threshold(0.0))
    assert [path.column.cost for path in result.columns] == [COST_A]


# ── In the loop ──────────────────────────────────────────────────────────────


def test_column_generation_from_scratch_with_a_graph_pricer() -> None:
    # Rows "cover row 0 ≥ 1" and "cover row 1 ≥ 1"; the master starts from two artificial
    # columns, ids 0 and 1, one per row.
    solver = ScriptedMasterSolver()
    master = MasterProblem(solver)
    master.add_rows(2, RowSense.GreaterEqual, 1.0)

    def solve(lp: ScriptedMasterSolver) -> SolverStatus:
        # The LP's answer: each row is covered by its path once found, else by its artificial
        # column; its dual is the cost of what covers it.
        covering = {row: column.cost for column in lp.columns[2:] for row, _ in column.entries}
        lp.dual_values = [covering.get(row, lp.columns[row].cost) for row in range(2)]
        lp.objective_value = sum(lp.dual_values)
        lp.values = {row: 0.0 if row in covering else 1.0 for row in range(2)}
        lp.values.update({column.id: 1.0 for column in lp.columns[2:]})
        return SolverStatus.Optimal

    solver.on_solve = solve
    master.add_pricer(GraphPricer(two_path_graph()))
    result = ColumnGeneration(master).run()

    # A first (it dominates B while both duals are artificial), then B, then the proof.
    assert result.status == ColumnGenerationStatus.Optimal
    assert result.iterations == 3
    assert result.objective == COST_A + COST_B
    assert result.statistics.generated_columns == 2
    assert [column.cost for column in solver.columns[2:]] == [COST_A, COST_B]
