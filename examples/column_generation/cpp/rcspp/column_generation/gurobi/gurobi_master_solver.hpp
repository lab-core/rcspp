// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <gurobi_c++.h>

#include <cstddef>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "rcspp/column_generation/master_solver.hpp"
#include "rcspp/column_generation/master_solver_support.hpp"
#include "rcspp/column_generation/types.hpp"

namespace rcspp::column_generation {

/// @brief A @c MasterSolver over a Gurobi model the user built.
///
/// The user creates the master in Gurobi (its rows, and any variables of their own, such as
/// slacks or starting columns) and passes the rows that pricing columns enter, in the order that
/// @c Row::index refers to. The master solver adds the generated columns to that model and reads
/// its duals. During a run the model is the master solver's: what it holds between two solves
/// must not be relied on. When the run ends, @c ColumnGeneration has it put back the user's
/// variable types (@c restore_user_model), so the user can keep working with the model, which
/// then holds the generated columns too, continuous.
///
/// - **Sense:** a maximizing model works too: the master solver negates the costs it adds, the
///   objective and the duals, so that the master is seen as a minimization.
/// - **Integrality:** every LP solve relaxes the model's integer variables in place, those the
///   user adds during the run included, a binary kept within 0 and 1 by bounds whatever its own;
///   the integer solve restores them and gives the generated columns the requested type. The model
///   must become an LP once relaxed (no SOS or general constraints). The types restored are those
///   the variables had when first seen: a type the user changes during the run is overwritten, and
///   variables must not be deleted during it.
/// - **Solver options:** every solve sets `TimeLimit`, `Threads` and `OutputFlag` (and `MIPGap`
///   for the integer solve) from its parameters, and puts the user's values back after it. LP
///   solves use primal simplex while `Method` is automatic (the basis stays primal feasible after
///   columns are added).
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
        /// @throws std::invalid_argument For a row with two different finite bounds (a range:
        ///                               build it in the model instead), or with bounds that
        ///                               no Gurobi row holds (NaN, `lower = +∞`, `upper = −∞`).
        ///                               A refused batch adds no row.
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

        /// @brief Puts back the types of the user's variables, after an LP solve relaxed them;
        ///        the generated columns stay continuous.
        ///
        /// Gurobi applies the types at the model's next update (or optimize): until then, the
        /// last solve's values can still be read.
        void restore_user_model() override;

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

        // Bounds the user's @p binaries within 0 and 1 while they are continuous, keeping their
        // own bounds to put back.
        void bound_binaries(std::vector<GRBVar>& binaries);

        // Gives back the user's types, and @p type to the generated columns.
        void restore(char type);

        // Records the user's non-continuous variables added since the last call; called before
        // changing any type.
        void record_new_user_types();

        // Optimizes; an "infeasible or unbounded" answer is solved again without dual
        // reductions, to tell the two apart, within what is left of the time limit.
        SolverStatus optimize();

        GRBModel& model_;
        std::vector<GRBConstr> rows_;
        SolverColumns<GRBVar> columns_;
        // The user's non-continuous variables and their types, and how many of the model's
        // variables have been looked at.
        std::vector<std::pair<GRBVar, char>> user_types_;
        size_t recorded_variables_ = 0;
        // Every variable is continuous (an LP solve came last).
        bool relaxed_ = false;
        // The user's binaries made continuous, with their own bounds.
        struct RelaxedBinary {
                GRBVar variable;
                double lower;
                double upper;
        };
        std::vector<RelaxedBinary> relaxed_binaries_;
};

}  // namespace rcspp::column_generation
