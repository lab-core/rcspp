#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.
"""The pricing graph of a vehicle routing problem with time windows."""

from __future__ import annotations

from vrp.instance import Instance, distance

from rcspp import presets
from rcspp.graph import ResourceGraph


def sink(instance: Instance) -> int:
    """The sink's node: the depot again, as the end of a route, numbered after the last
    customer."""
    return len(instance.customers)


def pricing_graph(instance: Instance) -> ResourceGraph:
    """A route is a path from the depot (node 0) to the sink, through customers 1 to n.

    Resources: the distance, which is the cost; the time, within each customer's window
    (travel plus the service at the customer left); the load, within the capacity. Nothing
    keeps a route from visiting a customer twice: column generation over these routes gives a
    weaker bound than over elementary ones, and a faster search (see ``presets`` for the
    elementary and ng-path resources).

    The master has one row per customer, row ``i - 1`` for customer ``i``: an arc out of a
    customer covers its row.
    """
    depot = instance.depot
    end = sink(instance)
    windows = {
        customer.number: (customer.ready_time, customer.due_time) for customer in instance.customers
    }

    # The route is back at the depot before it closes.
    windows[end] = windows[depot.number]

    # The resources: the cost first.
    graph = ResourceGraph()
    presets.add_cost_resource(graph)
    presets.add_window_resource(graph, "real", windows)
    presets.add_capacity_resource(graph, "real", instance.capacity)

    # The nodes.
    graph.add_node(depot.number, source=True)

    for customer in instance.customers[1:]:
        graph.add_node(customer.number)

    graph.add_node(end, sink=True)

    # The arcs: from the depot or a customer to every other customer, and from every customer
    # to the sink. The depot's arcs cover no row.
    for origin in instance.customers:
        rows = [] if origin is depot else [(origin.number - 1, 1.0)]
        destinations = [(customer.number, customer) for customer in instance.customers[1:]]

        if origin is not depot:
            destinations.append((end, depot))

        for node, destination in destinations:
            if destination is origin:
                continue

            length = distance(origin, destination)
            consumption = (length, origin.service_time + length, destination.demand)
            graph.add_arc(consumption, origin.number, node, length, rows)

    return graph
