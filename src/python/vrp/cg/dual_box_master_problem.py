from gurobipy import GRB, LinExpr, Model
from vrp.cg.mp_solution import MPSolution
from vrp.cg.master_problem import MasterProblem

class DualBoxMasterProblem(MasterProblem):
    def __init__(self, node_ids, dual_box_center, dual_box_radius, perturbation=0.0):
        super().__init__(node_ids)
        self.left_special_var_by_id = {}
        self.right_special_var_by_id = {}
        self.perturbation_ = perturbation
        if perturbation > 0.0:
            self.perturbation_values_ = [perturbation for i in range(len(node_ids))]
            self.have_perturbation_ = True
            self.__left_perturbation_constraints_by_id = {}
            self.__right_perturbation_constraints_by_id = {}
        else:
            self.perturbation_values_ = None
            self.have_perturbation_ = False
        if isinstance(dual_box_center, dict):
            self.dual_box_center_ = [dual_box_center[node_id] for node_id in node_ids]
        else:
            self.dual_box_center_ = dual_box_center
        if isinstance(dual_box_radius, dict):
            self.dual_box_radius_ = [dual_box_radius[node_id] for node_id in node_ids]
        else:
            self.dual_box_radius_ = dual_box_radius
    
    def add_variables(self, paths):
        super().add_variables(paths)

        for i in range(len(self.node_ids_)):
            node_id = self.node_ids_[i]
            special_var_name_left = f"y1_{node_id}"
            special_var_left = self.model_.addVar(lb=0.0, 
                                                 ub=GRB.INFINITY, 
                                                 vtype=GRB.CONTINUOUS, 
                                                 name=special_var_name_left)
            self.left_special_var_by_id[node_id] = special_var_left

            special_var_name_right = f"y2_{node_id}"
            special_var_right = self.model_.addVar(lb=0.0, 
                                                  ub=GRB.INFINITY, 
                                                  vtype=GRB.CONTINUOUS, 
                                                  name=special_var_name_right)
            self.right_special_var_by_id[node_id] = special_var_right

    def set_objective(self):
        self._MasterProblem__objective_lin_expr.clear()
        total_cost = 0.0

        for path_id, path in self._MasterProblem__paths_by_id.items():
            path_var = self._MasterProblem__path_variables_by_id[path_id]
            total_cost += path.cost
            self._MasterProblem__objective_lin_expr += path.cost * path_var

        for i in range(len(self.node_ids_)):
            node_id = self.node_ids_[i]
            special_var_left = self.left_special_var_by_id[node_id]
            special_var_right = self.right_special_var_by_id[node_id]
            self._MasterProblem__objective_lin_expr += (self.dual_box_radius_[i] - self.dual_box_center_[i])*special_var_left
            self._MasterProblem__objective_lin_expr += (self.dual_box_radius_[i] + self.dual_box_center_[i])*special_var_right

        self.model_.setObjective(self._MasterProblem__objective_lin_expr)

    def add_constraints(self):
        for i in range(len(self.node_ids_)):
            self.add_node_constraint(i)

    def add_node_constraint(self, i):
        node_id = self.node_ids_[i]
        constr_lin_expr_lhs = LinExpr()
        constr_lin_expr_rhs = 1.0

        for path_id, path in self._MasterProblem__paths_by_id.items():
            path_var = self._MasterProblem__path_variables_by_id[path_id]
            path_visits_node = path.visited_nodes.count(node_id)
            constr_lin_expr_lhs += path_visits_node * path_var
        
        constr_lin_expr_lhs -= self.left_special_var_by_id[node_id]
        constr_lin_expr_lhs += self.right_special_var_by_id[node_id]

        constr_name = f"c_{node_id}"
        constr = self.model_.addConstr(constr_lin_expr_lhs == constr_lin_expr_rhs, name=constr_name)
        self._MasterProblem__node_constraints_by_id[node_id] = constr

        if self.have_perturbation_:
            left_constr_lin_expr = LinExpr()
            left_constr_lin_expr += self.left_special_var_by_id[node_id]
            left_constr_lin_expr -= self.perturbation_values_[i]
            left_constr = self.model_.addConstr(left_constr_lin_expr <= 0, name=f"c_left_perturb_{node_id}")
            right_constr_lin_expr = LinExpr()
            right_constr_lin_expr += self.right_special_var_by_id[node_id]
            right_constr_lin_expr -= self.perturbation_values_[i]
            right_constr = self.model_.addConstr(right_constr_lin_expr <= 0, name=f"c_right_perturb_{node_id}")
            
            self.__left_perturbation_constraints_by_id[node_id] = left_constr
            self.__right_perturbation_constraints_by_id[node_id] = right_constr


    def extract_solution(self, model, dual=False):
        model_variables_by_var_name = {v.VarName: v for v in model.getVars()}
        model_constraints_by_constr_name = {c.ConstrName: c for c in model.getConstrs()}

        value_by_var_id = {}
        dual_by_var_id = {}

        if model.Status in [GRB.OPTIMAL, GRB.SUBOPTIMAL]:
            print(f"model.Status={model.Status} vs GRB.OPTIMAL={GRB.OPTIMAL}")
            # Variable values
            for path_id, path_var in self._MasterProblem__path_variables_by_id.items():
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

            # Dual values
            if dual:
                for node_id, node_constr in self._MasterProblem__node_constraints_by_id.items():
                    model_node_constr = model_constraints_by_constr_name[
                        node_constr.ConstrName
                    ]  # Necessary if model is relaxed
                    dual_by_var_id[node_id] = model_node_constr.Pi  # reduced cost / dual val

            cost = model.ObjVal

        solution = MPSolution()
        solution.value_by_var_id = value_by_var_id
        solution.dual_by_var_id = dual_by_var_id
        solution.cost = cost

        print(f"solution.cost={solution.cost}")
        return solution