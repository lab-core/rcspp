#  Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
#  All rights reserved.

from gurobipy import GRB, Column, LinExpr, Model, Env
from vrp.cg.mp_solution import MPSolution


class MasterProblem:
    def __init__(self, node_ids, verbose=True):
        self.node_ids_ = node_ids
        self.verbose = verbose
        if not self.verbose:
            with Env(empty=True) as env:
                env.setParam('OutputFlag', 0)
                env.start()
                self.model_ = Model("master_problem", env=env)
        else:
            self.model_ = Model("master_problem")

        # Maps
        self.__path_variables_by_id = {}
        self.__paths_by_id = {}
        self.__node_constraints_by_id = {}
        self.__objective_lin_expr = LinExpr()

    def construct_model(self, paths):
        self.add_variables(paths)
        self.add_constraints()
        self.set_objective()
        self.model_.update()

    def add_variables(self, paths):
        for path in paths:
            self._add_single_variable(path)

    def _add_single_variable(self, path):
        path_var_name = f"y_{path.id}"
        path_var = self.model_.addVar(lb=0.0, obj=path.cost, vtype=GRB.BINARY, name=path_var_name)
        self.__path_variables_by_id[path.id] = path_var
        self.__paths_by_id[path.id] = path

    def set_objective(self):
        self.model_.ModelSense = GRB.MINIMIZE
        self.model_.update()

    def add_constraints(self):
        for i in range(len(self.node_ids_)):
            self.add_node_constraint(i)

    def add_node_constraint(self, i):
        node_id = self.node_ids_[i]
        constr_lin_expr_lhs = LinExpr()
        constr_lin_expr_rhs = 1.0

        for path_id, path in self.__paths_by_id.items():
            path_var = self.__path_variables_by_id[path_id]
            path_visits_node = path.visited_nodes.count(node_id)
            constr_lin_expr_lhs += path_visits_node * path_var

        constr_name = f"c_{node_id}"
        constr = self.model_.addConstr(constr_lin_expr_lhs == constr_lin_expr_rhs, name=constr_name)
        self.__node_constraints_by_id[node_id] = constr

    def solve(self, relax=False):
        solution = MPSolution()

        if relax:
            relaxed_model = self.model_.relax()
            relaxed_model.optimize()
            solution = self.extract_solution(relaxed_model, dual=True)
        else:
            self.model_.optimize()
            solution = self.extract_solution(self.model_, dual=False)

        return solution

    def extract_solution(self, model, dual=False):
        model_variables_by_var_name = {v.VarName: v for v in model.getVars()}
        model_constraints_by_constr_name = {c.ConstrName: c for c in model.getConstrs()}

        value_by_var_id = {}
        dual_by_var_id = {}

        if model.Status in [GRB.OPTIMAL, GRB.SUBOPTIMAL]:
            if self.verbose:
                print(f"model.Status={model.Status} vs GRB.OPTIMAL={GRB.OPTIMAL}")
            # Variable values
            for path_id, path_var in self.__path_variables_by_id.items():
                model_path_var = model_variables_by_var_name[
                    path_var.VarName
                ]  # Necessary if model is relaxed
                value_by_var_id[path_id] = model_path_var.X  # .X gives solution value

            # Dual values
            if dual:
                for node_id, node_constr in self.__node_constraints_by_id.items():
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


    def add_column(self, path):
        col = Column()

        for node_id in self.node_ids_:
            coeff = path.visited_nodes.count(node_id)
            if coeff != 0:
                col.addTerms(coeff, self.__node_constraints_by_id[node_id])

        path_var_name = f"y_{path.id}"
        path_var = self.model_.addVar(
            lb=0.0,
            obj=path.cost,        # ← coût directement dans addVar, pas besoin de set_objective
            vtype=GRB.BINARY,
            name=path_var_name,
            column=col
        )
        self.__path_variables_by_id[path.id] = path_var
        self.__paths_by_id[path.id] = path

        self.model_.update()