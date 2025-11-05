// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once
#include <gurobi_c++.h>

#include <map>
#include <vector>

#include "mp_solution.hpp"
#include "path.hpp"

class MasterProblem {
    public:
        MasterProblem(const std::vector<size_t>& node_ids);

        void construct_model(const std::vector<Path>& paths);

        MPSolution solve(bool relax = false);

    private:
        static GRBEnv* env_;
        GRBModel model_;
        GRBLinExpr objective_lin_expr_;

        std::vector<size_t> node_ids_;

        std::map<size_t, GRBVar> path_variables_by_id_;
        std::map<size_t, GRBConstr> node_constraints_by_id_;
        std::map<size_t, Path> paths_by_id_;
        std::map<size_t, double> dual_by_id_;

        void add_variables(const std::vector<Path>& paths);

        void set_objective();

        void add_constraints();

        void add_node_constraint(size_t node_id);

        static GRBEnv init_env();

        [[nodiscard]] MPSolution extract_solution(const GRBModel& model, bool dual) const;
};
