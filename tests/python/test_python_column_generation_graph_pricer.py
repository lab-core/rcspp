#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The pure-Python graph pricer: a ResourceGraph priced from Python, alone."""

import gc
import math
import os
import sys
from types import SimpleNamespace
from typing import Any

import numpy as np
import pytest

sys.path.insert(
    0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "python", "src")
)

from rcspp._core.graph import (  # noqa: E402
    Algorithm,
    AlgorithmParams,
    AlgorithmStatus,
    SolveResult,
)
from rcspp.graph import BucketAlgorithmParams, ResourceGraph  # noqa: E402
from rcspp.python_column_generation import (  # noqa: E402
    GraphPricer,
    Pricer,
    PricingRequest,
    PricingStage,
    PricingStatus,
    StageRequest,
)
from rcspp.resource import (  # noqa: E402
    AdditionExtensionFunction,
    TrivialFeasibilityFunction,
    ValueCostFunction,
    ValueDominanceFunction,
)

# Path A, 0 -> 1 -> 3, costs 3 and covers row 0; path B, 0 -> 2 -> 3, costs 5 and covers row 1.
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


def threshold(value: float, timeout_s: float = math.inf) -> PricingRequest:
    """A request for the paths of reduced cost below `value`."""
    return PricingRequest(reduced_cost_threshold=value, timeout_s=timeout_s)


class Searches:
    """Stands in for ``graph.solve``: records each search and answers a complete one
    that found nothing, or what the test gives."""

    def __init__(self, answer: Any = None) -> None:
        self.answer = answer or SimpleNamespace(
            solutions=[], status=AlgorithmStatus.Complete, memory_pressure_triggered=False
        )
        self.algorithms: list[Algorithm] = []
        self.timeouts: list[float] = []

    def __call__(self, algorithm: Algorithm, *arguments: Any) -> Any:
        self.algorithms.append(algorithm)
        self.timeouts.append(arguments[1].timeout_s)
        return self.answer


# MARK: One pricing call


def test_returns_the_paths_below_the_threshold_as_columns() -> None:
    pricer = GraphPricer(two_path_graph())

    # Reduced costs: A 3 - 10 = -7, B 5 - 0 = 5.
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


def test_the_default_request_asks_for_negative_reduced_costs() -> None:
    pricer = GraphPricer(two_path_graph())
    assert len(pricer.price(np.array([10.0, 0.0])).columns) == 1
    assert pricer.price(np.array([1.0, 1.0])).columns == []


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


def test_every_call_starts_from_the_first_stage(monkeypatch: pytest.MonkeyPatch) -> None:
    pricer = GraphPricer(two_path_graph(), [PricingStage("greedy"), PricingStage("simple")])
    searches = Searches()
    monkeypatch.setattr(pricer.graph, "solve", searches)
    pricer.price(np.array([1.0, 1.0]), threshold(-0.5))
    pricer.price(np.array([1.0, 1.0]), threshold(-0.5))

    assert searches.algorithms == [Algorithm.Greedy, Algorithm.Simple] * 2


def test_a_stage_said_exact_proves() -> None:
    pricer = GraphPricer(two_path_graph(), [PricingStage(Algorithm.Greedy, exact=True)])
    result = pricer.price(np.array([1.0, 1.0]), threshold(-0.5))
    assert result.status == PricingStatus.Optimal and result.exact_stage


def test_an_exact_stage_that_proves_ends_the_call(monkeypatch: pytest.MonkeyPatch) -> None:
    # Nothing below the threshold, proven by the first stage: the next one does not run.
    pricer = GraphPricer(two_path_graph(), [PricingStage("simple"), PricingStage("greedy")])
    searches = Searches()
    monkeypatch.setattr(pricer.graph, "solve", searches)
    result = pricer.price(np.array([1.0, 1.0]), threshold(-0.5))

    assert searches.algorithms == [Algorithm.Simple]
    assert result.status == PricingStatus.Optimal and result.exact_stage


def test_a_stage_said_not_exact_does_not_prove() -> None:
    pricer = GraphPricer(two_path_graph(), [PricingStage("simple", exact=False)])
    result = pricer.price(np.array([1.0, 1.0]), threshold(-0.5))
    assert result.status == PricingStatus.SolverLimit and not result.exact_stage


def test_a_bucket_stage_and_its_params() -> None:
    stage = PricingStage("simple", BucketAlgorithmParams(range_buckets=10))
    pricer = GraphPricer(two_path_graph(), [stage])
    result = pricer.price(np.array([0.0, 10.0]), threshold(0.0))
    assert [path.column.cost for path in result.columns] == [COST_B]


def test_a_search_that_did_not_finish_proves_nothing(monkeypatch: pytest.MonkeyPatch) -> None:
    for status, trimmed in [
        (AlgorithmStatus.Timeout, False),
        (AlgorithmStatus.Interrupted, False),
        # Complete, but memory pressure dropped labels.
        (AlgorithmStatus.Complete, True),
    ]:
        pricer = GraphPricer(two_path_graph())
        answer = SimpleNamespace(solutions=[], status=status, memory_pressure_triggered=trimmed)
        monkeypatch.setattr(pricer.graph, "solve", Searches(answer))
        result = pricer.price(np.array([1.0, 1.0]), threshold(-0.5))
        assert result.status == PricingStatus.SolverLimit
        assert result.exact_stage and result.min_reduced_cost_bound == -math.inf


def test_a_path_at_the_threshold_is_not_a_column_but_bounds_the_best() -> None:
    # A path of reduced cost exactly the threshold is not below it.
    result = GraphPricer(two_path_graph()).price(np.array([5.0, 0.0]), threshold(-2.0))
    assert result.columns == []
    assert result.min_reduced_cost_bound == -2.0


# MARK: Time


def test_the_time_of_a_call_caps_the_limit_of_a_stage(monkeypatch: pytest.MonkeyPatch) -> None:
    capped, free = AlgorithmParams(), AlgorithmParams()
    capped.timeout_s = 100.0
    pricer = GraphPricer(
        two_path_graph(),
        [PricingStage("greedy", capped), PricingStage("simple", free)],
    )
    searches = Searches()
    monkeypatch.setattr(pricer.graph, "solve", searches)
    pricer.price(np.array([1.0, 1.0]), threshold(-0.5, timeout_s=50.0))

    first, second = searches.timeouts
    assert 49.0 < first <= 50.0 and 49.0 < second <= 50.0

    # Within the stage's own limit when that is the smaller.
    pricer.price(np.array([1.0, 1.0]), threshold(-0.5, timeout_s=500.0))
    assert searches.timeouts[2] == 100.0 and 499.0 < searches.timeouts[3] <= 500.0


def test_a_stage_gets_the_time_the_stages_before_left(monkeypatch: pytest.MonkeyPatch) -> None:
    # A clock that each search moves 10 seconds on.
    clock = [0.0]
    searches = Searches()

    def search(algorithm: Algorithm, *arguments: Any) -> Any:
        clock[0] += 10.0
        return searches(algorithm, *arguments)

    module = sys.modules[GraphPricer.__module__]
    monkeypatch.setattr(module, "time", SimpleNamespace(perf_counter=lambda: clock[0]))
    stages = [PricingStage("greedy"), PricingStage("greedy"), PricingStage("simple")]
    pricer = GraphPricer(two_path_graph(), stages)
    monkeypatch.setattr(pricer.graph, "solve", search)
    pricer.price(np.array([1.0, 1.0]), threshold(-0.5, timeout_s=50.0))

    assert searches.timeouts == [50.0, 40.0, 30.0]


def test_a_call_out_of_time_does_not_run_the_next_stage(monkeypatch: pytest.MonkeyPatch) -> None:
    pricer = GraphPricer(two_path_graph(), [PricingStage("greedy"), PricingStage("simple")])
    searches = Searches()
    monkeypatch.setattr(pricer.graph, "solve", searches)
    result = pricer.price(np.array([1.0, 1.0]), threshold(-0.5, timeout_s=0.0))

    assert searches.algorithms == [Algorithm.Greedy]
    assert searches.timeouts == [0.0]
    assert result.columns == []


def test_a_real_search_with_no_time_left_proves_nothing() -> None:
    result = GraphPricer(two_path_graph()).price(np.array([1.0, 1.0]), threshold(-0.5, 0.0))
    assert result.status == PricingStatus.SolverLimit


def test_the_params_are_copied_when_the_pricer_is_built(monkeypatch: pytest.MonkeyPatch) -> None:
    params = AlgorithmParams()
    params.timeout_s = 7.0
    pricer = GraphPricer(two_path_graph(), [PricingStage("simple", params)])
    params.timeout_s = 1.0
    searches = Searches()
    monkeypatch.setattr(pricer.graph, "solve", searches)
    pricer.price(np.array([1.0, 1.0]), threshold(-0.5, timeout_s=50.0))

    assert searches.timeouts == [7.0]

    # The caller's params are left as they were, and a call does not change them.
    assert params.timeout_s == 1.0


class BudgetParams(AlgorithmParams):
    """Params with a read-only property, computed from the others."""

    @property
    def budget_s(self) -> float:
        return 2.0 * self.timeout_s


def test_a_read_only_property_of_the_params_is_not_copied(monkeypatch: pytest.MonkeyPatch) -> None:
    params = BudgetParams()
    params.timeout_s = 3.0
    pricer = GraphPricer(two_path_graph(), [PricingStage("simple", params)])
    searches = Searches()
    monkeypatch.setattr(pricer.graph, "solve", searches)
    pricer.price(np.array([1.0, 1.0]), threshold(-0.5))
    assert searches.timeouts == [3.0]


def test_an_interruption_reaches_the_caller(monkeypatch: pytest.MonkeyPatch) -> None:
    pricer = GraphPricer(two_path_graph())

    def interrupted(*arguments: Any) -> Any:
        raise KeyboardInterrupt

    monkeypatch.setattr(pricer.graph, "solve", interrupted)

    with pytest.raises(KeyboardInterrupt):
        pricer.price(np.array([1.0, 1.0]), threshold(-0.5))


# MARK: The pricer itself


def test_a_graph_pricer_is_a_pricer_with_the_cost_scale_of_its_arcs() -> None:
    pricer = GraphPricer(two_path_graph())
    assert isinstance(pricer, Pricer)
    assert pricer.cost_scale() == COST_B


def test_the_cost_scale_of_a_graph_without_arcs_is_zero() -> None:
    graph = ResourceGraph()
    graph.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )
    graph.add_node(0, source=True)
    assert GraphPricer(graph).cost_scale() == 0.0


def test_the_cost_scale_ignores_the_reduced_costs() -> None:
    pricer = GraphPricer(two_path_graph())
    pricer.price(np.array([100.0, 100.0]), threshold(0.0))
    assert pricer.cost_scale() == COST_B


def test_arcs_added_after_the_pricer_are_priced_too() -> None:
    graph = two_path_graph()
    pricer = GraphPricer(graph)
    graph.add_arc(1.0, 0, 3, cost=1.0, rows=[(2, 1.0)])

    result = pricer.price(np.array([0.0, 0.0, 10.0]), threshold(0.0))
    assert [path.column.cost for path in result.columns] == [1.0]
    assert pricer.cost_scale() == COST_B


def test_the_pricer_keeps_its_graph_alive() -> None:
    def build() -> GraphPricer:
        return GraphPricer(two_path_graph())

    pricer = build()
    gc.collect()
    assert pricer.graph.get_arcs_size() == 4
    result = pricer.price(np.array([10.0, 0.0]), threshold(0.0))
    assert [path.column.cost for path in result.columns] == [COST_A]


def test_refused_graphs_and_stages() -> None:
    with pytest.raises(RuntimeError, match="no source or no sink"):
        GraphPricer(two_path_graph(sink=False)).price(np.array([0.0, 0.0]), threshold(0.0))

    with pytest.raises(IndexError, match="cost index"):
        GraphPricer(two_path_graph(), cost_index=3).price(np.array([0.0, 0.0]), threshold(0.0))

    with pytest.raises(ValueError, match="non-negative"):
        GraphPricer(two_path_graph(), cost_index=-1)

    with pytest.raises(ValueError, match="searches bidirectionally"):
        GraphPricer(two_path_graph(), [PricingStage("bidirectional", direction="forward")])

    with pytest.raises(ValueError, match="Unknown algorithm 'dijkstra'"):
        GraphPricer(two_path_graph(), [PricingStage("dijkstra")])

    with pytest.raises(ValueError, match="no stage"):
        GraphPricer(two_path_graph(), [])


def test_wrong_types_are_refused() -> None:
    with pytest.raises(TypeError, match="expected a ResourceGraph"):
        GraphPricer(object())  # type: ignore[arg-type]

    with pytest.raises(TypeError, match="cost_index must be an integer"):
        GraphPricer(two_path_graph(), cost_index=0.5)  # type: ignore[arg-type]

    with pytest.raises(TypeError, match="PricingStage"):
        GraphPricer(two_path_graph(), ["simple"])  # type: ignore[list-item]


def test_the_stages_are_one_exact_simple_stage_by_default(monkeypatch: pytest.MonkeyPatch) -> None:
    stage = PricingStage()
    assert (stage.algorithm, stage.params, stage.direction, stage.exact) == (
        "simple",
        None,
        None,
        None,
    )
    searches = Searches()
    pricer = GraphPricer(two_path_graph())
    monkeypatch.setattr(pricer.graph, "solve", searches)
    result = pricer.price(np.array([1.0, 1.0]), threshold(-0.5))
    assert searches.algorithms == [Algorithm.Simple]
    assert result.exact_stage


# MARK: Stages with their own search


def test_a_stage_request_has_the_defaults_of_a_search() -> None:
    request = StageRequest()
    assert (request.upper_bound, request.timeout_s) == (0.0, math.inf)
    assert request.preprocess and request.cost_index == 0


def test_a_stage_with_its_own_search_is_asked_what_the_call_asks() -> None:
    requests: list[StageRequest] = []

    def search(graph: ResourceGraph, request: StageRequest) -> SolveResult:
        requests.append(request)
        return graph.solve("simple", request.upper_bound, None, request.preprocess)

    pricer = GraphPricer(
        two_path_graph(), [PricingStage(search=search, exact=True)], preprocess=False
    )
    result = pricer.price(np.array([10.0, 0.0]), threshold(-1.0, timeout_s=5.0))

    assert [path.column.cost for path in result.columns] == [COST_A]
    assert result.status == PricingStatus.Optimal and result.exact_stage
    (request,) = requests
    assert request.upper_bound == -1.0 and 0.0 < request.timeout_s <= 5.0
    assert not request.preprocess and request.cost_index == 0


def test_a_search_said_not_exact_proves_nothing() -> None:
    def search(graph: ResourceGraph, request: StageRequest) -> SolveResult:
        return graph.solve("simple", request.upper_bound)

    stages = [PricingStage(search=search, exact=False), PricingStage("simple")]
    pricer = GraphPricer(two_path_graph(), stages)
    assert pricer.price(np.array([10.0, 0.0])).status == PricingStatus.SolverLimit

    # Nothing found: the next stage proves it.
    assert pricer.price(np.array([1.0, 1.0])).status == PricingStatus.Optimal


def test_the_paths_a_search_returns_at_the_threshold_are_not_columns() -> None:
    # A search of its own may return paths that are not below the threshold: here the path of
    # reduced cost -2, the threshold.
    found: list[float] = []

    def search(graph: ResourceGraph, request: StageRequest) -> SolveResult:
        result = graph.solve("simple", request.upper_bound + 1.0)
        found.extend(solution.cost for solution in result.solutions)
        return result

    pricer = GraphPricer(two_path_graph(), [PricingStage(search=search, exact=True)])
    result = pricer.price(np.array([5.0, 0.0]), threshold(-2.0))

    assert found == [-2.0]
    assert result.columns == []
    assert result.status == PricingStatus.Optimal and result.min_reduced_cost_bound == -2.0


def test_a_stage_with_its_own_search_needs_exact_and_nothing_else() -> None:
    def search(graph: ResourceGraph, request: StageRequest) -> SolveResult:
        return graph.solve()

    with pytest.raises(ValueError, match="must say if it is exact"):
        PricingStage(search=search)

    with pytest.raises(ValueError, match="takes no algorithm, params or direction"):
        PricingStage(search=search, exact=True, params=AlgorithmParams())

    with pytest.raises(ValueError, match="takes no algorithm, params or direction"):
        PricingStage(search=search, exact=True, direction="forward")


def test_a_search_that_returns_something_else_is_refused() -> None:
    def search(graph: ResourceGraph, request: StageRequest) -> Any:
        return []

    pricer = GraphPricer(two_path_graph(), [PricingStage(search=search, exact=True)])

    with pytest.raises(TypeError, match="PricingStage.search: expected a SolveResult, got list"):
        pricer.price(np.array([0.0, 0.0]))


# MARK: The stages and the last one run


def test_the_pricer_shows_its_stages_and_the_last_one_run() -> None:
    stages = [PricingStage("greedy"), PricingStage("tabu"), PricingStage("simple")]
    pricer = GraphPricer(two_path_graph(), stages)
    assert pricer.stages == tuple(stages) and pricer.last_stage_index is None

    # Found by the first stage, then nothing until the last one proves it.
    pricer.price(np.array([10.0, 0.0]))
    assert pricer.last_stage_index == 0
    pricer.price(np.array([1.0, 1.0]))
    assert pricer.last_stage_index == 2
    assert GraphPricer(two_path_graph()).stages == (PricingStage(),)


def test_the_next_stage_returns_what_it_finds() -> None:
    requests: list[StageRequest] = []

    def nothing(graph: ResourceGraph, request: StageRequest) -> SolveResult:
        requests.append(request)
        return SolveResult()

    stages = [PricingStage(search=nothing, exact=False), PricingStage("simple")]
    pricer = GraphPricer(two_path_graph(), stages)
    result = pricer.price(np.array([10.0, 0.0]), threshold(-0.5))

    assert [path.column.cost for path in result.columns] == [COST_A]
    assert result.status == PricingStatus.Optimal and result.exact_stage

    # The default options reach the stage.
    (request,) = requests
    assert request.upper_bound == -0.5 and request.preprocess and request.cost_index == 0


def test_a_search_must_be_callable_and_replaces_the_algorithm() -> None:
    not_a_search: Any = 42

    with pytest.raises(TypeError, match="^PricingStage: the search must be callable$"):
        PricingStage(search=not_a_search, exact=True)

    def search(graph: ResourceGraph, request: StageRequest) -> SolveResult:
        return graph.solve()

    with pytest.raises(ValueError, match="takes no algorithm, params or direction"):
        PricingStage("greedy", search=search, exact=True)
