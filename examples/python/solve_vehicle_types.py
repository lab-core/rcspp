#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Solves a vehicle routing problem with time windows and a fleet of several vehicle types:
column generation with one pricer per type, then an integer plan that says which vehicles
to use and their routes.

Run from the repository's root, after building rcspp (see ``examples/python/vrp/README.md``)::

    python examples/python/solve_vehicle_types.py C101_25 --master highspy
    python examples/python/solve_vehicle_types.py C101_25 --master highspy --fleet three-sizes
    python examples/python/solve_vehicle_types.py R101_25 --master gurobipy \\
        --vehicle-type "truck capacity=100% fixed=100 per-distance=1.2" \\
        --vehicle-type "van capacity=50% fixed=40 max=4"
"""

from __future__ import annotations

import argparse
import math
import os
import sys
import time
from collections import Counter

# The vrp package is next to this script; rcspp is the installed one (or a build's, through
# PYTHONPATH: see the README).
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from vrp.column_generation import MASTERS  # noqa: E402
from vrp.instance import find_instance, read_solomon  # noqa: E402
from vrp.vehicle_types import (  # noqa: E402
    DEFAULT_FLEET,
    FLEETS,
    SETTINGS,
    column_generation,
    describe,
    integer_plan,
    parse_vehicle_type,
)

from rcspp.column_generation import IterationInfo  # noqa: E402


def parse_arguments(arguments: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Column generation for a vehicle routing problem with several vehicle types."
    )
    parser.add_argument("instance", help="a Solomon file, or its name in instances/ (C101_25)")
    parser.add_argument("--master", choices=MASTERS, required=True, help="the master's LP solver")
    parser.add_argument(
        "--vehicle-type",
        action="append",
        default=[],
        metavar="TEXT",
        help=(
            'a vehicle type, e.g. "van capacity=50%% fixed=40 max=3"; repeat it for each '
            f"type. Settings: {', '.join(SETTINGS)}"
        ),
    )
    parser.add_argument(
        "--fleet",
        choices=FLEETS,
        help=f"a ready-made fleet (default: {DEFAULT_FLEET}, when no --vehicle-type is given)",
    )
    parser.add_argument(
        "--max-solutions",
        type=int,
        metavar="N",
        help="the most routes each pricing call returns (default: all)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=math.inf,
        metavar="S",
        help="column generation's time limit",
    )
    parser.add_argument(
        "--integer-timeout",
        type=float,
        default=60.0,
        metavar="S",
        help="the integer solve's time limit",
    )
    parser.add_argument(
        "--integer",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="solve the master with integer routes after the LP (default: yes)",
    )
    options = parser.parse_args(arguments)

    if options.fleet is not None and options.vehicle_type:
        parser.error("give --fleet or --vehicle-type, not both")

    return options


def main(arguments: list[str] | None = None) -> None:
    options = parse_arguments(arguments)
    instance = read_solomon(find_instance(options.instance))
    texts = options.vehicle_type or FLEETS[options.fleet or DEFAULT_FLEET]

    try:
        vehicle_types = [parse_vehicle_type(text, instance.capacity) for text in texts]
    except ValueError as error:
        raise SystemExit(f"error: {error}") from None

    print(f"{instance.name}: {len(instance.customers) - 1} customers")

    for vehicle_type in vehicle_types:
        print(f"  {describe(vehicle_type)}")

    # Column generation, one line per iteration.
    def print_iteration(info: IterationInfo) -> None:
        print(
            f"  iteration {info.iteration:4d}  LP {info.objective:14.4f}  "
            f"+{info.new_columns:5d} routes  {info.elapsed_seconds:8.2f} s"
        )

    problem, loop = column_generation(
        instance,
        vehicle_types,
        options.master,
        options.max_solutions,
        options.timeout,
        print_iteration,
    )
    started = time.perf_counter()
    result = loop.run()
    seconds = time.perf_counter() - started
    statistics = result.statistics
    print(f"{result.status.name}: LP {result.objective:.4f}, lower bound {result.lower_bound:.4f}")
    print(
        f"{result.iterations} iterations, {statistics.generated_columns} routes generated, "
        f"{seconds:.2f} s (master {statistics.relaxation_seconds:.2f} s, "
        f"pricing {statistics.pricing_seconds:.2f} s)"
    )

    if not options.integer:
        return

    # An integer plan over the routes generated: the vehicles used and their routes.
    started = time.perf_counter()
    status, routes = integer_plan(loop, problem, instance, vehicle_types, options.integer_timeout)
    seconds = time.perf_counter() - started

    if not routes:
        print(f"Integer solve: {status.name}, no solution ({seconds:.2f} s)")
        return

    total = sum(route.cost for route in routes)
    print(f"Integer solve: {status.name}, cost {total:.4f} ({seconds:.2f} s)")
    used = Counter(route.vehicle_type for route in routes)
    print("Fleet: " + ", ".join(f"{used[kind]} {kind.name}" for kind in vehicle_types))

    for route in routes:
        print(
            f"  {route.vehicle_type.name:>10}  cost {route.cost:10.4f}  "
            f"distance {route.distance:9.4f}  load {route.load:6g}  {route.customers}"
        )


if __name__ == "__main__":
    main()
