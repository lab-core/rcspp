// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <Highs.h>

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "rcspp/column_generation/master_solver.hpp"
#include "rcspp/column_generation/master_solver_support.hpp"
#include "rcspp/column_generation/types.hpp"

namespace rcspp::column_generation {

/// @brief A @c MasterSolver over a HiGHS model the user built.
///
/// The user creates the master in a @c Highs object (its rows, and any columns of their own, such
/// as slacks or starting columns) and passes the indices of the rows that pricing columns enter,
/// in the order that @c Row::index refers to. The master solver adds the generated columns to that
/// model and reads its duals, so the user can keep working with the model after the run. Rows
/// must not be deleted from the model during the run: that would shift the indices.
///
/// - **Sense:** a maximizing model works too: the master solver negates the costs it adds, the
///   objective and the duals, so that the master is seen as a minimization.
/// - **Integrality:** LP solves set HiGHS's `solve_relaxation` option, so the model's integer
///   columns stay integer and are solved as continuous; the integer solve unsets it and gives the
///   generated columns the requested type (a binary column is an integer one with an upper bound
///   of at most 1, until the next LP solve). The upper bounds that LP solve puts back are those
///   the columns had when added: a change the user makes to a generated column's upper bound
///   during the run is undone after a binary integer solve.
/// - **Solver options:** every solve sets `time_limit` (counted from the solve: HiGHS counts it
///   over all the model's solves), `threads` and `output_flag` from its parameters. HiGHS fixes
///   its thread count at its first solve in the process and refuses a run asking for another: the
///   master solver then runs with the count in force and logs a warning. The simplex method is
///   left to HiGHS, which takes primal simplex after added columns.
/// - **Status:** "infeasible or unbounded" is solved again once without presolve to tell the two
///   apart. An LP solve that ends with an unknown status, which HiGHS can give after a false
///   cycling detection, is solved again once from its basis on a cleared solver.
/// - **Errors:** an option HiGHS refuses throws @c std::invalid_argument.
///
/// Holds the model by reference: the caller keeps it alive.
class HighsMasterSolver final : public MasterSolver {
    public:
        /// @brief Wraps @p highs.
        ///
        /// @param highs The master, built by the user.
        /// @param rows  The indices of its rows that columns enter, in master row order.
        /// @throws std::out_of_range For an index that is not a row of the model.
        HighsMasterSolver(Highs& highs, std::vector<HighsInt> rows);

        [[nodiscard]] size_t number_of_rows() const override { return rows_.size(); }

        void row_bounds(std::span<double> lower, std::span<double> upper) const override;

        /// @brief Appends rows to the model, ranges included.
        ///
        /// @throws std::invalid_argument For bounds that no row holds (NaN, `lower = +∞`,
        ///                               `upper = −∞`, `lower > upper`). A refused batch adds
        ///                               no row.
        void add_rows(const RowBatch& rows) override;

        void add_columns(const ColumnBatch& columns) override;

        SolverStatus solve_relaxation(const RelaxationParams& params) override;

        [[nodiscard]] double objective() const override;

        void duals(std::span<double> out) const override;

        /// @copydoc MasterSolver::primal
        /// @throws std::out_of_range For an id this master solver did not receive.
        void primal(std::span<const ColumnId> ids, std::span<double> out) const override;

        SolverStatus solve_integer(const IntegerParams& params) override;

        /// @brief Sets the HiGHS option @p name.
        /// @throws std::invalid_argument When HiGHS refuses the name or the value.
        void set_option(std::string_view name, const OptionValue& value) override;

        [[nodiscard]] Capabilities capabilities() const override {
            return Capabilities{.add_rows = true, .integer_solve = true};
        }

        /// @brief The wrapped model.
        [[nodiscard]] Highs& highs() const { return highs_; }

        /// @brief The HiGHS column index of column @p id.
        /// @throws std::out_of_range For an id this master solver did not receive.
        [[nodiscard]] HighsInt column(ColumnId id) const;

    private:
        // +1 for a minimizing model, −1 for a maximizing one: turns its values into ours.
        [[nodiscard]] double sense_sign() const;

        // Sets the options every solve sets.
        void set_solve_options(double timeout_s, int threads, bool verbose);

        // Gives the generated columns their upper bounds back, after an integer solve made them
        // binary.
        void restore_upper_bounds();

        // Runs HiGHS once; a run it refuses is a solve error.
        HighsModelStatus run_once();

        // Runs HiGHS; a run refused for its thread count runs again with the count in force, an
        // "infeasible or unbounded" answer is solved again without presolve, and an LP stopped
        // with an unknown status again from its basis.
        SolverStatus run(bool integer);

        Highs& highs_;
        std::vector<HighsInt> rows_;
        SolverColumns<HighsInt> columns_;
        // The upper bound of each generated column, as added.
        std::vector<double> upper_bounds_;
        // The last integer solve made the generated columns binary.
        bool binary_bounds_ = false;
        // The thread count the current solve asks for, and the last one warned about.
        int requested_threads_ = 0;
        int warned_threads_ = 0;
};

}  // namespace rcspp::column_generation
