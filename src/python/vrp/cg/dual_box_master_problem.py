from gurobipy import GRB, LinExpr, Model
from vrp.cg.mp_solution import MPSolution
from vrp.cg.master_problem import MasterProblem

class DualBoxMasterProblem(MasterProblem):
    def __init__(self, node_ids, dual_box_center, dual_box_radius: float, 
                 penalty=None, verbose=True):
        super().__init__(node_ids, verbose=verbose)
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

    def add_variables(self, paths):
        super().add_variables(paths)

        for i, node_id in enumerate(self.node_ids_):
            # Coût initial selon le centre courant
            cost_left  =  self.dual_box_radius - self.dual_box_center_[i]
            cost_right =  self.dual_box_radius + self.dual_box_center_[i]

            left_var = self.model_.addVar(
                lb=0.0,
                ub=GRB.INFINITY,
                obj=cost_left,
                vtype=GRB.CONTINUOUS,
                name=f"y1_{node_id}",
            )
            self.left_special_var_by_id[node_id] = left_var

            right_var = self.model_.addVar(
                lb=0.0,
                ub=GRB.INFINITY,
                obj=cost_right,
                vtype=GRB.CONTINUOUS,
                name=f"y2_{node_id}",
            )
            self.right_special_var_by_id[node_id] = right_var

    def add_node_constraint(self, i):
        node_id = self.node_ids_[i]
        constr_lin_expr_lhs = LinExpr()

        for path_id, path in self._paths_by_id.items():
            path_var = self._path_variables_by_id[path_id]
            path_visits_node = path.visited_nodes.count(node_id)
            constr_lin_expr_lhs += path_visits_node * path_var

        constr_lin_expr_lhs -= self.left_special_var_by_id[node_id]
        constr_lin_expr_lhs += self.right_special_var_by_id[node_id]

        constr = self.model_.addConstr(
            constr_lin_expr_lhs == 1.0, name=f"c_{node_id}"
        )
        self._node_constraints_by_id[node_id] = constr

        if self.have_penalty_:
            left_constr_lin_expr = LinExpr()
            left_constr_lin_expr += self.left_special_var_by_id[node_id]
            left_constr_lin_expr -= self.penalty
            left_constr = self.model_.addConstr(left_constr_lin_expr <= 0, name=f"c_left_penalty_{node_id}")
            right_constr_lin_expr = LinExpr()
            right_constr_lin_expr += self.right_special_var_by_id[node_id]
            right_constr_lin_expr -= self.penalty
            right_constr = self.model_.addConstr(right_constr_lin_expr <= 0, name=f"c_right_penalty_{node_id}")
            
            self._left_penalty_constraints_by_id[node_id] = left_constr
            self._right_penalty_constraints_by_id[node_id] = right_constr

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
            self._left_penalty_constraints_by_id[node_id].RHS  = new_penalty
            self._right_penalty_constraints_by_id[node_id].RHS = new_penalty
            

    def extract_solution(self, model, dual=False):
        model_variables_by_var_name = {v.VarName: v for v in model.getVars()}
        model_constraints_by_constr_name = {c.ConstrName: c for c in model.getConstrs()}

        value_by_var_id = {}
        dual_by_var_id = {}

        if model.Status in [GRB.OPTIMAL, GRB.SUBOPTIMAL]:
            for path_id, path_var in self._path_variables_by_id.items():
                model_path_var = model_variables_by_var_name[
                    path_var.VarName
                ]  # Necessary if model is relaxed
                value_by_var_id[path_id] = model_path_var.X  # .X gives solution value

            for i in range(len(self.node_ids_)):
                node_id = self.node_ids_[i]
                model_left_special_var = model_variables_by_var_name[
                    self.left_special_var_by_id[node_id].VarName
                ]
                value_by_var_id[f"y1_{node_id}"] = model_left_special_var.X

                model_right_special_var = model_variables_by_var_name[
                    self.right_special_var_by_id[node_id].VarName
                ]
                value_by_var_id[f"y2_{node_id}"] = model_right_special_var.X

            if dual:
                for node_id, node_constr in self._node_constraints_by_id.items():
                    model_node_constr = model_constraints_by_constr_name[
                        node_constr.ConstrName
                    ]  # Necessary if model is relaxed
                    dual_by_var_id[node_id] = model_node_constr.Pi  # reduced cost / dual val

            cost = model.ObjVal

        solution = MPSolution()
        solution.value_by_var_id = value_by_var_id
        solution.dual_by_var_id  = dual_by_var_id
        solution.cost = cost

        print(f"solution.cost={solution.cost}")
        return solution