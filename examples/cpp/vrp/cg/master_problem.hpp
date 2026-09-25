// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once
#include <gurobi_c++.h>

#include <functional>
#include <map>
#include <vector>

#include "mp_solution.hpp"
#include "path.hpp"

class MasterProblem {
    public:
        MasterProblem(const std::vector<size_t>& node_ids);

        void construct_model(const std::vector<Path>& paths);

        MPSolution solve(bool integer = false);

        void add_columns(const std::vector<Path>& paths, size_t max_paths = 100);  // NOLINT

        /// @brief Every column in the model, by path id.
        [[nodiscard]] const std::map<size_t, Path>& paths() const { return paths_by_id_; }

        /// @brief Removes every column @p drop selects, between solves; returns how many.
        ///
        /// Removes each variable from the Gurobi model and its path from the maps, then updates the
        /// model. Throws `std::logic_error` if that would leave a customer with no column covering
        /// it, which would make the master infeasible.
        ///
        /// @param drop Whether a column should go.
        /// @return The number of columns removed.
        size_t remove_columns(const std::function<bool(const Path&)>& drop);

    private:
        static std::unique_ptr<GRBEnv> env_;
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
