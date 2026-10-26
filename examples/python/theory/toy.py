"""The toy instance of the theory pages, and the helpers every page reuses.

A depot and three customers (page 1), optionally with the time windows of page 2. The depot is
split into a source ``0`` and a sink ``SINK`` (written 0′ on the pages). Each master LP is solved
with HiGHS through SciPy, the solver whose dual values the pages quote.
"""

from __future__ import annotations

from collections.abc import Iterable, Mapping, Sequence
from dataclasses import dataclass
from fractions import Fraction
from itertools import permutations

import numpy as np
from scipy.optimize import Bounds, LinearConstraint, linprog, milp

from rcspp import presets
from rcspp.graph import AlgorithmParams, ResourceGraph, Row
from rcspp.resource import (
    AdditionExtensionFunction,
    MinMaxFeasibilityFunction,
    TimeWindowExtensionFunction,
    TimeWindowFeasibilityFunction,
    TrivialCostFunction,
    TrivialFeasibilityFunction,
    ValueCostFunction,
    ValueDominanceFunction,
)

SINK = 4
CUSTOMERS = (1, 2, 3)
DISTANCES = {(0, 1): 10, (0, 2): 10, (0, 3): 10, (1, 2): 4, (1, 3): 5, (2, 3): 6}
DEMANDS = {1: 4, 2: 5, 3: 6}
CAPACITY = 11
TIME_WINDOWS = {
    0: (0.0, 100.0),
    1: (0.0, 15.0),
    2: (20.0, 30.0),
    3: (10.0, 18.0),
    SINK: (0.0, 100.0),
}
NG_NEIGHBOURHOODS = {1: {1, 2}, 2: {1, 2}, 3: {1, 3}}
EPSILON = 1e-9


def sink_of(customers: Sequence[int]) -> int:
    """The sink's id: one more than the last customer."""
    return max(customers) + 1


def distance(
    i: int, j: int, sink: int = SINK, distances: Mapping[tuple[int, int], float] = DISTANCES
) -> float:
    """Distance between two nodes, the sink being the depot."""
    i, j = (0 if i == sink else i), (0 if j == sink else j)
    return distances[(min(i, j), max(i, j))]


def route_cost(
    nodes: Sequence[int], distances: Mapping[tuple[int, int], float] = DISTANCES, sink: int = SINK
) -> float:
    """The real (not reduced) cost of a path given by its nodes."""
    return sum(distance(i, j, sink, distances) for i, j in zip(nodes, nodes[1:]))


def pricing_graph(
    *,
    capacity: float = CAPACITY,
    windows: bool = False,
    memory: str | None = None,
    customers: Sequence[int] = CUSTOMERS,
    demands: Mapping[int, float] = DEMANDS,
    distances: Mapping[tuple[int, int], float] = DISTANCES,
    with_presets: bool = False,
    removed_arcs: Iterable[tuple[int, int]] = (),
    fleet_row: int | None = None,
) -> ResourceGraph:
    """The pricing graph of the pages: resources, nodes and arcs, with no duals yet.

    Resources, in order: reduced cost, then time (with ``windows``), then load, then a route
    memory (``memory`` is ``"ng"`` or ``"elementary"``). With ``with_presets`` the cost, time and
    load are registered through :mod:`rcspp.presets`, as page 5 does. ``removed_arcs`` leaves those
    arcs out (branching, page 6). ``fleet_row`` puts a row on every arc leaving the source (the
    vehicle-count cut of page 6).

    The dual of customer ``i`` is row ``i``, on every arc leaving ``i``.
    """
    sink = sink_of(customers)
    rg = ResourceGraph()
    if with_presets:
        presets.add_cost_resource(rg)
        if windows:
            presets.add_window_resource(rg, "real", TIME_WINDOWS)
        presets.add_capacity_resource(rg, "real", float(capacity))
    else:
        rg.add_real_resource(
            AdditionExtensionFunction(),
            TrivialFeasibilityFunction(),
            ValueCostFunction(),
            ValueDominanceFunction(),
        )
        if windows:
            rg.add_real_resource(
                TimeWindowExtensionFunction(TIME_WINDOWS),
                TimeWindowFeasibilityFunction(TIME_WINDOWS),
                TrivialCostFunction(),
                ValueDominanceFunction(),
            )
        rg.add_real_resource(
            AdditionExtensionFunction(),
            MinMaxFeasibilityFunction(0.0, float(capacity)),
            TrivialCostFunction(),
            ValueDominanceFunction(),
        )
    if memory == "ng":
        presets.add_ng_path_resource(rg, "bitset", NG_NEIGHBOURHOODS)
    elif memory == "elementary":
        presets.add_elementary_resource(rg, "bitset", [0, *customers, sink])

    rg.add_node(0, source=True)
    for i in customers:
        rg.add_node(i)
    rg.add_node(sink, sink=True)

    for i, j in arcs(customers, removed_arcs):
        d = distance(i, j, sink, distances)
        consumption = (d, d, demands.get(j, 0)) if windows else (d, demands.get(j, 0))
        if memory:
            consumption = (*consumption, set())
        rows = [] if i == 0 else [Row(i, 1.0)]
        if i == 0 and fleet_row is not None:
            rows = [Row(fleet_row, 1.0)]
        rg.add_arc(consumption, i, j, cost=d, rows=rows)
    return rg


def arcs(
    customers: Sequence[int] = CUSTOMERS, removed: Iterable[tuple[int, int]] = ()
) -> list[tuple[int, int]]:
    """The arcs of the pricing graph, in the order they are added, which is their id order.

    Every customer is linked to every other one, the source to every customer, and every customer
    to the sink.
    """
    sink, removed = sink_of(customers), set(removed)
    return [
        (i, j)
        for i in (0, *customers)
        for j in (*customers, sink)
        if i != j and not (i == 0 and j == sink) and (i, j) not in removed
    ]


def priced(rg: ResourceGraph, duals: Mapping[int, float]) -> ResourceGraph:
    """``rg`` with its reduced costs set from ``duals`` (row index → dual value)."""
    rg.update_reduced_costs({row: float(value) for row, value in duals.items()})
    return rg


def routes(result) -> list[tuple[float, list[int]]]:
    """A solve's routes as ``(reduced cost, nodes)``, cheapest first."""
    return [(round(s.cost, 9), list(s.path_node_ids)) for s in result.solutions]


def bidirectional_params(clock: int, half_way_point: float) -> AlgorithmParams:
    """Parameters for a bidirectional solve with ``clock`` as the critical resource."""
    params = AlgorithmParams()
    params.critical_resource_index = clock
    params.half_way_point = half_way_point
    return params


# --- The master problem --------------------------------------------------------------------------


@dataclass
class LPResult:
    """An LP or integer solution of the set partitioning master."""

    value: float
    x: list[float]
    duals: list[float]
    extra_dual: float | None = None
    uses_artificial: bool = False


ARTIFICIAL_COST = 1000.0  # more than any plan of the toy instance costs


def master_lp(
    columns: Sequence[Sequence[int]],
    costs: Sequence[float],
    customers: Sequence[int] = CUSTOMERS,
    *,
    min_vehicles: int | None = None,
    artificial: bool = False,
) -> LPResult:
    """Solve the LP relaxation of the set partitioning master with HiGHS.

    ``columns`` are paths given by their nodes; a column covers customer ``i`` as many times as it
    visits it. ``min_vehicles`` adds the vehicle-count cut ``sum x >= min_vehicles`` of page 6,
    whose dual is returned as ``extra_dual``. ``artificial`` adds one column per customer that
    covers it alone at ``ARTIFICIAL_COST`` and counts as no vehicle, which keeps the LP feasible
    when no real column covers a customer (branching, page 6); ``x`` leaves those columns out.
    """
    a_eq = np.array([[col.count(i) for col in columns] for i in customers], dtype=float)
    vehicles = np.ones(len(columns))
    costs = list(costs)
    if artificial:
        a_eq = np.hstack([a_eq, np.eye(len(customers))])
        vehicles = np.concatenate([vehicles, np.zeros(len(customers))])
        costs += [ARTIFICIAL_COST] * len(customers)
    kwargs = {}
    if min_vehicles is not None:
        kwargs = {"A_ub": -vehicles.reshape(1, -1), "b_ub": [-float(min_vehicles)]}
    res = linprog(
        costs, A_eq=a_eq, b_eq=np.ones(len(customers)), bounds=(0, None), method="highs", **kwargs
    )
    if res.status != 0:
        raise RuntimeError(f"master LP failed: {res.message}")
    x = [float(v) for v in res.x]
    extra = None if min_vehicles is None else -float(res.ineqlin.marginals[0])
    used = any(v > EPSILON for v in x[len(columns) :])
    return LPResult(
        float(res.fun), x[: len(columns)], [float(v) for v in res.eqlin.marginals], extra, used
    )


def master_ip(
    columns: Sequence[Sequence[int]], costs: Sequence[float], customers: Sequence[int] = CUSTOMERS
) -> LPResult:
    """Solve the set partitioning master as an integer program, over ``columns`` only."""
    a_eq = np.array([[col.count(i) for col in columns] for i in customers], dtype=float)
    res = milp(
        costs,
        constraints=LinearConstraint(a_eq, np.ones(len(customers)), np.ones(len(customers))),
        integrality=np.ones(len(columns)),
        bounds=Bounds(0, 1),
    )
    if res.status != 0:
        raise RuntimeError(f"master IP failed: {res.message}")
    return LPResult(float(res.fun), [float(v) for v in res.x], [])


@dataclass
class Iteration:
    """One column generation iteration: the RMP before pricing, and what pricing found."""

    columns: int
    value: float
    duals: list[float]
    best_reduced_cost: float
    added: list[int] | None


def column_generation(
    make_graph,
    initial_columns: Sequence[Sequence[int]],
    *,
    customers: Sequence[int] = CUSTOMERS,
    distances: Mapping[tuple[int, int], float] = DISTANCES,
    best_only: bool = True,
    min_vehicles: int | None = None,
    fleet_row: int | None = None,
    artificial: bool = False,
) -> tuple[list[Iteration], list[list[int]], LPResult]:
    """Column generation on the master, pricing with ``make_graph()``.

    With ``best_only`` each iteration adds only the cheapest route, as page 1 does by hand;
    otherwise it adds every negative one. ``min_vehicles`` and ``artificial`` are passed to
    :func:`master_lp`; ``fleet_row`` is the row that carries the vehicle-count cut's dual. Returns
    the iterations, the final columns and the final LP.
    """
    sink = sink_of(customers)
    columns = [list(c) for c in initial_columns]
    rg = make_graph()
    history: list[Iteration] = []
    while True:
        costs = [route_cost(c, distances, sink) for c in columns]
        lp = master_lp(columns, costs, customers, min_vehicles=min_vehicles, artificial=artificial)
        duals = dict(zip(customers, lp.duals))
        if fleet_row is not None:
            duals[fleet_row] = lp.extra_dual
        priced(rg, duals)
        found = routes(rg.solve(upper_bound=-EPSILON))
        new = [path for _, path in found if path not in columns]
        best = found[0][0] if found else 0.0
        history.append(Iteration(len(columns), lp.value, lp.duals, best, new[0] if new else None))
        if not new:
            return history, columns, lp
        columns += new[:1] if best_only else new


def single_customer_routes(customers: Sequence[int] = CUSTOMERS) -> list[list[int]]:
    """Depot → i → depot for every customer: the usual initial columns."""
    return [[0, i, sink_of(customers)] for i in customers]


def elementary_routes(
    customers: Sequence[int],
    demands: Mapping[int, float],
    capacity: float,
    distances: Mapping[tuple[int, int], float] = DISTANCES,
) -> list[list[int]]:
    """The cheapest route for every set of customers that fits in the vehicle.

    Each customer is visited at most once; of the orders that visit the same set, the cheapest is
    kept.
    """
    sink = sink_of(customers)
    best: dict[frozenset, list[int]] = {}
    for k in range(1, len(customers) + 1):
        for order in permutations(customers, k):
            if sum(demands[i] for i in order) > capacity:
                continue
            path = [0, *order, sink]
            key = frozenset(order)
            if key not in best or route_cost(path, distances, sink) < route_cost(
                best[key], distances, sink
            ):
                best[key] = path
    return list(best.values())


def fraction(value: float) -> Fraction:
    """A float as the simple fraction the pages write (½, ⅓…)."""
    return Fraction(value).limit_denominator(12)
