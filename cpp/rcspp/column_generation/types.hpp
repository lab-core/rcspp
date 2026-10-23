// Copyright (c) 2025 Laboratory for Combinatorial Optimization in Real-time Environment.
// All rights reserved.

#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <variant>

namespace rcspp::column_generation {

/// @brief Identifier of a master column, assigned by @c MasterProblem.
///
/// Stable for the column's lifetime: a master solver maps ids to its own column indices, which may
/// shift when columns are removed.
using ColumnId = std::uint64_t;

/// @brief Outcome of an LP or MIP solve by a @c MasterSolver.
enum class SolverStatus {
    Optimal,     ///< solved to optimality
    Infeasible,  ///< the model has no feasible solution
    Unbounded,   ///< the objective is unbounded below
    TimeLimit,   ///< stopped by the time limit
    Other,       ///< any other stop (numerical trouble, interruption…)
};

/// @brief Sense of a master row `a·x (sense) right_hand_side`.
enum class RowSense {
    Equal,         ///< `a·x = right_hand_side`
    GreaterEqual,  ///< `a·x ≥ right_hand_side`
    LessEqual,     ///< `a·x ≤ right_hand_side`
};

/// @brief Type of the generated columns in the final integer solve.
enum class ColumnType {
    Integer,  ///< general integer: a column may be used several times
    Binary,   ///< used at most once
};

/// @brief Rows to add, as ranges `lower ≤ a·x ≤ upper` (infinite bounds allowed).
struct RowBatch {
        /// @brief Lower bound of each row.
        std::span<const double> lower;
        /// @brief Upper bound of each row; same size as @c lower.
        std::span<const double> upper;
        /// @brief Optional row names (empty, or one per row).
        std::span<const std::string> names;
};

/// @brief Columns to add, in compressed sparse column (CSC) form.
///
/// Column `j` has the entries `row_indices[starts[j] .. starts[j + 1])` with the matching
/// `values`. The arrays are views: the caller keeps them alive during the call.
struct ColumnBatch {
        /// @brief Id of each column, assigned by the master.
        std::span<const ColumnId> ids;
        /// @brief Objective coefficient of each column.
        std::span<const double> cost;
        /// @brief Lower bound of each column.
        std::span<const double> lower;
        /// @brief Upper bound of each column.
        std::span<const double> upper;
        /// @brief Start of each column's entries, plus one end marker (`ids.size() + 1` values).
        std::span<const std::uint32_t> starts;
        /// @brief Row of each entry.
        std::span<const std::uint32_t> row_indices;
        /// @brief Coefficient of each entry.
        std::span<const double> values;

        /// @brief Number of columns in the batch.
        [[nodiscard]] size_t size() const { return ids.size(); }
};

/// @brief What a master solver can do beyond the required operations.
struct Capabilities {
        /// @brief Rows can be added through the master solver (false for a wrapped user model).
        bool add_rows = false;
        /// @brief @c MasterSolver::solve_integer is available.
        bool integer_solve = false;
        /// @brief @c MasterSolver::farkas returns a ray for an infeasible LP.
        bool farkas = false;
        /// @brief The basis can be read and set.
        bool basis = false;
};

/// @brief Options of one LP solve.
struct RelaxationParams {
        /// @brief Wall-clock limit in seconds.
        double timeout_s = std::numeric_limits<double>::infinity();
        /// @brief Number of threads; 0 lets the solver choose.
        int threads = 0;
        /// @brief Whether the solver prints its own log.
        bool verbose = false;
};

/// @brief Options of the final integer solve over the columns of the master.
struct IntegerParams {
        /// @brief Wall-clock limit in seconds.
        double timeout_s = std::numeric_limits<double>::infinity();
        /// @brief Relative optimality gap at which the solver may stop.
        double relative_gap = 1e-4;
        /// @brief Type given to the generated columns (the user's own variables keep theirs).
        ColumnType column_type = ColumnType::Integer;
        /// @brief Number of threads; 0 lets the solver choose.
        int threads = 0;
        /// @brief Whether the solver prints its own log.
        bool verbose = false;
};

/// @brief Value of a solver-specific option set by name.
using OptionValue = std::variant<bool, std::int64_t, double, std::string>;

}  // namespace rcspp::column_generation
