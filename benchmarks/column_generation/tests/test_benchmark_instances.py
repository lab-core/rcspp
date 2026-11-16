#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The instances: a toy Solomon file's graph, master rows and starting columns."""

import math
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
