// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <span>
#include <stdexcept>
#include <string_view>

#include "rcspp/column_generation/types.hpp"

namespace rcspp::column_generation {

/// @brief Adapter to an external LP/MIP solver holding one master LP.
///
/// Every master solver presents the master as a **minimization**: one wrapping a maximizing
/// model negates its costs and duals. Duals follow one convention: `π_i = ∂objective/∂rhs_i`,
/// so a `≥` row has `π ≥ 0`, a `≤` row `π ≤ 0` and an `=` row a free sign.
///
/// Rows are numbered `0 … number_of_rows() − 1`. Columns are named by the @c ColumnId the master
/// assigns; the master solver maps ids to its own indices. The loop calls these methods a few times
/// per iteration, never inside pricing.
class MasterSolver {
    public:
        MasterSolver() = default;
        MasterSolver(const MasterSolver&) = delete;
        MasterSolver& operator=(const MasterSolver&) = delete;
        MasterSolver(MasterSolver&&) = delete;
        MasterSolver& operator=(MasterSolver&&) = delete;
        virtual ~MasterSolver() = default;

        /// @brief Number of rows of the master.
        [[nodiscard]] virtual size_t number_of_rows() const = 0;

        /// @brief Bounds `lower ≤ a·x ≤ upper` of every row.
        ///
        /// @param lower Receives the lower bounds; size @c number_of_rows().
        /// @param upper Receives the upper bounds; size @c number_of_rows().
        virtual void row_bounds(std::span<double> lower, std::span<double> upper) const = 0;

        /// @brief Appends rows. Optional: only master solvers that set @c Capabilities::add_rows
        ///        override it; a master solver wrapping a user's model, whose rows exist, does not.
        ///
        /// New rows get the next row numbers.
        /// @throws std::logic_error Unless overridden.
        virtual void add_rows(const RowBatch& /*rows*/) {
            throw std::logic_error("MasterSolver::add_rows: this master solver cannot add rows");
        }

        /// @brief Appends columns.
        ///
        /// @param columns The columns to add, with their ids.
        virtual void add_columns(const ColumnBatch& columns) = 0;

        /// @brief Solves the LP relaxation; integer variables are relaxed in place.
        ///
        /// @param params Time limit, threads and log options.
        /// @return The solver's status.
        virtual SolverStatus solve_relaxation(const RelaxationParams& params) = 0;

        /// @brief Objective value of the last solve (minimization).
        [[nodiscard]] virtual double objective() const = 0;

        /// @brief Duals of the last LP solve, normalized as described above.
        ///
        /// @param out Receives one dual per row; size @c number_of_rows().
        virtual void duals(std::span<double> out) const = 0;

        /// @brief Values of the given columns in the last solve.
        ///
        /// @param ids The columns to read.
        /// @param out Receives one value per id.
        virtual void primal(std::span<const ColumnId> ids, std::span<double> out) const = 0;

        /// @brief Restores integrality, makes the generated columns of @p params' type, solves.
        ///
        /// @param params Time limit, gap and column type.
        /// @return The solver's status.
        virtual SolverStatus solve_integer(const IntegerParams& params) = 0;

        /// @brief Farkas ray of an infeasible LP, when @c Capabilities::farkas is set.
        ///
        /// @return @c false when the master solver gives no ray (the default).
        [[nodiscard]] virtual bool farkas(std::span<double> /*out*/) const { return false; }

        /// @brief Sets a solver-specific option by name.
        ///
        /// @param name  The solver's own option name.
        /// @param value The value.
        virtual void set_option(std::string_view name, const OptionValue& value) = 0;

        /// @brief What this master solver supports.
        [[nodiscard]] virtual Capabilities capabilities() const = 0;
};

}  // namespace rcspp::column_generation
