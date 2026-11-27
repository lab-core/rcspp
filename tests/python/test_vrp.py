#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The Python VRP example (examples/python): its instance reader, its pricing graph, its
column generation over each master and its command line."""

import importlib.util
import os
import random
import sys
from pathlib import Path

import pytest

_HERE = os.path.dirname(os.path.abspath(__file__))
# rcspp from the source tree; the example's vrp package and script from examples/python.
sys.path.insert(0, os.path.join(_HERE, "..", "..", "python", "src"))
sys.path.insert(0, os.path.join(_HERE, "..", "..", "examples", "python"))

import solve_vrp  # noqa: E402
from vrp.column_generation import (  # noqa: E402
    column_generation,
    integer_routes,
    master_solver,
    starting_routes,
)
from vrp.graph import pricing_graph, sink  # noqa: E402
from vrp.instance import Customer, Instance, find_instance, read_solomon  # noqa: E402

from rcspp._core.graph import Solution  # noqa: E402
from rcspp.column_generation import (  # noqa: E402
    ColumnGenerationStatus,
    IterationInfo,
    SolverStatus,
)


def has_gurobi_license() -> bool:
    """Whether gurobipy is installed and Gurobi can build a model on this machine."""
    if importlib.util.find_spec("gurobipy") is None:
        return False

    import gurobipy

    try:
        gurobipy.Model().dispose()
    except gurobipy.GurobiError:
        return False

    return True


MASTERS = [
    pytest.param(
        "highspy",
        marks=pytest.mark.skipif(
            importlib.util.find_spec("highspy") is None, reason="highspy not installed"
        ),
    ),
    pytest.param(
        "gurobipy", marks=pytest.mark.skipif(not has_gurobi_license(), reason="no Gurobi licence")
    ),
]


def customer(
    number: int, x: float, y: float, due_time: float = 500.0, demand: float = 10.0
) -> Customer:
    return Customer(number, x, y, demand, 0.0, due_time, 10.0)


def tiny_instance(capacity: float = 50.0) -> Instance:
    """The depot at (0, 0) and three customers around it, all in one vehicle's
    capacity."""
    depot = Customer(0, 0.0, 0.0, 0.0, 0.0, 1000.0, 0.0)
    return Instance(
        "tiny",
        3,
        capacity,
        [depot, customer(1, 1.0, 0.0), customer(2, 0.0, 1.0), customer(3, 1.0, 1.0)],
    )


def customers_of(route: Solution, instance: Instance) -> list[int]:
    return [node for node in route.path_node_ids if node not in (0, sink(instance))]


# MARK: Instance


def test_read_solomon() -> None:
    instance = read_solomon(find_instance("C101_25"))

    assert instance.name == "C101_25"
    assert instance.vehicles == 25
    assert instance.capacity == 200
    assert len(instance.customers) == 26
    assert instance.depot == Customer(0, 40.0, 50.0, 0.0, 0.0, 1236.0, 0.0)
    assert instance.customers[1] == Customer(1, 45.0, 68.0, 10.0, 912.0, 967.0, 90.0)


def test_find_instance_by_name_or_path() -> None:
    found = find_instance("C101_5")

    assert found.name == "C101_5.txt"
    assert find_instance("C101_5.txt") == found
    assert find_instance(str(found)) == found

    with pytest.raises(FileNotFoundError):
        find_instance("no_such_instance")


def test_read_solomon_refuses_a_file_without_customers(tmp_path: Path) -> None:
    path = tmp_path / "empty.txt"
    path.write_text("EMPTY\n\nVEHICLE\nNUMBER CAPACITY\n 3 50\n")

    with pytest.raises(ValueError):
        read_solomon(path)


# MARK: Graph


def test_graph_shape() -> None:
    instance = tiny_instance()
    graph = pricing_graph(instance)

    # The depot, three customers and the sink; the depot to each customer, each customer to the
    # two others and to the sink (add_arc refuses an arc into the source or out of the sink).
    assert graph.number_of_nodes() == 5
    assert graph.number_of_arcs() == 3 + 3 * 3


def test_subproblem_with_zero_duals() -> None:
    instance = tiny_instance()
    result = pricing_graph(instance).solve()

    assert len(result) >= 1

    for route in result.solutions:
        assert route.path_node_ids[0] == 0
        assert route.path_node_ids[-1] == sink(instance)
        assert route.cost >= 0.0


def test_duals_lower_the_reduced_costs() -> None:
    instance = tiny_instance()
    graph = pricing_graph(instance)
    best = graph.solve().solutions[0].cost

    # One dual per master row, row i - 1 for customer i.
    random.seed(25)
    graph.update_reduced_costs({row: random.uniform(1, 100) for row in range(3)})

    assert graph.solve().solutions[0].cost <= best - 1e-9


def test_time_windows() -> None:
    """A customer that cannot be reached before its due time is on no route."""
    depot = Customer(0, 0.0, 0.0, 0.0, 0.0, 1000.0, 0.0)
    late = customer(2, 5.0, 0.0, due_time=0.0)
    instance = Instance("windows", 1, 100.0, [depot, customer(1, 1.0, 0.0), late])

    for route in pricing_graph(instance).solve().solutions:
        assert 2 not in route.path_node_ids


def test_capacity() -> None:
    """With room for one customer, no route visits two."""
    instance = tiny_instance(capacity=15.0)
    routes = pricing_graph(instance).solve().solutions

    assert routes

    for route in routes:
        assert len(customers_of(route, instance)) <= 1


def test_subproblem_on_c101_5() -> None:
    instance = read_solomon(find_instance("C101_5"))
    routes = pricing_graph(instance).solve().solutions

    assert routes

    for route in routes:
        assert route.path_node_ids[0] == 0
        assert route.path_node_ids[-1] == sink(instance)


# MARK: Column generation


def test_starting_routes_cover_one_customer_each() -> None:
    instance = tiny_instance()
    routes = starting_routes(instance)

    assert [list(route.path_node_ids) for route in routes] == [[0, 1, 4], [0, 2, 4], [0, 3, 4]]
    assert [[row.index for row in route.column.rows] for route in routes] == [[0], [1], [2]]
    assert routes[0].column.cost == pytest.approx(2.0)


def test_unknown_master() -> None:
    with pytest.raises(ValueError, match="unknown master"):
        master_solver("cplex", 3)


@pytest.mark.parametrize("master", MASTERS)
def test_column_generation_on_the_tiny_instance(master: str) -> None:
    instance = tiny_instance()
    problem, loop = column_generation(instance, pricing_graph(instance), master)
    result = loop.run()

    assert result.status == ColumnGenerationStatus.Optimal

    status, routes = integer_routes(loop, problem)

    assert status == SolverStatus.Optimal
    assert sorted(customer for route in routes for customer in route.customers) == [1, 2, 3]
    assert problem.solver.objective() == pytest.approx(sum(route.distance for route in routes))


@pytest.mark.parametrize("master", MASTERS)
def test_column_generation_on_c101_25(master: str) -> None:
    instance = read_solomon(find_instance("C101_25"))
    iterations: list[IterationInfo] = []
    problem, loop = column_generation(
        instance, pricing_graph(instance), master, on_iteration=iterations.append
    )
    result = loop.run()

    assert result.status == ColumnGenerationStatus.Optimal
    assert result.objective == pytest.approx(191.8136197786562, rel=1e-6)
    assert len(iterations) == result.iterations

    # Every customer on exactly one route of the integer plan.
    status, routes = integer_routes(loop, problem)

    assert status == SolverStatus.Optimal
    assert sorted(customer for route in routes for customer in route.customers) == list(
        range(1, 26)
    )


def test_max_solutions_caps_each_pricing_call() -> None:
    pytest.importorskip("highspy")
    instance = read_solomon(find_instance("C101_25"))
    new_columns: list[int] = []
    _, loop = column_generation(
        instance,
        pricing_graph(instance),
        "highspy",
        max_solutions=5,
        on_iteration=lambda info: new_columns.append(info.new_columns),
    )
    result = loop.run()

    assert result.status == ColumnGenerationStatus.Optimal
    assert result.objective == pytest.approx(191.8136197786562, rel=1e-6)
    assert max(new_columns) <= 5


# MARK: Command line


def test_command_line_subproblem_only(capsys: pytest.CaptureFixture[str]) -> None:
    solve_vrp.main(["C101_5", "--subproblem-only"])
    output = capsys.readouterr().out

    assert "C101_5: 5 customers" in output
    assert "Routes found: " in output


def test_command_line_full_run(capsys: pytest.CaptureFixture[str]) -> None:
    pytest.importorskip("highspy")
    solve_vrp.main(["C101_25", "--master", "highspy"])
    output = capsys.readouterr().out

    assert "iteration    1" in output
    assert "Optimal: LP 191.8136" in output
    assert "Integer solve: Optimal" in output


def test_command_line_without_integer_solve(capsys: pytest.CaptureFixture[str]) -> None:
    pytest.importorskip("highspy")
    solve_vrp.main(["C101_5", "--master", "highspy", "--no-integer"])

    assert "Integer solve" not in capsys.readouterr().out


def test_command_line_needs_a_master(capsys: pytest.CaptureFixture[str]) -> None:
    with pytest.raises(SystemExit):
        solve_vrp.main(["C101_5"])

    assert "--master is required" in capsys.readouterr().err
