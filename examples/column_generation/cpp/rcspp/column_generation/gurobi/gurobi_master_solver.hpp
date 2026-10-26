// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <gurobi_c++.h>

#include <cstddef>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "rcspp/column_generation/master_solver.hpp"
#include "rcspp/column_generation/types.hpp"

namespace rcspp::column_generation {

/// @brief A @c MasterSolver over a Gurobi model the user built.
///
/// The user creates the master in Gurobi (its rows, and any variables of their own, such as
/// slacks or starting columns) and passes the rows that pricing columns enter, in the order that
/// @c Row::index refers to. The master solver adds the generated columns to that model and reads
/// its duals, so the user can keep working with the model after the run.
///
/// - **Sense:** a maximizing model works too: the master solver negates the costs it adds, the
///   objective and the duals, so that the master is seen as a minimization.
/// - **Integrality:** every LP solve relaxes the model's integer variables in place; the integer
///   solve restores them and gives the generated columns the requested type. The model must
///   become an LP once relaxed (no SOS or general constraints).
/// - **Solver options:** every solve sets `TimeLimit`, `Threads` and `OutputFlag` from its
///   parameters. LP solves use primal simplex while `Method` is automatic (the basis stays primal
///   feasible after columns are added), then put `Method` back.
/// - **Errors:** Gurobi's own errors are thrown as @c GRBException.
///
/// Holds the model by reference: the caller keeps it alive.
class GurobiMasterSolver final : public MasterSolver {
    public:
        /// @brief Wraps @p model.
        ///
        /// @param model The master, built by the user.
        /// @param rows  Its rows that columns enter: `=`, `≥` or `≤` constraints (not ranges),
        ///              in master row order.
        GurobiMasterSolver(GRBModel& model, std::vector<GRBConstr> rows);

        [[nodiscard]] size_t number_of_rows() const override { return rows_.size(); }

        void row_bounds(std::span<double> lower, std::span<double> upper) const override;

        /// @brief Appends `=`, `≥`, `≤` or free rows to the model.
        ///
        /// @throws std::invalid_argument For a row with two different finite bounds (a range):
        ///                               build it in the model instead.
        void add_rows(const RowBatch& rows) override;

        void add_columns(const ColumnBatch& columns) override;

        SolverStatus solve_relaxation(const RelaxationParams& params) override;

        [[nodiscard]] double objective() const override;

        void duals(std::span<double> out) const override;

        /// @copydoc MasterSolver::primal
        /// @throws std::out_of_range For an id this master solver did not receive.
        void primal(std::span<const ColumnId> ids, std::span<double> out) const override;

        SolverStatus solve_integer(const IntegerParams& params) override;

        /// @brief Sets the Gurobi parameter @p name (case-insensitive, as in Gurobi).
        void set_option(std::string_view name, const OptionValue& value) override;

        [[nodiscard]] Capabilities capabilities() const override {
            return Capabilities{.add_rows = true, .integer_solve = true};
        }

        /// @brief The wrapped model.
        [[nodiscard]] GRBModel& model() const { return model_; }

        /// @brief The Gurobi variable of column @p id.
        /// @throws std::out_of_range For an id this master solver did not receive.
        [[nodiscard]] GRBVar variable(ColumnId id) const;

    private:
        // +1 for a minimizing model, −1 for a maximizing one: turns its values into ours.
        [[nodiscard]] double sense_sign() const;

        // Makes every variable continuous.
        void relax();

        // Gives back the user's types, and @p type to the generated columns.
        void restore(char type);

        // Records the user's non-continuous variables, once, before changing any type.
        void remember_user_types();

        // Optimizes; an "infeasible or unbounded" answer is solved again without dual
        // reductions, to tell the two apart.
        SolverStatus optimize();

        GRBModel& model_;
        std::vector<GRBConstr> rows_;
        std::vector<GRBVar> columns_;
        std::unordered_map<ColumnId, size_t> column_index_;
        // The user's non-continuous variables and their types, once recorded.
        std::vector<std::pair<GRBVar, char>> user_types_;
        bool user_types_known_ = false;
        // Every variable is continuous (an LP solve came last).
        bool relaxed_ = false;
};

}  // namespace rcspp::column_generation
