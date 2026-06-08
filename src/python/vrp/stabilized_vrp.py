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
    def __init__(self, instance: Instance, dual_estimate = None):
        super().__init__(instance)
        self.kappa = 1
        self.penalty_value = 0.9
        self.meta_iteration = 1
        self.nb_new_center = 0
        self.dual_estimate = dual_estimate

    def solve(self, subproblem_max_nb_solutions: Optional[int] = None):
        from vrp.cg.master_problem import MasterProblem  # requires mip

        self.generate_initial_paths()

        self._n_iterations = 0

        self.start_time = time.time()

        self._initialize_dual_box(subproblem_max_nb_solutions)
        self._run_stabilized_cg(subproblem_max_nb_solutions)

        mp_sol = self.master_problem.solve(relax=False)
        mp_sol.dual_by_var_id = self.final_dual_by_id

        self._total_problem_time = time.time() - self.start_time
        print(f"Time ratio subproblem/total: {self._total_subproblem_time / self._total_problem_time} | Total time: {self._total_problem_time} s")

        return mp_sol
    
    def cg_iterations(self, subproblem_max_nb_solutions: Optional[int] = None):
        min_reduced_cost = -math.inf
        while min_reduced_cost < -self.EPSILON:
            print("*" * 45)
            print(f"iter={self._n_iterations}  min_rc={min_reduced_cost:.6f}")
            print("*" * 45)

            mp_sol = self.master_problem.solve(relax=True)
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
            self.master_problem.add_paths(new_paths)

            lb = sum([i for i in dual_by_id.values()]) + self._instance.get_nb_vehicles() * min_reduced_cost
            if lb > self.best_lagrangian_lb + self.EPSILON:
                self.best_lagrangian_lb = lb
                self._change_center(dual_by_id)

            self.special_var_values = self._get_special_var_values(mp_sol)
            self.max_special_var_value = max(self.special_var_values) if len(self.special_var_values) > 0 else 0.0
            print(f"Max special var value: {self.max_special_var_value}")

            if self.max_special_var_value < self.EPSILON:
                self._change_radius(self.box_radius*0.5)

            self._n_iterations += 1
            self._save_state(mp_sol)
            if min_reduced_cost >= -self.EPSILON:
                self.final_dual_by_id = dual_by_id
                self._lp_cost = mp_sol.cost
        print(f"\niter={self._n_iterations}  min_rc={min_reduced_cost:.6f}\n")
        return mp_sol
    
    def _initialize_dual_box(self, subproblem_max_nb_solutions):
        """Initialise le centre et le rayon de la boîte duale."""
        if self.dual_estimate is None:
            master = MasterProblem(self._instance.get_demand_customers_id())
            master.add_paths(self._paths)
            print("Constructing master problem with initial paths...")
            mp_sol = master.solve(relax=True)
            self._n_iterations += 1
            self.dual_box_centre = mp_sol.dual_by_var_id
            self.box_radius = dict_l1_norm(self.dual_box_centre)/(self.kappa * len(self.dual_box_centre))
            self.compute_first_lagrangian_bound(self.dual_box_centre, subproblem_max_nb_solutions)
            self._save_state(mp_sol)
        else:
            print("Using provided dual estimate to initialize the dual box master problem")
            self.dual_box_centre = self.dual_estimate
            self.box_radius = dict_l1_norm(self.dual_box_centre)/(self.kappa * len(self.dual_box_centre))
            self.compute_first_lagrangian_bound(self.dual_box_centre, subproblem_max_nb_solutions)
        

    def _run_stabilized_cg(self, subproblem_max_nb_solutions):
        """Boucle de méta-itération avec réduction de pénalité."""
        self.master_problem = DualBoxMasterProblem(self._instance.get_demand_customers_id(), self.dual_box_centre, self.box_radius, self.penalty_value)
        self.master_problem.add_paths(self._paths)
        print("Constructing dual box master problem with initial paths...")

        while True:
            solution = self.cg_iterations(subproblem_max_nb_solutions)
            if self.max_special_var_value < self.EPSILON:
                break
            else:
                self.penalty_value /= 10
                if self.penalty_value < self.EPSILON:
                    self.penalty_value = 0.0
                self.master_problem.update_penalty(self.penalty_value)
                print(f"New penalty value: {self.penalty_value}")
                self.meta_iteration += 1
        return solution
    
    def _get_special_var_values(self, solution):
        return [value for var, value in solution.value_by_var_id.items() if isinstance(var, str) and var.startswith("y")]
    
    def _change_center(self, new_center):
        self.dual_box_centre = new_center
        self.master_problem.update_center(self.dual_box_centre)
        new_radius = dict_l1_norm(new_center)/(self.kappa * len(new_center))
        self._change_radius(new_radius)
        self.nb_new_center += 1
        print(f"Changing center n°{self.nb_new_center}")

    def _change_radius(self, new_radius: float):
        self.box_radius = new_radius
        self.master_problem.update_radius(self.box_radius)
        print(f"New radius: {new_radius}")

    def _save_state(self, solution):
        state = {"time": time.time() - self.start_time,
                 "n_iter": self._n_iterations,
                 "n_cols": len(self._paths),
                 "value": solution.cost,
                 "best_lb": self.best_lagrangian_lb,
                 "n_meta_iter": self.meta_iteration,
                 "centre_changes": self.nb_new_center,
                 "radius": self.box_radius,
                 "penalty": self.penalty_value}
        
        self._state_history.append(state)

    def compute_first_lagrangian_bound(self, dual_by_id: dict, subproblem_max_nb_solutions):
        solutions = self.solve_subproblem(dual_by_id)
        if subproblem_max_nb_solutions is not None:
            solutions = solutions[:subproblem_max_nb_solutions]
        min_reduced_cost = min((s.cost for s in solutions), default=math.inf)
        improving = [s for s in solutions if s.cost < -self.EPSILON]
        new_paths = self.add_paths(improving)

        lb = sum([i for i in dual_by_id.values()]) + self._instance.get_nb_vehicles() * min_reduced_cost
        self.best_lagrangian_lb = lb