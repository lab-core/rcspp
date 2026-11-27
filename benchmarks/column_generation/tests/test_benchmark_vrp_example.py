#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The benchmark's Solomon instance and the Python VRP example (examples/python) build
the same model, so that a benchmark run measures what the example runs.

Their one difference is the rows: the benchmark keeps a free row 0 so that customer i covers
row i, the example has no such row and customer i covers row i - 1.
"""

import os
import random
import sys
from collections.abc import Callable
from pathlib import Path
from typing import Any

import pytest
from column_generation_benchmark.instances import SolomonInstance

sys.path.insert(
    0,
    os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "examples", "python"
    ),
)

INSTANCES = ["C101_25", "R201_25", "RC201_12"]

# An arc as add_arc received it: the consumptions, the ends, the cost and the rows covered.
Arc = tuple[tuple[float, ...], int, int, float, list[tuple[int, float]]]


def recorded_arcs(build_graph: Callable[[], Any], monkeypatch: pytest.MonkeyPatch) -> list[Arc]:
    """Every arc ``build_graph`` adds, in order, with its rows as pairs."""
    from rcspp.graph import ResourceGraph

    arcs: list[Arc] = []
    add_arc = ResourceGraph.add_arc

    def recording(
        graph: Any, consumption: Any, origin: int, destination: int, cost: float, rows: Any
    ) -> Any:
        # The benchmark passes Row objects, the example (index, coefficient) pairs.
        pairs: list[tuple[int, float]] = [
            row if isinstance(row, tuple) else (row.index, row.coefficient) for row in rows
        ]
        arcs.append((tuple(consumption), origin, destination, cost, pairs))
        return add_arc(graph, consumption, origin, destination, cost, rows)

    with monkeypatch.context() as patch:
        patch.setattr(ResourceGraph, "add_arc", recording)
        build_graph()

    return arcs


def shifted(arcs: list[Arc]) -> list[Arc]:
    """The arcs with the example's rows numbered as the benchmark's."""
    return [
        (consumption, origin, destination, cost, [(index + 1, value) for index, value in rows])
        for consumption, origin, destination, cost, rows in arcs
    ]


@pytest.mark.parametrize("name", INSTANCES)
def test_the_example_adds_the_benchmarks_arcs(
    build: Path, name: str, monkeypatch: pytest.MonkeyPatch
) -> None:
    from vrp.graph import pricing_graph
    from vrp.instance import find_instance, read_solomon

    benchmark = SolomonInstance.from_options({"name": name})
    example = read_solomon(find_instance(name))

    expected = recorded_arcs(lambda: benchmark.build_graph({}), monkeypatch)
    found = recorded_arcs(lambda: pricing_graph(example), monkeypatch)

    assert expected
    assert shifted(found) == expected


@pytest.mark.parametrize("name", INSTANCES)
def test_the_example_prices_the_benchmarks_routes(build: Path, name: str) -> None:
    """The same duals give the same routes: the resources (the presets in the example, written
    out in the benchmark) and the sink's window act alike."""
    from vrp.graph import pricing_graph
    from vrp.instance import find_instance, read_solomon

    benchmark = SolomonInstance.from_options({"name": name})
    customers = len(benchmark.customers) - 1
    random.seed(7)
    duals = [random.uniform(0.0, 60.0) for _ in range(customers)]

    benchmark_graph = benchmark.build_graph({})
    benchmark_graph.update_reduced_costs({row + 1: dual for row, dual in enumerate(duals)})
    example_graph = pricing_graph(read_solomon(find_instance(name)))
    example_graph.update_reduced_costs(dict(enumerate(duals)))

    def routes(graph: Any) -> list[tuple[float, list[int]]]:
        return sorted((round(route.cost, 9), list(route.path_node_ids)) for route in graph.solve())

    expected = routes(benchmark_graph)

    assert expected
    assert routes(example_graph) == expected


@pytest.mark.parametrize("name", INSTANCES)
def test_the_example_starts_from_the_benchmarks_routes(build: Path, name: str) -> None:
    from vrp.column_generation import starting_routes
    from vrp.instance import find_instance, read_solomon

    expected = SolomonInstance.from_options({"name": name}).starting_columns("single-customer")
    found = starting_routes(read_solomon(find_instance(name)))

    assert [list(route.path_node_ids) for route in found] == [
        list(route.path_node_ids) for route in expected
    ]
    assert [route.column.cost for route in found] == pytest.approx(
        [route.column.cost for route in expected]
    )
    assert [[row.index + 1 for row in route.column.rows] for route in found] == [
        [row.index for row in route.column.rows] for route in expected
    ]
