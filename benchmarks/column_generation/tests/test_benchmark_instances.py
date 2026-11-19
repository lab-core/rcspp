#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The instances: a toy Solomon file's graph, master rows and starting columns."""

import math
import re
from pathlib import Path
from typing import Any

import pytest
from column_generation_benchmark import instances
from column_generation_benchmark.instances import InstanceError, SolomonInstance

# A depot and three customers: 1 and 2 on the axes, 3 too late to share a route with 1.
TOY = """TOY

VEHICLE
NUMBER     CAPACITY
  3         20

CUSTOMER
CUST NO.  XCOORD.   YCOORD.    DEMAND   READY TIME  DUE DATE   SERVICE   TIME

    0      0          0          0          0       1000         0
    1      3          4         10          0        100        10
    2      0          5         10          0        100        10
    3      6          8         15        500        600        10
"""


@pytest.fixture
def toy(tmp_path: Path) -> SolomonInstance:
    path = tmp_path / "TOY_3.txt"
    path.write_text(TOY)
    return SolomonInstance(path)


def arcs(graph: Any) -> list[tuple[int, int, float, list[tuple[int, float]]]]:
    """Every arc of `graph`: origin, destination, cost and the rows it covers."""
    found: list[tuple[int, int, float, list[tuple[int, float]]]] = []

    while (arc := graph.get_arc(len(found))) is not None:
        rows = [(row.index, row.coefficient) for row in arc.rows]
        found.append((arc.origin().id, arc.destination().id, arc.cost, rows))

    return found


# MARK: Reading


def test_a_solomon_file_is_read(toy: SolomonInstance):
    assert toy.name == "TOY_3"
    assert (toy.vehicles, toy.capacity) == (3, 20.0)
    assert [customer.number for customer in toy.customers] == [0, 1, 2, 3]
    assert toy.customers[3].ready_time == 500.0
    assert toy.sink == 4


def test_an_instance_by_name_comes_from_the_tree(build: Path):
    instance = instances.make_instance({"type": "solomon", "name": "C101_25"})

    assert instance.name == "C101_25"
    assert len(instance.row_bounds()[0]) == 26


@pytest.mark.parametrize(
    ("text", "message"),
    [
        ("NAME\n", "no vehicles or no customers"),
        (TOY.replace("    2      0", "    7      0"), "numbered 0, 1, 2"),
    ],
)
def test_a_wrong_file_is_refused(tmp_path: Path, text: str, message: str):
    path = tmp_path / "WRONG.txt"
    path.write_text(text)

    with pytest.raises(InstanceError, match=message):
        SolomonInstance(path)


def test_a_missing_file_is_refused(tmp_path: Path):
    with pytest.raises(InstanceError, match="MISSING"):
        SolomonInstance(tmp_path / "MISSING.txt")


# MARK: Graph and rows


def test_the_graph_has_every_route_arc_but_the_empty_route(build: Path, toy: SolomonInstance):
    found = arcs(toy.build_graph({}))
    ends = [(origin, destination) for origin, destination, _, _ in found]

    assert ends == [
        (0, 1),
        (0, 2),
        (0, 3),
        (1, 2),
        (1, 3),
        (1, 4),
        (2, 1),
        (2, 3),
        (2, 4),
        (3, 1),
        (3, 2),
        (3, 4),
    ]
    by_ends = {(origin, destination): (cost, rows) for origin, destination, cost, rows in found}
    assert by_ends[(0, 1)] == (5.0, [])
    assert by_ends[(1, 4)] == (5.0, [(1, 1.0)])
    assert by_ends[(3, 2)] == (pytest.approx(math.hypot(6, 3)), [(3, 1.0)])


def test_the_graph_prices_routes_within_windows_and_capacity(build: Path, toy: SolomonInstance):
    graph = toy.build_graph({})
    graph.update_reduced_costs({1: 100.0, 2: 100.0, 3: 100.0})
    best = graph.solve()[0]

    # 1 and 2 share a route (load 20); 3 opens at 500, after 1's and 2's windows close.
    assert best.path_node_ids in ([0, 1, 2, 4], [0, 2, 1, 4])
    assert best.cost == pytest.approx(5 + math.hypot(3, 1) + 5 - 200)


def test_row_zero_is_free_and_every_customer_is_covered_once(toy: SolomonInstance):
    lower, upper = toy.row_bounds()

    assert lower == [-math.inf, 1.0, 1.0, 1.0]
    assert upper == [math.inf, 1.0, 1.0, 1.0]


# MARK: Starting columns


def test_single_customer_routes_cover_each_customer(build: Path, toy: SolomonInstance):
    routes = toy.starting_columns("single-customer")

    assert [route.path_node_ids for route in routes] == [[0, 1, 4], [0, 2, 4], [0, 3, 4]]
    assert [route.column.cost for route in routes] == [10.0, 10.0, 20.0]
    assert [[(row.index, row.coefficient) for row in route.column.rows] for route in routes] == [
        [(1, 1.0)],
        [(2, 1.0)],
        [(3, 1.0)],
    ]


def test_an_instance_type_declares_its_starts():
    assert SolomonInstance.STARTS == ("single-customer",)


def test_an_unknown_start_is_refused(toy: SolomonInstance):
    with pytest.raises(InstanceError, match="no 'artificial' start"):
        toy.starting_columns("artificial")


# MARK: Resolved solutions


def column(path: list[int], cost: float, value: float = 1.0) -> dict[str, Any]:
    """A column as solution.json lists it."""
    return {"id": 0, "value": value, "cost": cost, "path": path, "rows": []}


def toy_columns(toy: SolomonInstance) -> list[dict[str, Any]]:
    """Two good routes of the toy, 1 then 2, and 3 alone, as a solver gives them."""
    depot, one, two, three = toy.customers
    first = sum(
        instances.distance_between(*pair) for pair in [(depot, one), (one, two), (two, depot)]
    )
    second = 2 * instances.distance_between(depot, three)
    return [column([0, 1, 2, 4], first, 1.0000000000000002), column([0, 3, 4], second)]


def test_solomon_runs_solve_the_integer_master_with_binary_columns():
    assert instances.Instance.INTEGER == {"solve": False, "column_type": "integer"}
    assert SolomonInstance.INTEGER == {"solve": True, "column_type": "binary"}


def test_a_good_solution_resolves_into_routes_and_passes_its_check(toy: SolomonInstance):
    columns = toy_columns(toy)
    solution = toy.resolve_solution(columns)
    distance = columns[0]["cost"] + columns[1]["cost"]

    assert solution == {"routes": [[1, 2], [3]], "vehicles": 2, "distance": distance}
    assert toy.check_solution(solution, distance) == []


@pytest.mark.parametrize(
    ("routes", "message"),
    [
        ([[1, 2], [3], [2]], "customer 2 is served 2 times"),
        ([[1, 2]], "customer 3 is served 0 times"),
        ([[1, 2], [3], []], "route 2 is empty"),
        ([[1, 2], [3, 7]], r"route 1 visits \[7\], not customers"),
        # 3 is ready at 500: 1 is long past its due time then.
        ([[3, 1], [2]], "route 0 reaches customer 1 at .*, after its due time 100"),
        # 10 + 10 + 15 over a capacity of 20.
        ([[1, 2, 3]], "route 0 carries 35, over the capacity 20"),
    ],
)
def test_a_broken_solution_fails_its_check(
    toy: SolomonInstance, routes: list[list[int]], message: str
):
    errors = toy.check_solution({"routes": routes, "vehicles": len(routes), "distance": 0.0}, 0.0)

    assert any(re.fullmatch(message, error) for error in errors), errors


def test_a_route_back_after_the_depot_closes_fails_its_check(tmp_path: Path):
    # The depot closes at 15: customer 1 is 5 away and takes 10.
    path = tmp_path / "EARLY_3.txt"
    path.write_text(TOY.replace("0       1000", "0         15"))
    early = SolomonInstance(path)
    solution = early.resolve_solution([column([0, 1, 4], 10.0)])

    assert "route 0 is back at the depot at 20, after its due time 15" in early.check_solution(
        solution, 10.0
    )


def test_the_graph_prices_no_route_back_after_the_depot_closes(build: Path, tmp_path: Path):
    # The check refuses such a route: pricing must not make it.
    path = tmp_path / "EARLY_3.txt"
    path.write_text(TOY.replace("0       1000", "0         15"))
    graph = SolomonInstance(path).build_graph({})
    graph.update_reduced_costs({1: 100.0, 2: 100.0, 3: 100.0})

    assert [solution.path_node_ids for solution in graph.solve()] == []


def test_a_cost_that_is_not_the_distance_fails_its_check(toy: SolomonInstance):
    solution = toy.resolve_solution(toy_columns(toy))
    distance = solution["distance"]
    errors = toy.check_solution({**solution, "distance": distance + 1}, distance + 2)

    assert len(errors) == 2
    assert errors[0].startswith(f"the solution's distance {distance + 1!r} is not")
    assert errors[1].startswith(f"the objective {distance + 2!r} is not")
