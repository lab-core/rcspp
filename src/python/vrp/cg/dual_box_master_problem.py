import mip
from vrp.cg.mp_solution import MPSolution
from vrp.cg.master_problem import MasterProblem, _make_model

class DualBoxMasterProblem(MasterProblem):
    """Master problem avec stabilisation par boîte duale.
    
    Ajoute des variables de stabilisation y1 et y2 pour chaque nœud,
    avec contraintes : sum(path_visits) - y1 + y2 = 1
    et optionnellement : y1 ≤ penalty, y2 ≤ penalty
    """
    
    def __init__(self, node_ids, dual_box_center, dual_box_radius: float, 
                 penalty=None):
        super().__init__(node_ids)
        self.left_special_var_by_id = {}
        self.right_special_var_by_id = {}
        self.penalty = penalty
        self.have_penalty_ = (
            self.penalty is not None and isinstance(self.penalty, float)
        )
        if self.have_penalty_:
            self._left_penalty_constraints_by_id = {}
            self._right_penalty_constraints_by_id = {}

        if isinstance(dual_box_center, dict):
            self.dual_box_center_ = [dual_box_center[node_id] 
                                     for node_id in node_ids]
        else:
            self.dual_box_center_ = dual_box_center
        self.dual_box_radius = dual_box_radius

    def _build_model(self) -> None:
        """Construit le modèle LP avec variables de stabilisation."""
        m = _make_model("dual_box_master_problem")
        
        # Ajouter les variables de chemin
        for pid, cost, _ in self._columns:
            self._path_vars[pid] = m.add_var(
                name=f"y_{pid}", lb=0.0, obj=cost, var_type=mip.CONTINUOUS
            )
        
        # Ajouter les variables spéciales et construire les contraintes
        for i, node_id in enumerate(self.node_ids_):
            cost_left = self.dual_box_radius - self.dual_box_center_[i]
            cost_right = self.dual_box_radius + self.dual_box_center_[i]

            left_var = m.add_var(
                lb=0.0,
                ub=mip.INF,
                obj=cost_left,
                var_type=mip.CONTINUOUS,
                name=f"y1_{node_id}",
            )
            self.left_special_var_by_id[node_id] = left_var

            right_var = m.add_var(
                lb=0.0,
                ub=mip.INF,
                obj=cost_right,
                var_type=mip.CONTINUOUS,
                name=f"y2_{node_id}",
            )
            self.right_special_var_by_id[node_id] = right_var

            # Contrainte: sum(path_visits) - y1 + y2 = 1
            terms = [
                coeffs[node_id] * self._path_vars[pid]
                for pid, _, coeffs in self._columns
                if node_id in coeffs
            ]
            terms.append(-left_var)
            terms.append(right_var)
            
            self._constrs[node_id] = m.add_constr(
                mip.xsum(terms) == 1.0, name=f"c_{node_id}"
            )

            if self.have_penalty_:
                left_constr = m.add_constr(
                    left_var <= self.penalty,
                    name=f"c_left_penalty_{node_id}"
                )
                right_constr = m.add_constr(
                    right_var <= self.penalty,
                    name=f"c_right_penalty_{node_id}"
                )
                self._left_penalty_constraints_by_id[node_id] = left_constr
                self._right_penalty_constraints_by_id[node_id] = right_constr

        self._model = m

    def update_center(self, new_center: dict):
        """Met à jour les coefs objectif quand le centre de stabilisation change."""
        if isinstance(new_center, dict):
            self.dual_box_center_ = [new_center[node_id] 
                                     for node_id in self.node_ids_]
        else:
            self.dual_box_center_ = new_center

        for i, node_id in enumerate(self.node_ids_):
            self.left_special_var_by_id[node_id].obj = (
                self.dual_box_radius - self.dual_box_center_[i]
            )
            self.right_special_var_by_id[node_id].obj = (
                self.dual_box_radius + self.dual_box_center_[i]
            )

    def update_radius(self, new_radius: float):
        self.dual_box_radius = new_radius
        for i, node_id in enumerate(self.node_ids_):
            self.left_special_var_by_id[node_id].obj = (
                self.dual_box_radius - self.dual_box_center_[i]
            )
            self.right_special_var_by_id[node_id].obj = (
                self.dual_box_radius + self.dual_box_center_[i]
            )

    def update_penalty(self, new_penalty: float):
        """Met à jour la valeur RHS des contraintes de pénalité."""
        self.penalty = new_penalty
        for node_id in self.node_ids_:
            self._left_penalty_constraints_by_id[node_id].rhs = new_penalty
            self._right_penalty_constraints_by_id[node_id].rhs = new_penalty

    def _extract(self, dual: bool) -> MPSolution:
        """Extrait la solution du modèle, incluant les variables spéciales."""
        solution = super()._extract(dual)
        
        # Ajouter les valeurs des variables spéciales y1 et y2
        for node_id in self.node_ids_:
            solution.value_by_var_id[f"y1_{node_id}"] = self.left_special_var_by_id[node_id].x
            solution.value_by_var_id[f"y2_{node_id}"] = self.right_special_var_by_id[node_id].x
        
        return solution