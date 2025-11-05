// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "master_problem.hpp"

#include <algorithm>

#include "gurobi_c++.h"
#include "mp_solution.hpp"
#include "rcspp/utils/logger.hpp"

MasterProblem::MasterProblem(const std::vector<size_t>& node_ids)
    : node_ids_(node_ids), model_(MasterProblem::init_env()) {}

void MasterProblem::construct_model(const std::vector<Path>& paths) {
    add_variables(paths);

    add_constraints();

    set_objective();

    model_.update();
}

void MasterProblem::add_variables(const std::vector<Path>& paths) {
    for (const auto& path : paths) {
        std::string path_var_name = "y_" + std::to_string(path.id);

        auto path_var = model_.addVar(0.0, 1.0, 0.0, GRB_BINARY, path_var_name);
        path_variables_by_id_.emplace(path.id, path_var);
        paths_by_id_.emplace(path.id, path);
    }
}

void MasterProblem::set_objective() {
    objective_lin_expr_.clear();

    double total_cost = 0.0;

    for (const auto& id_path_pair : paths_by_id_) {
        auto path_id = id_path_pair.first;
        const auto& path = id_path_pair.second;
        const auto& path_var = path_variables_by_id_.at(path_id);

        total_cost += path.cost;
        objective_lin_expr_ += path.cost * path_var;
    }

    model_.setObjective(objective_lin_expr_);
}

void MasterProblem::add_constraints() {
    for (auto node_id : node_ids_) {
        add_node_constraint(node_id);
    }
}

void MasterProblem::add_node_constraint(size_t node_id) {
    GRBLinExpr constr_lin_expr_lhs;
    GRBLinExpr constr_lin_expr_rhs = 1;

    for (const auto& [path_id, path] : paths_by_id_) {
        const auto& path_var = path_variables_by_id_.at(path_id);

        double path_visits_node =
            std::count(path.visited_nodes.begin(), path.visited_nodes.end(), node_id);
        /*if (std::ranges::contains(path.visited_nodes, node_id)) {
          path_visits_node = 1.0;
        }*/

        constr_lin_expr_lhs += path_visits_node * path_var;
    }

    std::string constr_name = "c_" + std::to_string(node_id);

    auto constr =
        model_.addConstr(constr_lin_expr_lhs, GRB_EQUAL, constr_lin_expr_rhs, constr_name);

    node_constraints_by_id_.emplace(node_id, constr);
}

MPSolution MasterProblem::solve(bool relax) {
    LOG_TRACE(__FUNCTION__, '\n');

    MPSolution solution;

    if (relax) {
        auto relaxed_model = model_.relax();
        relaxed_model.optimize();
        solution = extract_solution(relaxed_model, relax);
    } else {
        model_.optimize();
        solution = extract_solution(model_, relax);
    }

    return solution;
}

GRBEnv* MasterProblem::env_ = nullptr;

GRBEnv MasterProblem::init_env() {
    if (MasterProblem::env_ != nullptr) {
        return *MasterProblem::env_;
    }

    MasterProblem::env_ = new GRBEnv(true);
    // Turn off console output
    MasterProblem::env_->set(GRB_IntParam_OutputFlag, 0);
    MasterProblem::env_->start();

    return *MasterProblem::env_;
}

MPSolution MasterProblem::extract_solution(const GRBModel& model, bool dual) const {
    std::map<size_t, double> value_by_var_id;
    std::map<size_t, double> dual_by_var_id;

    for (const auto& [path_id, path_var] : path_variables_by_id_) {
        auto var = model.getVar((int)path_id);
        value_by_var_id.emplace(path_id, var.get(GRB_DoubleAttr_X));
    }

    if (dual) {
        for (const auto& [node_id, node_constr] : this->node_constraints_by_id_) {
            auto constr = model.getConstr((int)node_id - 1);
            dual_by_var_id.emplace(node_id, constr.get(GRB_DoubleAttr_Pi));
        }
    }

    auto objective = model.getObjective();

    MPSolution solution;
    solution.value_by_var_id = value_by_var_id;
    solution.dual_by_var_id = dual_by_var_id;
    solution.cost = objective.getValue();

    return solution;
}
