#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""Solves the LP relaxation of a vehicle routing problem with time windows by column
generation, then an integer plan over the routes it generated.

Run from the repository's root, after building rcspp (see ``examples/python/vrp/README.md``)::

    python examples/python/solve_vrp.py instances/C101_25.txt --master highspy
    python examples/python/solve_vrp.py C101_25 --master gurobipy --no-integer
    python examples/python/solve_vrp.py C101_25 --subproblem-only
"""

from __future__ import annotations

import argparse
import math
import os
import sys
import time

# The vrp package is next to this script; rcspp is the installed one (or a build's, through
# PYTHONPATH: see the README).
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from vrp.column_generation import MASTERS, column_generation, integer_routes  # noqa: E402
from vrp.graph import pricing_graph  # noqa: E402
from vrp.instance import find_instance, read_solomon  # noqa: E402

from rcspp.column_generation import IterationInfo  # noqa: E402


def parse_arguments(arguments: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Column generation for a vehicle routing problem with time windows."
    )
    parser.add_argument("instance", help="a Solomon file, or its name in instances/ (C101_25)")
    parser.add_argument(
        "--master",
        choices=MASTERS,
        help="the master's LP solver (required, unless --subproblem-only)",
    )
    parser.add_argument(
        "--max-solutions",
        type=int,
        metavar="N",
        help="the most routes a pricing call returns (default: all)",
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
    parser.add_argument(
        "--subproblem-only",
        action="store_true",
        help="price once with zero duals and print the routes found: no master, no LP solver",
    )
    options = parser.parse_args(arguments)

    if options.master is None and not options.subproblem_only:
        parser.error("--master is required: highspy or gurobipy")

    return options


def main(arguments: list[str] | None = None) -> None:
    options = parse_arguments(arguments)
    instance = read_solomon(find_instance(options.instance))
    graph = pricing_graph(instance)
    print(
        f"{instance.name}: {len(instance.customers) - 1} customers, "
        f"capacity {instance.capacity:g}, "
        f"{graph.number_of_nodes()} nodes, {graph.number_of_arcs()} arcs"
    )

    # Pricing alone: with zero duals, the reduced cost of a route is its distance.
    if options.subproblem_only:
        result = graph.solve()
        print(f"Routes found: {len(result)} (search {result.status_string()})")

        for route in result.solutions:
            print(f"  {route.cost:10.4f}  {list(route.path_node_ids)}")

        return

    # Column generation, one line per iteration.
    def print_iteration(info: IterationInfo) -> None:
        print(
            f"  iteration {info.iteration:4d}  LP {info.objective:14.4f}  "
            f"+{info.new_columns:5d} routes  {info.elapsed_seconds:8.2f} s"
        )

    problem, loop = column_generation(
        instance, graph, options.master, options.max_solutions, options.timeout, print_iteration
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

    # An integer plan over the routes generated.
    started = time.perf_counter()
    status, routes = integer_routes(loop, problem, options.integer_timeout)
    seconds = time.perf_counter() - started

    if not routes:
        print(f"Integer solve: {status.name}, no solution ({seconds:.2f} s)")
        return

    total = sum(route.distance for route in routes)
    print(
        f"Integer solve: {status.name}, {len(routes)} routes, distance {total:.4f} "
        f"({seconds:.2f} s)"
    )

    for route in routes:
        print(f"  {route.distance:10.4f}  {route.customers}")


if __name__ == "__main__":
    main()
