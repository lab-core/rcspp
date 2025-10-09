#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.


class Path:
    def __init__(self, path_id, cost, visited_nodes):
        self.id = path_id
        self.cost = cost
        self.visited_nodes = list(visited_nodes)
