#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

import math
import time
from typing import Optional

from vrp.cg.dual_box_master_problem import DualBoxMasterProblem
from vrp.cg.master_problem import MasterProblem
from vrp.instance import Instance
from vrp.vrp import VRP
from utils.utils import dict_l1_norm

class StabilizedVRP(VRP):
    def __init__(self, instance: Instance, dual_estimate = None, verbose=True):
        super().__init__(instance, verbose)
        self.kappa = 1
        self.penalty_value = 0.9
        self.meta_iteration = 1
        self.nb_new_center = 0
        self.dual_estimate = dual_estimate

    def solve(self, subproblem_max_nb_solutions: Optional[int] = None):
        time_start = time.time()
        self.generate_initial_paths()
        self.max_special_var_value = math.inf
        self.min_reduced_cost = -math.inf

        self._initialize_dual_box(subproblem_max_nb_solutions)
        self._run_stabilized_cg(subproblem_max_nb_solutions)
        master_solution = self.last_iteration()

        self._total_problem_time = time.time() - time_start
        print(f"Time ratio subproblem/total: {self._total_subproblem_time / self._total_problem_time} | Total time: {self._total_problem_time} s")

        return master_solution
    
    def cg_iterations(self, subproblem_max_nb_solutions: Optional[int] = None):
        while True:
            self.print_begin_iteration()
            
            master_solution, negative_red_cost_solutions, self.min_reduced_cost = self.column_generation_iteration(subproblem_max_nb_solutions)
            
            lb = sum(master_solution.dual_by_var_id.values()) + self._instance.get_nb_vehicles() * self.min_reduced_cost
            if lb > self.best_lagrangian_lb + self.EPSILON:
                self.best_lagrangian_lb = lb
                self._change_center(master_solution.dual_by_var_id)
                
                self.vprint(f"New best lagrangian bound: {self.best_lagrangian_lb}")

            self.special_var_values = self._get_special_var_values(master_solution)
            self.max_special_var_value = max(self.special_var_values) if len(self.special_var_values) > 0 else 0.0

            if self.max_special_var_value < self.EPSILON:
                self._change_radius(self.box_radius*0.5)

            self.add_paths(negative_red_cost_solutions)
            self._n_iterations += 1

            if self.min_reduced_cost > -self.EPSILON:
                break

        self._lp_cost = master_solution.cost
        return master_solution
    
    def _initialize_dual_box(self, subproblem_max_nb_solutions):
        """Initialise le centre et le rayon de la boîte duale."""
        self.master_problem = MasterProblem(self._instance.get_demand_customers_id(), verbose=self._verbose)
        self.vprint("Constructing master problem with initial paths...")
        self.master_problem.construct_model(self._paths)

        if self.dual_estimate is None:
            self.first_iteration(subproblem_max_nb_solutions)
        else:
            self.vprint("Using provided dual estimate to initialize the dual box master problem")
            self.dual_box_center_ = self.dual_estimate
            self.box_radius = dict_l1_norm(self.dual_box_center_)/(self.kappa * len(self.dual_box_center_))
            self.compute_first_lagrangian_bound(self.dual_box_center_)

    def _run_stabilized_cg(self, subproblem_max_nb_solutions):
        """Boucle de méta-itération avec réduction de pénalité."""
        self.master_problem = DualBoxMasterProblem(self._instance.get_demand_customers_id(), self.dual_box_center_, self.box_radius, self.penalty_value, verbose=self._verbose)
        self.vprint("Constructing dual box master problem with initial paths...")
        self.master_problem.construct_model(self._paths)

        while True:
            solution = self.cg_iterations(subproblem_max_nb_solutions)
            if not self._reduce_penalty_if_needed(solution):
                break
            self.meta_iteration += 1

    def _reduce_penalty_if_needed(self, solution) -> bool:
        """Retourne True si la pénalité a été réduite (continuer), False sinon."""
        special_vals = self._get_special_var_values(solution)
        max_val = max(special_vals) if special_vals else 0.0
        self.vprint(f"Max special var value: {max_val}, count: {len(special_vals)}")

        if max_val < self.EPSILON:
            return False

        self.penalty_value /= 10
        if self.penalty_value < self.EPSILON:
            self.penalty_value = 0.0
        self.master_problem.update_penalty(self.penalty_value)
        return True
    
    def _get_special_var_values(self, solution):
        return [value for var, value in solution.value_by_var_id.items() if isinstance(var, str) and var.startswith("y")]
    
    def _change_center(self, new_center):
        self.dual_box_center_ = new_center
        self.master_problem.update_center(self.dual_box_center_)
        new_radius = dict_l1_norm(new_center)/(self.kappa * len(new_center))
        self._change_radius(new_radius)
        self.nb_new_center += 1
        self.vprint(f"Changing center n°{self.nb_new_center}")

    def _change_radius(self, new_radius: float):
        self.box_radius = new_radius
        self.master_problem.update_radius(self.box_radius)
