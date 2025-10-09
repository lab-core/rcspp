#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

# flake8: noqa

import os
import sys

import networkx as nx

relative_path = "../../out/build/x64-release/lib/"
absolute_path = os.path.abspath(relative_path)
os.add_dll_directory(absolute_path)
sys.path.append(absolute_path)

rcspp_path = relative_path + "/rcspp/"
sys.path.append(rcspp_path)

# os.add_dll_directory("C:\\gurobi1203\\win64\\bin")
# sys.path.append("C:\\gurobi1203\\win64\\bin")

from vrp.instance_reader import InstanceReader

from rcspp.graph import ResourceGraph
from rcspp.resource import (
    MinMaxFeasibilityFunction,
    RealAdditionExpansionFunction,
    RealTrivialFeasibilityFunction,
    RealValueCostFunction,
    RealValueDominanceFunction,
    TimeWindowExpansionFunction,
    TimeWindowFeasibilityFunction,
)


def construct_resource_graph():
    print("construct_resource_graph")

    resource_graph = ResourceGraph()

    distance_expansion_function = RealAdditionExpansionFunction()
    distance_feasibility_function = RealTrivialFeasibilityFunction()
    distance_cost_function = RealValueCostFunction()
    distance_dominance_function = RealValueDominanceFunction()

    resource_graph.add_real_resource(
        distance_expansion_function,
        distance_feasibility_function,
        distance_cost_function,
        distance_dominance_function,
    )

    print("add_real_resource: 1")

    time_expansion_function = RealAdditionExpansionFunction()
    time_feasibility_function = RealTrivialFeasibilityFunction()
    time_cost_function = RealValueCostFunction()
    time_dominance_function = RealValueDominanceFunction()

    resource_graph.add_real_resource(
        time_expansion_function,
        time_feasibility_function,
        time_cost_function,
        time_dominance_function,
    )

    print("add_real_resource: 2")

    demand_expansion_function = RealAdditionExpansionFunction()
    demand_feasibility_function = MinMaxFeasibilityFunction(0.0, 500.0)
    demand_cost_function = RealValueCostFunction()
    demand_dominance_function = RealValueDominanceFunction()

    resource_graph.add_real_resource(
        demand_expansion_function,
        demand_feasibility_function,
        demand_cost_function,
        demand_dominance_function,
    )

    print("add_real_resource: 3")

    return resource_graph


if __name__ == "__main__":
    print("Read instance...")
    instance_name = "R101_5"
    instance_path = "../../instances/" + instance_name + ".txt"
    instance_reader = InstanceReader(instance_path)
    instance = instance_reader.read()

    nx_graph = nx.DiGraph()

    nx_graph.add_node(1)
    nx_graph.add_node(2)
    nx_graph.add_node(3)
    nx_graph.add_node(4)

    nx_graph.add_edge(1, 2, resource=(1.0, 10.0, 100.0))
    nx_graph.add_edge(1, 3, resource=(5.0, 15.0, 150.0))
    nx_graph.add_edge(2, 4, resource=(27.0, 27.0, 270.0))
    nx_graph.add_edge(2, 3, resource=(3.0, 53.0, 123.0))
    nx_graph.add_edge(3, 4, resource=(32.0, 33.0, 43.0))

    resource_graph = construct_resource_graph()
    resource_graph.from_networkx(nx_graph)
