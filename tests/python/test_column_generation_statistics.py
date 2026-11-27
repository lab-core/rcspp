#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""What the pure-Python column generation loop tells about each iteration: the statistics
switches, the fields they fill, and the progress log."""

import logging
import math
import os
import sys

import numpy as np
import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from column_generation_fakes import (  # noqa: E402
    ScriptedMasterSolver,
    ScriptedPricer,
    make_path,
    proven_none,
)

from rcspp.column_generation import (  # noqa: E402
    ColumnBatch,
    ColumnGeneration,
    ColumnGenerationParams,
    ColumnGenerationResult,
    ColumnGenerationStatus,
    ColumnTag,
    ConvexityRow,
    GraphPricer,
    HighspyMasterSolver,
    IterationInfo,
    MasterProblem,
    PricingRequest,
    PricingResult,
    PricingStatus,
    RowSense,
    SolverStatus,
    StatisticsParams,
)
from rcspp.column_generation.types import FloatArray  # noqa: E402
from rcspp.graph import ResourceGraph  # noqa: E402
from rcspp.resource import (  # noqa: E402
    AdditionExtensionFunction,
    TrivialFeasibilityFunction,
    ValueCostFunction,
    ValueDominanceFunction,
)

PATH_COST = 10.0
ARTIFICIAL_COST = 100.0
CONVEXITY_DUAL = 2.0


class ForgetfulMasterSolver(ScriptedMasterSolver):
    """Forgets its LP solution once columns are added, as gurobipy may once its model
    changes."""

    def add_columns(self, columns: ColumnBatch) -> None:
        super().add_columns(columns)
        self.values = {}


class Toy:
    """One row "cover >= 1" with an artificial column, and a pricer with a convexity row
    (at most 1 path) that offers a path of cost 10 while its reduced cost is negative.

    First LP: the artificial column, value 100; second: the path, value 10. The convexity
    dual is 2 in both.
    """

    def __init__(self) -> None:
        self.solver = ForgetfulMasterSolver()
        self.master = MasterProblem(self.solver)
        self.master.add_rows(1, RowSense.GreaterEqual, 1.0)
        self.pricer = ScriptedPricer(self.respond)
        self.master.add_pricer(self.pricer, ConvexityRow(max_paths=1.0))
        self.master.add_artificial_columns([0], [1.0], ARTIFICIAL_COST)
        self.solver.on_solve = self.solve

    @staticmethod
    def solve(lp: ScriptedMasterSolver) -> SolverStatus:
        has_path = len(lp.columns) > 1
        lp.objective_value = PATH_COST if has_path else ARTIFICIAL_COST
        # With the path, its reduced cost is 0: 10 - (10 - 2) - 2.
        lp.dual_values = [
            PATH_COST - CONVEXITY_DUAL if has_path else ARTIFICIAL_COST,
            CONVEXITY_DUAL,
        ]
        lp.values = {1: 1.0} if has_path else {0: 1.0}
        return SolverStatus.Optimal

    @staticmethod
    def respond(duals: FloatArray, request: PricingRequest) -> PricingResult:
        result = proven_none()
        reduced_cost = PATH_COST - float(duals[0])

        if reduced_cost < request.reduced_cost_threshold:
            result.columns = [make_path(PATH_COST, [(0, 1.0)], reduced_cost)]

        return result


def run(**options: object) -> tuple[Toy, ColumnGenerationResult, list[IterationInfo]]:
    """A run of the toy with `options`, and the iterations seen."""
    toy = Toy()
    seen: list[IterationInfo] = []
    params = ColumnGenerationParams(
        initialization="none", on_iteration=seen.append, **options  # type: ignore[arg-type]
    )
    return toy, ColumnGeneration(toy.master, params).run(), seen


# MARK: The switches


def test_by_default_only_the_times_are_added() -> None:
    toy, result, seen = run()

    assert result.iterations == 2
    assert all(info.relaxation_seconds is not None for info in seen)
    assert all(info.pricing_seconds is not None for info in seen)

    for info in seen:
        assert info.pricers is None
        assert info.duals is None
        assert info.master_values is None
        assert info.added_columns is None

    # Pricers are not asked for their details.
    assert not any(request.search_details for request in toy.pricer.requests)


def test_without_timers_there_are_no_times() -> None:
    _, _, seen = run(statistics=StatisticsParams(timers=False))
    assert all(info.relaxation_seconds is None for info in seen)
    assert all(info.pricing_seconds is None for info in seen)


def test_the_times_of_an_iteration_add_up_to_the_runs() -> None:
    _, result, seen = run()
    relaxation = sum(info.relaxation_seconds or 0.0 for info in seen)
    pricing = sum(info.pricing_seconds or 0.0 for info in seen)

    assert relaxation == pytest.approx(result.statistics.relaxation_seconds)
    assert pricing == pytest.approx(result.statistics.pricing_seconds)


def test_each_pricer_says_what_it_did() -> None:
    toy, _, seen = run(statistics=StatisticsParams(pricers=True))
    first, second = (info.pricers for info in seen)

    assert first is not None and second is not None
    assert len(first) == len(second) == 1
    assert first[0].status == PricingStatus.Optimal
    assert first[0].new_columns == 1
    # The path's reduced cost 10 - 100, minus the convexity dual.
    assert first[0].min_reduced_cost == PATH_COST - ARTIFICIAL_COST - CONVEXITY_DUAL
    assert first[0].seconds >= 0.0
    # A pricer without stages gives no details.
    assert first[0].search_details == ()
    assert second[0].new_columns == 0
    assert second[0].min_reduced_cost is None
    assert all(request.search_details for request in toy.pricer.requests)


def test_the_search_counts_need_the_pricers() -> None:
    with pytest.raises(ValueError, match="search needs pricers"):
        StatisticsParams(search=True)

    StatisticsParams(pricers=True, search=True)


def test_the_duals_are_the_ones_priced_and_a_copy() -> None:
    toy, _, seen = run(statistics=StatisticsParams(duals=True))

    assert seen[0].duals is not None
    np.testing.assert_array_equal(seen[0].duals, [ARTIFICIAL_COST, CONVEXITY_DUAL])
    np.testing.assert_array_equal(seen[0].duals, toy.pricer.seen_duals[0])
    assert seen[0].duals is not toy.pricer.seen_duals[0]


def test_the_master_values_are_read_before_the_columns_go_in() -> None:
    # The solver forgets its solution once the path is added: the values must come first.
    _, _, seen = run(statistics=StatisticsParams(master_values=True))
    first, second = (info.master_values for info in seen)

    assert first is not None and second is not None
    np.testing.assert_array_equal(first[0], [0])
    np.testing.assert_array_equal(first[1], [1.0])
    assert first[0].dtype == np.uint64
    np.testing.assert_array_equal(second[0], [1])
    np.testing.assert_array_equal(second[1], [1.0])


def test_the_added_columns_are_the_masters() -> None:
    toy, _, seen = run(statistics=StatisticsParams(columns=True))
    first, second = (info.added_columns for info in seen)

    assert first is not None and len(first) == 1
    assert first[0] is toy.master.columns[1]
    assert first[0].id == 1 and first[0].pricer == 0 and first[0].tag == ColumnTag.Real
    assert first[0].solution.column.cost == PATH_COST
    assert second == ()


def test_the_switches_change_nothing_else() -> None:
    _, plain, plain_seen = run(statistics=StatisticsParams(timers=False))
    _, full, full_seen = run(
        statistics=StatisticsParams(
            pricers=True, search=True, duals=True, master_values=True, columns=True
        )
    )

    assert (plain.status, plain.objective, plain.iterations) == (
        full.status,
        full.objective,
        full.iterations,
    )
    assert [info.lower_bound for info in plain_seen] == [info.lower_bound for info in full_seen]


# MARK: The log


def logged(caplog: pytest.LogCaptureFixture, frequency: int) -> list[str]:
    """The lines a run of the toy logs at `frequency`."""
    caplog.clear()

    with caplog.at_level(logging.INFO, logger="rcspp.column_generation"):
        run(log_frequency=frequency)

    return [record.getMessage() for record in caplog.records]


def test_the_log_has_every_nth_iteration_the_last_one_and_the_end(
    caplog: pytest.LogCaptureFixture,
) -> None:
    every = logged(caplog, 1)
    assert len(every) == 3
    assert every[0].startswith("iteration 1 ")
    assert every[1].startswith("iteration 2 ")
    assert every[2].startswith("Optimal after 2 iterations")

    # Only the last iteration, then the end.
    rare = logged(caplog, 5)
    assert len(rare) == 2
    assert rare[0].startswith("iteration 2 ")

    assert logged(caplog, 0) == []


def test_a_log_line_shows_the_bound_and_the_gap(caplog: pytest.LogCaptureFixture) -> None:
    first, second, _ = logged(caplog, 1)

    # 100 + 1 path * (-90 - 2) = 8, a gap of 92 (92 %).
    assert "LP value 100" in first and "bound 8 " in first and "gap 92 (92.00%)" in first
    assert "bound 10 " in second and "gap 0 (0.00%)" in second


def test_an_unknown_bound_is_logged_as_a_dash(caplog: pytest.LogCaptureFixture) -> None:
    solver = ScriptedMasterSolver()
    master = MasterProblem(solver)
    master.add_rows(1, RowSense.GreaterEqual, 1.0)
    master.add_pricer(
        ScriptedPricer(lambda duals, request: PricingResult(status=PricingStatus.SolverLimit))
    )
    params = ColumnGenerationParams(
        log_frequency=1, statistics=StatisticsParams(timers=False), max_iterations=1
    )

    with caplog.at_level(logging.INFO, logger="rcspp.column_generation"):
        result = ColumnGeneration(master, params).run()

    line, end = (record.getMessage() for record in caplog.records)
    assert result.lower_bound == -math.inf
    assert "LP -  pricing -" in line and "bound -  gap -" in line
    assert end.startswith("NotProven after 1 iterations") and end.endswith("bound -")


def test_a_run_without_an_iteration_logs_its_end(caplog: pytest.LogCaptureFixture) -> None:
    toy = Toy()

    with caplog.at_level(logging.INFO, logger="rcspp.column_generation"):
        ColumnGeneration(
            toy.master, ColumnGenerationParams(log_frequency=1, max_iterations=0)
        ).run()

    (end,) = (record.getMessage() for record in caplog.records)
    assert end.startswith("IterationLimit after 0 iterations") and "LP value -" in end


@pytest.mark.parametrize("frequency", [1.5, True, "1"])
def test_a_log_frequency_that_is_not_an_integer_is_refused(frequency: object) -> None:
    with pytest.raises(TypeError, match="log_frequency"):
        ColumnGenerationParams(log_frequency=frequency)  # type: ignore[arg-type]


def test_a_negative_log_frequency_is_refused() -> None:
    with pytest.raises(ValueError, match="log_frequency"):
        ColumnGenerationParams(log_frequency=-1)


# MARK: A graph pricer in the loop


def covering_run(statistics: StatisticsParams) -> list[IterationInfo]:
    """Two rows "cover >= 1", each covered by one path of a graph pricer; the iterations
    seen."""
    graph = ResourceGraph()
    graph.add_real_resource(
        AdditionExtensionFunction(),
        TrivialFeasibilityFunction(),
        ValueCostFunction(),
        ValueDominanceFunction(),
    )

    for node in range(4):
        graph.add_node(node, source=node == 0, sink=node == 3)

    graph.add_arc(3.0, 0, 1, cost=3.0, rows=[(0, 1.0)])
    graph.add_arc(0.0, 1, 3, cost=0.0)
    graph.add_arc(5.0, 0, 2, cost=5.0, rows=[(1, 1.0)])
    graph.add_arc(0.0, 2, 3, cost=0.0)

    highspy = pytest.importorskip("highspy")
    model = highspy.Highs()
    model.setOptionValue("output_flag", False)
    master = MasterProblem(HighspyMasterSolver(model))
    master.add_rows(2, RowSense.GreaterEqual, 1.0)
    master.add_pricer(GraphPricer(graph))
    seen: list[IterationInfo] = []
    params = ColumnGenerationParams(statistics=statistics, on_iteration=seen.append)
    result = ColumnGeneration(master, params).run()
    assert result.status == ColumnGenerationStatus.Optimal
    return seen


def test_a_graph_pricer_gives_its_stages_and_the_counts_when_asked() -> None:
    with_counts = covering_run(StatisticsParams(pricers=True, search=True))
    without = covering_run(StatisticsParams(pricers=True))

    for seen, counted in ((with_counts, True), (without, False)):
        for info in seen:
            assert info.pricers is not None
            (pricer,) = info.pricers
            (stage,) = pricer.search_details
            assert stage.stage == 0 and stage.new_columns == pricer.new_columns
            assert (stage.extended_labels is not None) == counted
            assert stage.bidirectional is None

    # The first search finds a path, so it extends labels; the last one, where no path beats
    # the threshold, may find nothing to extend once the graph is reduced.
    first = with_counts[0].pricers
    assert first is not None and first[0].new_columns > 0
    labels = first[0].search_details[0].extended_labels
    assert labels is not None and labels > 0
