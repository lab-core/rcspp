#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

import math
import time
from typing import Optional

from vrp.cg.dual_box_master_problem import DualBoxMasterProblem
from vrp.instance import Instance
from vrp.vrp import VRP
from utils import dict_l1_norm

class DualBoxVRP(VRP):
    def __init__(self, instance: Instance, dual_box_center: dict, kappa=1, penalty = 1000, verbose=True):
        super().__init__(instance, verbose)
        self.dual_box_center_ = dual_box_center
        self.box_radius = dict_l1_norm(dual_box_center)/kappa
        self.penalty_value = penalty
        self.meta_iteration = 0
        self.compute_first_lagrangian_bound(dual_box_center)

    def solve(self, subproblem_max_nb_solutions: Optional[int] = None):
        time_start = time.time()
        self.generate_initial_paths()
        max_special_var_value = math.inf
        stabilized_iter_solution = None

        while True:
            stabilized_iter_solution = self.cg_iterations(subproblem_max_nb_solutions)

            special_var_values = [value for var, value in stabilized_iter_solution.value_by_var_id.items() if isinstance(var, str) and var.startswith("y")]
            max_special_var_value = max(special_var_values) if len(special_var_values) > 0 else 0.0
            print(f"Max special var value: {max_special_var_value} and number of special values: {len(special_var_values)}")

            if max_special_var_value > self.EPSILON:
                self.penalty_value = self.penalty_value/10
                if self.penalty_value < self.EPSILON:
                    self.penalty_value = None
            
            self.meta_iteration += 1

            if max_special_var_value < self.EPSILON:
                break

        master_solution = self.last_iteration()

        self._VRP__total_problem_time = time.time() - time_start
        print(f"Time ratio subproblem/total: {self._VRP__total_subproblem_time / self._VRP__total_problem_time} | Total time: {self._VRP__total_problem_time} s")

        return master_solution
    
    def cg_iterations(self, subproblem_max_nb_solutions: Optional[int] = None):
        min_reduced_cost = -math.inf

        while True:
            print(self.box_radius)
            self.print_begin_iteration(min_reduced_cost)
            
            master_problem = DualBoxMasterProblem(self._VRP__instance.get_demand_customers_id(), self.dual_box_center_, self.box_radius, self.penalty_value, verbose=self._VRP__verbose)
            master_solution, negative_red_cost_solutions, min_reduced_cost = self.column_generation_iteration(subproblem_max_nb_solutions, master_problem)

            #values = [value for var, value in master_solution.value_by_var_id.items() if isinstance(var, int)]
            #print(f"Nombre de vehicules ? : {sum(values)}")
            
            lb = sum([i for i in master_solution.dual_by_var_id.values()]) + self._VRP__instance.get_nb_vehicles() *min_reduced_cost
            if lb > self.best_lagrangian_lb:
                self.best_lagrangian_lb = lb
                self.dual_box_center_ = master_solution.dual_by_var_id
                print("Changement de centre")

            special_var_values = [value for var, value in master_solution.value_by_var_id.items() if isinstance(var, str) and var.startswith("y")]
            max_special_var_value = max(special_var_values) if len(special_var_values) > 0 else 0.0

            if max_special_var_value < self.EPSILON:
                self.box_radius *= 0.5

            self.add_paths(negative_red_cost_solutions)

            self._VRP__n_iterations += 1

            if min_reduced_cost > -self.EPSILON or self._VRP__n_iterations > 1000:
                break
            
        return master_solution