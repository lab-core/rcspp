#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The Python VRP example with several vehicle types (examples/python): one pricer per
type, checked on a fleet problem small enough to list every route, then on Solomon
instances and from the command line."""

import importlib.util
import math
import os
import sys
from dataclasses import replace

import pytest

_HERE = os.path.dirname(os.path.abspath(__file__))
# rcspp from the source tree; the example's vrp package and script from examples/python.
sys.path.insert(0, os.path.join(_HERE, "..", "..", "python", "src"))
sys.path.insert(0, os.path.join(_HERE, "..", "..", "examples", "python"))

import solve_vehicle_types  # noqa: E402
from vrp.column_generation import column_generation as single_type_column_generation  # noqa: E402
from vrp.graph import pricing_graph  # noqa: E402
from vrp.instance import Customer, Instance, find_instance, read_solomon  # noqa: E402
from vrp.vehicle_types import (  # noqa: E402
    FLEETS,
    VehicleType,
    column_generation,
    integer_plan,
    parse_vehicle_type,
    route_distance,
)

from rcspp.column_generation import (  # noqa: E402
    ColumnGenerationStatus,
    ColumnTag,
    MasterProblem,
    SolverStatus,
)

# Accuracy of the values checked: within what LP solvers return.
TOLERANCE = 1e-6


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


# MARK: A truck or vans


# The depot at (0, 0) and three customers, of demands 6, 5 and 3:
#
#   customer 1 at (3, 4), 5 from the depot
#   customer 2 at (3, -4), 5 from the depot, 8 from customer 1
#   customer 3 at (6, 0), 6 from the depot, 5 from customers 1 and 2
#
# Route lengths: one customer 10, 10 or 12; {1, 2} 18; {1, 3} and {2, 3} 16; all three 20.
def three_customers() -> Instance:
    """Three customers small enough to list every route.

    No service time, and customer 3 is due by 12: a route that came back to it (depot,
    3, 2, 3 arrives at 16) is late, so every route visits each customer at most once,
    and the routes listed are all of them.
    """
    depot = Customer(0, 0.0, 0.0, 0.0, 0.0, 1000.0, 0.0)
    return Instance(
        "three customers",
        3,
        11.0,
        [
            depot,
            Customer(1, 3.0, 4.0, 6.0, 0.0, 1000.0, 0.0),
            Customer(2, 3.0, -4.0, 5.0, 0.0, 1000.0, 0.0),
            Customer(3, 6.0, 0.0, 3.0, 0.0, 12.0, 0.0),
        ],
    )


# The truck carries 11, every pair but no three customers (14): {1} 20, {2} 20, {3} 22,
# {1, 2} 28, {1, 3} 26, {2, 3} 26. The van carries 9, not the pair {1, 2} (11), and costs 5
# less per route: {1} 15, {2} 15, {3} 17, {1, 3} 21, {2, 3} 21.
TRUCK = VehicleType("truck", capacity=11.0, fixed_cost=10.0)
VAN = VehicleType("van", capacity=9.0, fixed_cost=5.0)
BIKE = VehicleType("bike", capacity=6.0, fixed_cost=2.0)

# Each case: the fleet, the LP, the best plan, and the types it uses (every best plan, when
# there are several). The values come from every route above, listed by hand.
CASES = {
    # The LP takes half of truck {1, 2}, van {1, 3} and van {2, 3}; two vans do better.
    "truck and van": ([TRUCK, VAN], 35.0, 36.0, {"van"}),
    "trucks only": ([TRUCK], 40.0, 46.0, {"truck"}),
    "vans only": ([VAN], 36.0, 36.0, {"van"}),
    # One van serves at most two customers: a truck serves the third, or a pair.
    "at most one van": ([TRUCK, replace(VAN, max_vehicles=1)], 35.0, 41.0, {"truck", "van"}),
    "at most one of each": (
        [replace(TRUCK, max_vehicles=1), replace(VAN, max_vehicles=1)],
        35.0,
        41.0,
        {"truck", "van"},
    ),
    # A van that drives at most 12 serves one customer: the LP is the trucks' again.
    "short van routes": ([TRUCK, replace(VAN, max_distance=12.0)], 40.0, 41.0, {"truck", "van"}),
    # The truck at 1.2 per distance: {1} 22, {2} 22, {3} 24.4, {1, 2} 31.6, {1, 3} and
    # {2, 3} 29.2.
    "dearer truck, at most one van": (
        [replace(TRUCK, cost_per_distance=1.2), replace(VAN, max_vehicles=1)],
        36.8,
        43.0,
        {"truck", "van"},
    ),
    # The bike carries one customer: {1} 12, {2} 12, {3} 14. With a van: 33.
    "truck, van and bike": ([TRUCK, VAN, BIKE], 33.0, 33.0, {"van", "bike"}),
}


def check_routes(
    instance: Instance,
    vehicle_types: list[VehicleType],
    problem: MasterProblem,
    elementary: bool = True,
) -> None:
    """Every route generated fits its own type's capacity and length, and costs what its
    type charges; with ``elementary``, it also visits each customer at most once."""
    for column in problem.columns:
        if column.tag != ColumnTag.Real:
            continue

        vehicle_type = vehicle_types[column.pricer]
        customers = list(column.solution.path_node_ids)[1:-1]
        length = route_distance(instance, customers)

        if elementary:
            assert len(set(customers)) == len(customers)

        assert (
            sum(instance.customers[number].demand for number in customers) <= vehicle_type.capacity
        )
        assert length <= vehicle_type.max_distance + TOLERANCE
        assert column.solution.column.cost == pytest.approx(vehicle_type.route_cost(length))


@pytest.mark.parametrize("master", MASTERS)
@pytest.mark.parametrize("case", CASES)
def test_a_fleet_of_three_customers(master: str, case: str) -> None:
    vehicle_types, lp, best_plan, used_types = CASES[case]
    instance = three_customers()
    problem, loop = column_generation(instance, vehicle_types, master)
    result = loop.run()

    assert result.status == ColumnGenerationStatus.Optimal
    assert result.objective == pytest.approx(lp, abs=TOLERANCE)
    check_routes(instance, vehicle_types, problem)

    # Over the routes generated, the best plan: here they hold one.
    status, routes = integer_plan(loop, problem, instance, vehicle_types)

    assert status == SolverStatus.Optimal
    assert sum(route.cost for route in routes) == pytest.approx(best_plan, abs=TOLERANCE)
    assert {route.vehicle_type.name for route in routes} == used_types
    assert sorted(customer for route in routes for customer in route.customers) == [1, 2, 3]


@pytest.mark.parametrize("master", MASTERS)
def test_one_truck_cannot_serve_every_customer(master: str) -> None:
    # The three customers weigh 14, more than the truck carries.
    vehicle_types = [replace(TRUCK, max_vehicles=1)]
    _, loop = column_generation(three_customers(), vehicle_types, master)

    assert loop.run().status == ColumnGenerationStatus.Infeasible


def test_each_capacity_changes_the_lp() -> None:
    pytest.importorskip("highspy")
    instance = three_customers()

    def lp(vehicle_types: list[VehicleType]) -> float:
        return column_generation(instance, vehicle_types, "highspy")[1].run().objective

    # With room for all three customers, one route serves them: 25 by van, 30 by truck.
    assert lp([TRUCK, replace(VAN, capacity=15.0)]) == pytest.approx(25.0, abs=TOLERANCE)
    assert lp([replace(TRUCK, capacity=15.0), VAN]) == pytest.approx(30.0, abs=TOLERANCE)


# MARK: Solomon instances


@pytest.mark.parametrize("master", MASTERS)
def test_one_vehicle_type_is_the_plain_vrp(master: str) -> None:
    instance = read_solomon(find_instance("C101_25"))
    vehicle_types = [parse_vehicle_type(text, instance.capacity) for text in FLEETS["single"]]
    result = column_generation(instance, vehicle_types, master)[1].run()
    plain = single_type_column_generation(instance, pricing_graph(instance), master)[1].run()

    assert result.objective == pytest.approx(plain.objective, rel=1e-9)
    assert result.objective == pytest.approx(191.8136197786562, rel=1e-6)


@pytest.mark.parametrize("fleet", ["truck-and-van", "three-sizes"])
def test_every_route_fits_its_type_on_c101_25(fleet: str) -> None:
    pytest.importorskip("highspy")
    instance = read_solomon(find_instance("C101_25"))
    vehicle_types = [parse_vehicle_type(text, instance.capacity) for text in FLEETS[fleet]]
    problem, loop = column_generation(instance, vehicle_types, "highspy")

    assert loop.run().status == ColumnGenerationStatus.Optimal

    # Solomon's time windows do not keep a route from visiting a customer twice.
    check_routes(instance, vehicle_types, problem, elementary=False)

    status, routes = integer_plan(loop, problem, instance, vehicle_types)

    assert status == SolverStatus.Optimal
    assert sorted(customer for route in routes for customer in route.customers) == list(
        range(1, 26)
    )


def test_a_limited_type_starts_from_artificial_columns() -> None:
    pytest.importorskip("highspy")
    instance = read_solomon(find_instance("C101_25"))
    vehicle_types = [parse_vehicle_type("truck max=3", instance.capacity)]
    result = column_generation(instance, vehicle_types, "highspy")[1].run()

    # Three trucks are enough for C101_25: the plain VRP's LP uses fewer.
    assert result.status == ColumnGenerationStatus.Optimal
    assert result.objective == pytest.approx(191.8136197786562, rel=1e-6)


# MARK: Vehicle types from text


def test_a_vehicle_type_with_every_setting() -> None:
    text = "van capacity=50% fixed=40 per-distance=0.9 max-distance=150 max=3"

    assert parse_vehicle_type(text, 200.0) == VehicleType("van", 100.0, 40.0, 0.9, 150.0, 3)


def test_a_vehicle_type_by_name_only_has_the_defaults() -> None:
    assert parse_vehicle_type("vehicle", 200.0) == VehicleType("vehicle", 200.0)
    assert parse_vehicle_type("truck capacity=150", 200.0).capacity == 150.0


@pytest.mark.parametrize(
    ("text", "message"),
    [
        ("capacity=10", "its name comes first"),
        ("van speed=3", "unknown setting 'speed'"),
        ("van fixed=10 fixed=20", "fixed given twice"),
        ("van fixed=ten", "fixed='ten' is not a number"),
        ("van max=2.5", "max='2.5' is not a whole number"),
        ("van max=many", "max='many' is not a number"),
        ("van capacity=half%", "is not a number"),
    ],
)
def test_a_wrong_vehicle_type_says_why(text: str, message: str) -> None:
    with pytest.raises(ValueError, match=message):
        parse_vehicle_type(text, 200.0)


def test_every_fleet_reads() -> None:
    for texts in FLEETS.values():
        assert [parse_vehicle_type(text, 200.0) for text in texts]


def test_no_vehicle_type() -> None:
    with pytest.raises(ValueError, match="no vehicle type"):
        column_generation(three_customers(), [], "highspy")


# MARK: Command line


def test_command_line_default_fleet(capsys: pytest.CaptureFixture[str]) -> None:
    pytest.importorskip("highspy")
    solve_vehicle_types.main(["C101_25", "--master", "highspy"])
    output = capsys.readouterr().out

    assert "truck: capacity 200, fixed cost 100, 1.2 per distance" in output
    assert "van: capacity 60, fixed cost 20, 1 per distance" in output
    assert "Integer solve: Optimal" in output
    assert "Fleet: " in output


def test_command_line_vehicle_types(capsys: pytest.CaptureFixture[str]) -> None:
    pytest.importorskip("highspy")
    solve_vehicle_types.main(
        [
            "C101_25",
            "--master",
            "highspy",
            "--no-integer",
            "--vehicle-type",
            "truck max=2",
            "--vehicle-type",
            "van capacity=50% fixed=5 max-distance=150",
        ]
    )
    output = capsys.readouterr().out

    assert "truck: capacity 200, fixed cost 0, 1 per distance, at most 2" in output
    assert "van: capacity 100, fixed cost 5, 1 per distance, routes up to 150 long" in output
    assert "Optimal: LP" in output
    assert "Integer solve" not in output


def test_command_line_refuses_a_fleet_and_vehicle_types(capsys: pytest.CaptureFixture[str]) -> None:
    with pytest.raises(SystemExit):
        solve_vehicle_types.main(
            ["C101_5", "--master", "highspy", "--fleet", "single", "--vehicle-type", "van"]
        )

    assert "not both" in capsys.readouterr().err


def test_command_line_refuses_a_wrong_vehicle_type() -> None:
    with pytest.raises(SystemExit, match="unknown setting 'speed'"):
        solve_vehicle_types.main(["C101_5", "--master", "highspy", "--vehicle-type", "van speed=3"])


def test_capacity_share_uses_the_instance_capacity() -> None:
    assert parse_vehicle_type("van capacity=25%", 1000.0).capacity == pytest.approx(250.0)
    assert math.isinf(parse_vehicle_type("van", 1000.0).max_distance)
