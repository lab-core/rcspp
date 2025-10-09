#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

import math
import time
from typing import Optional

from vrp.cg.master_problem import MasterProblem
from vrp.cg.mp_solution import MPSolution
from vrp.cg.path import Path
from vrp.instance import Customer, Instance

from rcspp.graph import ResourceGraph, Solution
from rcspp.resource import (
    MinMaxFeasibilityFunction,
    RealAdditionExpansionFunction,
    RealTrivialFeasibilityFunction,
    RealValueCostFunction,
    RealValueDominanceFunction,
    TimeWindowExpansionFunction,
    TimeWindowFeasibilityFunction,
)


class VRP:
    EPSILON = 0.00000001

    def __init__(self, instance: Instance):
        self.__instance = instance
        self.__min_time_window_by_arc_id = {}
        self.__max_time_window_by_node_id = {}
        self.__path_id = 0
        self.__time_window_by_customer_id = self.initialize_time_windows()
        self.__resource_graph = self.construct_resource_graph()
        self.add_nodes_and_arcs(self.__resource_graph)
        self.__paths = []
        self.__total_subproblem_time = 0.0
        self.__subproblem_graph = None

    def initialize_time_windows(self):
        # print("initialize_time_windows")

        time_window_by_customer_id: dict[int, tuple[float, float]] = {}

        customers_by_id = self.__instance.get_customers_by_id()
        for customer_id, customer in customers_by_id.items():
            time_window_by_customer_id[customer_id] = (
                customer.ready_time,
                customer.due_time,
            )

        # Add sink node
        sink_id = len(customers_by_id)
        time_window_by_customer_id[sink_id] = (0.0, math.inf)

        arc_id = 0
        for customer_orig_id, customer_orig in customers_by_id.items():
            for customer_dest_id, customer_dest in customers_by_id.items():
                if customer_orig_id != customer_dest_id:
                    # Default values
                    min_time = 0.0
                    max_time = math.inf
                    if customer_dest_id in time_window_by_customer_id:
                        min_time, max_time = time_window_by_customer_id[customer_dest_id]

                    self.__min_time_window_by_arc_id[arc_id] = min_time
                    self.__max_time_window_by_node_id[customer_dest_id] = max_time

                    arc_id += 1

            # Handle arcs to sink
            min_time, max_time = 0.0, math.inf
            if sink_id in time_window_by_customer_id:
                min_time, max_time = time_window_by_customer_id[sink_id]

            self.__min_time_window_by_arc_id[arc_id] = min_time
            self.__max_time_window_by_node_id[sink_id] = max_time

            arc_id += 1

        return time_window_by_customer_id

    def add_nodes_and_arcs(
        self,
        resource_graph: ResourceGraph,
        dual_by_id: Optional[dict[int, float]] = None,
    ):
        time_start = time.time()

        self.add_all_nodes_to_graph(resource_graph)

        time_nodes = time.time()

        self.add_all_arcs_to_graph(resource_graph, dual_by_id)

        time_end = time.time()

        print(f"construct_graph Time: {int((time_end - time_start) * 1000)} ms")
        print(f"construct_graph Time Nodes: {int((time_nodes - time_start) * 1000)} ms")
        print(f"construct_graph Time Arcs: {int((time_end - time_nodes) * 1000)} ms")

    def add_all_nodes_to_graph(self, resource_graph: ResourceGraph) -> None:
        # print("add_all_nodes_to_graph")

        customers_by_id = self.__instance.get_customers_by_id()
        sink_id = len(customers_by_id)

        for customer_id, customer in customers_by_id.items():
            resource_graph.add_node(customer_id, customer.depot)

            if customer.depot:
                self.depot_id_ = customer.id
                # Add the depot as a sink as well
                resource_graph.add_node(sink_id, False, True)

    def add_all_arcs_to_graph(
        self,
        resource_graph: ResourceGraph,
        dual_by_id: Optional[dict[int, float]] = None,
    ) -> None:
        # print("add_all_arcs_to_graph")

        customers_by_id = self.__instance.get_customers_by_id()
        sink_id = len(customers_by_id)

        arc_id = 0
        for customer_orig_id, customer_orig in customers_by_id.items():
            for customer_dest_id, customer_dest in customers_by_id.items():
                if customer_orig_id != customer_dest_id:
                    self.add_arc_to_graph(
                        resource_graph,
                        customer_orig_id,
                        customer_dest_id,
                        customer_orig,
                        customer_dest,
                        dual_by_id,
                        arc_id,
                    )
                    arc_id += 1

            # Add arcs to sink
            sink_customer = customers_by_id[self.depot_id_]
            self.add_arc_to_graph(
                resource_graph,
                customer_orig_id,
                sink_id,
                customer_orig,
                sink_customer,
                dual_by_id,
                arc_id,
            )
            arc_id += 1

        # print(f"Number of arcs added: {arc_id}")

    def add_arc_to_graph(
        self,
        resource_graph: ResourceGraph,
        customer_orig_id: int,
        customer_dest_id: int,
        customer_orig: Customer,
        customer_dest: Customer,
        dual_by_id: Optional[dict[int, float]],
        arc_id: int,
    ) -> None:
        # print("add_arc_to_graph")

        origin_node_id = customer_orig_id
        destination_node_id = customer_dest_id

        distance = self.calculate_distance(customer_orig, customer_dest)

        customer_pi = 0.0
        if not customer_orig.depot and dual_by_id is not None:
            customer_pi = dual_by_id[customer_orig_id]

        reduced_cost = distance - customer_pi
        if customer_orig.depot and customer_dest.depot:
            reduced_cost = math.inf

        travel_time = customer_orig.service_time + distance
        demand = customer_dest.demand

        # print(f"reduced_cost: {reduced_cost} | travel_time: {reduced_cost} "
        #       f"| demand: {reduced_cost}")
        # print(f"origin_node_id: {origin_node_id} | destination_node_id: "
        #       f"{destination_node_id} | arc_id: {arc_id}")

        arc = resource_graph.add_arc(  # noqa: F841
            ([(reduced_cost,), (travel_time,), (demand,)],),
            origin_node_id,
            destination_node_id,
            arc_id,
            reduced_cost,
        )

        # print(f"{arc.id}: ({arc.get_origin().id},{arc.get_destination().id}) "
        #       f"-> {arc.cost} ({customer_pi}) | {travel_time} | {demand}")

    def update_all_arcs_to_graph(
        self,
        resource_graph: ResourceGraph,
        dual_by_id: Optional[dict[int, float]] = None,
    ) -> None:
        customers_by_id = self.__instance.get_customers_by_id()
        sink_id = len(customers_by_id)

        arc_id = 0
        for customer_orig_id, customer_orig in customers_by_id.items():
            for customer_dest_id, customer_dest in customers_by_id.items():
                if customer_orig_id != customer_dest_id:
                    self.update_arc_to_graph(
                        resource_graph,
                        customer_orig_id,
                        customer_dest_id,
                        customer_orig,
                        customer_dest,
                        dual_by_id,
                        arc_id,
                    )
                    arc_id += 1

            # Add arcs to sink
            sink_customer = customers_by_id[self.depot_id_]
            self.update_arc_to_graph(
                resource_graph,
                customer_orig_id,
                sink_id,
                customer_orig,
                sink_customer,
                dual_by_id,
                arc_id,
            )
            arc_id += 1

        # print(f"Number of arcs added: {arc_id}")

    def update_arc_to_graph(
        self,
        resource_graph: ResourceGraph,
        customer_orig_id: int,
        customer_dest_id: int,
        customer_orig: Customer,
        customer_dest: Customer,
        dual_by_id: Optional[dict[int, float]],
        arc_id: int,
    ) -> None:
        origin_node_id = customer_orig_id  # noqa: F841
        destination_node_id = customer_dest_id  # noqa: F841

        distance = self.calculate_distance(customer_orig, customer_dest)

        customer_pi = 0.0
        if not customer_orig.depot and dual_by_id is not None:
            customer_pi = dual_by_id[customer_orig_id]

        reduced_cost = distance - customer_pi
        if customer_orig.depot and customer_dest.depot:
            reduced_cost = math.inf

        travel_time = customer_orig.service_time + distance
        demand = customer_dest.demand

        # print(f"reduced_cost: {reduced_cost} | travel_time: {reduced_cost} | "
        #       f"demand: {reduced_cost}")
        # print(f"origin_node_id: {origin_node_id} | destination_node_id: "
        #       f"{destination_node_id} | arc_id: {arc_id}")

        arc = resource_graph.get_arc(arc_id)

        resource_graph.update_arc(
            arc, ([(reduced_cost,), (travel_time,), (demand,)],), reduced_cost
        )

        # print(f"{arc.id}: ({arc.get_origin().id},{arc.get_destination().id})"
        #       f" -> {arc.cost} ({customer_pi}) | {travel_time} | {demand}")

    def calculate_distance(self, customer1: Customer, customer2: Customer) -> float:
        return math.sqrt(
            (customer2.pos_x - customer1.pos_x) ** 2 + (customer2.pos_y - customer1.pos_y) ** 2
        )

    def construct_resource_graph(self, dual_by_id: Optional[dict[int, float]] = None):
        resource_graph = ResourceGraph()

        # print(f"Add distance resource...")
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

        # print(f"Add time resource...")
        time_expansion_function = TimeWindowExpansionFunction(self.__min_time_window_by_arc_id)
        time_feasibility_function = TimeWindowFeasibilityFunction(self.__max_time_window_by_node_id)
        time_cost_function = RealValueCostFunction()
        time_dominance_function = RealValueDominanceFunction()

        resource_graph.add_real_resource(
            time_expansion_function,
            time_feasibility_function,
            time_cost_function,
            time_dominance_function,
        )

        # print(f"Add demand resource...")
        demand_expansion_function = RealAdditionExpansionFunction()
        demand_feasibility_function = MinMaxFeasibilityFunction(0.0, self.__instance.get_capacity())
        demand_cost_function = RealValueCostFunction()
        demand_dominance_function = RealValueDominanceFunction()

        resource_graph.add_real_resource(
            demand_expansion_function,
            demand_feasibility_function,
            demand_cost_function,
            demand_dominance_function,
        )

        self.add_nodes_and_arcs(resource_graph, dual_by_id)

        return resource_graph

    def update_resource_graph(
        self,
        resource_graph: ResourceGraph,
        dual_by_id: Optional[dict[int, float]] = None,
    ):
        print("update_resource_graph")

        self.update_all_arcs_to_graph(resource_graph, dual_by_id)

        return resource_graph

    def generate_initial_paths(self):
        depot_customer = self.__instance.get_depot_customer()

        customers_by_id = self.__instance.get_customers_by_id()

        for customer_id in self.__instance.get_demand_customers_id():
            customer = customers_by_id[customer_id]
            path_cost = self.calculate_distance(depot_customer, customer) + self.calculate_distance(
                customer, depot_customer
            )
            path_time = path_cost + customer.service_time  # noqa: F841
            path_demand = customer.demand  # noqa: F841

            visited_nodes = [depot_customer.id, customer_id, depot_customer.id]
            path = Path(self.__path_id, path_cost, visited_nodes)
            self.__paths.append(path)

            self.__path_id += 1

        return self.__paths

    def add_paths(self, solutions: list[Solution]):
        # print(f"VRP::add_paths: {len(solutions)}")

        for solution in solutions:
            solution_cost = self.calculate_solution_cost(solution)
            path = Path(self.__path_id, solution_cost, solution.path_node_ids)
            self.__paths.append(path)
            self.__path_id += 1

    def calculate_solution_cost(self, solution: Solution):
        cost = 0.0

        for arc_id in solution.path_arc_ids:
            cost += self.__resource_graph.get_arc(arc_id).cost

        return cost

    def solve(self, subproblem_max_nb_solutions: Optional[int] = None):
        mp_solution = MPSolution()

        self.generate_initial_paths()

        min_reduced_cost = -math.inf

        final_dual_by_id = {}

        nb_iter = 0
        while min_reduced_cost < -self.EPSILON:
            print("*********************************************")
            print(
                f"nb_iter={nb_iter} | min_reduced_cost={min_reduced_cost} "
                f"| EPSILON={self.EPSILON}"
            )
            print("*********************************************")

            master_problem = MasterProblem(self.__instance.get_demand_customers_id())

            master_problem.construct_model(self.__paths)

            master_solution = master_problem.solve(True)

            dual_by_id = master_solution.dual_by_var_id

            subproblem_time_start = time.time()
            solutions = self.solve_subproblem(dual_by_id)
            subproblem_time_end = time.time()
            self.__total_subproblem_time += subproblem_time_end - subproblem_time_start

            if len(solutions) > 0:
                print(f"Solution RCSPP cost: {solutions[0].cost}")
            else:
                print("No solution found!")

            if subproblem_max_nb_solutions is not None:
                nb_solutions = min(subproblem_max_nb_solutions, len(solutions))
                solutions = solutions[:nb_solutions]

            negative_red_cost_solutions = []

            min_reduced_cost = math.inf
            for sol in solutions:
                if sol.cost < min_reduced_cost:
                    min_reduced_cost = sol.cost
                if sol.cost < -self.EPSILON:
                    negative_red_cost_solutions.append(sol)

            self.add_paths(negative_red_cost_solutions)

            nb_iter += 1

            if min_reduced_cost >= -self.EPSILON:
                final_dual_by_id = master_solution.dual_by_var_id

        print("\n*********************************************\n")
        print(
            f"nb_iter={nb_iter} | min_reduced_cost={min_reduced_cost} " f"| EPSILON={self.EPSILON}"
        )
        print("\n*********************************************\n")

        master_problem = MasterProblem(self.__instance.get_demand_customers_id())
        master_problem.construct_model(self.__paths)
        master_solution = master_problem.solve()

        master_solution.dual_by_var_id = final_dual_by_id

        return mp_solution

    def solve_subproblem(self, dual_by_id: Optional[dict[int, float]] = None):
        if self.__subproblem_graph is None:
            self.__subproblem_graph = self.construct_resource_graph(dual_by_id)
        else:
            self.update_resource_graph(self.__subproblem_graph, dual_by_id)

        # subproblem = Subproblem(resource_graph)

        time_start = time.time()
        solutions = self.__subproblem_graph.solve()
        time_end = time.time()

        print(f"Solve: {time_end - time_start}")

        return solutions
