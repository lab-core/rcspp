#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

import math
import time
from typing import Optional

from vrp.cg.path import Path
from vrp.instance import Customer, Instance

from rcspp.graph import ResourceGraph, Row, Solution
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


class VRP:
    EPSILON = 0.00000001

    def __init__(self, instance: Instance):
        self._instance = instance
        self._time_window_by_node_id = {}
        self._path_id = 0
        self.initialize_time_windows()
        # Build the subproblem graph once; dual costs are updated in-place each iteration.
        self._resource_graph = self.construct_resource_graph()
        self._paths = []
        self._total_subproblem_time = 0.0
        self._total_problem_time = 0.0

        self._state_history = []
        self._dual_values_history = []
        self.best_lagrangian_lb = -math.inf

    def initialize_time_windows(self):
        customers_by_id = self._instance.get_customers_by_id()
        for customer_id, customer in customers_by_id.items():
            self._time_window_by_node_id[customer_id] = (
                customer.ready_time,
                customer.due_time,
            )

        # Add sink node
        sink_id = len(customers_by_id)
        self._time_window_by_node_id[sink_id] = (0.0, math.inf)

        return self._time_window_by_node_id

    # ── Graph construction ────────────────────────────────────────────────────

    def construct_resource_graph(self) -> ResourceGraph:
        resource_graph = ResourceGraph()

        # Resource 0: distance / reduced cost (used as the optimisation objective)
        resource_graph.add_real_resource(
            AdditionExtensionFunction(),
            TrivialFeasibilityFunction(),
            ValueCostFunction(),
            ValueDominanceFunction(),
        )

        # Resource 1: cumulative travel time (time-window feasibility)
        resource_graph.add_real_resource(
            TimeWindowExtensionFunction(self._time_window_by_node_id),
            TimeWindowFeasibilityFunction(self._time_window_by_node_id),
            TrivialCostFunction(),
            ValueDominanceFunction(),
        )

        # Resource 2: cumulative demand (capacity feasibility)
        resource_graph.add_real_resource(
            AdditionExtensionFunction(),
            MinMaxFeasibilityFunction(0.0, self._instance.get_capacity()),
            TrivialCostFunction(),
            ValueDominanceFunction(),
        )

        self._add_nodes_and_arcs(resource_graph)
        return resource_graph

    def _add_nodes_and_arcs(self, resource_graph: ResourceGraph) -> None:
        t0 = time.time()
        customers_by_id = self._instance.get_customers_by_id()
        sink_id = len(customers_by_id)

        for customer_id, customer in customers_by_id.items():
            resource_graph.add_node(customer_id, customer.depot)
            if customer.depot:
                self.depot_id_ = customer.id
                resource_graph.add_node(sink_id, False, True)

        for customer_orig_id, customer_orig in customers_by_id.items():
            for customer_dest_id, customer_dest in customers_by_id.items():
                # Skip self-loops and arcs back to the depot source; the return
                # to depot is represented by the explicit arc to the sink below.
                if customer_orig_id != customer_dest_id and not customer_dest.depot:
                    self._add_arc(
                        resource_graph,
                        customer_orig_id,
                        customer_dest_id,
                        customer_orig,
                        customer_dest,
                    )
            sink_customer = customers_by_id[self.depot_id_]
            self._add_arc(resource_graph, customer_orig_id, sink_id, customer_orig, sink_customer)

        print(f"construct_graph: {int((time.time() - t0) * 1000)} ms")

    def _add_arc(
        self,
        resource_graph: ResourceGraph,
        orig_id: int,
        dest_id: int,
        orig: Customer,
        dest: Customer,
    ) -> None:
        distance = self.calculate_distance(orig, dest)
        travel_time = orig.service_time + distance
        demand = dest.demand

        # Depot→depot arc: assign an infinite base cost so it is never used.
        if orig.depot and dest.depot:
            base_cost = math.inf
            rows: list[Row] = []
        else:
            base_cost = distance
            # Non-depot origin: store the dual coefficient so update_reduced_costs
            # can compute  reduced_cost = distance - π_{orig_id}  without rebuilding.
            rows = [] if orig.depot else [Row(orig_id, 1.0)]

        resource_graph.add_arc(
            (base_cost, travel_time, demand),
            orig_id,
            dest_id,
            base_cost,
            rows,
        )

    # ── Utility ───────────────────────────────────────────────────────────────

    def calculate_distance(self, c1: Customer, c2: Customer) -> float:
        return math.sqrt((c2.pos_x - c1.pos_x) ** 2 + (c2.pos_y - c1.pos_y) ** 2)

    # ── Initial paths ─────────────────────────────────────────────────────────

    def generate_initial_paths(self):
        depot = self._instance.get_depot_customer()
        customers_by_id = self._instance.get_customers_by_id()
        sink_id = len(customers_by_id)
        for customer_id in self._instance.get_demand_customers_id():
            customer = customers_by_id[customer_id]
            path_cost = self.calculate_distance(depot, customer) + self.calculate_distance(
                customer, depot
            )
            path = Path(self._path_id, path_cost, [depot.id, customer_id, sink_id])
            self._paths.append(path)
            self._path_id += 1
        return self._paths

    def add_paths(self, solutions: list[Solution]):
        new_paths = []
        for solution in solutions:
            cost = self.calculate_solution_cost(solution)
            path = Path(self._path_id, cost, solution.path_node_ids)
            self._paths.append(path)
            self._path_id += 1
            new_paths.append(path)
        return new_paths

    def calculate_solution_cost(self, solution: Solution) -> float:
        """Return the true (non-reduced) cost by summing base arc costs."""
        return sum(self._resource_graph.get_arc(arc_id).cost for arc_id in solution.path_arc_ids)

    # ── Column generation ─────────────────────────────────────────────────────

    def solve(self, subproblem_max_nb_solutions: Optional[int] = None):
        from vrp.cg.master_problem import MasterProblem  # requires mip

        self.generate_initial_paths()

        master = MasterProblem(self._instance.get_demand_customers_id())
        master.add_paths(self._paths)

        min_reduced_cost = -math.inf
        final_dual_by_id: dict[int, float] = {}
        self._n_iterations = 0

        self.start_time = time.time()

        while min_reduced_cost < -self.EPSILON:
            print("*" * 45)
            print(f"iter={self._n_iterations}  min_rc={min_reduced_cost:.6f}")
            print("*" * 45)

            mp_sol = master.solve(relax=True)
            dual_by_id = mp_sol.dual_by_var_id
            self._dual_values_history.append(dual_by_id)

            t0 = time.time()
            solutions = self.solve_subproblem(dual_by_id)
            self._total_subproblem_time += time.time() - t0

            if solutions:
                print(f"best RCSPP reduced cost: {solutions[0].cost:.6f}")
            else:
                print("No solution found!")

            if subproblem_max_nb_solutions is not None:
                solutions = solutions[:subproblem_max_nb_solutions]

            min_reduced_cost = min((s.cost for s in solutions), default=math.inf)

            improving = [s for s in solutions if s.cost < -self.EPSILON]
            new_paths = self.add_paths(improving)
            master.add_paths(new_paths)

            lb = sum([i for i in dual_by_id.values()]) + self._instance.get_nb_vehicles() * min_reduced_cost
            if lb > self.best_lagrangian_lb + self.EPSILON:
                self.best_lagrangian_lb = lb

            self._n_iterations += 1
            self._save_state(mp_sol)
            if min_reduced_cost >= -self.EPSILON:
                final_dual_by_id = dual_by_id
                self._lp_cost = mp_sol.cost

        print(f"\niter={self._n_iterations}  min_rc={min_reduced_cost:.6f}\n")

        mp_sol = master.solve(relax=False)
        mp_sol.dual_by_var_id = final_dual_by_id
        self._total_problem_time = time.time() - self.start_time

        print(f"Time ratio subproblem/total: {self._total_subproblem_time / self._total_problem_time} | Total time: {self._total_problem_time} s")

        return mp_sol

    def solve_subproblem(self, dual_by_id: Optional[dict[int, float]] = None):
        """Update arc reduced costs in-place, then solve.

        The graph is built once in ``__init__``; only extender resource 0 (the cost
        resource) is rewritten each iteration.
        """
        if dual_by_id is not None:
            self._resource_graph.update_reduced_costs(dual_by_id)

        t0 = time.time()
        solutions = self._resource_graph.solve()
        print(f"Solve: {time.time() - t0:.3f}s")
        return solutions
    
    def _save_state(self, solution):
        state = {"time": time.time() - self.start_time,
                 "n_iter": self._n_iterations,
                 "n_cols": len(self._paths),
                 "value": solution.cost,
                 "best_lb": self.best_lagrangian_lb}
        
        self._state_history.append(state)