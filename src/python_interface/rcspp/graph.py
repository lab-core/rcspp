#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

import importlib

_ext = importlib.import_module("rcsppy")

from typing import Optional  # noqa: E402

import networkx as nx  # noqa: E402


class ResourceGraph(_ext.graph.ResourceGraph):
    def __init__(self, nx_graph: Optional[nx.DiGraph] = None, **kwargs):
        super().__init__(**kwargs)

        print("PYTHON: ResourceGraph.__init__")

        if nx_graph is not None:
            self.from_networkx(nx_graph)

    def from_networkx(self, nx_graph: Optional[nx.DiGraph]):
        print("PYTHON: ResourceGraph.from_networkx")
        print(f"nb_nodes: {len(nx_graph.nodes(data=True))}")
        for node_id, data in nx_graph.nodes(data=True):
            print(f"node_id={node_id} -> {data}")
            source = "source" in data and data["source"] is True
            sink = "sink" in data and data["sink"] is True
            self.add_node(int(node_id), source, sink)

        print(f"nb_edges: {len(nx_graph.edges(data=True))}")
        for u, v, data in nx_graph.edges(data=True):
            print(f"(u,v)=({u},{v}) -> {data}")

            resource_initializer_list = None
            if "resource" in data:
                resource_initializer_list = ([(res,) for res in data.get("resource")],)

            arc_id = None
            if "id" in data:
                arc_id = data.get("id")

            cost = 0.0
            if "cost" in data:
                cost = data.get("cost")

            if resource_initializer_list is not None:
                print(f"resource_initializer_list={resource_initializer_list}")
                self.add_arc(resource_initializer_list, int(u), int(v), arc_id, cost)
            else:
                self.add_arc(int(u), int(v), arc_id, cost)

            print("END")


# Export all other graph submodule symbols, except ResourceGraph
for k in dir(_ext.graph):
    if not k.startswith("_") and k != "ResourceGraph":
        globals()[k] = getattr(_ext.graph, k)

# Overwrite the ResourceGraph class exposed in graph.cpp
_ext.graph.ResourceGraph = ResourceGraph
