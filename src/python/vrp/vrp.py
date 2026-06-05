#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

import math
import time
from typing import Optional

from vrp.cg.path import Path
from vrp.instance import Customer, Instance

from utils.utils import dict_addition, dict_dot_product, dict_l1_norm, dict_scalar_mult

from rcspp.graph import ResourceGraph, Row, Solution, Algorithm, AlgorithmParams
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

    def __init__(self, instance: Instance, verbose = True):
        self._instance = instance
        self._path_id = 0
        self._paths = []
        self._total_subproblem_time = 0.0
        self._total_problem_time = 0.0
        self._dual_values_history = []
        self._state_history = []
        self._n_iterations = 0
        self._lp_cost = 0.0
        self.final_dual_by_id = {}
        self._smoothing = False
        self._verbose = verbose


        self._time_window_by_node_id = {}
        self.initialize_time_windows()
        self._resource_graph = self.construct_resource_graph()

        self.params = AlgorithmParams()
        self.params.num_labels_to_extend_by_node = 100
        #self.params.stop_after_X_solutions = 5

        self.algo = Algorithm.Simple

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
    
    def convex_combinaison_to_dict(self, dict1:dict, dict2:dict, alpha:float) -> dict:
        if not 0 <= alpha <= 1:
            raise ValueError("Alpha must be between 0 and 1")
        
        if dict1.keys() != dict2.keys():
            raise ValueError("Dicts must have the same keys")

        return {key: alpha*dict1[key] + (1-alpha)*dict2[key] for key in dict1}

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

    def solve(self, subproblem_max_nb_solutions: Optional[int] = None, algorithm: Optional[str] = None):
        from vrp.cg.master_problem import MasterProblem  # requires mip

        if algorithm is not None:
            self.algo = self.get_algo(algorithm)

        self.time_start = time.time()

        self.best_lagrangian_lb = - math.inf

        self.generate_initial_paths()

        self.master_problem = MasterProblem(self._instance.get_demand_customers_id())
        self.master_problem.add_paths(self._paths)

        self._n_iterations = 0

        if self._smoothing:
            self._smoothing = False
            self.first_iteration(subproblem_max_nb_solutions)
            self._smoothing = True

        master_solution = self.cg_iterations(subproblem_max_nb_solutions)

        master_solution = self.last_iteration()

        self._total_problem_time = time.time() - self.time_start
        print(f"Time ratio subproblem/total: {self._total_subproblem_time / self._total_problem_time} | Total time: {self._total_problem_time} s")

        return master_solution

    def get_negative_reduced_cost_column(self, dual_by_id:dict[int, float], subproblem_max_nb_solutions: Optional[int] = None):
        solutions = self.solve_subproblem(dual_by_id)

        if len(solutions) > 0:
            print(f"best RCSPP reduced cost: {solutions[0].cost:.6f}")
        else:
            print("No solution found!")


        if subproblem_max_nb_solutions is not None:
            nb_solutions = min(subproblem_max_nb_solutions, len(solutions))
            solutions = solutions[:nb_solutions]

        min_reduced_cost = min((s.cost for s in solutions), default=math.inf)

        improving = [s for s in solutions if s.cost < -self.EPSILON]
        new_paths = self.add_paths(improving)
        self.master_problem.add_paths(new_paths)

        return improving, min_reduced_cost

    def column_generation_iteration(self, subproblem_max_nb_solutions: Optional[int] = None):

        master_solution = self.master_problem.solve(relax=True)        

        dual_by_id = master_solution.dual_by_var_id
        self._dual_values_history.append(dual_by_id)

        if self._smoothing:
            self._last_outer_point = dual_by_id
            dual_by_id = self.convex_combinaison_to_dict(self._smoothing_center, dual_by_id, self._smoothing_parameter)

        improving, min_reduced_cost = self.get_negative_reduced_cost_column(dual_by_id, subproblem_max_nb_solutions)

        if min_reduced_cost >= -self.EPSILON:
            self.final_dual_by_id = dual_by_id

        return master_solution, min_reduced_cost

    def cg_iterations(self, subproblem_max_nb_solutions: Optional[int] = None):
        self.min_reduced_cost = -math.inf
        
        while True:
            self.print_begin_iteration()

            if self._smoothing:
                self._smoothing_parameter = max(0, 1- self._mis_price_k*(1-self._smoothing_parameter))

            master_solution, self.min_reduced_cost = self.column_generation_iteration(subproblem_max_nb_solutions)

            lb = sum(master_solution.dual_by_var_id.values()) + self._instance.get_nb_vehicles() * self.min_reduced_cost
            if lb > self.best_lagrangian_lb + self.EPSILON:
                self.best_lagrangian_lb = lb

            self._n_iterations += 1

            self._save_state(master_solution)
        
            if self._smoothing:
                if self.min_reduced_cost >= -self.EPSILON:
                    self._smoothing_center = self.final_dual_by_id
                    negative_red_cost_solutions, self.min_reduced_cost = self.get_negative_reduced_cost_column(self._last_outer_point, subproblem_max_nb_solutions)
                    self._mis_price_k += 1
                    self.vprint("Mis Price")
                else:
                    self._mis_price_k = 1
                    g = self.compute_subgradient(negative_red_cost_solutions[0])
                    out_minus_in = dict_addition(self._last_outer_point, dict_scalar_mult(self._smoothing_center, -1))
                    dot = dict_dot_product(g, out_minus_in)
                    if dot > 0:
                        self.increase_alpha()
                    else:
                        self.decrease_alpha()
                    


            if self.min_reduced_cost > -self.EPSILON:
                break

        self._lp_cost = master_solution.cost
        return master_solution

    def last_iteration(self):
        master_solution = self.master_problem.solve()

        master_solution.dual_by_var_id = self.final_dual_by_id
        return master_solution

    

    def solve_subproblem(self, dual_by_id: Optional[dict[int, float]] = None):
        """Update arc reduced costs in-place, then solve.

        The graph is built once in ``__init__``; only extender resource 0 (the cost
        resource) is rewritten each iteration.
        """
        if dual_by_id is not None:
            self._resource_graph.update_reduced_costs(dual_by_id)

        t0 = time.time()
        solutions = self._resource_graph.solve(self.algo, params=self.params)
        print(f"Solve: {time.time() - t0:.3f}s")
        self._total_subproblem_time += time.time() - t0
        return solutions

    def enable_smoothing(self, alpha:float):
        self._smoothing_parameter = alpha
        self._smoothing = True
        self._mis_price_k = 1

    def disable_smoothing(self):
        self._smoothing = False

    def increase_alpha(self):
        self._smoothing_parameter = self._smoothing_parameter + (1-self._smoothing_parameter)*0.1

    def decrease_alpha(self):
        self._smoothing_parameter = max(0, self._smoothing_parameter-0.1)

    def compute_subgradient(self, neg_cost_sol):
        g = {}
        for i in self._instance.get_demand_customers_id():
            if i in neg_cost_sol.path_node_ids:
               ai = 1
            else:
                ai = 0
            g[i] = 1 - ai
        return g

    def get_state_history(self):
        return self._state_history
    
    def get_dual_values_history(self):
        return self._dual_values_history
    
    def get_n_iterations(self):
        return self._n_iterations

    def get_lp_cost(self):
        return self._lp_cost

    def get_total_problem_time(self):
        return self._total_problem_time

    def get_total_subproblem_time(self):
        return self._total_subproblem_time
    
    def compute_first_lagrangian_bound(self, dual):
        center_reduced_cost_solution = self.solve_subproblem(dual)
        self.best_lagrangian_lb = sum([i for i in dual.values()]) + self._instance.get_nb_vehicles() * center_reduced_cost_solution[0].cost

    def print_begin_iteration(self):
        self.vprint("*********************************************")
        self.vprint(
                    f"nb_iter={self._n_iterations} | min_reduced_cost={self.min_reduced_cost} "
                    f"| EPSILON={self.EPSILON}",
                    f"------------------------------------------------------------------ Iter: {self._n_iterations}------------------"
                )
        self.vprint("*********************************************")

    def first_iteration(self, subproblem_max_nb_solutions: Optional[int] = None):
        master_solution, self.min_reduced_cost = self.column_generation_iteration(subproblem_max_nb_solutions)

        dual_by_id = master_solution.dual_by_var_id

        self._smoothing_center = dual_by_id
        self._last_outer_point = dual_by_id

        self.dual_box_center_ = dual_by_id
        if hasattr(self, 'kappa'):
            self.box_radius = dict_l1_norm(self.dual_box_center_)/(self.kappa * len(self.dual_box_center_))

        lb = sum([i for i in master_solution.dual_by_var_id.values()]) + self._instance.get_nb_vehicles() *self.min_reduced_cost
        self.best_lagrangian_lb = lb

        self._n_iterations += 1

    def vprint(self, message, quiet_message=None):
        if self._verbose:
            print(message)
        elif quiet_message is not None:
            print(quiet_message)

    def _save_state(self, solution):
        state = {"time": time.time() - self.time_start,
                 "n_iter": self._n_iterations,
                 "n_cols": len(self._paths),
                 "value": solution.cost,
                 "best_lb": self.best_lagrangian_lb}
        
        if self._smoothing:
            state["smoothing_parameter"] = self._smoothing_parameter
        
        self._state_history.append(state)

    def get_algo(self, name: str):
        if name == "simple":
            return Algorithm.Simple
        if name == "greedy":
            return Algorithm.Greedy
        if name == "pulling":
            return Algorithm.Pulling
        if name == "pushing":
            return Algorithm.Pushing