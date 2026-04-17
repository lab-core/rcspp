#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

import math
import time
from typing import Optional, final

from vrp.cg.dual_box_master_problem import DualBoxMasterProblem, MasterProblem
from vrp.instance import Instance
from vrp.vrp import VRP

class DualBoxVRP(VRP):
    def __init__(self, instance: Instance, dual_box_center: dict, box_radius: float = 100.0, penalty: float = 0.0):
        super().__init__(instance)
        self.dual_box_center_ = dual_box_center
        self.dual_box_radius_ = [box_radius for _ in self._VRP__instance.get_demand_customers_id()]
        self.penalty_value = penalty
        self.have_penalty_ = (penalty is not None)
        self.meta_iteration = 0

    def solve(self, subproblem_max_nb_solutions: Optional[int] = None):
        time_start = time.time()
        self.generate_initial_paths()
        max_special_var_value = math.inf
        stabilized_iter_solution = None

        while max_special_var_value > self.EPSILON:
            if stabilized_iter_solution is not None:
                self.dual_box_center_ = stabilized_iter_solution.dual_by_var_id

            stabilized_iter_solution = self.cg_iterations(subproblem_max_nb_solutions)

            special_var_values = [value for var, value in stabilized_iter_solution.value_by_var_id.items() if isinstance(var, str) and var.startswith("y")]
            max_special_var_value = max(special_var_values) if len(special_var_values) > 0 else 0.0
            print(f"Max special var value: {max_special_var_value} and number of special values: {len(special_var_values)}")

            if self.have_penalty_ and max_special_var_value > self.EPSILON:
                self.penalty_value *= 0.9
                if self.penalty_value < self.EPSILON:
                    self.penalty_value = 0
            
            self.meta_iteration += 1
            
        self._VRP__lp_cost = stabilized_iter_solution.cost

        master_solution = self.last_iteration()

        self._VRP__total_problem_time = time.time() - time_start
        print(f"Time ratio subproblem/total: {self._VRP__total_subproblem_time / self._VRP__total_problem_time} | Total time: {self._VRP__total_problem_time} s")

        return master_solution
    
    def cg_iterations(self, subproblem_max_nb_solutions: Optional[int] = None):
        min_reduced_cost = -math.inf

        while min_reduced_cost < -self.EPSILON:
            print("*********************************************")
            print(
                f"nb_iter={self._VRP__n_iterations} | min_reduced_cost={min_reduced_cost} "
                f"| EPSILON={self.EPSILON}"
            )
            print("*********************************************")

            master_problem = DualBoxMasterProblem(self._VRP__instance.get_demand_customers_id(), self.dual_box_center_, self.dual_box_radius_, self.penalty_value)
            master_solution, negative_red_cost_solutions, min_reduced_cost = self.column_generation_iteration(subproblem_max_nb_solutions, master_problem)

            self.add_paths(negative_red_cost_solutions)

            self._VRP__n_iterations += 1
            
        return master_solution
