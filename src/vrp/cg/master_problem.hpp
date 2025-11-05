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

        MPSolution solve(bool integer = false);

        void add_columns(const std::vector<Path>& paths);

    private:
        static GRBEnv* env_;
        GRBModel model_;

        std::vector<size_t> node_ids_;

        std::map<size_t, GRBVar> path_variables_by_id_;
        std::map<size_t, GRBConstr> node_constraints_by_id_;
        std::map<size_t, Path> paths_by_id_;
        std::map<size_t, double> dual_by_id_;

        void add_constraints();

        static GRBEnv init_env();

        [[nodiscard]] MPSolution extract_solution(const GRBModel& model, bool integer) const;
};
