// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#include "master_problem.hpp"

#include <algorithm>

#include "gurobi_c++.h"
#include "mp_solution.hpp"
#include "rcspp/rcspp.hpp"

MasterProblem::MasterProblem(const std::vector<size_t>& node_ids)
    : node_ids_(node_ids), model_(MasterProblem::init_env()) {}

void MasterProblem::construct_model(const std::vector<Path>& paths) {
    add_constraints();
    model_.update();
    add_columns(paths);
}

void MasterProblem::add_columns(const std::vector<Path>& paths) {
    for (const auto& path : paths) {
        // Create column for the path variable
        GRBColumn col;
        for (const auto& [node_id, cons] : node_constraints_by_id_) {
            double path_visits_node =
                std::count(path.visited_nodes.begin(), path.visited_nodes.end(), node_id);
            if (path_visits_node > 0) {
                col.addTerm(path_visits_node, cons);
            }
        }
        // Create the path variable
        std::string path_var_name = "y_" + std::to_string(path.id);
        auto path_var =
            model_.addVar(0.0, GRB_INFINITY, path.cost, GRB_CONTINUOUS, col, path_var_name);
        path_variables_by_id_.emplace(path.id, path_var);
        paths_by_id_.emplace(path.id, path);
    }
}

void MasterProblem::add_constraints() {
    for (auto node_id : node_ids_) {
        GRBLinExpr constr_lin_expr_lhs;
        GRBLinExpr constr_lin_expr_rhs = 1;
        std::string constr_name = "c_" + std::to_string(node_id);
        auto constr =
            model_.addConstr(constr_lin_expr_lhs, GRB_EQUAL, constr_lin_expr_rhs, constr_name);
        node_constraints_by_id_.emplace(node_id, constr);
    }
}

MPSolution MasterProblem::solve(bool integer) {
    LOG_TRACE(__FUNCTION__, '\n');

    MPSolution solution;

    if (!integer) {
        model_.optimize();
        solution = extract_solution(model_, integer);
    } else {
        for (auto& [path_id, var] : path_variables_by_id_) {
            var.set(GRB_CharAttr_VType, GRB_INTEGER);
        }

        model_.optimize();
        solution = extract_solution(model_, integer);

        for (auto& [path_id, var] : path_variables_by_id_) {
            var.set(GRB_CharAttr_VType, GRB_CONTINUOUS);
        }
    }

    return solution;
}

std::unique_ptr<GRBEnv> MasterProblem::env_ = nullptr;

GRBEnv MasterProblem::init_env() {
    if (MasterProblem::env_ != nullptr) {
        return *MasterProblem::env_;
    }

    MasterProblem::env_ = std::make_unique<GRBEnv>(true);
    // Turn off console output
    MasterProblem::env_->set(GRB_IntParam_OutputFlag, 0);
    MasterProblem::env_->start();

    return *MasterProblem::env_;
}

MPSolution MasterProblem::extract_solution(const GRBModel& model, bool integer) const {
    std::map<size_t, double> value_by_var_id;
    std::map<size_t, double> dual_by_var_id;

    for (const auto& [path_id, path_var] : path_variables_by_id_) {
        auto var = model.getVar((int)path_id);
        value_by_var_id.emplace(path_id, var.get(GRB_DoubleAttr_X));
    }

    if (!integer) {
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
