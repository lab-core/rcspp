#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

import math
import time
from typing import Optional, final

from vrp.cg.modified_master_problem import ModifiedMasterProblem, MasterProblem
from vrp.instance import Instance
from vrp.vrp import VRP
import numpy as np

class StabilizedVRP(VRP):
    def __init__(self, instance: Instance, dual_box_center: dict):
        super().__init__(instance)
        self.dual_box_center_ = dual_box_center

    def solve(self, subproblem_max_nb_solutions: Optional[int] = None):
        time_start = time.time()

        self.dual_box_radius_ = [100.0 for _ in self._VRP__instance.get_demand_customers_id()]

        self.generate_initial_paths()

        max_special_var_value = math.inf


        nb_stabilization_iterations = 0
        while max_special_var_value > self.EPSILON:
            print("\n##############################################\n")
            print(
                f"nb_iter_stabilization={nb_stabilization_iterations} | max_special_var_value={max_special_var_value} " f"| EPSILON={self.EPSILON}"
            )
            print("\n##############################################\n")
            stabilized_iter_solution, final_dual_by_id  = self.cg_iteration(subproblem_max_nb_solutions)

            special_var_values = [value for var, value in stabilized_iter_solution.value_by_var_id.items() if isinstance(var, str) and var.startswith("y")]
            max_special_var_value = max(special_var_values) if len(special_var_values) > 0 else 0.0
            print(f"Max special var value: {max_special_var_value}")
            nb_stabilization_iterations += 1
            

        master_problem = MasterProblem(self._VRP__instance.get_demand_customers_id())
        master_problem.construct_model(self._VRP__paths)
        master_solution = master_problem.solve()

        master_solution.dual_by_var_id = final_dual_by_id

        self._VRP__total_problem_time = time.time() - time_start
        print(f"Time ratio subproblem/total: {self._VRP__total_subproblem_time / self._VRP__total_problem_time} | Total time: {self._VRP__total_problem_time} s")

        return master_solution
    
    def cg_iteration(self, subproblem_max_nb_solutions: Optional[int] = None):
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

            master_problem = ModifiedMasterProblem(self._VRP__instance.get_demand_customers_id(), self.dual_box_center_, self.dual_box_radius_)

            master_problem.construct_model(self._VRP__paths)

            master_solution = master_problem.solve(True)        

            dual_by_id = master_solution.dual_by_var_id

            subproblem_time_start = time.time()
            solutions = self.solve_subproblem(dual_by_id)
            
            subproblem_time_end = time.time()
            self._VRP__total_subproblem_time += subproblem_time_end - subproblem_time_start

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

        return master_solution, final_dual_by_id
    
    def solve_subproblem(self, dual_by_id):
        return super().solve_subproblem(dual_by_id)
