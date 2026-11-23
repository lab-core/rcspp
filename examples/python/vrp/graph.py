#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The pricing graph of a vehicle routing problem with time windows."""

from __future__ import annotations

import math

from vrp.instance import Instance, distance

from rcspp import presets
from rcspp.graph import ResourceGraph


def sink(instance: Instance) -> int:
    """The sink's node: the depot again, as the end of a route, numbered after the last
    customer."""
    return len(instance.customers)


def pricing_graph(
    instance: Instance,
    capacity: float | None = None,
    fixed_cost: float = 0.0,
    cost_per_distance: float = 1.0,
    max_distance: float = math.inf,
) -> ResourceGraph:
    """A route is a path from the depot (node 0) to the sink, through customers 1 to n.

    Resources: the cost; the time, within each customer's window (travel plus the service at
    the customer left); the load, within the capacity; and, with ``max_distance``, the
    distance driven. Nothing keeps a route from visiting a customer twice: column generation
    over these routes gives a weaker bound than over elementary ones, and a faster search
    (see ``presets`` for the elementary and ng-path resources).

    The master has one row per customer, row ``i - 1`` for customer ``i``: an arc out of a
    customer covers its row.

    Args:
        instance: The customers.
        capacity: The vehicle's capacity; the instance's by default.
        fixed_cost: Paid once per route, on the arc that leaves the depot: what using one
            more vehicle costs, whatever it drives.
        cost_per_distance: The cost of each unit of distance driven.
        max_distance: The longest route allowed; none by default, and then the graph has no
            distance resource.
    """
    depot = instance.depot
    end = sink(instance)
    windows = {
        customer.number: (customer.ready_time, customer.due_time) for customer in instance.customers
    }

    # The route is back at the depot before it closes.
    windows[end] = windows[depot.number]
    limits_distance = max_distance != math.inf

    # The resources: the cost first.
    graph = ResourceGraph()
    presets.add_cost_resource(graph)
    presets.add_window_resource(graph, "real", windows)
    presets.add_capacity_resource(
        graph, "real", instance.capacity if capacity is None else capacity
    )

    if limits_distance:
        presets.add_capacity_resource(graph, "real", max_distance)

    # The nodes.
    graph.add_node(depot.number, source=True)

    for customer in instance.customers[1:]:
        graph.add_node(customer.number)

    graph.add_node(end, sink=True)

    # The arcs: from the depot or a customer to every other customer, and from every customer
    # to the sink. The depot's arcs cover no row, and carry the fixed cost.
    for origin in instance.customers:
        rows = [] if origin is depot else [(origin.number - 1, 1.0)]
        destinations = [(customer.number, customer) for customer in instance.customers[1:]]

        if origin is not depot:
            destinations.append((end, depot))

        for node, destination in destinations:
            if destination is origin:
                continue

            length = distance(origin, destination)
            cost = cost_per_distance * length

            if origin is depot:
                cost += fixed_cost

            consumption = [cost, origin.service_time + length, destination.demand]

            if limits_distance:
                consumption.append(length)

            graph.add_arc(tuple(consumption), origin.number, node, cost, rows)

    return graph
